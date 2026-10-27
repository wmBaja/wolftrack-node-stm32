#ifndef USB_IDENTITY_H
#define USB_IDENTITY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_usb_identity_on_dtr(bool asserted);
void app_poll_usb_identity(void);

#ifdef __cplusplus
}
#endif

#endif /* USB_IDENTITY_H */
