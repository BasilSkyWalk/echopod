#include "bluetooth_link.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/audio.h"

static bool same_device(const HalBluetoothDevice *first, const HalBluetoothDevice *second)
{
    for (size_t index = 0; index < HAL_BLUETOOTH_ADDRESS_SIZE; index++) {
        if (first->address[index] != second->address[index]) {
            return false;
        }
    }
    for (size_t index = 0; index < HAL_BLUETOOTH_NAME_CAPACITY; index++) {
        if (first->name[index] != second->name[index]) {
            return false;
        }
        if (first->name[index] == '\0') {
            return true;
        }
    }
    return true;
}

static void announce(BluetoothLink *link, const char *text, const char *name, uint32_t now_ms)
{
    StringBuilder builder = string_builder_make(link->toast_text, BLUETOOTH_LINK_TOAST_CAPACITY);
    string_builder_append(&builder, text);
    if (name != NULL) {
        string_builder_append(&builder, name);
    }
    if (link->toast != NULL) {
        toast_show(link->toast, link->toast_text, now_ms);
    }
}

static void wake(BluetoothLink *link, uint32_t now_ms)
{
    hal_bluetooth_set_power(true);
    link->dozing = false;
    link->connected = false;
    link->streamed = false;
    link->reconnecting = false;
    link->forgetting = false;
    link->retried_ms = now_ms - BLUETOOTH_LINK_RETRY_MS;
    link->unlinked_ms = now_ms;
}

void bluetooth_link_start(BluetoothLink *link, Toast *toast, uint32_t now_ms)
{
    *link = (BluetoothLink){.toast = toast};
    bluetooth_store_load(&link->saved);
    if (link->saved.enabled) {
        wake(link, now_ms);
    }
}

void bluetooth_link_set_enabled(BluetoothLink *link, bool enabled, uint32_t now_ms)
{
    if (link->saved.enabled != enabled) {
        link->saved.enabled = enabled;
        bluetooth_store_save(&link->saved);
    }
    if (enabled) {
        wake(link, now_ms);
        return;
    }
    hal_bluetooth_set_power(false);
    link->connected = false;
    link->streamed = false;
    link->reconnecting = false;
    link->dozing = false;
}

void bluetooth_link_browse(BluetoothLink *link, bool browsing, uint32_t now_ms)
{
    link->browsing = browsing;
    link->searched = true;
    link->unlinked_ms = now_ms;
    if (browsing && link->saved.enabled && link->dozing) {
        wake(link, now_ms);
    }
}

void bluetooth_link_pair(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms)
{
    link->forgetting = false;
    link->reconnecting = false;
    link->retried_ms = now_ms;
    hal_bluetooth_connect(device);
}

void bluetooth_link_connect_saved(BluetoothLink *link, uint32_t now_ms)
{
    if (!link->saved.has_device) {
        return;
    }
    link->forgetting = false;
    link->reconnecting = true;
    link->searched = false;
    link->retried_ms = now_ms;
    hal_bluetooth_connect(&link->saved.device);
}

void bluetooth_link_forget(BluetoothLink *link)
{
    if (link->connected || hal_bluetooth_status()->state == HAL_BLUETOOTH_CONNECTING) {
        hal_bluetooth_disconnect();
        link->forgetting = true;
    }
    link->connected = false;
    link->reconnecting = false;
    if (link->saved.has_device) {
        link->saved.has_device = false;
        link->saved.device = (HalBluetoothDevice){.address = {0}, .name = {0}};
        bluetooth_store_save(&link->saved);
    }
}

void bluetooth_link_sleep(BluetoothLink *link)
{
    if (!link->saved.enabled || link->dozing) {
        return;
    }
    hal_bluetooth_set_power(false);
    link->dozing = true;
    link->connected = false;
    link->streamed = false;
    link->reconnecting = false;
}

static void remember(BluetoothLink *link, const HalBluetoothDevice *device)
{
    if (link->saved.has_device && same_device(&link->saved.device, device)) {
        return;
    }
    link->saved.device = *device;
    link->saved.has_device = true;
    bluetooth_store_save(&link->saved);
}

static void follow_connection(BluetoothLink *link, const HalBluetoothStatus *status, Player *player,
                              uint32_t now_ms)
{
    bool connected = status->state == HAL_BLUETOOTH_CONNECTED;
    if (!connected) {
        link->forgetting = false;
    }
    if (connected && !link->connected && !link->forgetting) {
        link->connected = true;
        link->reconnecting = false;
        link->streamed = false;
        link->buds_paused = false;
        link->changed_ms = now_ms;
        link->seen = status->remote;
        remember(link, &status->connected);
        announce(link, "Connected to ", status->connected.name, now_ms);
    } else if (!connected && link->connected) {
        link->connected = false;
        if (link->streamed && player->state == PLAYER_PLAYING) {
            player_toggle_pause(player);
        }
        link->streamed = false;
        link->unlinked_ms = now_ms;
        link->retried_ms = now_ms;
        announce(link, "Bluetooth disconnected", NULL, now_ms);
    }
}

static void find_device(BluetoothLink *link, const HalBluetoothStatus *status, uint32_t now_ms)
{
    if (link->connected || link->forgetting) {
        return;
    }
    if (status->state != HAL_BLUETOOTH_READY) {
        return;
    }
    if (status->pairing == HAL_BLUETOOTH_PAIRED) {
        if (now_ms - link->retried_ms >= BLUETOOTH_LINK_CONFIRM_MS) {
            hal_bluetooth_disconnect();
            link->retried_ms = now_ms - BLUETOOTH_LINK_RETRY_MS + BLUETOOTH_LINK_RESET_MS;
        }
        return;
    }
    link->reconnecting = false;
    bool due = now_ms - link->retried_ms >= BLUETOOTH_LINK_RETRY_MS;
    if (link->saved.has_device && due && (!link->browsing || link->searched)) {
        bluetooth_link_connect_saved(link, now_ms);
    } else if (link->browsing && !link->searched) {
        link->searched = true;
        hal_bluetooth_scan();
    } else if (link->browsing && !link->saved.has_device) {
        hal_bluetooth_scan();
    }
}

static uint8_t pressed(uint8_t count, uint8_t seen)
{
    return (uint8_t)(count - seen);
}

static void follow_remote(BluetoothLink *link, const HalBluetoothStatus *status, Player *player,
                          uint32_t now_ms)
{
    HalBluetoothRemote remote = status->remote;
    HalBluetoothRemote seen = link->seen;
    link->seen = remote;
    if (player->state != PLAYER_PAUSED) {
        link->buds_paused = false;
    }
    if (!link->connected) {
        return;
    }
    bool settled = now_ms - link->changed_ms >= BLUETOOTH_LINK_ECHO_MS;
    if (settled && pressed(remote.pauses, seen.pauses) > 0 && link->streamed &&
        player->state == PLAYER_PLAYING) {
        player_toggle_pause(player);
        link->buds_paused = true;
    }
    if (settled && pressed(remote.plays, seen.plays) > 0 && link->buds_paused &&
        player->state == PLAYER_PAUSED) {
        player_toggle_pause(player);
        link->buds_paused = false;
    }
    for (uint8_t count = pressed(remote.nexts, seen.nexts); count > 0; count--) {
        player_skip(player, 1);
    }
    for (uint8_t count = pressed(remote.previouses, seen.previouses); count > 0; count--) {
        player_skip(player, -1);
    }
}

static void follow_player(BluetoothLink *link, const Player *player, uint32_t now_ms)
{
    if (player->state != PLAYER_PLAYING) {
        link->wired = false;
    } else if (hal_audio_jack() != HAL_AUDIO_JACK_NONE) {
        link->wired = true;
    }
    bool stream = link->connected && player->state == PLAYER_PLAYING && !link->wired;
    if (stream != link->streamed) {
        link->streamed = stream;
        link->changed_ms = now_ms;
        hal_bluetooth_set_stream(stream);
    }
}

static void doze(BluetoothLink *link, uint32_t now_ms)
{
    if (link->connected || link->browsing || link->dozing) {
        return;
    }
    if (now_ms - link->unlinked_ms >= BLUETOOTH_LINK_IDLE_OFF_MS) {
        hal_bluetooth_set_power(false);
        link->dozing = true;
        link->reconnecting = false;
    }
}

void bluetooth_link_step(BluetoothLink *link, Player *player, uint32_t now_ms)
{
    if (!link->saved.enabled || link->dozing) {
        return;
    }
    const HalBluetoothStatus *status = hal_bluetooth_status();
    follow_connection(link, status, player, now_ms);
    follow_remote(link, status, player, now_ms);
    follow_player(link, player, now_ms);
    find_device(link, status, now_ms);
    doze(link, now_ms);
}

const char *bluetooth_link_summary(const BluetoothLink *link)
{
    if (!link->saved.enabled) {
        return "Off";
    }
    if (link->connected) {
        return link->saved.device.name;
    }
    if (link->dozing) {
        return "Not connected";
    }
    switch (hal_bluetooth_status()->state) {
    case HAL_BLUETOOTH_STARTING:
        return "Starting…";
    case HAL_BLUETOOTH_NO_REPLY:
        return "Not responding";
    case HAL_BLUETOOTH_SCANNING:
        return "Searching…";
    case HAL_BLUETOOTH_CONNECTING:
        return "Connecting…";
    default:
        return link->saved.has_device ? "Not connected" : "On";
    }
}
