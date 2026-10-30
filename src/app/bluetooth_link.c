#include "bluetooth_link.h"

#include <stddef.h>

#include "core/string_builder.h"
#include "hal/audio.h"

static bool same_address(const HalBluetoothDevice *first, const HalBluetoothDevice *second)
{
    for (size_t index = 0; index < HAL_BLUETOOTH_ADDRESS_SIZE; index++) {
        if (first->address[index] != second->address[index]) {
            return false;
        }
    }
    return true;
}

static bool same_name(const HalBluetoothDevice *first, const HalBluetoothDevice *second)
{
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

static int find_known(const BluetoothLink *link, const HalBluetoothDevice *device)
{
    for (int index = 0; index < link->saved.count; index++) {
        if (same_address(&link->saved.devices[index], device)) {
            return index;
        }
    }
    return -1;
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

static void reset_session(BluetoothLink *link, uint32_t now_ms)
{
    link->connected = false;
    link->streamed = false;
    link->forgetting = false;
    link->held = false;
    link->attempting = false;
    link->has_target = false;
    link->paired_seen = false;
    link->search_now = false;
    link->searched = true;
    link->retried_ms = now_ms - BLUETOOTH_LINK_RETRY_MS;
    link->unlinked_ms = now_ms;
}

static void wake(BluetoothLink *link, uint32_t now_ms)
{
    hal_bluetooth_set_power(true);
    link->dozing = false;
    reset_session(link, now_ms);
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
    link->dozing = false;
    reset_session(link, now_ms);
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

static bool busy(void)
{
    HalBluetoothState state = hal_bluetooth_status()->state;
    return state == HAL_BLUETOOTH_CONNECTED || state == HAL_BLUETOOTH_CONNECTING;
}

static void aim_at(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms)
{
    link->held = false;
    link->forgetting = false;
    link->target = *device;
    link->has_target = true;
    link->retried_ms = now_ms - BLUETOOTH_LINK_RETRY_MS;
    if (busy() || link->paired_seen) {
        hal_bluetooth_disconnect();
        link->paired_seen = false;
        link->retried_ms += BLUETOOTH_LINK_RESET_MS;
    }
}

void bluetooth_link_choose(BluetoothLink *link, int index, uint32_t now_ms)
{
    if (index < 0 || index >= link->saved.count) {
        return;
    }
    const HalBluetoothDevice *device = &link->saved.devices[index];
    if (link->connected && same_address(&link->linked, device)) {
        hal_bluetooth_disconnect();
        link->held = true;
        link->has_target = false;
        return;
    }
    aim_at(link, device, now_ms);
}

void bluetooth_link_pair(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms)
{
    aim_at(link, device, now_ms);
}

void bluetooth_link_search(BluetoothLink *link, uint32_t now_ms)
{
    if (busy() || link->paired_seen) {
        hal_bluetooth_disconnect();
        link->paired_seen = false;
    }
    link->held = true;
    link->has_target = false;
    link->search_now = true;
    link->unlinked_ms = now_ms;
}

void bluetooth_link_forget(BluetoothLink *link, int index)
{
    if (index < 0 || index >= link->saved.count) {
        return;
    }
    const HalBluetoothDevice *device = &link->saved.devices[index];
    bool linked = link->connected && same_address(&link->linked, device);
    bool aimed = (link->attempting || link->has_target) && same_address(&link->target, device);
    if (linked || (aimed && (busy() || link->paired_seen))) {
        hal_bluetooth_disconnect();
        link->forgetting = linked;
        link->paired_seen = false;
    }
    if (aimed) {
        link->has_target = false;
        link->attempting = false;
    }
    if (linked) {
        link->connected = false;
    }
    for (int next = index + 1; next < link->saved.count; next++) {
        link->saved.devices[next - 1] = link->saved.devices[next];
    }
    link->saved.count--;
    bluetooth_store_save(&link->saved);
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
    link->attempting = false;
    link->paired_seen = false;
}

static void remember(BluetoothLink *link, const HalBluetoothDevice *device)
{
    int known = find_known(link, device);
    if (known == 0 && same_name(&link->saved.devices[0], device)) {
        return;
    }
    int last = known >= 0 ? known : link->saved.count;
    if (last >= BLUETOOTH_STORE_DEVICES) {
        last = BLUETOOTH_STORE_DEVICES - 1;
    }
    for (int index = last; index > 0; index--) {
        link->saved.devices[index] = link->saved.devices[index - 1];
    }
    link->saved.devices[0] = *device;
    if (known < 0 && link->saved.count < BLUETOOTH_STORE_DEVICES) {
        link->saved.count++;
    }
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
        link->linked = status->connected;
        link->held = false;
        link->has_target = false;
        link->attempting = false;
        link->paired_seen = false;
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
        if (!link->has_target) {
            link->retried_ms = now_ms;
        }
        announce(link, "Disconnected from ", link->linked.name, now_ms);
    }
}

static const HalBluetoothDevice *next_target(const BluetoothLink *link)
{
    if (link->has_target) {
        return &link->target;
    }
    if (!link->held && link->saved.count > 0) {
        return &link->saved.devices[0];
    }
    return NULL;
}

static void attempt(BluetoothLink *link, const HalBluetoothDevice *device, uint32_t now_ms)
{
    link->target = *device;
    link->has_target = false;
    link->attempting = true;
    link->searched = false;
    link->retried_ms = now_ms;
    hal_bluetooth_connect(&link->target);
}

static void start_scan(BluetoothLink *link)
{
    link->search_now = false;
    link->searched = true;
    link->scan_tried = false;
    hal_bluetooth_scan();
}

static int found_known(const BluetoothLink *link, const HalBluetoothStatus *status)
{
    for (uint8_t index = 0; index < status->device_count; index++) {
        if ((status->found_now & 1u << index) == 0) {
            continue;
        }
        int known = find_known(link, &status->devices[index]);
        if (known >= 0) {
            return known;
        }
    }
    return -1;
}

static bool wait_for_link(BluetoothLink *link, const HalBluetoothStatus *status, uint32_t now_ms)
{
    if (status->pairing != HAL_BLUETOOTH_PAIRED) {
        link->paired_seen = false;
        return false;
    }
    if (!link->paired_seen) {
        link->paired_seen = true;
        link->paired_ms = now_ms;
    } else if (now_ms - link->paired_ms >= BLUETOOTH_LINK_CONFIRM_MS) {
        hal_bluetooth_disconnect();
        link->paired_seen = false;
        link->has_target = true;
        link->retried_ms = now_ms - BLUETOOTH_LINK_RETRY_MS + BLUETOOTH_LINK_RESET_MS;
    }
    return true;
}

static void find_device(BluetoothLink *link, const HalBluetoothStatus *status, uint32_t now_ms)
{
    if (link->connected || link->forgetting) {
        return;
    }
    if (status->state == HAL_BLUETOOTH_CONNECTING) {
        link->attempting = true;
        return;
    }
    if (status->state == HAL_BLUETOOTH_SCANNING && !link->held && !link->scan_tried &&
        found_known(link, status) >= 0) {
        hal_bluetooth_stop_scan();
        return;
    }
    if (status->state != HAL_BLUETOOTH_READY || wait_for_link(link, status, now_ms)) {
        return;
    }
    link->attempting = false;
    if (link->search_now) {
        start_scan(link);
        return;
    }
    int found = found_known(link, status);
    if (found >= 0 && !link->held && !link->scan_tried) {
        link->scan_tried = true;
        attempt(link, &link->saved.devices[found], now_ms);
        return;
    }
    const HalBluetoothDevice *device = next_target(link);
    bool due = now_ms - link->retried_ms >= BLUETOOTH_LINK_RETRY_MS;
    if (device != NULL && due && (link->has_target || !link->browsing || link->searched)) {
        attempt(link, device, now_ms);
    } else if (link->browsing && (!link->searched || device == NULL)) {
        start_scan(link);
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
        link->attempting = false;
        link->paired_seen = false;
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

static bool connecting(const BluetoothLink *link)
{
    return link->attempting || link->paired_seen || link->has_target ||
           hal_bluetooth_status()->state == HAL_BLUETOOTH_CONNECTING;
}

BluetoothDeviceState bluetooth_link_device_state(const BluetoothLink *link, int index)
{
    if (index < 0 || index >= link->saved.count || !link->saved.enabled || link->dozing) {
        return BLUETOOTH_DEVICE_IDLE;
    }
    const HalBluetoothDevice *device = &link->saved.devices[index];
    if (link->connected && same_address(&link->linked, device)) {
        return link->streamed ? BLUETOOTH_DEVICE_PLAYING : BLUETOOTH_DEVICE_CONNECTED;
    }
    if (connecting(link) && same_address(&link->target, device)) {
        return BLUETOOTH_DEVICE_CONNECTING;
    }
    bool starting = hal_bluetooth_status()->state == HAL_BLUETOOTH_STARTING;
    if (starting && index == 0 && !link->held && !link->connected) {
        return BLUETOOTH_DEVICE_CONNECTING;
    }
    return BLUETOOTH_DEVICE_IDLE;
}

bool bluetooth_link_knows(const BluetoothLink *link, const HalBluetoothDevice *device)
{
    return find_known(link, device) >= 0;
}

bool bluetooth_link_nearby(const BluetoothLink *link, int index)
{
    const HalBluetoothStatus *status = hal_bluetooth_status();
    if (index < 0 || index >= link->saved.count || link->connected) {
        return false;
    }
    for (uint8_t found = 0; found < status->device_count; found++) {
        if (same_address(&status->devices[found], &link->saved.devices[index])) {
            return true;
        }
    }
    return false;
}

const char *bluetooth_link_summary(const BluetoothLink *link)
{
    if (!link->saved.enabled) {
        return "Off";
    }
    if (link->connected) {
        return link->linked.name;
    }
    if (link->dozing) {
        return "Not connected";
    }
    HalBluetoothState state = hal_bluetooth_status()->state;
    if (state == HAL_BLUETOOTH_STARTING) {
        return "Starting…";
    }
    if (state == HAL_BLUETOOTH_NO_REPLY) {
        return "Not responding";
    }
    if (state == HAL_BLUETOOTH_SCANNING) {
        return "Searching…";
    }
    if (connecting(link)) {
        return "Connecting…";
    }
    return link->saved.count > 0 ? "Not connected" : "On";
}
