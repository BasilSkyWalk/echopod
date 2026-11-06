#include "overlay.h"
#include "usb_session.h"

const UsbOverlayEntry USB_OVERLAY_ENTRY = {USB_OVERLAY_MAGIC, usb_storage_session, usb_dac_session};
