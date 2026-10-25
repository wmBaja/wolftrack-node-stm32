#include "dfu_trigger.h"

#include <stddef.h>

bool dfu_is_1200_baud_request(const uint8_t *buf, uint16_t len)
{
  uint32_t baud;

  if ((buf == NULL) || (len < 4U)) {
    return false;
  }

  baud = (uint32_t)buf[0]
       | ((uint32_t)buf[1] << 8)
       | ((uint32_t)buf[2] << 16)
       | ((uint32_t)buf[3] << 24);
  return baud == 1200U;
}

#if !defined(DFU_TRIGGER_HOST_TEST)

#include "main.h"
#include "cmsis_os.h"
#include "usb_device.h"
#include "usbd_core.h"

#define DFU_SYSTEM_MEMORY_BASE (0x1FFF0000UL)
#define DFU_POST_LEAVE_RESET_MAGIC (0xDF59B008UL)

static volatile uint8_t s_dfu_requested = 0U;

extern USBD_HandleTypeDef hUsbDeviceFS;

void app_request_dfu_bootloader(void)
{
  s_dfu_requested = 1U;
}

void app_early_boot_dfu_check(void)
{
  uint32_t boot_sp;
  void (*boot_jump)(void);

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();

  /* ROM DFU can enter the application without a peripheral reset.  Complete
     one self-clearing reset so the application always starts from reset state. */
  if (TAMP->BKP1R == DFU_POST_LEAVE_RESET_MAGIC) {
    TAMP->BKP1R = 0x00000000UL;
    __DSB();
    NVIC_SystemReset();
  }

  if (TAMP->BKP0R != DFU_BOOT_MAGIC_WORD) {
    return;
  }

  TAMP->BKP0R = 0x00000000UL;
  TAMP->BKP1R = DFU_POST_LEAVE_RESET_MAGIC;
  __DSB();

  __HAL_RCC_GPIOA_CLK_ENABLE();
  GPIOA->MODER = (GPIOA->MODER & ~GPIO_MODER_MODE3) | GPIO_MODER_MODE3_0;
  GPIOA->OTYPER &= ~GPIO_OTYPER_OT3;
  GPIOA->BSRR = GPIO_PIN_3;

  __disable_irq();
  SysTick->CTRL = 0U;
  SysTick->LOAD = 0U;
  SysTick->VAL = 0U;
  SCB->VTOR = 0x00000000UL;

  __HAL_RCC_SYSCFG_CLK_ENABLE();
  SYSCFG->MEMRMP = SYSCFG_MEMRMP_MEM_MODE_0;
  __DSB();
  __ISB();

  boot_sp = *(__IO uint32_t *)DFU_SYSTEM_MEMORY_BASE;
  boot_jump = (void (*)(void))(*(__IO uint32_t *)(DFU_SYSTEM_MEMORY_BASE + 4UL));

  __enable_irq();
  __set_MSP(boot_sp);
  boot_jump();

  for (;;) {
  }
}

void app_poll_dfu_trigger(void)
{
  GPIO_InitTypeDef gpio_init = {0};

  if (s_dfu_requested == 0U) {
    return;
  }

  osDelay(50U);
  (void)USBD_Stop(&hUsbDeviceFS);
  (void)USBD_DeInit(&hUsbDeviceFS);

  __HAL_RCC_GPIOA_CLK_ENABLE();
  gpio_init.Pin = GPIO_PIN_12;
  gpio_init.Mode = GPIO_MODE_OUTPUT_PP;
  gpio_init.Pull = GPIO_NOPULL;
  gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &gpio_init);
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_12, GPIO_PIN_RESET);
  osDelay(10U);

  __HAL_RCC_PWR_CLK_ENABLE();
  HAL_PWR_EnableBkUpAccess();
  TAMP->BKP0R = DFU_BOOT_MAGIC_WORD;
  __DSB();
  NVIC_SystemReset();
}

#endif /* !DFU_TRIGGER_HOST_TEST */
