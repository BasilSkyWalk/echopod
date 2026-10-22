import argparse
import sys
from pathlib import Path

ECHO_MINI_CONFIG = (
    "\tconfig.emDeviceType = RKNANO_DEVICE;\n"
    "\tconfig.usPid = 0x262D;\n"
    "\tconfig.usVid = 0x2207;\n"
    "\tm_deviceConfigSet.push_back(config);\n"
    "\n"
)

RKNANO_CONFIG = (
    "\tconfig.emDeviceType = RKNANO_DEVICE;\n"
    "\tconfig.usPid = 0x3226;\n"
    "\tconfig.usVid = 0x071B;\n"
    "\tm_deviceConfigSet.push_back(config);\n"
    "\n"
)

EDITS = {
    "RKComm.h": [("#define CMD_TIMEOUT 0\n", "#define CMD_TIMEOUT 5000\n")],
    "main.cpp": [("#define DEFAULT_RW_LBA 128\n", "#define DEFAULT_RW_LBA 32\n")],
    "RKScan.cpp": [
        (RKNANO_CONFIG, RKNANO_CONFIG + ECHO_MINI_CONFIG),
        ("\t\t\tdesc.usbcdUsb = descriptor.bcdUSB;\n",
         "\t\t\tdesc.usbcdUsb = descriptor.idVendor == 0x2207 && descriptor.idProduct == 0x262D\n"
         "\t\t\t\t? descriptor.bcdDevice : descriptor.bcdUSB;\n"),
    ],
}


class PatchError(Exception):
    pass


def patched_text(name, text):
    for old, new in EDITS[name]:
        if new in text:
            raise PatchError(f"{name} is already patched")
        if text.count(old) != 1:
            raise PatchError(f"{name} doesn't match the expected upstream source")
        text = text.replace(old, new)
    return text


def patch(root):
    root = Path(root)
    results = {}
    for name in EDITS:
        raw = (root / name).read_bytes().decode("utf-8")
        crlf = "\r\n" in raw
        text = patched_text(name, raw.replace("\r\n", "\n"))
        results[name] = text.replace("\n", "\r\n") if crlf else text
    for name, text in results.items():
        (root / name).write_bytes(text.encode("utf-8"))


def main(argv=None):
    parser = argparse.ArgumentParser(description="Apply the Echo Mini changes to rkdeveloptool")
    parser.add_argument("source", type=Path, help="rkdeveloptool source directory")
    arguments = parser.parse_args(argv)
    try:
        patch(arguments.source)
    except PatchError as error:
        print(f"error: {error}", file=sys.stderr)
        return 1
    print("ok: Echo Mini changes applied")
    return 0


if __name__ == "__main__":
    sys.exit(main())
