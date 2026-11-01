#include "update.h"

#include <stddef.h>

#include "app/app.h"
#include "board.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "device/fat.h"
#include "device/updater.h"
#include "emmc.h"

enum {
    MESSAGE_CAPACITY = 128,
    PERCENT_CAPACITY = 8,
    PERCENT_STEP = 5,
    PERCENT = 100,
    HIGHEST_NUMBER = 97,
    TENS_AT = 7,
    UNITS_AT = 8
};

static const char UPDATE_TEMPLATE[] = "/HIFIEC00.IMG";
static char update_path[sizeof UPDATE_TEMPLATE];
static const char *install_title;

static FatVolume volume;
static FatFile file;
static uint32_t last_percent;
static bool flash_rewritten;

bool hal_system_restart_pending(void)
{
    return flash_rewritten;
}

static bool read_update(void *context, uint32_t offset, void *buffer, uint32_t length)
{
    (void)context;
    return fat_read(&volume, &file, offset, buffer, length);
}

static void show_percent(uint32_t percent)
{
    char text[MESSAGE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    char number[PERCENT_CAPACITY];
    StringBuilder digits = string_builder_make(number, sizeof number);
    string_builder_append_unsigned(&digits, percent);
    const char *arguments[] = {number};
    i18n_format(&builder, STR_UPDATE_DONT_UNPLUG, arguments, 1);
    app_show_notice(install_title, text);
    board_display_service();
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

static bool open_highest_numbered(void)
{
    for (size_t index = 0; index < sizeof UPDATE_TEMPLATE; index++) {
        update_path[index] = UPDATE_TEMPLATE[index];
    }
    for (int number = HIGHEST_NUMBER; number >= 0; number--) {
        update_path[TENS_AT] = (char)('0' + number / 10);
        update_path[UNITS_AT] = (char)('0' + number % 10);
        if (fat_open(&volume, update_path, &file)) {
            return true;
        }
    }
    return false;
}

static bool open_update(UpdateStorage *storage)
{
    EmmcInfo info;
    BlockDevice device = {emmc_volume_read, NULL, NULL};
    if (!emmc_start(&info) || !fat_mount(&volume, device) || !open_highest_numbered()) {
        return false;
    }
    *storage = (UpdateStorage){emmc_read,          emmc_write,         NULL,
                               info.layout.slot_1, info.layout.slot_2, info.layout.slot_sectors};
    return true;
}

static void install(const UpdateFile *update, const UpdateStorage *storage, bool stock)
{
    install_title = i18n_text(stock ? STR_UPDATE_INSTALLING_FIIO : STR_UPDATE_INSTALLING);
    last_percent = 0;
    show_percent(0);
    flash_rewritten = true;
    if (updater_install(update, storage, (UpdateProgress){report, NULL})) {
        app_show_notice(i18n_text(stock ? STR_UPDATE_FIIO_INSTALLED : STR_UPDATE_INSTALLED),
                        i18n_text(STR_UPDATE_UNPLUG_AND_RESTART));
    } else {
        app_show_notice(i18n_text(STR_UPDATE_FAILED), i18n_text(STR_UPDATE_RETRY));
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
    const char *name = update_path + 1;
    char text[MESSAGE_CAPACITY];
    StringBuilder builder = string_builder_make(text, sizeof text);
    const char *arguments[] = {name};
    i18n_format(&builder, STR_UPDATE_CHECKING, arguments, 1);
    app_show_notice(i18n_text(STR_UPDATE_TITLE), text);
    UpdateCheck check = updater_check(&update, &storage);
    if (check == UPDATE_VALID || check == UPDATE_STOCK) {
        install(&update, &storage, check == UPDATE_STOCK);
    } else if (check == UPDATE_INVALID) {
        builder = string_builder_make(text, sizeof text);
        i18n_format(&builder, STR_UPDATE_INVALID_IMAGE, arguments, 1);
        app_show_notice(i18n_text(STR_UPDATE_NOT_INSTALLED), text);
        board_wait_for_key();
    }
}
