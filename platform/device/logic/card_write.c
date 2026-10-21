#include "device/card_write.h"

bool card_write(const CardWriter *writer, uint32_t address, uint32_t count, const uint32_t *words)
{
    if (count == 0) {
        return true;
    }
    if (count == 1) {
        return writer->write_data(writer->context, CARD_WRITE_SINGLE_BLOCK, address, 1, words) &&
               writer->wait_ready(writer->context);
    }
    bool written =
        writer->write_data(writer->context, CARD_WRITE_MULTIPLE_BLOCK, address, count, words);
    if (!writer->stop(writer->context)) {
        return false;
    }
    return writer->wait_ready(writer->context) && written;
}
