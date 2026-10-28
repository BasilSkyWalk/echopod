import argparse
import importlib.util
import os
import re
import string
import struct
import subprocess
import sys
import time
import unicodedata
from dataclasses import dataclass
from datetime import datetime, timedelta
from pathlib import Path

import cover_export
import library_index

ECHO_FOLDER = ".echo"
MEBIBYTE = 1024 * 1024
WRITE_CHUNK_BYTES = MEBIBYTE
SCAN_MAGIC = b"MESC"
SCAN_RECORD = struct.Struct("<4sIII16s")
SCAN_PRINT_BYTES = 16
WORD_MASK = 0xFFFFFFFF
SECOND_SEED = cover_export.FNV_OFFSET ^ 0x5BD1E995
PATH_CAPACITY = 1024
MAX_DEPTH = 32
EXFAT_SIZE_LIMIT = 1 << 32
COUNTED_SUFFIXES = tuple(sorted(suffix.encode("ascii") for suffix in
                                library_index.AUDIO_SUFFIXES | library_index.PLAYLIST_SUFFIXES))
ART_NAMES = frozenset(f"{stem}{suffix}".encode("ascii")
                      for stem in cover_export.FOLDER_IMAGE_STEMS
                      for suffix in cover_export.FOLDER_IMAGE_SUFFIXES)
SYSTEM_FOLDER = b"system volume information"
UNIX_EPOCH = datetime(1970, 1, 1)
DOS_FIRST_YEAR = 1980
DOS_LAST_YEAR = 2107
NANOSECONDS = 1_000_000_000
FAT_NAMES = {"fat", "fat12", "fat16", "fat32", "vfat", "msdos"}
EXFAT_NAMES = {"exfat"}
DRIVE_LETTER = re.compile(r"^[A-Za-z]:$")
SEM_FAILCRITICALERRORS = 0x0001
MAC_MOUNT_LINE = re.compile(r"^.* on (.+) \(([^,)]+)(?:, ([^)]*))?\)$")
MOUNT_ESCAPE = re.compile(r"\\([0-7]{3})")
REQUIREMENTS = Path(__file__).resolve().with_name("requirements.txt")
MAC_CAVEAT = ("on a Mac the player's file times are a best guess; after the player has scanned "
              "the card, --check shows whether they match")
LINUX_CAVEAT = ("on Linux the card's file times are read as UTC (or the mount's time_offset); "
                "after the player has scanned the card, --check shows whether they match")
OTHER_CAVEAT = "this isn't a FAT32 or exFAT card, so the file times may not match the player's"


@dataclass(frozen=True)
class CardFile:
    name: str
    standard_size: int
    header_size: int


LIBRARY_FILE = CardFile("library.db", 4 * MEBIBYTE, library_index.HEADER_SIZE)
COVERS_FILE = CardFile("covers.bin", 80 * MEBIBYTE, cover_export.HEADER.size)
SCAN_FILE = CardFile("scan.bin", 4096, SCAN_RECORD.size)


@dataclass(frozen=True)
class CardEntry:
    name: bytes
    directory: bool
    size: int
    modified: int
    location: object = None


@dataclass(frozen=True)
class Fingerprint:
    complete: bool
    first: int
    second: int
    entries: int


@dataclass(frozen=True)
class CardVolume:
    file_system: str
    supported: bool
    exfat: bool
    fixed_offset: int
    caveat: str


@dataclass
class SyncOptions:
    library: bool = True
    covers: bool = True
    fingerprint: bool = True
    dry_run: bool = False
    verbose: bool = False


class SyncError(Exception):
    pass


class FingerprintHash:
    def __init__(self):
        self.first = cover_export.FNV_OFFSET
        self.second = SECOND_SEED
        self.entries = 0

    def mix(self, data):
        self.first = cover_export.fnv1a(data, self.first)
        self.second = cover_export.fnv1a(data, self.second)

    def mix_entry(self, folder, entry):
        self.mix(folder + b"/" + entry.name + struct.pack(
            "<III", 1 if entry.directory else 0, entry.size & WORD_MASK,
            entry.modified & WORD_MASK))
        self.entries += 1


def fingerprint_counts(name, directory):
    folded = name.lower()
    if directory:
        return folded != SYSTEM_FOLDER
    return folded in ART_NAMES or any(
        len(folded) > len(suffix) and folded.endswith(suffix) for suffix in COUNTED_SUFFIXES)


def child_path(folder, name):
    return (b"" if folder == b"/" else folder) + b"/" + name


def walk_folder(state, list_folder, folder, entries, depth):
    for entry in entries:
        if entry.name.startswith(b".") or not fingerprint_counts(entry.name, entry.directory):
            continue
        state.mix_entry(folder, entry)
        if not entry.directory:
            continue
        path = child_path(folder, entry.name)
        if depth + 1 >= MAX_DEPTH or len(path) + 1 > PATH_CAPACITY:
            return False
        listing = list_folder(entry.location)
        if listing is not None and not walk_folder(state, list_folder, path, listing, depth + 1):
            return False
    return True


def music_fingerprint(list_folder, root_location):
    state = FingerprintHash()
    listing = list_folder(root_location)
    complete = listing is not None and walk_folder(state, list_folder, b"/", listing, 0)
    return Fingerprint(complete, state.first, state.second, state.entries)


def dos_stamp(local_seconds):
    moment = UNIX_EPOCH + timedelta(seconds=local_seconds)
    if not DOS_FIRST_YEAR <= moment.year <= DOS_LAST_YEAR:
        return 0
    date = (moment.year - DOS_FIRST_YEAR) << 9 | moment.month << 5 | moment.day
    clock = moment.hour << 11 | moment.minute << 5 | moment.second // 2
    return date << 16 | clock


def current_offset():
    return time.localtime().tm_gmtoff


def card_name(name, platform=sys.platform):
    if platform == "win32":
        return name.encode("utf-8", "surrogatepass")
    if platform == "darwin":
        name = unicodedata.normalize("NFC", name)
    return name.encode("utf-8", "surrogateescape")


def card_stamp(stat, volume):
    seconds = stat.st_mtime_ns // NANOSECONDS
    if volume.exfat:
        return seconds // 2 & WORD_MASK
    return dos_stamp(seconds + volume.fixed_offset)


def host_entry(entry, volume):
    stat = entry.stat(follow_symlinks=False)
    directory = entry.is_dir(follow_symlinks=False)
    if volume.exfat and not directory and stat.st_size >= EXFAT_SIZE_LIMIT:
        return None
    return CardEntry(card_name(entry.name), directory, 0 if directory else stat.st_size,
                     card_stamp(stat, volume), entry.path)


def host_lister(volume):
    def list_folder(location):
        try:
            with os.scandir(location) as listing:
                found = [entry for entry in listing if not entry.name.startswith(".")]
        except OSError:
            return None
        entries = (host_entry(entry, volume) for entry in found)
        return [entry for entry in entries if entry is not None]
    return list_folder


def card_fingerprint(card, volume):
    return music_fingerprint(host_lister(volume), str(card))


def scan_record(fingerprint, version):
    return SCAN_RECORD.pack(SCAN_MAGIC, fingerprint.first, fingerprint.second,
                            fingerprint.entries, version)


def stored_fingerprint(data):
    if len(data) < SCAN_RECORD.size:
        return None
    magic, first, second, entries, _ = SCAN_RECORD.unpack_from(data)
    return Fingerprint(True, first, second, entries) if magic == SCAN_MAGIC else None


def stored_version(data):
    if len(data) < SCAN_RECORD.size:
        return bytes(SCAN_RECORD.size - SCAN_PRINT_BYTES)
    return bytes(data[SCAN_PRINT_BYTES:SCAN_RECORD.size])


def read_prefix(path, length):
    try:
        with open(path, "rb") as file:
            return file.read(length)
    except OSError:
        return b""


def target_size(existing, needed, standard):
    if existing is not None and existing >= needed:
        return existing
    return max(standard, needed)


def existing_size(path):
    try:
        return path.stat().st_size
    except FileNotFoundError:
        return None


def zero_fill(file, start, end):
    file.seek(start)
    while start < end:
        length = min(WRITE_CHUNK_BYTES, end - start)
        file.write(bytes(length))
        start += length


def settle(file):
    file.flush()
    os.fsync(file.fileno())


def write_card_file(path, data, card_file):
    size = target_size(existing_size(path), len(data), card_file.standard_size)
    header = min(card_file.header_size, len(data))
    with open(path, "r+b" if path.exists() else "w+b") as file:
        file.write(bytes(header))
        settle(file)
        file.seek(header)
        file.write(data[header:])
        end = file.seek(0, os.SEEK_END)
        zero_fill(file, end, size)
        settle(file)
        file.seek(0)
        file.write(data[:header])
        settle(file)
    return size


def forget_fingerprint(path):
    if existing_size(path):
        with open(path, "r+b") as file:
            file.write(bytes(len(SCAN_MAGIC)))
            settle(file)


def file_system_name(card):
    if sys.platform == "win32":
        return windows_file_system(card)
    mounts = linux_mounts() if sys.platform.startswith("linux") else mac_mounts()
    mount = mount_for(card, mounts)
    return (mount[1], mount[2]) if mount else (None, "")


def windows_file_system(card):
    import ctypes
    name = ctypes.create_unicode_buffer(64)
    found = ctypes.windll.kernel32.GetVolumeInformationW(
        ctypes.c_wchar_p(card.anchor), None, 0, None, None, None, name, len(name))
    return (name.value if found else None), ""


def linux_mounts():
    try:
        lines = Path("/proc/self/mounts").read_text(encoding="utf-8",
                                                    errors="surrogateescape").splitlines()
    except OSError:
        return []
    mounts = []
    for line in lines:
        fields = line.split()
        if len(fields) >= 4:
            point = MOUNT_ESCAPE.sub(lambda match: chr(int(match.group(1), 8)), fields[1])
            mounts.append((point, fields[2], fields[3]))
    return mounts


def mac_mounts():
    try:
        output = subprocess.run(["mount"], capture_output=True, text=True, check=False).stdout
    except OSError:
        return []
    mounts = []
    for line in output.splitlines():
        match = MAC_MOUNT_LINE.match(line)
        if match:
            mounts.append((match.group(1), match.group(2), match.group(3) or ""))
    return mounts


def mount_for(card, mounts):
    target = os.path.realpath(card)
    best = None
    for point, kind, options in mounts:
        inside = target == point or target.startswith(point.rstrip("/") + "/")
        if inside and (best is None or len(point) > len(best[0])):
            best = (point, kind, options)
    return best


def mount_option_offset(options):
    for option in options.split(","):
        if option == "tz=UTC":
            return 0
        if option.startswith("time_offset="):
            return int(option.split("=", 1)[1]) * 60
    return 0


def card_volume(name, options="", platform=sys.platform):
    kind = (name or "").lower()
    exfat = kind in EXFAT_NAMES
    if kind not in FAT_NAMES and not exfat:
        return CardVolume(name or "unknown", False, False, current_offset(), OTHER_CAVEAT)
    label = "exFAT" if exfat else "FAT32" if kind == "fat32" else "FAT"
    if exfat:
        return CardVolume(label, True, True, 0, "")
    if platform.startswith("linux"):
        return CardVolume(label, True, False, mount_option_offset(options), LINUX_CAVEAT)
    if platform == "win32":
        return CardVolume(label, True, False, current_offset(), "")
    return CardVolume(label, True, False, current_offset(), MAC_CAVEAT)


def detect_volume(card):
    name, options = file_system_name(card)
    return card_volume(name, options)


def card_root(text):
    if DRIVE_LETTER.match(text):
        text += os.sep
    return Path(text).expanduser().resolve()


def windows_drive_roots():
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetErrorMode(SEM_FAILCRITICALERRORS)
    mask = kernel32.GetLogicalDrives()
    return [Path(f"{letter}:\\") for index, letter in enumerate(string.ascii_uppercase)
            if mask >> index & 1]


def mounted_roots(platform=sys.platform):
    if platform == "win32":
        return windows_drive_roots()
    if platform == "darwin":
        return sorted(Path("/Volumes").glob("*"))
    user = os.environ.get("USER", "")
    return sorted([*Path("/media", user).glob("*"), *Path("/run/media", user).glob("*")])


def player_cards(roots):
    cards = []
    for root in roots:
        try:
            if (root / ECHO_FOLDER).is_dir():
                cards.append(root)
        except OSError:
            continue
    return cards


def typed_path(text, platform=sys.platform):
    text = text.strip()
    if len(text) >= 2 and text[0] == text[-1] and text[0] in "\"'":
        return text[1:-1]
    if platform != "win32":
        return text.replace("\\ ", " ")
    return text


def ask_for_card(cards, read=input):
    if len(cards) == 1:
        answer = typed_path(read(f"Card to sync [{cards[0]}]: "))
        return answer or str(cards[0])
    if cards:
        print("Cards that have been in an EchoPod player: " +
              ", ".join(str(card) for card in cards))
    answer = typed_path(read("Card drive or folder (for example E: or /Volumes/CARD; you can "
                             "drag it here): "))
    if not answer:
        raise SyncError("No card given. Nothing was written.")
    return answer


def check_target(card, volume, echo_exists):
    if not card.is_dir():
        raise SyncError(f"{card} isn't a folder or drive. Give the card's drive, for example E:\\")
    if echo_exists:
        return
    if not os.path.ismount(card):
        raise SyncError(f"{card} isn't the top of a memory card. The player reads music from the "
                        "whole card, so give the card's drive (for example E:\\ on Windows or "
                        "/Volumes/CARD on a Mac).")
    if not volume.supported:
        raise SyncError(f"{card} is {volume.file_system}, not a FAT32 or exFAT memory card.")


def ensure_echo_folder(echo, options, assume_yes):
    if echo.is_dir():
        return
    if options.dry_run:
        print(f"Would create {echo}")
        return
    if not assume_yes:
        if not sys.stdin.isatty():
            raise SyncError(f"{echo} doesn't exist yet. Run again with --yes to create it.")
        answer = input(f"{echo} doesn't exist yet (the player hasn't used this card). "
                       "Create it? [y/N] ")
        if answer.strip().lower() not in ("y", "yes"):
            raise SyncError("Nothing was written.")
    echo.mkdir()


def require_dependencies():
    packages = {"mutagen": "mutagen", "PIL": "Pillow"}
    missing = [package for module, package in packages.items()
               if importlib.util.find_spec(module) is None]
    if missing:
        raise SyncError(f"{', '.join(missing)} missing. Install the tools' requirements with: "
                        f'"{sys.executable}" -m pip install -r "{REQUIREMENTS}"')


def step(text):
    print(f"{text:<28}", end="", flush=True)


def human_size(size):
    if size >= MEBIBYTE:
        return f"{size / MEBIBYTE:.1f} MB"
    return f"{max(1, round(size / 1024))} KB"


def plural(count, word, words=None):
    return f"{count:,} {word if count == 1 else words or word + 's'}"


def build_library(card):
    step("Reading the music...")
    tracks, playlists, failures = library_index.scan(card)
    model = library_index.build_library(tracks, playlists)
    try:
        data = library_index.encode_library(model)
    except ValueError as error:
        raise SyncError(f"the library is too big for the player: {error}") from error
    print(f"{plural(len(model.tracks), 'track')}, {plural(len(model.albums), 'album')}, "
          f"{plural(len(model.playlists), 'playlist')}")
    for failure in failures:
        print(f"  skipped (tags unreadable) {failure}")
    for playlist in model.playlists:
        if playlist.unresolved:
            missing = plural(playlist.unresolved, "entry", "entries")
            print(f"  playlist {playlist.name}: {missing} not found")
    return model, data


def album_sources(card, model, library_path):
    if model is not None:
        return cover_export.album_sources(model)
    try:
        return cover_export.album_sources_from_library(library_path.read_bytes())
    except (OSError, ValueError):
        print("(no library on the card yet, reading the music first)")
        return cover_export.album_sources(build_library(card)[0])


def art_describer(volume):
    if not volume.supported:
        return None

    def describe(path):
        return card_name(path.name), card_stamp(path.stat(), volume)
    return describe


def covers_capacity(path, count):
    return target_size(existing_size(path), cover_export.stamped_size(count),
                       COVERS_FILE.standard_size)


def build_covers(card, sources, volume, options):
    step("Finding album covers...")
    covers = cover_export.collect_covers(card, sources, cover_export.cover_record,
                                         art_describer(volume))
    capacity = covers_capacity(card / ECHO_FOLDER / COVERS_FILE.name, len(covers.records))
    data = cover_export.encode_stamped(covers.records, covers.stamps, capacity)
    print(f"{plural(len(covers.records), 'cover')}, {len(covers.missing):,} without art")
    for album in covers.missing if options.verbose else ():
        print(f"  no cover: {album}")
    return data


def take_fingerprint(card, volume):
    try:
        return card_fingerprint(card, volume)
    except OSError as error:
        print(f"  couldn't read {error.filename}: {error.strerror}")
        return Fingerprint(False, 0, 0, 0)


def write_or_report(path, data, card_file, options):
    existing = existing_size(path)
    size = target_size(existing, len(data), card_file.standard_size)
    if options.dry_run:
        place = "a new" if existing is None else "the" if existing == size else "a larger"
        print(f"  would write {card_file.name}: {human_size(len(data))} into {place} "
              f"{human_size(size)} file")
        return
    try:
        write_card_file(path, data, card_file)
    except OSError as error:
        raise SyncError(f"couldn't write {path}: {error.strerror or error}") from error
    print(f"  {card_file.name:<11} {human_size(len(data))} in a {human_size(size)} file")


def record_fingerprint(echo, card, volume, before, options):
    step("Checking the music again...")
    after = take_fingerprint(card, volume)
    if not (before.complete and after.complete):
        print("incomplete")
        print("  The music couldn't be fingerprinted completely (folders more than 31 levels "
              "deep, paths over 1,023 bytes or an unreadable file), so the player will rescan.")
        return False
    if before != after:
        print("changed")
        print("  The music changed while this ran. The player will rescan it.")
        return False
    print(f"{after.entries:,} files and folders, unchanged")
    path = echo / SCAN_FILE.name
    record = scan_record(after, stored_version(read_prefix(path, SCAN_RECORD.size)))
    write_or_report(path, record, SCAN_FILE, options)
    return True


def sync(card, volume, options):
    echo = card / ECHO_FOLDER
    library_path = echo / LIBRARY_FILE.name
    before = None
    if options.library and options.fingerprint:
        step("Fingerprinting the music...")
        before = take_fingerprint(card, volume)
        print(f"{before.entries:,} files and folders" if before.complete else "incomplete")
    model, library_data, covers_data = None, None, None
    if options.library:
        model, library_data = build_library(card)
    if options.covers:
        covers_data = build_covers(card, album_sources(card, model, library_path), volume, options)
    if before is not None and not options.dry_run:
        forget_fingerprint(echo / SCAN_FILE.name)
    print(f"{'Planned for' if options.dry_run else 'Writing'} {echo}")
    if library_data is not None:
        write_or_report(library_path, library_data, LIBRARY_FILE, options)
    if covers_data is not None:
        write_or_report(echo / COVERS_FILE.name, covers_data, COVERS_FILE, options)
    if before is None:
        return False
    return record_fingerprint(echo, card, volume, before, options)


def print_next_steps(options, recorded, volume, seconds):
    print(f"Done in {seconds:.0f} s.")
    if options.dry_run:
        print("Nothing was written (--dry-run).")
        return
    print("Next: eject the card safely, then put it back in the player.")
    if recorded:
        print("The player will use this library without scanning the card again.")
        if volume.caveat:
            print(f"(Note: {volume.caveat}.)")
    elif options.library and options.fingerprint:
        print("The player will scan the card once more when it starts or after USB.")
    elif options.library:
        print("After USB the player compares the music with its own last scan, as usual.")
    if options.library and not options.covers:
        print("Covers were left as they were: new albums show without a cover until the next "
              "rescan on the player (Settings, Rescan library).")


def run_check(card, volume):
    print(f"Checking {card} ({volume.file_system})")
    computed = take_fingerprint(card, volume)
    stored = stored_fingerprint(read_prefix(card / ECHO_FOLDER / SCAN_FILE.name,
                                            SCAN_RECORD.size))
    print(f"  this computer: {describe(computed)}")
    print(f"  the player's record: {describe(stored)}")
    if stored is not None and computed.complete and computed == stored:
        print("They match: the player won't rescan this card.")
        return 0
    if stored is None:
        print("The card has no fingerprint yet, so the player will rescan it after USB.")
        return 1
    print("They differ: either the music changed since the player's last scan, or this computer "
          "reads the file times differently from the player (see docs/user-guide.md).")
    return 1


def describe(fingerprint):
    if fingerprint is None:
        return "none"
    if not fingerprint.complete:
        return "incomplete"
    return (f"{fingerprint.first:08x} {fingerprint.second:08x}, "
            f"{fingerprint.entries:,} files and folders")


def options_from(arguments):
    return SyncOptions(library=not arguments.covers_only, covers=not arguments.no_covers,
                       fingerprint=not arguments.no_fingerprint, dry_run=arguments.dry_run,
                       verbose=arguments.verbose)


def parse_arguments(argv):
    parser = argparse.ArgumentParser(
        description="Build the EchoPod library and album covers for a memory card on this "
                    "computer, so the player doesn't have to scan it.")
    parser.add_argument("card", nargs="?",
                        help="the card's drive or mount point, for example E:\\ or "
                             "/Volumes/CARD; without it, the tool asks")
    parts = parser.add_mutually_exclusive_group()
    parts.add_argument("--no-covers", action="store_true",
                       help="build the library only and leave covers.bin as it is")
    parts.add_argument("--covers-only", action="store_true",
                       help="rebuild covers.bin from the card's library.db only")
    parser.add_argument("--no-fingerprint", action="store_true",
                        help="leave scan.bin alone, so the player decides itself whether to "
                             "rescan")
    parser.add_argument("--check", action="store_true",
                        help="only compare this computer's music fingerprint with the player's")
    parser.add_argument("--dry-run", action="store_true",
                        help="do everything except writing to the card")
    parser.add_argument("--yes", action="store_true",
                        help="create the card's .echo folder without asking")
    parser.add_argument("--verbose", action="store_true", help="list the albums without art")
    return parser.parse_args(argv)


def card_argument(arguments):
    if arguments.card is not None:
        return arguments.card
    if not sys.stdin.isatty():
        raise SyncError("Give the card's drive or mount point, for example E: or /Volumes/CARD.")
    return ask_for_card(player_cards(mounted_roots()))


def wait_before_closing():
    try:
        input("Press Enter to close.")
    except (EOFError, KeyboardInterrupt):
        pass


def main(argv=None):
    arguments = parse_arguments(argv)
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(errors="replace")
    try:
        return run(arguments)
    except (EOFError, KeyboardInterrupt):
        print("\nStopped. Nothing more was written.", file=sys.stderr)
        return 1
    finally:
        if arguments.card is None and sys.stdin.isatty():
            wait_before_closing()


def run(arguments):
    started = time.monotonic()
    try:
        card = card_root(card_argument(arguments))
        volume = detect_volume(card)
        echo = card / ECHO_FOLDER
        check_target(card, volume, echo.is_dir())
        if arguments.check:
            return run_check(card, volume)
        require_dependencies()
        options = options_from(arguments)
        print(f"EchoPod sync: {card} ({volume.file_system})")
        ensure_echo_folder(echo, options, arguments.yes)
        recorded = sync(card, volume, options)
    except SyncError as error:
        print(f"echo_sync: {error}", file=sys.stderr)
        return 2
    print_next_steps(options, recorded, volume, time.monotonic() - started)
    return 0


if __name__ == "__main__":
    sys.exit(main())
