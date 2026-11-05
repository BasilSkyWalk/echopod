"""Turns the listen history EchoPod keeps on a card into a .scrobbler.log for Last.fm uploaders.

The player writes /.echo/history.bin, a ring of 512-byte records (see docs/library-format.md).
A record says when a play started and ended in milliseconds since the player was switched on,
plus a session number that goes up once per start, and, when the player's clock was set (by
echo-sync, or by hand in Settings with a time zone), the Unix time the record was written. The
player also writes a "computer" mark whenever it connects to a computer over USB. A session with
a timed record is dated from it exactly. The rest is dated by counting back from now: the newest
record is taken to have happened at the moment the tool runs (or at --now), which is right when
the tool runs while the player is connected. Within one session the times are exact; across a
power-off the gap is unknown, so earlier sessions are placed back to back, ending where the next
one started.

The output is the Rockbox/iPod AudioScrobbler 1.1 log that existing uploaders read. Plays already
exported are remembered in /.echo/history-exported.txt, so running the tool again only adds new
ones.
"""
import argparse
import json
import re
import struct
import sys
import time
from dataclasses import asdict, dataclass
from pathlib import Path

SLOT_BYTES = 512
MAGIC = b"EHST"
FIELDS = struct.Struct("<4sIIBBHIIIIIII")
TEXT_END = SLOT_BYTES - 4
PLAY = 1
COMPUTER = 2
FNV_OFFSET = 2166136261
FNV_PRIME = 16777619
SHORTEST_SCROBBLE_MS = 30000
ENOUGH_LISTENING_MS = 240000
CONTINUE_SLACK_MS = 5000
HISTORY_NAME = "history.bin"
EXPORTED_NAME = "history-exported.txt"
LOG_NAME = ".scrobbler.log"
CLIENT = "EchoPod"
VERSION_LINE = re.compile(r'set\(ECHO_FIRMWARE_VERSION\s+"([^"]+)"')


def fnv1a(data):
    value = FNV_OFFSET
    for byte in data:
        value = ((value ^ byte) * FNV_PRIME) & 0xFFFFFFFF
    return value


@dataclass
class Record:
    sequence: int
    session: int
    kind: int
    number: int
    start_ms: int
    end_ms: int
    unix_time: int
    played_ms: int
    duration_ms: int
    start_position_ms: int
    end_position_ms: int
    artist: str = ""
    album: str = ""
    title: str = ""


def pack_slot(record):
    texts = b"\0".join(text.encode("utf-8") for text in (record.artist, record.album,
                                                        record.title)) + b"\0"
    body = FIELDS.pack(MAGIC, record.sequence, record.session, record.kind, record.number, 0,
                       record.start_ms, record.end_ms, record.unix_time, record.played_ms,
                       record.duration_ms, record.start_position_ms, record.end_position_ms)
    body = (body + texts).ljust(TEXT_END, b"\0")[:TEXT_END]
    return body + struct.pack("<I", fnv1a(body))


def unpack_slot(slot):
    if len(slot) < SLOT_BYTES or slot[:4] != MAGIC:
        return None
    if struct.unpack_from("<I", slot, TEXT_END)[0] != fnv1a(slot[:TEXT_END]):
        return None
    values = FIELDS.unpack_from(slot)
    if values[1] == 0:
        return None
    texts = slot[FIELDS.size:TEXT_END].split(b"\0")
    names = [text.decode("utf-8", errors="replace") for text in (texts + [b"", b"", b""])[:3]]
    return Record(values[1], values[2], values[3], values[4], *values[6:], *names)


def read_history(data):
    slots = len(data) // SLOT_BYTES
    found = []
    for index in range(slots):
        record = unpack_slot(data[index * SLOT_BYTES:(index + 1) * SLOT_BYTES])
        if record is not None and (record.sequence - 1) % slots == index:
            found.append(record)
    if not found:
        return []
    newest = max(record.sequence for record in found)
    return sorted((record for record in found if record.sequence > newest - slots),
                  key=lambda record: record.sequence)


def session_offsets(records, now_ms):
    sessions = []
    for record in records:
        if not sessions or sessions[-1][0] != record.session:
            sessions.append((record.session, []))
        sessions[-1][1].append(record)
    offsets = {}
    later_start = now_ms
    for session, members in reversed(sessions):
        clocked = [record for record in members if record.unix_time]
        if clocked:
            offset = clocked[-1].unix_time * 1000 - clocked[-1].end_ms
        else:
            offset = later_start - members[-1].end_ms
        offsets[session] = offset
        later_start = offset
    return offsets


def listened(duration_ms, played_ms):
    return (duration_ms > SHORTEST_SCROBBLE_MS and
            played_ms >= min(duration_ms // 2, ENOUGH_LISTENING_MS))


def same_song(first, second):
    return (first.artist, first.album, first.title, first.duration_ms) == (
        second.artist, second.album, second.title, second.duration_ms)


def continues(previous, record):
    return (previous is not None and record is not None and same_song(previous, record) and
            record.start_position_ms >= CONTINUE_SLACK_MS and
            abs(record.start_position_ms - previous.end_position_ms) <= CONTINUE_SLACK_MS)


@dataclass
class Play:
    sequence: int
    artist: str
    album: str
    title: str
    number: int
    length_s: int
    rating: str
    timestamp: int
    played_ms: int


def plays_of(records, now_ms):
    offsets = session_offsets(records, now_ms)
    songs = [record for record in records if record.kind == PLAY]
    plays = []
    previous = None
    chain_played = 0
    chain_start = 0
    chain_done = False
    for index, record in enumerate(songs):
        start = (offsets[record.session] + record.start_ms) // 1000
        if continues(previous, record):
            chain_played += record.played_ms
        else:
            chain_played, chain_start, chain_done = record.played_ms, start, False
        following = songs[index + 1] if index + 1 < len(songs) else None
        rating = None
        if not chain_done and listened(record.duration_ms, chain_played):
            rating, chain_done = "L", True
        elif not chain_done and not continues(record, following):
            rating = "S"
        if rating is not None:
            plays.append(Play(record.sequence, record.artist, record.album, record.title,
                              record.number, record.duration_ms // 1000, rating, chain_start,
                              chain_played))
        previous = record
    return plays


def field(text):
    return text.replace("\t", " ").replace("\r", " ").replace("\n", " ")


def log_line(play):
    number = str(play.number) if play.number else ""
    return "\t".join([field(play.artist), field(play.album), field(play.title), number,
                      str(play.length_s), play.rating, str(play.timestamp), ""]) + "\n"


def firmware_version():
    cmake = Path(__file__).resolve().parents[1] / "platform" / "device" / "CMakeLists.txt"
    try:
        found = VERSION_LINE.search(cmake.read_text(encoding="utf-8"))
    except OSError:
        found = None
    return found.group(1) if found else ""


def log_header(version):
    client = f"{CLIENT} {version}".rstrip()
    return f"#AUDIOSCROBBLER/1.1\n#TZ/UTC\n#CLIENT/{client}\n"


def read_exported(path, newest):
    try:
        exported = int(path.read_text(encoding="ascii").split()[0])
    except (OSError, ValueError, IndexError):
        return 0
    return exported if exported <= newest else 0


def write_log(output, plays, version):
    fresh = not output.exists() or output.stat().st_size == 0
    with output.open("a", encoding="utf-8", newline="\n") as log:
        if fresh:
            log.write(log_header(version))
        elif not output.read_bytes().endswith(b"\n"):
            log.write("\n")
        for play in plays:
            log.write(log_line(play))


@dataclass
class Export:
    plays: list
    skipped: int
    lost: int


def export(card, output=None, now_ms=None, include_skipped=False, everything=False,
           dry_run=False, version=None):
    root = Path(card)
    echo = root / ".echo"
    records = read_history((echo / HISTORY_NAME).read_bytes())
    now_ms = int(time.time() * 1000) if now_ms is None else now_ms
    newest = records[-1].sequence if records else 0
    marker = echo / EXPORTED_NAME
    exported = 0 if everything else read_exported(marker, newest)
    oldest = records[0].sequence if records else 1
    lost = max(0, oldest - exported - 1) if exported else 0
    fresh = [play for play in plays_of(records, now_ms) if play.sequence > exported]
    chosen = [play for play in fresh if include_skipped or play.rating == "L"]
    if not dry_run:
        if chosen:
            write_log(Path(output) if output else root / LOG_NAME, chosen,
                      firmware_version() if version is None else version)
        if records:
            marker.write_text(f"{newest}\n", encoding="ascii")
    return Export(chosen, len(fresh) - len([play for play in fresh if play.rating == "L"]),
                  lost)


def clock_text(timestamp):
    return time.strftime("%Y-%m-%d %H:%M", time.localtime(timestamp))


def main(arguments=None):
    parser = argparse.ArgumentParser(
        description="Write the songs EchoPod played to a .scrobbler.log for Last.fm or "
                    "ListenBrainz uploaders. Run it while the player is connected over USB.")
    parser.add_argument("card", help="the card's root folder, for example E:\\ or /media/card")
    parser.add_argument("--output", help="where to write the log (default: .scrobbler.log at "
                                         "the card's root); an existing log is added to")
    parser.add_argument("--now", type=int,
                        help="the Unix time the newest record happened at (default: now)")
    parser.add_argument("--include-skipped", action="store_true",
                        help="also write the skipped plays, rated S")
    parser.add_argument("--all", action="store_true",
                        help="export every play on the card again, not only the new ones")
    parser.add_argument("--json", action="store_true",
                        help="print the new plays as JSON and write nothing")
    options = parser.parse_args(arguments)
    root = Path(options.card)
    source = root / ".echo" / HISTORY_NAME
    if not source.is_file():
        print(f"no listen history at {source}", file=sys.stderr)
        return 1
    now_ms = None if options.now is None else options.now * 1000
    result = export(root, options.output, now_ms, options.include_skipped or options.json,
                    options.all, dry_run=options.json)
    if options.json:
        print(json.dumps([asdict(play) for play in result.plays], indent=2, ensure_ascii=False))
        return 0
    listened_count = len([play for play in result.plays if play.rating == "L"])
    skipped_count = len(result.plays) - listened_count
    target = options.output or root / LOG_NAME
    counts = f"{listened_count} listened" + (f" and {skipped_count} skipped" if skipped_count
                                               else "")
    if result.plays:
        print(f"Wrote {counts} plays to {target} (from "
              f"{clock_text(result.plays[0].timestamp)} to "
              f"{clock_text(result.plays[-1].timestamp)}).")
    else:
        print("No new plays to export.")
    if result.skipped and not options.include_skipped:
        print(f"Left out {result.skipped} skipped plays (--include-skipped writes them).")
    if result.lost:
        print(f"{result.lost} older records were overwritten before they were exported.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
