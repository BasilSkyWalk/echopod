#pragma once

#include "codec/flac.h"

void flac_bits_start(FlacBits *bits, FlacInput input, uint32_t offset);
uint32_t flac_bits_offset(const FlacBits *bits);
uint32_t flac_bits_read(FlacBits *bits, uint32_t count);
int32_t flac_bits_read_signed(FlacBits *bits, uint32_t count);
uint32_t flac_bits_read_unary(FlacBits *bits);
void flac_bits_align(FlacBits *bits);
void flac_bits_reset_crc(FlacBits *bits);
