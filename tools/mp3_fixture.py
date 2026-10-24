import argparse
import subprocess
import sys
from pathlib import Path

FIXTURES = [
    {"name": "cbr_128_44k_stereo_tagged", "rate": 44100, "channels": 2, "seconds": 1.5,
     "flags": ["-b:a", "128k", "-id3v2_version", "3", "-write_id3v1", "1",
               "-metadata", "title=Fixture"]},
    {"name": "cbr_192_48k_stereo_bare", "rate": 48000, "channels": 2, "seconds": 1.0,
     "flags": ["-b:a", "192k", "-write_xing", "0", "-id3v2_version", "0"]},
    {"name": "vbr_48k_mono", "rate": 48000, "channels": 1, "seconds": 2.0,
     "flags": ["-q:a", "2", "-id3v2_version", "0"]},
    {"name": "mpeg2_22k_stereo", "rate": 22050, "channels": 2, "seconds": 1.0,
     "flags": ["-b:a", "64k", "-id3v2_version", "0"]},
]


def generate(fixture, directory):
    channel_layout = "stereo" if fixture["channels"] == 2 else "mono"
    source = (f"sine=frequency=440:sample_rate={fixture['rate']}:duration={fixture['seconds']},"
              f"aformat=channel_layouts={channel_layout}")
    output = directory / f"{fixture['name']}.mp3"
    command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "lavfi", "-i", source,
               "-c:a", "libmp3lame", *fixture["flags"], str(output)]
    subprocess.run(command, check=True)
    return output


def parse_arguments(argv):
    parser = argparse.ArgumentParser(description="Generate the MP3 test fixtures with ffmpeg")
    parser.add_argument("--output", type=Path, required=True, help="fixture directory")
    return parser.parse_args(argv)


def main(argv=None):
    arguments = parse_arguments(argv)
    arguments.output.mkdir(parents=True, exist_ok=True)
    for fixture in FIXTURES:
        path = generate(fixture, arguments.output)
        print(f"{path.name}: {path.stat().st_size} bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
