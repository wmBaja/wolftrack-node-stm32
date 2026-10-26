#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "dfu_trigger.h"

int main(void)
{
  static const uint8_t baud_1200[] = {0xB0U, 0x04U, 0x00U, 0x00U, 0x00U, 0x00U, 0x08U};
  static const uint8_t baud_115200[] = {0x00U, 0xC2U, 0x01U, 0x00U, 0x00U, 0x00U, 0x08U};

  assert(!dfu_is_1200_baud_request(NULL, 0U));
  assert(!dfu_is_1200_baud_request(baud_1200, 3U));
  assert(dfu_is_1200_baud_request(baud_1200, sizeof(baud_1200)));
  assert(!dfu_is_1200_baud_request(baud_115200, sizeof(baud_115200)));
  return 0;
}
