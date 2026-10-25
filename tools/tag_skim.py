import io
import os
from pathlib import Path

FLAC_MAGIC = b"fLaC"
FLAC_PADDING = 1
FLAC_PICTURE = 6
FLAC_LAST_BLOCK = 0x80
FLAC_KIND_MASK = 0x7F
FLAC_PICTURE_FIELDS = 16
ID3_MAGIC = b"ID3"
ID3_HEADER_SIZE = 10
ID3_FRAME_HEADER_SIZE = 10
ID3_UNSYNCHRONISED = 0x80
ID3_EXTENDED_HEADER = 0x40
ID3_PLAIN_FRAME_FLAGS = b"\x00\x00"
ID3_PICTURE_FRAME = b"APIC"
SKIMMED_VERSIONS = (3, 4)
OPEN_FLAGS = os.O_RDONLY | getattr(os, "O_BINARY", 0) | getattr(os, "O_RANDOM", 0)


def synchsafe(data):
    return data[0] << 21 | data[1] << 14 | data[2] << 7 | data[3]


def read_at(handle, offset, length):
    handle.seek(offset)
    return handle.read(length)


def read_u32(handle, offset):
    return int.from_bytes(read_at(handle, offset, 4).rjust(4, b"\x00"), "big")


def flac_picture_data(handle, body, end):
    mime_length = read_u32(handle, body + 4)
    description_at = body + 8 + mime_length
    data_length_at = description_at + 4 + read_u32(handle, description_at) + FLAC_PICTURE_FIELDS
    data_start = data_length_at + 4
    data_end = min(data_start + read_u32(handle, data_length_at), end)
    return [(data_start, data_end)] if data_start < data_end else []


def flac_picture_ranges(handle):
    if read_at(handle, 0, 4) != FLAC_MAGIC:
        return []
    ranges, offset = [], 4
    while True:
        header = read_at(handle, offset, 4)
        if len(header) < 4:
            return ranges
        body, length = offset + 4, int.from_bytes(header[1:4], "big")
        kind = header[0] & FLAC_KIND_MASK
        if kind == FLAC_PICTURE:
            ranges += flac_picture_data(handle, body, body + length)
        elif kind == FLAC_PADDING and length > 0:
            ranges.append((body, body + length))
        offset = body + length
        if header[0] & FLAC_LAST_BLOCK:
            return ranges


def id3_frame_length(header, major):
    size = header[4:8]
    return synchsafe(size) if major == 4 else int.from_bytes(size, "big")


def skimmable_id3(header):
    return (len(header) == ID3_HEADER_SIZE and header[:3] == ID3_MAGIC
            and header[3] in SKIMMED_VERSIONS
            and not header[5] & (ID3_UNSYNCHRONISED | ID3_EXTENDED_HEADER))


def id3_picture_ranges(handle):
    header = read_at(handle, 0, ID3_HEADER_SIZE)
    if not skimmable_id3(header):
        return []
    major, end = header[3], ID3_HEADER_SIZE + synchsafe(header[6:10])
    ranges, offset = [], ID3_HEADER_SIZE
    while offset + ID3_FRAME_HEADER_SIZE <= end:
        frame = read_at(handle, offset, ID3_FRAME_HEADER_SIZE)
        if len(frame) < ID3_FRAME_HEADER_SIZE or frame[0] == 0:
            break
        length = id3_frame_length(frame, major)
        body = offset + ID3_FRAME_HEADER_SIZE
        if frame[:4] == ID3_PICTURE_FRAME and frame[8:10] == ID3_PLAIN_FRAME_FLAGS:
            ranges.append((body, min(body + length, end)))
        offset = body + length
    return ranges


def picture_ranges(handle):
    return sorted(flac_picture_ranges(handle) + id3_picture_ranges(handle))


class SkimmingReader(io.RawIOBase):
    def __init__(self, path, handle, ranges, size):
        super().__init__()
        self.name = str(path)
        self.handle = handle
        self.ranges = ranges
        self.size = size
        self.position = 0
        self.disk_bytes = 0

    def readable(self):
        return True

    def seekable(self):
        return True

    def tell(self):
        return self.position

    def seek(self, offset, whence=io.SEEK_SET):
        base = {io.SEEK_SET: 0, io.SEEK_CUR: self.position, io.SEEK_END: self.size}[whence]
        self.position = max(0, base + offset)
        return self.position

    def span_at(self, position):
        for start, end in self.ranges:
            if start <= position < end:
                return end, True
            if position < start:
                return start, False
        return self.size, False

    def read_span(self, view, filled, limit, skipped):
        if skipped:
            view[filled:filled + limit] = bytes(limit)
            return limit
        self.handle.seek(self.position)
        data = self.handle.read(limit)
        view[filled:filled + len(data)] = data
        self.disk_bytes += len(data)
        return len(data)

    def readinto(self, buffer):
        view, filled = memoryview(buffer).cast("B"), 0
        while filled < len(view) and self.position < self.size:
            end, skipped = self.span_at(self.position)
            limit = min(len(view) - filled, end - self.position)
            count = self.read_span(view, filled, limit, skipped)
            if count == 0:
                break
            filled += count
            self.position += count
        return filled

    def close(self):
        if not self.closed:
            self.handle.close()
        super().close()


def open_skimming(path):
    path = Path(path)
    handle = os.fdopen(os.open(path, OPEN_FLAGS), "rb", buffering=0)
    try:
        ranges = picture_ranges(handle)
        size = os.fstat(handle.fileno()).st_size
    except Exception:
        handle.close()
        raise
    return SkimmingReader(path, handle, ranges, size)
