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
    UNKNOWN_SIZE = 1
};

static void set_state(BtModule *module, HalBluetoothState state, uint32_t now_ms)
{
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

static void add_device(HalBluetoothStatus *status, const uint8_t *payload, uint32_t length)
{
    if (length <= ADDRESS_SIZE || status->device_count >= HAL_BLUETOOTH_DEVICE_CAPACITY ||
        device_known(status, payload)) {
        return;
    }
    HalBluetoothDevice *device = &status->devices[status->device_count++];
    for (uint32_t index = 0; index < ADDRESS_SIZE; index++) {
        device->address[index] = payload[index];
    }
    uint32_t name_length = length - ADDRESS_SIZE;
    for (uint32_t index = 0; index < name_length; index++) {
        device->name[index] = (char)payload[ADDRESS_SIZE + index];
    }
    device->name[name_length] = '\0';
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

static void handle_frame(BtModule *module, const uint8_t *bytes, uint32_t size, uint32_t now_ms)
{
    module->status.link.frames++;
    module->scan_answered = true;
    switch (bytes[1]) {
    case 'V':
        handle_ready(module, bytes[3], now_ms);
        break;
    case 'S':
        handle_scan(module, bytes, size, now_ms);
        break;
    case 'Q':
        module->status.link_status = (char)bytes[2];
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

bool bt_module_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    if (module->status.state != HAL_BLUETOOTH_READY) {
        return false;
    }
    set_state(module, HAL_BLUETOOTH_SCANNING, now_ms);
    module->status.device_count = 0;
    module->scan_answered = false;
    module->scan_repeated = false;
    return command(frame, 'S');
}

static bool follow_scan(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    uint32_t elapsed = now_ms - module->state_since_ms;
    if (elapsed >= BT_MODULE_SCAN_LIMIT_MS) {
        set_state(module, HAL_BLUETOOTH_READY, now_ms);
        return command(frame, 'P');
    }
    if (!module->scan_answered && !module->scan_repeated && elapsed >= BT_MODULE_SCAN_RETRY_MS) {
        module->scan_repeated = true;
        return command(frame, 'S');
    }
    return false;
}

static bool query_status(BtModule *module, uint32_t now_ms, BtModuleFrame *frame)
{
    if (now_ms - module->last_query_ms < BT_MODULE_QUERY_PERIOD_MS) {
        return false;
    }
    module->last_query_ms = now_ms;
    return command(frame, 'Q');
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
    case HAL_BLUETOOTH_READY:
        return query_status(module, now_ms, frame);
    default:
        return false;
    }
}
