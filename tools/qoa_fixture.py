import argparse
import struct
import subprocess
import tempfile
from pathlib import Path

MAGIC = b"qoaf"
SLICE_SAMPLES = 20
SLICES_PER_FRAME = 256
FRAME_SAMPLES = SLICE_SAMPLES * SLICES_PER_FRAME
LMS_LEN = 4
QUANT_TAB = [7, 7, 7, 5, 5, 3, 3, 1, 0, 0, 2, 2, 4, 4, 6, 6, 6]
RECIPROCAL_TAB = [65536, 9363, 3121, 1457, 781, 475, 311, 216, 156, 117, 90, 71, 57, 47, 39, 32]
DEQUANT_TAB = [
    [1, -1, 3, -3, 5, -5, 7, -7],
    [5, -5, 18, -18, 32, -32, 49, -49],
    [16, -16, 53, -53, 95, -95, 147, -147],
    [34, -34, 113, -113, 203, -203, 315, -315],
    [63, -63, 210, -210, 378, -378, 588, -588],
    [104, -104, 345, -345, 621, -621, 966, -966],
    [158, -158, 528, -528, 950, -950, 1477, -1477],
    [228, -228, 760, -760, 1368, -1368, 2128, -2128],
    [316, -316, 1053, -1053, 1895, -1895, 2947, -2947],
    [422, -422, 1405, -1405, 2529, -2529, 3934, -3934],
    [548, -548, 1828, -1828, 3290, -3290, 5117, -5117],
    [696, -696, 2320, -2320, 4176, -4176, 6496, -6496],
    [868, -868, 2893, -2893, 5207, -5207, 8099, -8099],
    [1064, -1064, 3548, -3548, 6386, -6386, 9933, -9933],
    [1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005],
    [1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336],
]


def clamp(value, low, high):
    return low if value < low else high if value > high else value


def clamp_s16(value):
    return clamp(value, -32768, 32767)


def lms_predict(lms):
    return sum(weight * history for weight, history in zip(lms[1], lms[0])) >> 13


def lms_update(lms, sample, delta):
    step = delta >> 4
    lms[1] = [weight + (-step if history < 0 else step) for weight, history in zip(lms[1], lms[0])]
    lms[0] = lms[0][1:] + [sample]


def qoa_div(value, scalefactor):
    n = (value * RECIPROCAL_TAB[scalefactor] + (1 << 15)) >> 16
    return n + ((value > 0) - (value < 0)) - ((n > 0) - (n < 0))


def new_lms():
    return [[0, 0, 0, 0], [0, 0, -(1 << 13), 1 << 14]]


def encode_frame(samples, channels, rate, frame_len, lms, previous):
    slices = (frame_len + SLICE_SAMPLES - 1) // SLICE_SAMPLES
    frame_size = 8 + LMS_LEN * 4 * channels + 8 * slices * channels
    out = bytearray(struct.pack(">Q", channels << 56 | rate << 32 | frame_len << 16 | frame_size))
    for channel in range(channels):
        history, weights = lms[channel]
        out += struct.pack(">4h", *history)
        out += struct.pack(">4h", *weights)
    for sample_index in range(0, frame_len, SLICE_SAMPLES):
        for channel in range(channels):
            slice_len = clamp(SLICE_SAMPLES, 0, frame_len - sample_index)
            begin = sample_index * channels + channel
            end = (sample_index + slice_len) * channels + channel
            best_rank = None
            best_slice = 0
            best_lms = None
            best_scalefactor = 0
            for step in range(16):
                scalefactor = (step + previous[channel]) & 15
                trial = [[list(history), list(weights)] for history, weights in lms]
                rank = 0
                encoded = scalefactor
                for index in range(begin, end, channels):
                    sample = samples[index]
                    predicted = lms_predict(trial[channel])
                    residual = sample - predicted
                    scaled = clamp(qoa_div(residual, scalefactor), -8, 8)
                    quantized = QUANT_TAB[scaled + 8]
                    dequantized = DEQUANT_TAB[scalefactor][quantized]
                    reconstructed = clamp_s16(predicted + dequantized)
                    weights_now = trial[channel][1]
                    penalty = (sum(weight * weight for weight in weights_now) >> 18) - 0x8FF
                    penalty = max(penalty, 0)
                    error = sample - reconstructed
                    rank += error * error + penalty * penalty
                    if best_rank is not None and rank > best_rank:
                        break
                    lms_update(trial[channel], reconstructed, dequantized)
                    encoded = (encoded << 3) | quantized
                if best_rank is None or rank < best_rank:
                    best_rank = rank
                    best_slice = encoded
                    best_lms = trial
                    best_scalefactor = scalefactor
            previous[channel] = best_scalefactor
            lms[channel] = best_lms[channel]
            best_slice <<= (SLICE_SAMPLES - slice_len) * 3
            out += struct.pack(">Q", best_slice)
    return bytes(out)


def encode(samples, channels, rate):
    lms = [new_lms() for _ in range(channels)]
    previous = [0] * channels
    total = len(samples) // channels
    out = bytearray(MAGIC + struct.pack(">I", total))
    for first in range(0, total, FRAME_SAMPLES):
        frame_len = min(FRAME_SAMPLES, total - first)
        frame = samples[first * channels:(first + frame_len) * channels]
        out += encode_frame(frame, channels, rate, frame_len, lms, previous)
    return bytes(out)


def decode_frame(data):
    word = struct.unpack(">Q", data[:8])[0]
    channels = word >> 56
    rate = (word >> 32) & 0xFFFFFF
    samples = (word >> 16) & 0xFFFF
    frame_size = word & 0xFFFF
    cursor = 8
    lms = []
    for _ in range(channels):
        history = struct.unpack(">4h", data[cursor:cursor + 8])
        weights = struct.unpack(">4h", data[cursor + 8:cursor + 16])
        lms.append([list(history), list(weights)])
        cursor += 16
    output = [0] * (samples * channels)
    for sample_index in range(0, samples, SLICE_SAMPLES):
        for channel in range(channels):
            slice_word = struct.unpack(">Q", data[cursor:cursor + 8])[0]
            cursor += 8
            scalefactor = slice_word >> 60
            slice_word = (slice_word << 4) & 0xFFFFFFFFFFFFFFFF
            end = min(sample_index + SLICE_SAMPLES, samples)
            for index in range(sample_index, end):
                predicted = lms_predict(lms[channel])
                quantized = slice_word >> 61
                dequantized = DEQUANT_TAB[scalefactor][quantized]
                reconstructed = clamp_s16(predicted + dequantized)
                output[index * channels + channel] = reconstructed
                slice_word = (slice_word << 3) & 0xFFFFFFFFFFFFFFFF
                lms_update(lms[channel], reconstructed, dequantized)
    return frame_size, rate, output


def decode(data):
    total = struct.unpack(">I", data[4:8])[0]
    channels = data[8]
    rate = int.from_bytes(data[9:12], "big")
    cursor = 8
    output = []
    while len(output) < total * channels and cursor < len(data):
        frame_size, _, samples = decode_frame(data[cursor:])
        output += samples
        cursor += frame_size
    return channels, rate, output


def triangle(index, period, amplitude):
    position = index % (period * 2)
    value = position if position < period else period * 2 - position
    return (value - period) * amplitude // period


def signal(frames, channels):
    samples = []
    for index in range(frames):
        envelope = 1200 + (index // 700 % 5) * 3600
        left = triangle(index, 44, envelope) + triangle(index * 3, 173, 900)
        right = triangle(index + 11, 67, envelope - 500) + triangle(index * 7, 331, 700)
        if channels == 1:
            samples.append(clamp_s16((left + right) // 2))
        else:
            samples.append(clamp_s16(left))
            samples.append(clamp_s16(right))
    return samples


def tool(name, args):
    subprocess.run([name, *args], check=True, capture_output=True)


def write_fixture(output, name, frames, channels, rate):
    samples = signal(frames, channels)
    data = encode(samples, channels, rate)
    output.joinpath(f"{name}.qoa").write_bytes(data)
    decoded_channels, decoded_rate, decoded = decode(data)
    if (decoded_channels, decoded_rate, len(decoded)) != (channels, rate, len(samples)):
        raise SystemExit(f"{name}: self decode disagrees with the encoder")
    with tempfile.TemporaryDirectory() as scratch:
        source = Path(scratch) / "in.qoa"
        pcm = Path(scratch) / "out.pcm"
        source.write_bytes(data)
        tool("ffmpeg", ["-y", "-v", "error", "-i", str(source), "-f", "s16le",
                        "-acodec", "pcm_s16le", str(pcm)])
        reference = pcm.read_bytes()
    expected = struct.pack(f"<{len(decoded)}h", *decoded)
    if reference != expected:
        raise SystemExit(f"{name}: ffmpeg decode disagrees with the reference decoder")
    output.joinpath(f"{name}.pcm").write_bytes(reference)


FIXTURES = {
    "stereo_44k": (12000, 2, 44100),
    "mono_22k": (777, 1, 22050),
    "stereo_48k": (5171, 2, 48000),
}


def main():
    parser = argparse.ArgumentParser(description="Write QOA codec fixtures and their PCM")
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    arguments.output.mkdir(parents=True, exist_ok=True)
    for name, (frames, channels, rate) in FIXTURES.items():
        write_fixture(arguments.output, name, frames, channels, rate)


if __name__ == "__main__":
    main()
