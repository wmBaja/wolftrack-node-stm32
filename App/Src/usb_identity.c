#include "usb_identity.h"

#include <stdint.h>

#include "cmsis_os.h"
#include "usbd_cdc_if.h"

#if !defined(APP_BOARD_NAME) || !defined(APP_BUILD_TYPE)
#error "CMake must define the board name and build type"
#endif

static const uint8_t s_identity_message[] =
    "\r\n"
    "+--------------------------------+\r\n"
    "|       Wolftrack Sensor Node    |\r\n"
    "+--------------------------------+\r\n"
    "  Board:    " APP_BOARD_NAME "\r\n"
    "  Build:    " APP_BUILD_TYPE "\r\n"
    "  CAN:      ext-ID, 500 kbit/s / 1 Mbit/s\r\n" // TODO: Read actual CAN settings
    "  CAN IDs:  pending assignment\r\n" // TODO: Read actual CAN IDs
    "+--------------------------------+\r\n";

static volatile uint8_t s_dtr_asserted = 0U;
static volatile uint8_t s_identity_requested = 0U;
static uint8_t s_identity_waiting = 0U;
static uint32_t s_identity_due_tick = 0U;

enum { kIdentityOpenDelayMs = 25U };

void app_usb_identity_on_dtr(const bool asserted)
{
  if (asserted && (s_dtr_asserted == 0U)) {
    s_identity_requested = 1U;
  }
  s_dtr_asserted = asserted ? 1U : 0U;
}

void app_poll_usb_identity(void)
{
  if (s_identity_requested == 0U) {
    return;
  }

  const uint32_t now = osKernelGetTickCount();

  if (s_identity_waiting == 0U) {
    s_identity_due_tick = now + kIdentityOpenDelayMs;
    s_identity_waiting = 1U;
    return;
  }

  if ((int32_t)(now - s_identity_due_tick) < 0) {
    return;
  }

  if (CDC_Transmit_FS((uint8_t *)s_identity_message,
                      (uint16_t)(sizeof(s_identity_message) - 1U)) == USBD_OK) {
    s_identity_requested = 0U;
    s_identity_waiting = 0U;
  }
}
