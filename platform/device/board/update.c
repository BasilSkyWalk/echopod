#include "usb_session.h"

#include <stddef.h>

#include "app/app.h"
#include "app/update_record.h"
#include "board.h"
#include "core/i18n.h"
#include "core/string_builder.h"
#include "device/fat.h"
#include "device/updater.h"
#include "emmc.h"
#include "storage_reset.h"

enum {
    MESSAGE_CAPACITY = 128,
    PERCENT_CAPACITY = 8,
    PERCENT_STEP = 5,
    PERCENT = 100,
    HIGHEST_8GB = 97,
    HIGHEST_512MB = 898,
    LOWEST_512MB = 300,
    TENS_AT = 7,
    UNITS_AT = 8,
    SECTORS_PER_MIB = 2048
};

static const char UPDATE_TEMPLATE_8GB[] = "/HIFIEC00.IMG";
static const char UPDATE_TEMPLATE_512MB[] = "/MINIV000.IMG";
static const char MODEL_UPDATE[] = "/ECHOMINI.IMG";
static char update_path[sizeof UPDATE_TEMPLATE_8GB];
static const char *install_title;

static FatVolume volume;
static FatFile file;
static uint32_t last_percent;
static UsbShared *shared;

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

static bool open_highest_numbered(bool small_model)
{
    _Static_assert(sizeof MODEL_UPDATE == sizeof UPDATE_TEMPLATE_8GB, "same length");
    _Static_assert(sizeof UPDATE_TEMPLATE_512MB == sizeof UPDATE_TEMPLATE_8GB, "same length");
    if (!small_model) {
        for (size_t index = 0; index < sizeof MODEL_UPDATE; index++) {
            update_path[index] = MODEL_UPDATE[index];
        }
        if (fat_open(&volume, update_path, &file)) {
            return true;
        }
    }
    const char *template = small_model ? UPDATE_TEMPLATE_512MB : UPDATE_TEMPLATE_8GB;
    for (size_t index = 0; index < sizeof update_path; index++) {
        update_path[index] = template[index];
    }
    int highest = small_model ? HIGHEST_512MB : HIGHEST_8GB;
    int lowest = small_model ? LOWEST_512MB : 0;
    for (int number = highest; number >= lowest; number--) {
        if (small_model) {
            update_path[TENS_AT - 1] = (char)('0' + number / 100);
            update_path[TENS_AT] = (char)('0' + number / 10 % 10);
            update_path[UNITS_AT] = (char)('0' + number % 10);
        } else {
            update_path[TENS_AT] = (char)('0' + number / 10);
            update_path[UNITS_AT] = (char)('0' + number % 10);
        }
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
    if (!emmc_start(&info) || !info.layout.slots_valid || !fat_mount(&volume, device) ||
        !open_highest_numbered(info.sd_nand)) {
        return false;
    }
    *storage = (UpdateStorage){.read = emmc_read,
                               .write = emmc_write,
                               .context = NULL,
                               .slot_1 = info.layout.slot_1,
                               .slot_2 = info.layout.slot_2,
                               .slot_sectors = info.layout.slot_sectors,
                               .model = info.sd_nand ? UPDATE_MODEL_512MB : UPDATE_MODEL_8GB};
    return true;
}

static HalUpdateRecord outcome(HalUpdateResult result, uint8_t detail)
{
    HalUpdateRecord record = {.result = result, .detail = detail, .emmc_status = 0};
    record.emmc_replied = emmc_status(&record.emmc_status);
    return record;
}

static void remember(HalUpdateRecord record)
{
    board_start_record_update(record);
    storage_reset();
    (void)update_record_save(record);
}

static void install(const UpdateFile *update, const UpdateStorage *storage, bool stock)
{
    install_title = i18n_text(stock ? STR_UPDATE_INSTALLING_FIIO : STR_UPDATE_INSTALLING);
    last_percent = 0;
    show_percent(0);
    shared->flash_rewritten = true;
    bool installed = updater_install(update, storage, (UpdateProgress){report, NULL});
    remember(outcome(installed ? HAL_UPDATE_INSTALLED : HAL_UPDATE_FAILED, (uint8_t)last_percent));
    if (installed) {
        app_show_notice(i18n_text(stock ? STR_UPDATE_FIIO_INSTALLED : STR_UPDATE_INSTALLED),
                        i18n_text(STR_UPDATE_UNPLUG_AND_RESTART));
    } else {
        app_show_notice(i18n_text(STR_UPDATE_FAILED), i18n_text(STR_UPDATE_RETRY));
    }
    board_wait_for_key();
}

void update_after_usb(UsbShared *state)
{
    UpdateStorage storage;
    shared = state;
    if (!open_update(&storage)) {
        return;
    }
    _Static_assert(UPDATER_SCRATCH_WORDS * 4 <= 64 * 512, "the USB write buffer holds the scratch");
    updater_use_scratch(usb_session_buffer());
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
    } else if (check == UPDATE_ALREADY_INSTALLED) {
        remember(outcome(HAL_UPDATE_ALREADY_INSTALLED, 0));
    } else if (check == UPDATE_INVALID) {
        remember(outcome(HAL_UPDATE_REFUSED, (uint8_t)updater_refusal()));
        builder = string_builder_make(text, sizeof text);
        i18n_format(&builder, STR_UPDATE_INVALID_IMAGE, arguments, 1);
        string_builder_append(&builder, " (E");
        string_builder_append_unsigned(&builder, (uint32_t)updater_refusal());
        string_builder_append_char(&builder, '/');
        string_builder_append_unsigned(&builder, storage.slot_sectors / SECTORS_PER_MIB);
        string_builder_append_char(&builder, ')');
        app_show_notice(i18n_text(STR_UPDATE_NOT_INSTALLED), text);
        board_wait_for_key();
    }
}
