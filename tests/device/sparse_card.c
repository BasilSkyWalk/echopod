#include "sparse_card.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
    SECTOR_SIZE = 512,
    HEADER_SIZE = 12,
    ENTRY_SIZE = 4 + SECTOR_SIZE
};

static uint32_t read_le32(const uint8_t *bytes)
{
    return (uint32_t)bytes[0] | (uint32_t)bytes[1] << 8 | (uint32_t)bytes[2] << 16 |
           (uint32_t)bytes[3] << 24;
}

static bool read_file(const char *path, uint8_t **data, long *size)
{
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        return false;
    }
    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);
    *data = malloc((size_t)*size);
    bool ok = *data != NULL && fread(*data, 1, (size_t)*size, file) == (size_t)*size;
    fclose(file);
    return ok;
}

static bool expand(SparseCard *card, const uint8_t *data, long size)
{
    if (size < HEADER_SIZE || memcmp(data, "EMSP", 4) != 0) {
        return false;
    }
    card->sector_count = read_le32(data + 4);
    uint32_t entries = read_le32(data + 8);
    card->bytes = calloc(card->sector_count, SECTOR_SIZE);
    if (card->bytes == NULL || size < HEADER_SIZE + (long)entries * ENTRY_SIZE) {
        return false;
    }
    for (uint32_t index = 0; index < entries; index++) {
        const uint8_t *entry = data + HEADER_SIZE + (size_t)index * ENTRY_SIZE;
        uint32_t lba = read_le32(entry);
        if (lba < card->sector_count) {
            memcpy(card->bytes + (size_t)lba * SECTOR_SIZE, entry + 4, SECTOR_SIZE);
        }
    }
    return true;
}

bool sparse_card_load(SparseCard *card, const char *name)
{
    char path[1024];
    snprintf(path, sizeof path, "%s/cards/%s.sparse", ECHO_FIXTURE_DIR, name);
    *card = (SparseCard){.bytes = NULL};
    uint8_t *data = NULL;
    long size = 0;
    bool ok = read_file(path, &data, &size) && expand(card, data, size);
    free(data);
    return ok;
}

void sparse_card_free(SparseCard *card)
{
    free(card->bytes);
    card->bytes = NULL;
}

static bool read_sectors(void *context, uint32_t lba, uint32_t count, void *buffer)
{
    SparseCard *card = context;
    if (lba >= card->sector_count || count > card->sector_count - lba) {
        return false;
    }
    card->reads += count;
    card->calls++;
    memcpy(buffer, card->bytes + (size_t)lba * SECTOR_SIZE, (size_t)count * SECTOR_SIZE);
    return true;
}

static bool write_sectors(void *context, uint32_t lba, uint32_t count, const void *buffer)
{
    SparseCard *card = context;
    if (lba >= card->sector_count || count > card->sector_count - lba) {
        return false;
    }
    card->writes += count;
    card->write_calls++;
    memcpy(card->bytes + (size_t)lba * SECTOR_SIZE, buffer, (size_t)count * SECTOR_SIZE);
    return true;
}

BlockDevice sparse_card_device(SparseCard *card)
{
    return (BlockDevice){read_sectors, card, write_sectors};
}
