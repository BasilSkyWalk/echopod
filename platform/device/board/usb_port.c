#include <stddef.h>

#include "hal/clock.h"
#include "usb.h"
#include "usb_registers.h"

static void arm_setup(void)
{
    write_register(out_control(0) + 0x10u, EP0_OUT_SETUP_SIZE);
    set_bits(out_control(0), EP_ENABLE | EP_CLEAR_NAK);
}

static void arm_bulk_out(const UsbPort *port)
{
    write_register(out_control(BULK_ENDPOINT) + 0x10u, ONE_PACKET | port->bulk_packet);
    set_bits(out_control(BULK_ENDPOINT), EP_ENABLE | EP_CLEAR_NAK);
}

static uint32_t tx_space(const UsbPort *port, uint32_t endpoint)
{
    if (port->link.dedicated_fifos) {
        return read_register(in_control(endpoint) + 0x18u) & TX_SPACE_MASK;
    }
    uint32_t status = read_register(GNPTXSTS);
    bool queue_has_room = (status & TX_QUEUE_MASK) != 0;
    return queue_has_room ? status & TX_SPACE_MASK : 0;
}

static bool send(const UsbPort *port, uint32_t endpoint, const uint8_t *bytes, uint32_t length)
{
    uint32_t words = (length + 3) / 4;
    uint32_t start = hal_clock_milliseconds();
    bool space_unreported = port->link.dedicated_fifos && endpoint == 0;
    while (!space_unreported && tx_space(port, endpoint) < words) {
        if (hal_clock_milliseconds() - start > SPACE_TIMEOUT_MS) {
            return false;
        }
    }
    write_register(in_control(endpoint) + 0x10u, ONE_PACKET | length);
    set_bits(in_control(endpoint), EP_ENABLE | EP_CLEAR_NAK);
    for (uint32_t word = 0; word < words; word++) {
        uint32_t value = 0;
        for (uint32_t byte = 0; byte < 4 && word * 4 + byte < length; byte++) {
            value |= (uint32_t)bytes[word * 4 + byte] << (8 * byte);
        }
        write_register(fifo(endpoint), value);
    }
    return true;
}

static bool send_control(const UsbPort *port, const uint8_t *bytes, uint32_t length)
{
    uint32_t sent = 0;
    while (length - sent > USB_PACKET_SIZE) {
        if (!send(port, 0, bytes + sent, USB_PACKET_SIZE) ||
            !wait_clear(in_control(0), EP_ENABLE, SPACE_TIMEOUT_MS)) {
            return false;
        }
        sent += USB_PACKET_SIZE;
    }
    return send(port, 0, bytes == NULL ? NULL : bytes + sent, length - sent);
}

static uint32_t next_frame_parity(void)
{
    return (read_register(DSTS) & DSTS_ODD_FRAME) != 0 ? EP_SET_EVEN_FRAME : EP_SET_ODD_FRAME;
}

static void arm_iso_out(void)
{
    uint32_t packet = USB_AUDIO_MAX_PACKET;
    uint32_t control = read_register(out_control(BULK_ENDPOINT)) & ~EP_MAX_PACKET_MASK;
    write_register(out_control(BULK_ENDPOINT) + 0x10u, ONE_PACKET | packet);
    write_register(out_control(BULK_ENDPOINT),
                   control | packet | EP_ENABLE | EP_CLEAR_NAK | next_frame_parity());
}

static bool streaming(const UsbPort *port)
{
    return port->audio != NULL && port->audio->alt != 0;
}

static void start_stream(const UsbPort *port)
{
    if (streaming(port) && (read_register(out_control(BULK_ENDPOINT)) & EP_ENABLE) == 0) {
        arm_iso_out();
    }
}

static void handle_incomplete_iso(void)
{
    uint32_t control = read_register(out_control(BULK_ENDPOINT));
    bool odd_now = (read_register(DSTS) & DSTS_ODD_FRAME) != 0;
    bool armed_for_now = ((control & EP_ODD_FRAME) != 0) == odd_now;
    if ((control & EP_ENABLE) != 0 && armed_for_now) {
        write_register(out_control(BULK_ENDPOINT), control | next_frame_parity());
    }
    write_register(GINTSTS, GINT_INCOMPLETE_ISO_OUT);
}

static void clear_endpoint_interrupts(uint32_t endpoint)
{
    write_register(in_control(endpoint) + 0x08u, EP_INT_ALL);
    write_register(out_control(endpoint) + 0x08u, EP_INT_ALL);
}

static void handle_reset(UsbPort *port)
{
    write_register(DCFG, read_register(DCFG) & ~DCFG_ADDRESS_MASK);
    for (uint32_t endpoint = 0; endpoint < 4; endpoint++) {
        clear_endpoint_interrupts(endpoint);
        if (endpoint > 0 && (read_register(in_control(endpoint)) & EP_ENABLE) != 0) {
            write_register(in_control(endpoint), EP_IDLE_CONTROL);
        }
        if (endpoint > 0 && (read_register(out_control(endpoint)) & EP_ENABLE) != 0) {
            write_register(out_control(endpoint), EP_IDLE_CONTROL);
        }
    }
    usb_flush_fifos();
    if (port->configured) {
        port->configured_resets++;
    }
    port->configured = false;
    port->in_busy = false;
    port->report_busy = false;
    port->control_out_pending = false;
    if (port->msc != NULL) {
        msc_reset(port->msc);
    }
    if (port->audio != NULL) {
        usb_audio_reset(port->audio);
    }
    arm_setup();
    write_register(GINTSTS, GINT_USB_RESET);
}

static void activate_bulk_endpoints(const UsbPort *port)
{
    uint32_t tx_fifo = port->link.dedicated_fifos ? 1u << EP_TX_FIFO_SHIFT : 0;
    write_register(in_control(BULK_ENDPOINT), port->bulk_packet | tx_fifo | EP_BULK_ACTIVE);
    write_register(out_control(BULK_ENDPOINT), port->bulk_packet | EP_BULK_ACTIVE);
    set_bits(DAINTMSK, DAINT_EP1);
    set_bits(DCTL, DCTL_CLEAR_GLOBAL_IN_NAK);
    arm_bulk_out(port);
}

static void activate_endpoints(const UsbPort *port)
{
    if (port->audio == NULL) {
        activate_bulk_endpoints(port);
        return;
    }
    write_register(out_control(BULK_ENDPOINT), USB_AUDIO_MAX_PACKET | EP_ISO_ACTIVE);
    uint32_t tx_fifo = port->link.dedicated_fifos ? 1u << EP_TX_FIFO_SHIFT : 0;
    write_register(in_control(HID_ENDPOINT), USB_HID_PACKET_SIZE | tx_fifo | EP_INTERRUPT_ACTIVE);
    set_bits(DAINTMSK, DAINT_EP1 | DAINT_EP2_IN);
    set_bits(GINTMSK, GINT_INCOMPLETE_ISO_OUT);
}

static void clear_halt(uint16_t endpoint)
{
    uint32_t number = endpoint & 0x0Fu;
    uint32_t control = (endpoint & 0x80u) != 0 ? in_control(number) : out_control(number);
    write_register(control, (read_register(control) & ~EP_STALL) | EP_SET_DATA0);
}

static void apply_action(UsbPort *port, UsbReply reply)
{
    switch (reply.action) {
    case USB_ACTION_SET_ADDRESS:
        write_register(DCFG, (read_register(DCFG) & ~DCFG_ADDRESS_MASK) |
                                 ((uint32_t)reply.value << DCFG_ADDRESS_SHIFT));
        break;
    case USB_ACTION_CONFIGURE:
        port->configured = reply.value == 1;
        if (port->configured) {
            activate_endpoints(port);
        }
        break;
    case USB_ACTION_STREAM:
        start_stream(port);
        break;
    case USB_ACTION_CLEAR_HALT:
        clear_halt(reply.value);
        break;
    case USB_ACTION_MSC_RESET:
        msc_reset(port->msc);
        port->in_busy = false;
        break;
    case USB_ACTION_NONE:
        break;
    }
}

static void finish_setup(UsbPort *port, UsbSetup setup, UsbReply reply)
{
    apply_action(port, reply);
    if (reply.kind == USB_REPLY_STALL) {
        set_bits(in_control(0), EP_STALL);
        set_bits(out_control(0), EP_STALL);
    } else if (reply.kind == USB_REPLY_DATA) {
        send_control(port, reply.data, reply.length);
    } else if ((setup.request_type & 0x80u) == 0) {
        send(port, 0, NULL, 0);
    }
}

static void process_setup(UsbPort *port)
{
    UsbSetup setup = usb_setup_parse((const uint8_t *)port->setup);
    UsbReply reply = port->audio != NULL ? usb_audio_setup(port->audio, setup)
                                         : usb_device_setup(&port->device, setup);
    port->setup_pending = false;
    if (reply.kind == USB_REPLY_RECEIVE) {
        port->control_out = setup;
        port->control_out_pending = true;
        return;
    }
    finish_setup(port, setup, reply);
}

static void finish_control_out(UsbPort *port, uint32_t length)
{
    port->control_out_pending = false;
    UsbReply reply = usb_audio_receive(port->audio, port->control_out,
                                       (const uint8_t *)port->out_packet, (uint16_t)length);
    finish_setup(port, port->control_out, reply);
}

static void deliver(UsbPort *port, uint32_t endpoint, uint32_t length)
{
    const uint8_t *bytes = (const uint8_t *)port->out_packet;
    if (endpoint == 0 && port->control_out_pending) {
        finish_control_out(port, length);
    } else if (endpoint == BULK_ENDPOINT && port->audio == NULL) {
        msc_receive(port->msc, bytes, length);
    } else if (endpoint == BULK_ENDPOINT && port->receive_stream != NULL) {
        port->receive_stream(bytes, length);
    }
}

static void read_fifo(uint32_t *words, uint32_t capacity_words, uint32_t bytes)
{
    for (uint32_t index = 0; index < (bytes + 3) / 4; index++) {
        uint32_t value = read_register(fifo(0));
        if (index < capacity_words) {
            words[index] = value;
        }
    }
}

static void handle_receive(UsbPort *port)
{
    uint32_t status = read_register(GRXSTSP);
    uint32_t endpoint = status & RX_STATUS_ENDPOINT_MASK;
    uint32_t count = (status >> RX_STATUS_COUNT_SHIFT) & RX_STATUS_COUNT_MASK;
    uint32_t type = (status >> RX_STATUS_TYPE_SHIFT) & RX_STATUS_TYPE_MASK;
    if (type == PACKET_SETUP_DATA) {
        read_fifo(port->setup, sizeof port->setup / 4, count);
        port->setup_pending = true;
    } else if (type == PACKET_OUT_DATA) {
        read_fifo(port->out_packet, sizeof port->out_packet / 4, count);
        port->packet_frame =
            (uint16_t)((read_register(DSTS) >> DSTS_FRAME_SHIFT) & DSTS_FRAME_MASK);
        uint32_t capacity = sizeof port->out_packet;
        deliver(port, endpoint, count < capacity ? count : capacity);
    }
}

static void handle_out_endpoints(UsbPort *port)
{
    uint32_t pending = read_register(DAINT);
    if ((pending & (1u << 16)) != 0) {
        uint32_t events = read_register(out_control(0) + 0x08u);
        write_register(out_control(0) + 0x08u, events);
        if ((events & EP_INT_SETUP) != 0 && port->setup_pending) {
            process_setup(port);
        }
        arm_setup();
    }
    if ((pending & (1u << (16 + BULK_ENDPOINT))) != 0) {
        uint32_t events = read_register(out_control(BULK_ENDPOINT) + 0x08u);
        write_register(out_control(BULK_ENDPOINT) + 0x08u, events);
        if ((events & EP_INT_TRANSFER_DONE) != 0 && port->audio == NULL) {
            arm_bulk_out(port);
        } else if ((events & EP_INT_TRANSFER_DONE) != 0 && streaming(port)) {
            arm_iso_out();
        }
    }
}

static void handle_in_endpoints(UsbPort *port)
{
    uint32_t pending = read_register(DAINT);
    if ((pending & 1u) != 0) {
        write_register(in_control(0) + 0x08u, read_register(in_control(0) + 0x08u));
    }
    if ((pending & (1u << BULK_ENDPOINT)) != 0) {
        uint32_t events = read_register(in_control(BULK_ENDPOINT) + 0x08u);
        write_register(in_control(BULK_ENDPOINT) + 0x08u, events);
        if ((events & EP_INT_TRANSFER_DONE) != 0) {
            port->in_busy = false;
        }
    }
    if ((pending & (1u << HID_ENDPOINT)) != 0) {
        uint32_t events = read_register(in_control(HID_ENDPOINT) + 0x08u);
        write_register(in_control(HID_ENDPOINT) + 0x08u, events);
        if ((events & EP_INT_TRANSFER_DONE) != 0) {
            port->report_busy = false;
        }
    }
}

static bool wait_for_space(const UsbPort *port, uint32_t endpoint, uint32_t words)
{
    uint32_t start = hal_clock_milliseconds();
    while (tx_space(port, endpoint) < words) {
        if (hal_clock_milliseconds() - start > SPACE_TIMEOUT_MS) {
            return false;
        }
    }
    return true;
}

static bool send_bulk(const UsbPort *port, const uint32_t *words, uint32_t length)
{
    uint32_t packet = port->bulk_packet;
    uint32_t packets = length == 0 ? 1 : (length + packet - 1) / packet;
    uint32_t total_words = (length + 3) / 4;
    write_register(in_control(BULK_ENDPOINT) + 0x10u, packets << PACKET_COUNT_SHIFT | length);
    set_bits(in_control(BULK_ENDPOINT), EP_ENABLE | EP_CLEAR_NAK);
    for (uint32_t sent = 0; sent < total_words;) {
        uint32_t chunk = total_words - sent < PACKET_WORDS ? total_words - sent : PACKET_WORDS;
        if (!wait_for_space(port, BULK_ENDPOINT, chunk)) {
            return false;
        }
        for (uint32_t index = 0; index < chunk; index++) {
            write_register(fifo(BULK_ENDPOINT), words[sent + index]);
        }
        sent += chunk;
    }
    return true;
}

static void pump_bulk_in(UsbPort *port)
{
    static uint32_t chunk[BULK_CHUNK_BYTES / 4];
    uint32_t length = 0;
    if (!port->configured || port->msc == NULL) {
        return;
    }
    if (port->in_busy) {
        msc_prefetch(port->msc);
        return;
    }
    if (msc_next_in(port->msc, (uint8_t *)chunk, BULK_CHUNK_BYTES, &length)) {
        port->in_busy = send_bulk(port, chunk, length);
    }
}

bool usb_wait_for_host(UsbPort *port, uint32_t timeout_ms)
{
    uint32_t start = hal_clock_milliseconds();
    while (hal_clock_milliseconds() - start < timeout_ms) {
        if ((read_register(GINTSTS) & GINT_USB_RESET) != 0) {
            handle_reset(port);
            return true;
        }
    }
    return false;
}

static void take_speed(UsbPort *port)
{
    uint32_t speed = (read_register(DSTS) >> DSTS_SPEED_SHIFT) & DSTS_SPEED_MASK;
    bool high = port->link.high_speed_allowed && speed == DSTS_HIGH_SPEED;
    usb_device_set_speed(&port->device, high ? USB_SPEED_HIGH : USB_SPEED_FULL);
    port->bulk_packet = usb_device_bulk_packet_size(&port->device);
}

void usb_poll(UsbPort *port)
{
    uint32_t events = read_register(GINTSTS);
    if ((events & GINT_USB_RESET) != 0) {
        handle_reset(port);
    }
    if ((events & GINT_ENUMERATION_DONE) != 0) {
        take_speed(port);
        arm_setup();
        write_register(GINTSTS, GINT_ENUMERATION_DONE);
    }
    if ((events & GINT_RX_LEVEL) != 0) {
        handle_receive(port);
    }
    if ((events & GINT_OUT_ENDPOINT) != 0) {
        handle_out_endpoints(port);
    }
    if ((events & GINT_IN_ENDPOINT) != 0) {
        handle_in_endpoints(port);
    }
    if ((events & GINT_INCOMPLETE_ISO_OUT) != 0 && port->audio != NULL) {
        handle_incomplete_iso();
    }
    pump_bulk_in(port);
}

bool usb_reply_pending(const UsbPort *port)
{
    return port->in_busy || port->msc->phase == MSC_PHASE_STATUS;
}

bool usb_send_report(UsbPort *port, uint8_t report)
{
    if (port->audio == NULL || !port->configured || port->report_busy) {
        return false;
    }
    usb_audio_set_report(port->audio, report);
    port->report_busy = send(port, HID_ENDPOINT, &report, USB_HID_REPORT_LENGTH);
    return port->report_busy;
}

bool usb_port_connect(UsbPort *port)
{
    port->bulk_packet = USB_PACKET_SIZE;
    usb_device_set_speed(&port->device, USB_SPEED_FULL);
    return usb_connect(&port->link);
}
