import argparse
import math
import subprocess
import sys
import tempfile
from pathlib import Path

RATE = 48000
FIXTURES = [
    {"name": "stereo_small_pages", "channels": 2, "frames": 19200,
     "flags": ["-b:a", "96k", "-oggpagesize", "700"]},
    {"name": "mono_60ms_speech", "channels": 1, "frames": 14400,
     "flags": ["-b:a", "16k", "-application", "voip", "-frame_duration", "60"]},
    {"name": "stereo_low_rate", "channels": 2, "frames": 9600,
     "flags": ["-b:a", "24k", "-application", "voip"]},
    {"name": "stereo_high_rate", "channels": 2, "frames": 9600,
     "flags": ["-b:a", "256k"]},
]
TAGS = {"title": "Svefn-g-englar ✓", "artist": "Sigur Rós",
        "albumartist": "Sigur Rós & Friends", "album": "Ágætis byrjun",
        "genre": "Post-Rock", "date": "1999-06-12", "discnumber": "1/2", "tracknumber": "4/10"}
PICTURE_KEY = b"metadata_block_picture="


class Noise:
    def __init__(self, seed):
        self.state = seed

    def next(self):
        self.state = (self.state * 1103515245 + 12345) & 0x7FFFFFFF
        return self.state / 0x7FFFFFFF * 2 - 1


def source_bytes(fixture):
    noise = Noise(5 + fixture["channels"])
    out = bytearray()
    for index in range(fixture["frames"]):
        time = index / RATE
        for channel in range(fixture["channels"]):
            tone = math.sin(2 * math.pi * (330 + 110 * channel) * time)
            overtone = 0.3 * math.sin(2 * math.pi * 1700 * time + channel)
            value = int((0.5 * tone + 0.2 * overtone + 0.02 * noise.next()) * 32767)
            out += max(-32768, min(32767, value)).to_bytes(2, "little", signed=True)
    return bytes(out)


def run(*command):
    subprocess.run(command, check=True)


def encode(fixture, raw, output):
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "input.raw"
        source.write_bytes(raw)
        run("ffmpeg", "-y", "-v", "error", "-f", "s16le", "-ar", str(RATE),
            "-ac", str(fixture["channels"]), "-i", str(source), "-map_metadata", "-1",
            "-fflags", "+bitexact", "-flags:a", "+bitexact", "-c:a", "libopus",
            *fixture["flags"], str(output))


def decode(source, output):
    run("ffmpeg", "-y", "-v", "error", "-c:a", "libopus", "-i", str(source), "-f", "s16le",
        "-c:a", "pcm_s16le", str(output))


def ogg_picture(path):
    data = path.read_bytes()
    found = data.lower().find(PICTURE_KEY)
    length = int.from_bytes(data[found - 4:found], "little")
    return data[found + len(PICTURE_KEY):found + length].decode("ascii")


def make_tagged(output, picture_source):
    arguments = ["ffmpeg", "-y", "-v", "error", "-f", "lavfi", "-i",
                 f"sine=frequency=440:sample_rate={RATE}:duration=1", "-ac", "2", "-map_metadata",
                 "-1", "-fflags", "+bitexact", "-flags:a", "+bitexact", "-c:a", "libopus"]
    tags = dict(TAGS, METADATA_BLOCK_PICTURE=ogg_picture(picture_source))
    for key, value in tags.items():
        arguments += ["-metadata", f"{key}={value}"]
    run(*arguments, str(output))


def parse_arguments():
    parser = argparse.ArgumentParser(description="Write the Opus files and libopus' decoded samples used by the C tests")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--picture-from", type=Path, required=True,
                        help="tagged.ogg from tests/fixtures/tags, whose cover the tagged file reuses")
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output.mkdir(parents=True, exist_ok=True)
    for fixture in FIXTURES:
        opus = arguments.output / f"{fixture['name']}.opus"
        encode(fixture, source_bytes(fixture), opus)
        decode(opus, arguments.output / f"{fixture['name']}.pcm")
        print(f"wrote {fixture['name']}")
    make_tagged(arguments.output / "tagged.opus", arguments.picture_from)
    print("wrote tagged")
    return 0


if __name__ == "__main__":
    sys.exit(main())
