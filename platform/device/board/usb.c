#include "usb.h"

#include <stddef.h>

#include "board.h"
#include "cru.h"
#include "gpio.h"
#include "hal/clock.h"
#include "registers.h"

#define USB_BASE 0x60080000u
#define GOTGCTL 0x000u
#define GOTGINT 0x004u
#define GAHBCFG 0x008u
#define GRSTCTL 0x010u
#define GINTSTS 0x014u
#define GINTMSK 0x018u
#define GRXSTSP 0x020u
#define GRXFSIZ 0x024u
#define GNPTXFSIZ 0x028u
#define GNPTXSTS 0x02Cu
#define GSNPSID 0x040u
#define GHWCFG3 0x04Cu
#define GHWCFG4 0x050u
#define DIEPTXF1 0x104u
#define DCFG 0x800u
#define DCTL 0x804u
#define DIEPMSK 0x810u
#define DOEPMSK 0x814u
#define DAINT 0x818u
#define DAINTMSK 0x81Cu
#define PCGCCTL 0xE00u

#define GRSTCTL_SOFT_RESET (1u << 0)
#define GRSTCTL_RX_FLUSH (1u << 4)
#define GRSTCTL_TX_FLUSH (1u << 5)
#define GRSTCTL_FLUSH_ALL_TX 0x420u
#define GRSTCTL_AHB_IDLE (1u << 31)
#define GRSTCTL_AFTER_FLUSH 0x8u
#define GAHBCFG_TX_EMPTY_LEVEL (1u << 7)
#define GINT_RX_LEVEL (1u << 4)
#define GINT_USB_RESET (1u << 12)
#define GINT_ENUMERATION_DONE (1u << 13)
#define GINT_IN_ENDPOINT (1u << 18)
#define GINT_OUT_ENDPOINT (1u << 19)
#define GINTMSK_VALUE 0xC00C3C10u
#define DCFG_CLEARED_BITS 0x7F3u
#define DCFG_FULL_SPEED 0x1u
#define DCFG_ADDRESS_SHIFT 4
#define DCFG_ADDRESS_MASK (0x7Fu << DCFG_ADDRESS_SHIFT)
#define DCTL_SOFT_DISCONNECT (1u << 1)
#define DCTL_CLEAR_GLOBAL_IN_NAK (1u << 8)
#define EP_ENABLE (1u << 31)
#define EP_CLEAR_NAK (1u << 26)
#define EP_SET_DATA0 (1u << 28)
#define EP_STALL (1u << 21)
#define EP_TX_FIFO_SHIFT 22
#define EP_IDLE_CONTROL 0x48000000u
#define EP_BULK_ACTIVE 0x18088000u
#define EP_INT_TRANSFER_DONE (1u << 0)
#define EP_INT_SETUP (1u << 3)
#define EP_INT_ALL 0xFFFFu
#define EP0_OUT_SETUP_SIZE 0x60080040u
#define ONE_PACKET (1u << 19)
#define DIEPMSK_VALUE 0x2Fu
#define DOEPMSK_VALUE 0x0Fu
#define DAINT_EP0 0x00010001u
#define DAINT_EP1 0x00020002u
#define RX_STATUS_ENDPOINT_MASK 0xFu
#define RX_STATUS_COUNT_SHIFT 4
#define RX_STATUS_COUNT_MASK 0x7FFu
#define RX_STATUS_TYPE_SHIFT 17
#define RX_STATUS_TYPE_MASK 0xFu
#define HWCFG3_DEPTH_SHIFT 16
#define HWCFG4_DEDICATED_FIFOS (1u << 25)
#define SHARED_RX_SIZE 0x120u
#define SHARED_TX_SIZE 0x01000120u
#define DEDICATED_TX0_SIZE 0x00200120u
#define DEDICATED_TX1_SIZE 0x01000140u
#define SHARED_MINIMUM_DEPTH 0x220u
#define DEDICATED_MINIMUM_DEPTH 0x240u
#define TX_SPACE_MASK 0xFFFFu
#define GRF_VBUS 0xE8u
#define GRF_VBUS_PRESENT (1u << 1)
#define INT_ALL 0xFFFFFFFFu

enum {
    GATE_USB_A = 0x07,
    GATE_USB_B = 0x6A,
    GATE_GPIO1 = 0x95,
    RESET_USB_A = 0x00,
    RESET_USB_B = 0x01,
    RESET_USB_C = 0x02,
    PACKET_OUT_DATA = 2,
    PACKET_SETUP_DATA = 6,
    BULK_ENDPOINT = 1,
    RESET_TIMEOUT_MS = 100,
    SPACE_TIMEOUT_MS = 100,
    PHY_SETTLE_US = 5,
    PHY_READY_MS = 10,
    CLOCK_GATE_MS = 10,
    DISCONNECT_HOLD_MS = 150
};

typedef struct GrfWrite {
    uint32_t offset;
    uint32_t value;
} GrfWrite;

static const GrfWrite PHY_ON_FIRST[] = {
    {0xB8, 0x00010000}, {0xB4, 0x000E0002}, {0x98, 0x00040004}, {0xB4, 0x00010000}};
static const GrfWrite PHY_ON_SECOND[] = {
    {0x88, 0x0C000C00}, {0x84, 0x1FE00003}, {0x8C, 0x001C0014}, {0x80, 0x00070004}};
static const GrfWrite PHY_OFF[] = {
    {0xB4, 0x00010001}, {0xB4, 0x000E0004}, {0xB8, 0x00010001}, {0x98, 0x00040000}};

static const GpioPin AMP_ENABLE = {1, 4};
static const GpioPin AUDIO_MUTE = {2, 0};
static const GpioPin DAC_POWER = {2, 2};

static uint32_t read_register(uint32_t offset)
{
    return register_read(USB_BASE + offset);
}

static void write_register(uint32_t offset, uint32_t value)
{
    register_write(USB_BASE + offset, value);
}

static void set_bits(uint32_t offset, uint32_t bits)
{
    write_register(offset, read_register(offset) | bits);
}

static uint32_t in_control(uint32_t endpoint)
{
    return 0x900u + 0x20u * endpoint;
}

static uint32_t out_control(uint32_t endpoint)
{
    return 0xB00u + 0x20u * endpoint;
}

static uint32_t fifo(uint32_t endpoint)
{
    return 0x1000u + 0x1000u * endpoint;
}

static void write_grf(const GrfWrite *writes, size_t count)
{
    for (size_t index = 0; index < count; index++) {
        register_write(GRF_BASE + writes[index].offset, writes[index].value);
    }
}

static bool wait_clear(uint32_t offset, uint32_t bits, uint32_t timeout_ms)
{
    uint32_t start = hal_clock_milliseconds();
    while ((read_register(offset) & bits) != 0) {
        if (hal_clock_milliseconds() - start > timeout_ms) {
            return false;
        }
    }
    return true;
}

bool usb_vbus_present(void)
{
    return (register_read(GRF_BASE + GRF_VBUS) & GRF_VBUS_PRESENT) != 0;
}

static void silence_audio(void)
{
    cru_gate(GATE_GPIO1, true);
    gpio_mux(AMP_ENABLE, GPIO_FUNCTION_GPIO);
    gpio_mux(AUDIO_MUTE, GPIO_FUNCTION_GPIO);
    gpio_mux(DAC_POWER, GPIO_FUNCTION_GPIO);
    gpio_output(AMP_ENABLE, false);
    gpio_output(AUDIO_MUTE, true);
    gpio_output(DAC_POWER, false);
}

void usb_power_up(void)
{
    cru_gate(GATE_USB_A, true);
    cru_gate(GATE_USB_B, true);
    cru_reset(RESET_USB_B, false);
    cru_reset(RESET_USB_C, false);
    cru_reset(RESET_USB_A, false);
    board_delay_us(PHY_SETTLE_US);
    write_grf(PHY_ON_FIRST, sizeof PHY_ON_FIRST / sizeof PHY_ON_FIRST[0]);
    board_delay_ms(PHY_READY_MS);
    write_grf(PHY_ON_SECOND, sizeof PHY_ON_SECOND / sizeof PHY_ON_SECOND[0]);
    silence_audio();
}

void usb_power_down(void)
{
    set_bits(DCTL, DCTL_SOFT_DISCONNECT);
    board_delay_ms(DISCONNECT_HOLD_MS);
    write_grf(PHY_OFF, sizeof PHY_OFF / sizeof PHY_OFF[0]);
    cru_reset(RESET_USB_B, true);
    cru_reset(RESET_USB_C, true);
    cru_reset(RESET_USB_A, true);
    cru_gate(GATE_USB_A, false);
    cru_gate(GATE_USB_B, false);
}

static UsbConnectStep reset_core(void)
{
    set_bits(DCTL, DCTL_SOFT_DISCONNECT);
    uint32_t start = hal_clock_milliseconds();
    while ((read_register(GRSTCTL) & GRSTCTL_AHB_IDLE) == 0) {
        if (hal_clock_milliseconds() - start > RESET_TIMEOUT_MS) {
            return USB_CONNECT_AHB_IDLE;
        }
    }
    write_register(GRSTCTL, GRSTCTL_SOFT_RESET);
    if (!wait_clear(GRSTCTL, GRSTCTL_SOFT_RESET, RESET_TIMEOUT_MS)) {
        return USB_CONNECT_SOFT_RESET;
    }
    write_register(PCGCCTL, 0xF);
    board_delay_ms(CLOCK_GATE_MS);
    write_register(PCGCCTL, 0);
    set_bits(GAHBCFG, GAHBCFG_TX_EMPTY_LEVEL);
    write_register(DCFG, (read_register(DCFG) & ~DCFG_CLEARED_BITS) | DCFG_FULL_SPEED);
    return USB_CONNECT_OK;
}

static bool size_fifos(UsbPort *port)
{
    uint32_t depth = read_register(GHWCFG3) >> HWCFG3_DEPTH_SHIFT;
    port->dedicated_fifos = (read_register(GHWCFG4) & HWCFG4_DEDICATED_FIFOS) != 0;
    write_register(GRXFSIZ, SHARED_RX_SIZE);
    if (port->dedicated_fifos) {
        write_register(GNPTXFSIZ, DEDICATED_TX0_SIZE);
        write_register(DIEPTXF1, DEDICATED_TX1_SIZE);
        return depth >= DEDICATED_MINIMUM_DEPTH;
    }
    write_register(GNPTXFSIZ, SHARED_TX_SIZE);
    return depth >= SHARED_MINIMUM_DEPTH;
}

static bool flush_fifos(void)
{
    write_register(GRSTCTL, GRSTCTL_FLUSH_ALL_TX);
    bool ok = wait_clear(GRSTCTL, GRSTCTL_TX_FLUSH, RESET_TIMEOUT_MS);
    write_register(GRSTCTL, GRSTCTL_RX_FLUSH);
    ok = ok && wait_clear(GRSTCTL, GRSTCTL_RX_FLUSH, RESET_TIMEOUT_MS);
    write_register(GRSTCTL, GRSTCTL_AFTER_FLUSH);
    return ok;
}

static void reset_endpoint_zero(void)
{
    write_register(in_control(0), EP_IDLE_CONTROL);
    write_register(out_control(0), EP_IDLE_CONTROL);
    write_register(in_control(0) + 0x10u, 0);
    write_register(out_control(0) + 0x10u, 0);
    write_register(in_control(0) + 0x08u, EP_INT_ALL);
    write_register(out_control(0) + 0x08u, EP_INT_ALL);
    write_register(DIEPMSK, DIEPMSK_VALUE);
    write_register(DOEPMSK, DOEPMSK_VALUE);
    write_register(DAINT, INT_ALL);
    write_register(DAINTMSK, DAINT_EP0);
    write_register(GINTSTS, INT_ALL);
    write_register(GOTGINT, INT_ALL);
    write_register(GINTMSK, GINTMSK_VALUE);
}

static UsbConnectStep prepare_core(UsbPort *port)
{
    UsbConnectStep step = reset_core();
    if (step != USB_CONNECT_OK) {
        return step;
    }
    if (!size_fifos(port)) {
        return USB_CONNECT_FIFO_DEPTH;
    }
    return flush_fifos() ? USB_CONNECT_OK : USB_CONNECT_FLUSH;
}

UsbProbe usb_probe(void)
{
    return (UsbProbe){read_register(GSNPSID), read_register(GOTGCTL), read_register(GHWCFG3),
                      read_register(GHWCFG4), read_register(GINTSTS)};
}

bool usb_connect(UsbPort *port)
{
    port->connect_step = prepare_core(port);
    if (port->connect_step != USB_CONNECT_OK) {
        return false;
    }
    reset_endpoint_zero();
    board_delay_ms(DISCONNECT_HOLD_MS);
    write_register(DCTL, read_register(DCTL) & ~DCTL_SOFT_DISCONNECT);
    return true;
}

static void arm_setup(void)
{
    write_register(out_control(0) + 0x10u, EP0_OUT_SETUP_SIZE);
    set_bits(out_control(0), EP_ENABLE | EP_CLEAR_NAK);
}

static void arm_bulk_out(void)
{
    write_register(out_control(BULK_ENDPOINT) + 0x10u, ONE_PACKET | USB_PACKET_SIZE);
    set_bits(out_control(BULK_ENDPOINT), EP_ENABLE | EP_CLEAR_NAK);
}

static uint32_t tx_space(const UsbPort *port, uint32_t endpoint)
{
    uint32_t status = port->dedicated_fifos ? read_register(in_control(endpoint) + 0x18u)
                                            : read_register(GNPTXSTS);
    return status & TX_SPACE_MASK;
}

static bool send(const UsbPort *port, uint32_t endpoint, const uint8_t *bytes, uint32_t length)
{
    uint32_t words = (length + 3) / 4;
    uint32_t start = hal_clock_milliseconds();
    bool space_unreported = port->dedicated_fifos && endpoint == 0;
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
    flush_fifos();
    port->configured = false;
    port->in_busy = false;
    msc_reset(port->msc);
    arm_setup();
    write_register(GINTSTS, GINT_USB_RESET);
}

static void activate_bulk_endpoints(const UsbPort *port)
{
    uint32_t tx_fifo = port->dedicated_fifos ? 1u << EP_TX_FIFO_SHIFT : 0;
    write_register(in_control(BULK_ENDPOINT), USB_PACKET_SIZE | tx_fifo | EP_BULK_ACTIVE);
    write_register(out_control(BULK_ENDPOINT), USB_PACKET_SIZE | EP_BULK_ACTIVE);
    set_bits(DAINTMSK, DAINT_EP1);
    set_bits(DCTL, DCTL_CLEAR_GLOBAL_IN_NAK);
    arm_bulk_out();
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
            activate_bulk_endpoints(port);
        }
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

static void process_setup(UsbPort *port)
{
    UsbSetup setup = usb_setup_parse((const uint8_t *)port->setup);
    UsbReply reply = usb_device_setup(&port->device, setup);
    port->setup_pending = false;
    apply_action(port, reply);
    if (reply.kind == USB_REPLY_STALL) {
        set_bits(in_control(0), EP_STALL);
        set_bits(out_control(0), EP_STALL);
    } else if (reply.kind == USB_REPLY_DATA) {
        send(port, 0, reply.data, reply.length);
    } else if ((setup.request_type & 0x80u) == 0) {
        send(port, 0, NULL, 0);
    }
}

static void read_fifo(uint32_t *words, uint32_t bytes)
{
    for (uint32_t index = 0; index < (bytes + 3) / 4; index++) {
        uint32_t value = read_register(fifo(0));
        if (index < USB_PACKET_SIZE / 4) {
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
        read_fifo(port->setup, count);
        port->setup_pending = true;
    } else if (type == PACKET_OUT_DATA) {
        read_fifo(port->out_packet, count);
        if (endpoint == BULK_ENDPOINT) {
            msc_receive(port->msc, (const uint8_t *)port->out_packet,
                        count < USB_PACKET_SIZE ? count : USB_PACKET_SIZE);
        }
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
        if ((events & EP_INT_TRANSFER_DONE) != 0) {
            arm_bulk_out();
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
}

static void pump_bulk_in(UsbPort *port)
{
    static uint8_t packet[USB_PACKET_SIZE];
    uint32_t length = 0;
    if (port->configured && !port->in_busy && msc_next_in(port->msc, packet, &length)) {
        port->in_busy = send(port, BULK_ENDPOINT, packet, length);
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

void usb_poll(UsbPort *port)
{
    uint32_t events = read_register(GINTSTS);
    if ((events & GINT_USB_RESET) != 0) {
        handle_reset(port);
    }
    if ((events & GINT_ENUMERATION_DONE) != 0) {
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
    pump_bulk_in(port);
}

bool usb_reply_pending(const UsbPort *port)
{
    return port->in_busy || port->msc->phase == MSC_PHASE_STATUS;
}
