import argparse
import io
import struct
import subprocess
import tempfile
from pathlib import Path

from PIL import Image

TITLE = "Svefn-g-englar ✓"
ARTIST = "Sigur Rós"
ALBUM_ARTIST = "Sigur Rós & Friends"
ALBUM = "Ágætis byrjun"
GENRE = "Post-Rock"
DATE = "1999-06-12"
DISC = "1/2"
TRACK = "4/10"
SECONDS = 1
DSD_RATE = 2822400
DSD_TENTHS = 1


def cover_jpeg():
    buffer = io.BytesIO()
    Image.new("RGB", (16, 8), (200, 30, 40)).save(buffer, format="JPEG", quality=90)
    return buffer.getvalue()


def run(*command):
    subprocess.run(command, check=True, capture_output=True)


def encode(output, codec_arguments, rate=44100):
    run("ffmpeg", "-y", "-v", "error", "-f", "lavfi", "-i",
        f"sine=frequency=440:sample_rate={rate}:duration={SECONDS}", "-ac", "2",
        "-map_metadata", "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact",
        *codec_arguments, str(output))


def vorbis_comments(tags):
    tags["title"] = TITLE
    tags["artist"] = ARTIST
    tags["albumartist"] = ALBUM_ARTIST
    tags["album"] = ALBUM
    tags["genre"] = GENRE
    tags["date"] = DATE
    tags["discnumber"] = DISC
    tags["tracknumber"] = TRACK


def flac_picture():
    from mutagen.flac import Picture
    picture = Picture()
    picture.type = 3
    picture.mime = "image/jpeg"
    picture.width, picture.height, picture.depth = 16, 8, 24
    picture.data = cover_jpeg()
    return picture


def make_flac(path):
    from mutagen.flac import FLAC
    encode(path, ["-c:a", "flac"])
    audio = FLAC(path)
    audio.clear()
    vorbis_comments(audio)
    audio.add_picture(flac_picture())
    audio.save()


def make_untagged_flac(path):
    from mutagen.flac import FLAC
    encode(path, ["-c:a", "flac"], rate=48000)
    audio = FLAC(path)
    audio.clear()
    audio.save()


def make_ogg(path):
    import base64
    from mutagen.oggvorbis import OggVorbis
    encode(path, ["-c:a", "vorbis", "-strict", "-2"])
    audio = OggVorbis(path)
    audio.clear()
    vorbis_comments(audio)
    audio["metadata_block_picture"] = [base64.b64encode(flac_picture().write()).decode("ascii")]
    audio.save()


def id3_frames(tags, version):
    from mutagen import id3
    tags.add(id3.TIT2(encoding=3, text=TITLE))
    tags.add(id3.TPE1(encoding=3, text=ARTIST))
    tags.add(id3.TPE2(encoding=1, text=ALBUM_ARTIST))
    tags.add(id3.TALB(encoding=3, text=ALBUM))
    tags.add(id3.TCON(encoding=0, text="(17)" if version == 3 else GENRE))
    tags.add(id3.TRCK(encoding=0, text=TRACK))
    tags.add(id3.TPOS(encoding=0, text=DISC))
    if version == 3:
        tags.add(id3.TYER(encoding=0, text=DATE[:4]))
    else:
        tags.add(id3.TDRC(encoding=0, text=DATE))
    tags.add(id3.APIC(encoding=0, mime="image/jpeg", type=3, desc="", data=cover_jpeg()))


def tag_mp3(path, version):
    from mutagen.id3 import ID3
    tags = ID3()
    id3_frames(tags, version)
    tags.save(path, v2_version=version, v1=0)


def make_mp3_v24(path):
    encode(path, ["-c:a", "libmp3lame", "-b:a", "128k"])
    tag_mp3(path, 4)


def make_mp3_v23(path):
    encode(path, ["-c:a", "libmp3lame", "-q:a", "2"], rate=48000)
    tag_mp3(path, 3)


def make_mp3_v1(path):
    from mutagen.id3 import ID3, TALB, TCON, TIT2, TPE1, TRCK, TDRC
    encode(path, ["-c:a", "libmp3lame", "-b:a", "96k", "-write_xing", "0"], rate=32000)
    tags = ID3()
    tags.add(TIT2(encoding=0, text="Old Song"))
    tags.add(TPE1(encoding=0, text="Old Artist"))
    tags.add(TALB(encoding=0, text="Old Album"))
    tags.add(TDRC(encoding=0, text="1987"))
    tags.add(TRCK(encoding=0, text="7"))
    tags.add(TCON(encoding=0, text="Rock"))
    tags.save(path, v1=2)
    data = path.read_bytes()
    header_size = 10 + int.from_bytes(bytes(b & 0x7F for b in data[6:10]), "big")
    path.write_bytes(data[header_size:])


def mp4_tags(audio):
    from mutagen.mp4 import MP4Cover
    audio["\xa9nam"] = [TITLE]
    audio["\xa9ART"] = [ARTIST]
    audio["aART"] = [ALBUM_ARTIST]
    audio["\xa9alb"] = [ALBUM]
    audio["\xa9gen"] = [GENRE]
    audio["\xa9day"] = [DATE]
    audio["trkn"] = [(4, 10)]
    audio["disk"] = [(1, 2)]
    audio["covr"] = [MP4Cover(cover_jpeg(), imageformat=MP4Cover.FORMAT_JPEG)]


def make_m4a(path, codec):
    from mutagen.mp4 import MP4
    encode(path, ["-c:a", codec])
    audio = MP4(path)
    audio.clear()
    mp4_tags(audio)
    audio.save()


def make_aac(path):
    make_m4a(path, "aac")


def make_alac(path):
    make_m4a(path, "alac")


def make_wav(path):
    from mutagen.wave import WAVE
    encode(path, ["-c:a", "pcm_s16le"], rate=8000)
    audio = WAVE(path)
    audio.add_tags()
    id3_frames(audio.tags, 4)
    audio.save()


def make_wma(path):
    from mutagen.asf import ASF, ASFByteArrayAttribute
    encode(path, ["-c:a", "wmav2", "-b:a", "128k"])
    audio = ASF(path)
    audio.clear()
    audio["Title"] = [TITLE]
    audio["Author"] = [ARTIST]
    audio["WM/AlbumArtist"] = [ALBUM_ARTIST]
    audio["WM/AlbumTitle"] = [ALBUM]
    audio["WM/Genre"] = [GENRE]
    audio["WM/Year"] = [DATE[:4]]
    audio["WM/TrackNumber"] = ["4"]
    audio["WM/PartOfSet"] = [DISC]
    jpeg = cover_jpeg()
    mime = "image/jpeg".encode("utf-16-le") + b"\x00\x00"
    picture = struct.pack("<bI", 3, len(jpeg)) + mime + b"\x00\x00" + jpeg
    audio["WM/Picture"] = [ASFByteArrayAttribute(picture)]
    audio.save()


def ape_header(rate, channels, total_frames, blocks_per_frame, final_frame_blocks):
    descriptor = struct.pack("<4sHHIIIIIII16s", b"MAC ", 3990, 0, 52, 24, 0, 0, 1000, 0, 0,
                             bytes(16))
    header = struct.pack("<HHIIIHHI", 2000, 0, blocks_per_frame, final_frame_blocks,
                         total_frames, 16, channels, rate)
    return descriptor + header


def make_ape(path):
    from mutagen.apev2 import APEBinaryValue, APEv2
    path.write_bytes(ape_header(44100, 2, 3, 73728, 4321) + bytes(1000))
    tags = APEv2()
    tags["Title"] = TITLE
    tags["Artist"] = ARTIST
    tags["Album Artist"] = ALBUM_ARTIST
    tags["Album"] = ALBUM
    tags["Genre"] = GENRE
    tags["Year"] = DATE[:4]
    tags["Track"] = TRACK
    tags["Disc"] = DISC
    tags["Cover Art (Front)"] = APEBinaryValue(b"cover.jpg\x00" + cover_jpeg())
    tags.save(path)


def make_dsf(path):
    from mutagen.dsf import DSF
    rate, channels, samples = DSD_RATE, 2, DSD_RATE * DSD_TENTHS // 10
    block = 4096
    blocks = (samples // 8 + block - 1) // block
    data = bytes(block * blocks * channels)
    fmt = struct.pack("<4sQIIIIIIQII", b"fmt ", 52, 1, 0, 2, channels, rate, 1, samples, block, 0)
    data_chunk = struct.pack("<4sQ", b"data", 12 + len(data)) + data
    total = 28 + len(fmt) + len(data_chunk)
    path.write_bytes(struct.pack("<4sQQQ", b"DSD ", 28, total, 0) + fmt + data_chunk)
    audio = DSF(path)
    audio.add_tags()
    id3_frames(audio.tags, 4)
    audio.save()


def dff_chunk(name, body):
    return name + struct.pack(">Q", len(body)) + body + (b"\x00" if len(body) % 2 else b"")


def make_dff(path):
    from mutagen.dsdiff import DSDIFF
    rate, channels = DSD_RATE, 2
    properties = (b"SND " + dff_chunk(b"FS  ", struct.pack(">I", rate)) +
                  dff_chunk(b"CHNL", struct.pack(">H", channels) + b"SLFTSRGT") +
                  dff_chunk(b"CMPR", b"DSD " + bytes([14]) + b"not compressed\x00"))
    body = (b"DSD " + dff_chunk(b"FVER", struct.pack(">I", 0x01050000)) +
            dff_chunk(b"PROP", properties) +
            dff_chunk(b"DSD ", bytes(rate // 8 * channels * DSD_TENTHS // 10)))
    path.write_bytes(b"FRM8" + struct.pack(">Q", len(body)) + body)
    audio = DSDIFF(path)
    audio.add_tags()
    id3_frames(audio.tags, 4)
    audio.save()


FIXTURES = {
    "tagged.flac": make_flac,
    "untagged.flac": make_untagged_flac,
    "tagged.ogg": make_ogg,
    "id3v24.mp3": make_mp3_v24,
    "id3v23_vbr.mp3": make_mp3_v23,
    "id3v1_no_xing.mp3": make_mp3_v1,
    "aac.m4a": make_aac,
    "alac.m4a": make_alac,
    "tagged.wav": make_wav,
    "tagged.wma": make_wma,
    "tagged.ape": make_ape,
    "tagged.dsf": make_dsf,
    "tagged.dff": make_dff,
}


def main():
    parser = argparse.ArgumentParser(description="Write small tagged audio files for tag tests")
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)
    for name, make in FIXTURES.items():
        with tempfile.TemporaryDirectory() as scratch:
            path = Path(scratch) / name
            make(path)
            (arguments.output / name).write_bytes(path.read_bytes())


if __name__ == "__main__":
    main()
