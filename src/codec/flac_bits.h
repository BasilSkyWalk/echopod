#pragma once

#include "codec/flac.h"

void flac_bits_start(FlacBits *bits, FlacInput input, uint32_t offset);
uint32_t flac_bits_offset(const FlacBits *bits);
uint32_t flac_bits_read(FlacBits *bits, uint32_t count);
int32_t flac_bits_read_signed(FlacBits *bits, uint32_t count);
bool flac_bits_read_rice(FlacBits *bits, int32_t *out, uint32_t count, uint32_t parameter);
uint32_t flac_bits_read_unary(FlacBits *bits);
void flac_bits_align(FlacBits *bits);
void flac_bits_reset_crc(FlacBits *bits);
uint8_t flac_bits_crc8(FlacBits *bits);
uint16_t flac_bits_crc16(FlacBits *bits);
