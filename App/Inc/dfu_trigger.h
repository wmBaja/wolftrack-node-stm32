#ifndef DFU_TRIGGER_H
#define DFU_TRIGGER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DFU_BOOT_MAGIC_WORD (0xDF59B007UL)

bool dfu_is_1200_baud_request(const uint8_t *buf, uint16_t len);
void app_request_dfu_bootloader(void);
void app_early_boot_dfu_check(void);
void app_poll_dfu_trigger(void);

#ifdef __cplusplus
}
#endif

#endif /* DFU_TRIGGER_H */
