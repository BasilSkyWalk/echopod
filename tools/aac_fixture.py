import argparse
import math
import subprocess
import sys
import tempfile
from pathlib import Path

FIXTURES = [
    {"name": "stereo_44k", "rate": 44100, "channels": 2, "frames": 12000, "bitrate": "128k",
     "flags": []},
    {"name": "mono_48k", "rate": 48000, "channels": 1, "frames": 9000, "bitrate": "64k",
     "flags": []},
    {"name": "stereo_96k_faststart", "rate": 96000, "channels": 2, "frames": 10000,
     "bitrate": "192k", "flags": ["-movflags", "+faststart"]},
]


class Noise:
    def __init__(self, seed):
        self.state = seed

    def next(self):
        self.state = (self.state * 1103515245 + 12345) & 0x7FFFFFFF
        return self.state / 0x7FFFFFFF * 2 - 1


def source_bytes(fixture):
    noise = Noise(3 + fixture["channels"])
    out = bytearray()
    for index in range(fixture["frames"]):
        time = index / fixture["rate"]
        for channel in range(fixture["channels"]):
            tone = math.sin(2 * math.pi * (440 + 220 * channel) * time)
            overtone = 0.3 * math.sin(2 * math.pi * 1250 * time + channel)
            value = int((0.5 * tone + 0.2 * overtone + 0.02 * noise.next()) * 32767)
            out += max(-32768, min(32767, value)).to_bytes(2, "little", signed=True)
    return bytes(out)


def run(*command):
    subprocess.run(command, check=True)


def encode(fixture, raw, output):
    with tempfile.TemporaryDirectory() as directory:
        source = Path(directory) / "input.raw"
        source.write_bytes(raw)
        run("ffmpeg", "-y", "-v", "error", "-f", "s16le", "-ar", str(fixture["rate"]),
            "-ac", str(fixture["channels"]), "-i", str(source), "-map_metadata", "-1",
            "-fflags", "+bitexact", "-flags:a", "+bitexact", "-c:a", "aac",
            "-b:a", fixture["bitrate"], "-aac_pns", "0", *fixture["flags"], str(output))


def decode(source, output):
    run("ffmpeg", "-y", "-v", "error", "-i", str(source), "-f", "s16le", "-c:a", "pcm_s16le",
        str(output))


def parse_arguments():
    parser = argparse.ArgumentParser(description="Write the AAC files and ffmpeg's decoded samples used by the C tests")
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


def main():
    arguments = parse_arguments()
    arguments.output.mkdir(parents=True, exist_ok=True)
    for fixture in FIXTURES:
        m4a = arguments.output / f"{fixture['name']}.m4a"
        encode(fixture, source_bytes(fixture), m4a)
        decode(m4a, arguments.output / f"{fixture['name']}.pcm")
        print(f"wrote {fixture['name']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
