#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hal/clock.h"
#include "registers.h"

#define USB_BASE 0x60080000u
#define GOTGCTL 0x000u
#define GOTGINT 0x004u
#define GAHBCFG 0x008u
#define GUSBCFG 0x00Cu
#define GRSTCTL 0x010u
#define GINTSTS 0x014u
#define GINTMSK 0x018u
#define GRXSTSP 0x020u
#define GRXFSIZ 0x024u
#define GNPTXFSIZ 0x028u
#define GNPTXSTS 0x02Cu
#define GSNPSID 0x040u
#define GHWCFG2 0x048u
#define GHWCFG3 0x04Cu
#define GHWCFG4 0x050u
#define DIEPTXF1 0x104u
#define DCFG 0x800u
#define DCTL 0x804u
#define DSTS 0x808u
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
#define GINT_INCOMPLETE_ISO_OUT (1u << 21)
#define GINTMSK_VALUE 0xC00C3C10u
#define DCFG_CLEARED_BITS 0x7F3u
#define DCFG_FULL_SPEED 0x1u
#define DCFG_HIGH_SPEED 0x0u
#define DSTS_SPEED_SHIFT 1
#define DSTS_SPEED_MASK 0x3u
#define DSTS_HIGH_SPEED 0x0u
#define DSTS_ODD_FRAME (1u << 8)
#define DSTS_FRAME_SHIFT 8
#define DSTS_FRAME_MASK 0x3FFFu
#define DCFG_ADDRESS_SHIFT 4
#define DCFG_ADDRESS_MASK (0x7Fu << DCFG_ADDRESS_SHIFT)
#define DCTL_SOFT_DISCONNECT (1u << 1)
#define DCTL_CLEAR_GLOBAL_IN_NAK (1u << 8)
#define EP_ENABLE (1u << 31)
#define EP_CLEAR_NAK (1u << 26)
#define EP_SET_DATA0 (1u << 28)
#define EP_SET_EVEN_FRAME (1u << 28)
#define EP_SET_ODD_FRAME (1u << 29)
#define EP_ODD_FRAME (1u << 16)
#define EP_ISO_ACTIVE 0x00048000u
#define EP_INTERRUPT_ACTIVE 0x100C8000u
#define EP_MAX_PACKET_MASK 0x7FFu
#define EP_STALL (1u << 21)
#define EP_TX_FIFO_SHIFT 22
#define EP_IDLE_CONTROL 0x48000000u
#define EP_BULK_ACTIVE 0x18088000u
#define EP_INT_TRANSFER_DONE (1u << 0)
#define EP_INT_SETUP (1u << 3)
#define EP_INT_ALL 0xFFFFu
#define EP0_OUT_SETUP_SIZE 0x60080040u
#define ONE_PACKET (1u << 19)
#define PACKET_COUNT_SHIFT 19
#define DIEPMSK_VALUE 0x2Fu
#define DOEPMSK_VALUE 0x0Fu
#define DAINT_EP0 0x00010001u
#define DAINT_EP1 0x00020002u
#define DAINT_EP2_IN 0x00000004u
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
#define TX_QUEUE_MASK 0x00FF0000u
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
    HID_ENDPOINT = USB_HID_ENDPOINT_NUMBER,
    PACKET_WORDS = USB_PACKET_SIZE / 4,
    BULK_CHUNK_BYTES = 512,
    RESET_TIMEOUT_MS = 100,
    SPACE_TIMEOUT_MS = 100,
    PHY_SETTLE_US = 5,
    PHY_READY_MS = 10,
    CLOCK_GATE_MS = 10,
    DISCONNECT_HOLD_MS = 150
};

static inline uint32_t read_register(uint32_t offset)
{
    return register_read(USB_BASE + offset);
}

static inline void write_register(uint32_t offset, uint32_t value)
{
    register_write(USB_BASE + offset, value);
}

static inline void set_bits(uint32_t offset, uint32_t bits)
{
    write_register(offset, read_register(offset) | bits);
}

static inline uint32_t in_control(uint32_t endpoint)
{
    return 0x900u + 0x20u * endpoint;
}

static inline uint32_t out_control(uint32_t endpoint)
{
    return 0xB00u + 0x20u * endpoint;
}

static inline uint32_t fifo(uint32_t endpoint)
{
    return 0x1000u + 0x1000u * endpoint;
}

static inline bool wait_clear(uint32_t offset, uint32_t bits, uint32_t timeout_ms)
{
    uint32_t start = hal_clock_milliseconds();
    while ((read_register(offset) & bits) != 0) {
        if (hal_clock_milliseconds() - start > timeout_ms) {
            return false;
        }
    }
    return true;
}
