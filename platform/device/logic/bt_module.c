#include "device/bt_module.h"

enum {
    ADDRESS_SIZE = HAL_BLUETOOTH_ADDRESS_SIZE,
    SCAN_HEADER_SIZE = 4,
    SCAN_PAYLOAD_LIMIT = 38,
    CONNECTED_HEADER_SIZE = 5,
    SHORT_FRAME_SIZE = 2,
    STATUS_FRAME_SIZE = 3,
    WORD_FRAME_SIZE = 4,
    COMMAND_SIZE = 5,
    ADDRESS_COMMAND_SIZE = 10,
    STREAM_ON_LEVEL = 0x1E,
    PLAY_NOTICE = 0x86,
    PAUSE_NOTICE = 0x87,
    PLAY_STATE_STARTED = 0,
    PLAY_STATE_PAUSED = 1,
    PLAY_STATE_PLAYING = 2,
    UNKNOWN_SIZE = 1
};

static void set_state(BtModule *module, HalBluetoothState state, uint32_t now_ms)
{
    if (state == HAL_BLUETOOTH_CONNECTED && module->status.state != HAL_BLUETOOTH_CONNECTED) {
        module->resend_stream = true;
        module->resend_on = false;
    }
    if (state != HAL_BLUETOOTH_CONNECTED) {
        module->status.streaming = false;
        module->play_notice = 0;
        module->play_state = PLAY_STATE_STARTED;
        module->resend_stream = false;
    }
    module->status.state = state;
    module->state_since_ms = now_ms;
    module->last_query_ms = now_ms;
}

void bt_module_power_on(BtModule *module, uint32_t now_ms)
{
    *module = (BtModule){.status = {.state = HAL_BLUETOOTH_STARTING}};
    set_state(module, HAL_BLUETOOTH_STARTING, now_ms);
}

void bt_module_power_off(BtModule *module)
{
    *module = (BtModule){.status = {.state = HAL_BLUETOOTH_OFF}};
}

static bool is_digit(uint8_t byte)
{
    return byte >= '0' && byte <= '9';
}

static bool word_is(const uint8_t *bytes, char first, char second)
{
    return bytes[0] == (uint8_t)first && bytes[1] == (uint8_t)second;
}

static uint32_t payload_length(const uint8_t *digits)
{
    return 10u * (uint32_t)(digits[0] - '0') + (uint32_t)(digits[1] - '0');
}

static uint32_t scan_frame_size(const uint8_t *bytes, uint32_t available)
{
    if (available < SCAN_HEADER_SIZE || !is_digit(bytes[2]) || !is_digit(bytes[3])) {
        return SCAN_HEADER_SIZE;
    }
    uint32_t length = payload_length(bytes + 2);
    return length >= SCAN_PAYLOAD_LIMIT ? SCAN_HEADER_SIZE : SCAN_HEADER_SIZE + length;
}

static uint32_t status_frame_size(const uint8_t *bytes, uint32_t available)
{
    if (available < STATUS_FRAME_SIZE || bytes[2] != 'O') {
        return STATUS_FRAME_SIZE;
    }
    if (available < CONNECTED_HEADER_SIZE || !is_digit(bytes[3]) || !is_digit(bytes[4])) {
        return CONNECTED_HEADER_SIZE;
    }
    return CONNECTED_HEADER_SIZE + payload_length(bytes + 3);
}

static uint32_t result_frame_size(const uint8_t *bytes, uint32_t available)
{
    if (available < WORD_FRAME_SIZE) {
        return WORD_FRAME_SIZE;
    }
    bool result = word_is(bytes + 2, 'O', 'K') || word_is(bytes + 2, 'E', 'R');
    return result ? WORD_FRAME_SIZE : SHORT_FRAME_SIZE;
}

static uint32_t frame_size(const uint8_t *bytes, uint32_t available)
{
    switch (bytes[1]) {
    case 'S':
        return scan_frame_size(bytes, available);
    case 'Q':
        return status_frame_size(bytes, available);
    case 'T':
    case 'H':
        return result_frame_size(bytes, available);
    case 'V':
    case 'K':
    case 'L':
        return WORD_FRAME_SIZE;
    case 'A':
    case 'B':
    case 'P':
    case 'C':
        return SHORT_FRAME_SIZE;
    default:
        return UNKNOWN_SIZE;
    }
}

static bool device_known(const HalBluetoothStatus *status, const uint8_t *address)
{
    for (uint8_t index = 0; index < status->device_count; index++) {
        const uint8_t *known = status->devices[index].address;
        uint8_t same = 0;
        while (same < ADDRESS_SIZE && known[same] == address[same]) {
            same++;
        }
        if (same == ADDRESS_SIZE) {
            return true;
        }
    }
    return false;
}

static void copy_device(HalBluetoothDevice *device, const uint8_t *payload, uint32_t length)
{
    *device = (HalBluetoothDevice){0};
    if (length < ADDRESS_SIZE) {
        return;
    }
    for (uint32_t index = 0; index < ADDRESS_SIZE; index++) {
        device->address[index] = payload[index];
    }
    uint32_t name_length = length - ADDRESS_SIZE;
    if (name_length > HAL_BLUETOOTH_NAME_CAPACITY - 1) {
        name_length = HAL_BLUETOOTH_NAME_CAPACITY - 1;
    }
    for (uint32_t index = 0; index < name_length; index++) {
        device->name[index] = (char)payload[ADDRESS_SIZE + index];
    }
}

static void add_device(HalBluetoothStatus *status, const uint8_t *payload, uint32_t length)
{
    if (length <= ADDRESS_SIZE || status->device_count >= HAL_BLUETOOTH_DEVICE_CAPACITY ||
        device_known(status, payload)) {
        return;
    }
    copy_device(&status->devices[status->device_count++], payload, length);
}

static void end_scan(BtModule *module, uint32_t now_ms)
{
    if (module->status.state == HAL_BLUETOOTH_SCANNING) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
    }
}

static void handle_scan(BtModule *module, const uint8_t *bytes, uint32_t size, uint32_t now_ms)
{
    if (word_is(bytes + 2, 'E', 'N') || size == SCAN_HEADER_SIZE) {
        if (!word_is(bytes + 2, 'E', 'R')) {
            end_scan(module, now_ms);
        }
        return;
    }
    add_device(&module->status, bytes + SCAN_HEADER_SIZE, size - SCAN_HEADER_SIZE);
}

static void handle_ready(BtModule *module, uint8_t version, uint32_t now_ms)
{
    module->status.version = (char)version;
    if (module->status.state == HAL_BLUETOOTH_STARTING) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
    }
}

static void handle_status(BtModule *module, const uint8_t *bytes, uint32_t size, uint32_t now_ms)
{
    module->status.link_status = (char)bytes[2];
    if (bytes[2] == 'O') {
        copy_device(&module->status.connected, bytes + CONNECTED_HEADER_SIZE,
                    size - CONNECTED_HEADER_SIZE);
        set_state(module, HAL_BLUETOOTH_CONNECTED, now_ms);
    } else if (module->status.state == HAL_BLUETOOTH_CONNECTED) {
        module->status.pairing = HAL_BLUETOOTH_PAIRING_NONE;
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
    }
}

static void finish_connect(BtModule *module, HalBluetoothPairing pairing, uint32_t now_ms)
{
    module->status.pairing = pairing;
    if (module->status.state == HAL_BLUETOOTH_CONNECTING) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
    }
}

static void mark_connected(BtModule *module, uint32_t now_ms)
{
    module->status.pairing = HAL_BLUETOOTH_PAIRED;
    module->status.connected = module->target;
    set_state(module, HAL_BLUETOOTH_CONNECTED, now_ms);
}

static void handle_link(BtModule *module, const uint8_t *bytes, uint32_t size, uint32_t now_ms)
{
    bool failed = size == WORD_FRAME_SIZE && word_is(bytes + 2, 'E', 'R');
    if (failed) {
        finish_connect(module, HAL_BLUETOOTH_PAIRING_FAILED, now_ms);
    } else if (bytes[1] == 'H') {
        mark_connected(module, now_ms);
    } else if (word_is(bytes + 2, 'O', 'K')) {
        finish_connect(module, HAL_BLUETOOTH_PAIRED, now_ms);
    }
}

static void handle_disconnected(BtModule *module, uint32_t now_ms)
{
    HalBluetoothState state = module->status.state;
    module->status.pairing = HAL_BLUETOOTH_PAIRING_NONE;
    if (state == HAL_BLUETOOTH_CONNECTED || state == HAL_BLUETOOTH_CONNECTING) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
    }
}

static void handle_play_report(BtModule *module, bool playing, uint32_t now_ms)
{
    if (module->status.state != HAL_BLUETOOTH_CONNECTED &&
        module->status.pairing == HAL_BLUETOOTH_PAIRED) {
        mark_connected(module, now_ms);
    }
    module->play_state = playing ? PLAY_STATE_PLAYING : PLAY_STATE_PAUSED;
    module->query_paused = module->status.state == HAL_BLUETOOTH_CONNECTED;
    module->query_paused_ms = now_ms;
    if (playing) {
        module->status.remote.plays++;
    } else {
        module->status.remote.pauses++;
    }
}

static char event_letter(uint8_t byte)
{
    return byte >= 'A' && byte <= 'Z' ? (char)byte : '.';
}

static void note_event(HalBluetoothLink *link, const uint8_t *bytes, uint32_t size)
{
    if (bytes[1] == 'Q' || bytes[1] == 'S') {
        return;
    }
    if (link->event_count == HAL_BLUETOOTH_EVENT_CAPACITY) {
        for (uint32_t index = 1; index < HAL_BLUETOOTH_EVENT_CAPACITY; index++) {
            link->events[index - 1][0] = link->events[index][0];
            link->events[index - 1][1] = link->events[index][1];
        }
        link->event_count--;
    }
    link->events[link->event_count][0] = event_letter(bytes[1]);
    link->events[link->event_count][1] = size > 2 ? event_letter(bytes[2]) : '.';
    link->event_count++;
}

static void handle_frame(BtModule *module, const uint8_t *bytes, uint32_t size, uint32_t now_ms)
{
    module->status.link.frames++;
    module->answered = true;
    note_event(&module->status.link, bytes, size);
    switch (bytes[1]) {
    case 'V':
        handle_ready(module, bytes[3], now_ms);
        break;
    case 'S':
        handle_scan(module, bytes, size, now_ms);
        break;
    case 'Q':
        handle_status(module, bytes, size, now_ms);
        break;
    case 'L':
    case 'H':
        handle_link(module, bytes, size, now_ms);
        break;
    case 'T':
        handle_disconnected(module, now_ms);
        break;
    case 'P':
    case 'C':
        handle_play_report(module, bytes[1] == 'C', now_ms);
        break;
    case 'A':
        module->status.remote.previouses++;
        break;
    case 'B':
        module->status.remote.nexts++;
        break;
    default:
        break;
    }
}

static uint32_t parse_at(BtModule *module, uint32_t start, bool silent, uint32_t now_ms)
{
    const uint8_t *bytes = module->chunk + start;
    uint32_t available = module->chunk_length - start;
    if (bytes[0] != 'C') {
        return UNKNOWN_SIZE;
    }
    if (available < SHORT_FRAME_SIZE) {
        return silent ? available : 0;
    }
    uint32_t size = frame_size(bytes, available);
    if (size > available) {
        return silent ? available : 0;
    }
    if (size > UNKNOWN_SIZE) {
        handle_frame(module, bytes, size, now_ms);
    }
    return size;
}

static void drop_front(BtModule *module, uint32_t count)
{
    uint32_t kept = module->chunk_length - count;
    for (uint32_t index = 0; index < kept; index++) {
        module->chunk[index] = module->chunk[count + index];
    }
    module->chunk_length = (uint8_t)kept;
}

static void parse(BtModule *module, bool silent, uint32_t now_ms)
{
    uint32_t start = 0;
    while (start < module->chunk_length) {
        uint32_t consumed = parse_at(module, start, silent, now_ms);
        if (consumed == 0) {
            break;
        }
        start += consumed;
    }
    drop_front(module, start);
}

static void remember(HalBluetoothLink *link, uint8_t byte)
{
    link->received++;
    if (link->recent_length < HAL_BLUETOOTH_RECENT_CAPACITY) {
        link->recent[link->recent_length++] = byte;
        return;
    }
    for (uint32_t index = 1; index < HAL_BLUETOOTH_RECENT_CAPACITY; index++) {
        link->recent[index - 1] = link->recent[index];
    }
    link->recent[HAL_BLUETOOTH_RECENT_CAPACITY - 1] = byte;
}

void bt_module_receive(BtModule *module, uint8_t byte, uint32_t now_ms)
{
    remember(&module->status.link, byte);
    module->last_byte_ms = now_ms;
    if (module->chunk_length == BT_MODULE_CHUNK_CAPACITY) {
        parse(module, false, now_ms);
    }
    if (module->chunk_length < BT_MODULE_CHUNK_CAPACITY) {
        module->chunk[module->chunk_length++] = byte;
    }
}

static bool command(BtModuleFrame *frame, char letter)
{
    *frame = (BtModuleFrame){.bytes = {'C', (uint8_t)letter, '0', '0', 0}, .length = COMMAND_SIZE};
    return true;
}

static bool address_command(BtModuleFrame *frame, char letter, const uint8_t *address)
{
    *frame =
        (BtModuleFrame){.bytes = {'C', (uint8_t)letter, '0', '6'}, .length = ADDRESS_COMMAND_SIZE};
    for (uint32_t index = 0; index < ADDRESS_SIZE; index++) {
        frame->bytes[WORD_FRAME_SIZE + index] = address[index];
    }
    return true;
}

static void start_waiting(BtModule *module, HalBluetoothState state, uint32_t now_ms)
{
    set_state(module, state, now_ms);
    module->answered = false;
    module->repeated = false;
}

bool bt_module_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    if (module->status.state != HAL_BLUETOOTH_READY) {
        return false;
    }
    start_waiting(module, HAL_BLUETOOTH_SCANNING, now_ms);
    module->status.device_count = 0;
    return command(frame, 'S');
}

bool bt_module_connect(BtModule *module, const HalBluetoothDevice *device, uint32_t now_ms,
                       BtModuleFrame *frame)
{
    HalBluetoothState state = module->status.state;
    if (state != HAL_BLUETOOTH_READY && state != HAL_BLUETOOTH_SCANNING) {
        return false;
    }
    module->target = *device;
    module->status.pairing = HAL_BLUETOOTH_PAIRING_NONE;
    start_waiting(module, HAL_BLUETOOTH_CONNECTING, now_ms);
    return address_command(frame, 'L', module->target.address);
}

bool bt_module_disconnect(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    HalBluetoothState state = module->status.state;
    bool paired = state == HAL_BLUETOOTH_READY && module->status.pairing == HAL_BLUETOOTH_PAIRED;
    if (state == HAL_BLUETOOTH_CONNECTING) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
    } else if (state != HAL_BLUETOOTH_CONNECTED && !paired) {
        return false;
    }
    module->status.pairing = HAL_BLUETOOTH_PAIRING_NONE;
    return command(frame, 'T');
}

static bool follow_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    uint32_t elapsed = now_ms - module->state_since_ms;
    if (elapsed >= BT_MODULE_SCAN_LIMIT_MS) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
        return command(frame, 'P');
    }
    if (!module->answered && !module->repeated && elapsed >= BT_MODULE_SCAN_RETRY_MS) {
        module->repeated = true;
        return command(frame, 'S');
    }
    return false;
}

static bool follow_connect(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    uint32_t elapsed = now_ms - module->state_since_ms;
    if (elapsed >= BT_MODULE_CONNECT_LIMIT_MS) {
        module->status.pairing = HAL_BLUETOOTH_PAIRING_FAILED;
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
        return command(frame, 'T');
    }
    if (!module->answered && !module->repeated && elapsed >= BT_MODULE_CONNECT_RETRY_MS) {
        module->repeated = true;
        return address_command(frame, 'H', module->target.address);
    }
    return false;
}

static bool query_status(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    if (module->query_paused && now_ms - module->query_paused_ms < BT_MODULE_QUERY_PAUSE_MS) {
        return false;
    }
    module->query_paused = false;
    if (now_ms - module->last_query_ms < BT_MODULE_QUERY_PERIOD_MS) {
        return false;
    }
    module->last_query_ms = now_ms;
    return command(frame, 'Q');
}

static bool answer_play_report(BtModule *module, BtModuleFrame *frame)
{
    if (!module->resend_stream) {
        return false;
    }
    module->resend_stream = false;
    *frame = (BtModuleFrame){.bytes = {'C', 'Y', '0', module->resend_on ? STREAM_ON_LEVEL : 0, 0},
                             .length = COMMAND_SIZE};
    return true;
}

static bool send_play_notice(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    if (module->play_notice == 0 ||
        now_ms - module->stream_changed_ms < BT_MODULE_PLAY_NOTICE_DELAY_MS) {
        return false;
    }
    *frame =
        (BtModuleFrame){.bytes = {'C', module->play_notice, '0', '0', 0}, .length = COMMAND_SIZE};
    module->play_notice = 0;
    return true;
}

static bool wait_for_ready(BtModule *module, uint32_t now_ms)
{
    if (now_ms - module->state_since_ms >= BT_MODULE_READY_TIMEOUT_MS) {
        set_state(module, HAL_BLUETOOTH_NO_REPLY, now_ms);
    }
    return false;
}

bool bt_module_poll(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    parse(module, now_ms - module->last_byte_ms >= BT_MODULE_SILENCE_MS, now_ms);
    switch (module->status.state) {
    case HAL_BLUETOOTH_STARTING:
        return wait_for_ready(module, now_ms);
    case HAL_BLUETOOTH_SCANNING:
        return follow_scan(module, now_ms, frame);
    case HAL_BLUETOOTH_CONNECTING:
        return follow_connect(module, now_ms, frame);
    case HAL_BLUETOOTH_CONNECTED:
        return answer_play_report(module, frame) || send_play_notice(module, now_ms, frame) ||
               query_status(module, now_ms, frame);
    case HAL_BLUETOOTH_READY:
        return query_status(module, now_ms, frame);
    default:
        return false;
    }
}

bool bt_module_set_stream(BtModule *module, bool on, uint32_t now_ms, BtModuleFrame *frame)
{
    if (module->status.state != HAL_BLUETOOTH_CONNECTED) {
        return false;
    }
    module->status.streaming = on;
    module->play_notice = 0;
    if (on && module->play_state == PLAY_STATE_PAUSED) {
        module->play_notice = PLAY_NOTICE;
        module->play_state = PLAY_STATE_PLAYING;
    } else if (!on && module->play_state != PLAY_STATE_PAUSED) {
        module->play_notice = PAUSE_NOTICE;
        module->play_state = PLAY_STATE_PAUSED;
    }
    module->stream_changed_ms = now_ms;
    *frame = (BtModuleFrame){.bytes = {'C', 'Y', '0', on ? STREAM_ON_LEVEL : 0, 0},
                             .length = COMMAND_SIZE};
    return true;
}
