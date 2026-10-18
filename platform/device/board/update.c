#include "update.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/string_builder.h"
#include "device/fat.h"
#include "device/updater.h"
#include "emmc.h"

enum {
    MESSAGE_CAPACITY = 64,
    PERCENT_STEP = 5,
    PERCENT = 100
};

static const char UPDATE_PATH[] = "/HIFIEC90.IMG";

static FatVolume volume;
static FatFile file;
static uint32_t last_percent;

static bool read_update(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    return fat_read(&volume, &file, offset, buffer, length);
}

static void show_percent(uint32_t percent)
{
    char text[MESSAGE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    string_builder_append(&builder, "Don't unplug or turn off. ");
    string_builder_append_unsigned(&builder, percent);
    string_builder_append_char(&builder, '%');
    app_show_notice("Installing update", text);
}

static void report(void *context, uint32_t done, uint32_t total)
{
    (void)context;
    uint32_t percent = done * PERCENT / total;
    if (percent >= last_percent + PERCENT_STEP) {
        last_percent = percent;
        show_percent(percent);
    }
}

static bool open_update(UpdateStorage *storage)
{
    EmmcInfo info;
    BlockDevice device = {emmc_volume_read, NULL, NULL};
    if (!emmc_start(&info) || !fat_mount(&volume, device) ||
        !fat_open(&volume, UPDATE_PATH, &file)) {
        return false;
    }
    *storage = (UpdateStorage){emmc_read,          emmc_write,         NULL,
                               info.layout.slot_1, info.layout.slot_2, info.layout.slot_sectors};
    return true;
}

static void install(const UpdateFile *update, const UpdateStorage *storage)
{
    last_percent = 0;
    show_percent(0);
    if (updater_install(update, storage, (UpdateProgress){report, NULL})) {
        app_show_notice("Update installed",
                        "Unplug USB, then hold Power to turn off and on again.");
    } else {
        app_show_notice("Update failed", "Copy HIFIEC90.IMG again and retry before turning off.");
    }
    board_wait_for_key();
}

void update_after_usb(void)
{
    UpdateStorage storage;
    if (!open_update(&storage)) {
        return;
    }
    UpdateFile update = {read_update, file.size, NULL};
    app_show_notice("Update", "Checking HIFIEC90.IMG");
    UpdateCheck check = updater_check(&update, &storage);
    if (check == UPDATE_VALID) {
        install(&update, &storage);
    } else if (check == UPDATE_INVALID) {
        app_show_notice("Update not installed",
                        "HIFIEC90.IMG isn't a valid image for this player.");
        board_wait_for_key();
    }
}
