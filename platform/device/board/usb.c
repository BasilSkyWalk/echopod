#include "usb.h"

#include <stddef.h>

#include "board.h"
#include "cru.h"
#include "gpio.h"
#include "hal/clock.h"
#include "hal/system.h"
#include "registers.h"
#include "usb_registers.h"

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

static HalUsbCore core_registers;

static const GpioPin AMP_ENABLE = {1, 4};
static const GpioPin AUDIO_MUTE = {2, 0};
static const GpioPin DAC_POWER = {2, 2};

static void write_grf(const GrfWrite *writes, size_t count)
{
    for (size_t index = 0; index < count; index++) {
        register_write(GRF_BASE + writes[index].offset, writes[index].value);
    }
}

bool usb_vbus_present(void)
{
    return (register_read(GRF_BASE + GRF_VBUS) & GRF_VBUS_PRESENT) != 0;
}

void usb_silence_audio(void)
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

HalUsbCore hal_system_usb_core(void)
{
    return core_registers;
}

static void capture_core_registers(void)
{
    core_registers = (HalUsbCore){.captured = true,
                                  .hwcfg2 = read_register(GHWCFG2),
                                  .hwcfg3 = read_register(GHWCFG3),
                                  .hwcfg4 = read_register(GHWCFG4),
                                  .usbcfg = read_register(GUSBCFG),
                                  .rx_fifo = read_register(GRXFSIZ),
                                  .nptx_fifo = read_register(GNPTXFSIZ)};
}

static UsbConnectStep reset_core(bool high_speed)
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
    capture_core_registers();
    write_register(PCGCCTL, 0xF);
    board_delay_ms(CLOCK_GATE_MS);
    write_register(PCGCCTL, 0);
    set_bits(GAHBCFG, GAHBCFG_TX_EMPTY_LEVEL);
    uint32_t speed = high_speed ? DCFG_HIGH_SPEED : DCFG_FULL_SPEED;
    write_register(DCFG, (read_register(DCFG) & ~DCFG_CLEARED_BITS) | speed);
    return USB_CONNECT_OK;
}

static void size_fifos(UsbLink *link)
{
    uint32_t depth = read_register(GHWCFG3) >> HWCFG3_DEPTH_SHIFT;
    link->dedicated_fifos = (read_register(GHWCFG4) & HWCFG4_DEDICATED_FIFOS) != 0;
    if (link->dedicated_fifos && depth >= DEDICATED_MINIMUM_DEPTH) {
        write_register(GRXFSIZ, SHARED_RX_SIZE);
        write_register(GNPTXFSIZ, DEDICATED_TX0_SIZE);
        write_register(DIEPTXF1, DEDICATED_TX1_SIZE);
    } else if (!link->dedicated_fifos && depth >= SHARED_MINIMUM_DEPTH) {
        write_register(GRXFSIZ, SHARED_RX_SIZE);
        write_register(GNPTXFSIZ, SHARED_TX_SIZE);
    }
}

bool usb_flush_fifos(void)
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

static UsbConnectStep prepare_core(UsbLink *link)
{
    UsbConnectStep step = reset_core(link->high_speed_allowed);
    if (step != USB_CONNECT_OK) {
        return step;
    }
    size_fifos(link);
    return usb_flush_fifos() ? USB_CONNECT_OK : USB_CONNECT_FLUSH;
}

UsbProbe usb_probe(void)
{
    return (UsbProbe){read_register(GSNPSID),  read_register(GOTGCTL), read_register(GHWCFG3),
                      read_register(GHWCFG4),  read_register(GINTSTS), read_register(GRXFSIZ),
                      read_register(GNPTXFSIZ)};
}

bool usb_connect(UsbLink *link)
{
    link->step = prepare_core(link);
    if (link->step != USB_CONNECT_OK) {
        return false;
    }
    reset_endpoint_zero();
    board_delay_ms(DISCONNECT_HOLD_MS);
    write_register(DCTL, read_register(DCTL) & ~DCTL_SOFT_DISCONNECT);
    return true;
}

bool usb_host_reset_pending(void)
{
    return (read_register(GINTSTS) & GINT_USB_RESET) != 0;
}
