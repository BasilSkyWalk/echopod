#include "check.h"
#include "device/usb_fallback.h"

static void a_high_speed_session_that_never_works_falls_back_after_the_timeout(void)
{
    UsbFallback fallback;
    usb_fallback_start(&fallback, true, 1000);
    CHECK(!usb_fallback_due(&fallback, false, 1000 + USB_FALLBACK_MS - 1));
    CHECK(usb_fallback_due(&fallback, false, 1000 + USB_FALLBACK_MS));
}

static void it_falls_back_only_once(void)
{
    UsbFallback fallback;
    usb_fallback_start(&fallback, true, 0);
    CHECK(usb_fallback_due(&fallback, false, USB_FALLBACK_MS));
    CHECK(!usb_fallback_due(&fallback, false, USB_FALLBACK_MS + 1));
    CHECK(!usb_fallback_due(&fallback, false, 3 * USB_FALLBACK_MS));
}

static void a_working_session_never_falls_back(void)
{
    UsbFallback fallback;
    usb_fallback_start(&fallback, true, 0);
    CHECK(!usb_fallback_due(&fallback, true, 500));
    CHECK(!usb_fallback_due(&fallback, false, 2 * USB_FALLBACK_MS));
}

static void a_full_speed_session_never_falls_back(void)
{
    UsbFallback fallback;
    usb_fallback_start(&fallback, false, 0);
    CHECK(!usb_fallback_due(&fallback, false, 5 * USB_FALLBACK_MS));
}

static void the_timeout_survives_the_clock_wrapping(void)
{
    UsbFallback fallback;
    usb_fallback_start(&fallback, true, UINT32_MAX - 100);
    CHECK(!usb_fallback_due(&fallback, false, 200));
    CHECK(usb_fallback_due(&fallback, false, USB_FALLBACK_MS));
}

int main(void)
{
    static const TestCase cases[] = {
        TEST_CASE(a_high_speed_session_that_never_works_falls_back_after_the_timeout),
        TEST_CASE(it_falls_back_only_once),
        TEST_CASE(a_working_session_never_falls_back),
        TEST_CASE(a_full_speed_session_never_falls_back),
        TEST_CASE(the_timeout_survives_the_clock_wrapping),
    };
    return test_run_all(cases, TEST_CASE_COUNT(cases));
}
