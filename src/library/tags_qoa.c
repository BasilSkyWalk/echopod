#include "tags_internal.h"

enum {
    QOA_HEADER_BYTES = 16,
    QOA_RATE_MASK = 0xFFFFFF
};

bool tags_read_qoa(TagReader *reader)
{
    uint8_t bytes[QOA_HEADER_BYTES];
    if (!tag_read(reader, 0, bytes, sizeof bytes) ||
        !tag_same_text((const char *)bytes, 4, "qoaf")) {
        return false;
    }
    uint32_t samples = tag_be32(bytes + 4);
    uint32_t channels = bytes[8];
    uint32_t rate = tag_be32(bytes + 8) & QOA_RATE_MASK;
    if (samples == 0 || channels == 0 || rate == 0) {
        return false;
    }
    tag_set_duration(reader, (double)samples / (double)rate);
    return true;
}
