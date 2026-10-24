#include "device/card_write.h"

bool card_write_start(const CardWriter *writer, uint32_t address, uint32_t count,
                      const uint32_t *words)
{
    if (count == 0) {
        return true;
    }
    if (count == 1) {
        return writer->write_data(writer->context, CARD_WRITE_SINGLE_BLOCK, address, 1, words);
    }
    bool written =
        writer->write_data(writer->context, CARD_WRITE_MULTIPLE_BLOCK, address, count, words);
    if (!writer->stop(writer->context)) {
        return false;
    }
    if (!written) {
        writer->wait_ready(writer->context);
    }
    return written;
}

bool card_write_finish(const CardWriter *writer)
{
    return writer->wait_ready(writer->context);
}

bool card_write(const CardWriter *writer, uint32_t address, uint32_t count, const uint32_t *words)
{
    if (count == 0) {
        return true;
    }
    return card_write_start(writer, address, count, words) && card_write_finish(writer);
}
