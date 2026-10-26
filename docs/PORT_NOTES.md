# bajacan Port Notes

## Scope and source review

The source reference is `reference/wolftrack-embedded/bajacan`.  Its
`main.cpp` owns a table-driven sampling loop: it validates each
`MessageGroupConfig`, calls the sensor lifecycle callbacks, concatenates fixed
sensor payloads into one CAN FD frame per group, and handles a sleep control
frame.  Front and rear configurations provide the tables; the sensor libraries
provide contexts and callbacks.

| Arduino/AVR reference concept | STM32G473 FreeRTOS replacement |
| --- | --- |
| `setup()` / `loop()` | `app_init()` creates fixed RTOS objects and tasks at boot; dedicated tasks own CAN, ADC, and group polling. |
| `millis()` / `delay()` | `osKernelGetTickCount()` and `osDelay()` / `osDelayUntil()`; convert durations with `pdMS_TO_TICKS` where needed. |
| Wrap-prone timestamp comparisons | Signed-delta comparisons: `(int32_t)(now - deadline) >= 0`. |
| `CANFDMessage` and `frame.pad()` | `CanFdFrame` plus pure CAN-FD DLC-length rounding and zero padding. |
| MCP2517FD on Arduino SPI via `ACAN2517FD` | STM32 FDCAN1 (PB8/PB9) through the TCAN1044V-Q1; RX FIFO0 and TX queues communicate with tasks. |
| MCP2517FD interrupt handler | FDCAN HAL callback posts a `CanFdFrame` to an RTOS queue with a `FromISR` API. |
| `SPI.begin()` and synchronous SPI transactions | SPI1 HAL transfers, ultimately full-duplex DMA frames for ADS131M08; task notification from PC14 DRDY starts acquisition. |
| `Wire` / `Wire1` | HAL I2C1 / I2C2 with bounded timeouts and per-device drivers. |
| `analogRead()`, AVR ADC0 configuration | STM32 internal ADC HAL/DMA for later internal-ADC sensor modules; ADS131M08 is the external precision ADC on SPI1. |
| `attachInterrupt()` pulse/PWM readers | GPIO EXTI or timer capture ISR; ISR records minimal state or uses only `FromISR` APIs. |
| `noInterrupts()` / `interrupts()` around snapshots | FreeRTOS critical sections or short, carefully scoped IRQ masking. |
| `avr/sleep.h`, power-down sleep, MCP wake IRQ | Phase 6 STM32 Stop-mode state machine: suspend sensors, STB high, FDCAN clock-stop handshake, PB8 EXTI wake, then restore clocks and peripherals. |
| `Serial` debug printing | ITM/SWO debug output behind `APP_DEBUG`, default off. |

Reference sensor-library roles to retain as STM32 modules are: fixed-size
analog payloads; analog-angle RPM filtering; edge-captured pulse RPM; AS5600
angle and status; MLX90614 temperature; BNO085 data/statistics; I2C bus
helpers; and the CAN driver.  Their Arduino transport and interrupt plumbing
will be replaced, but their board-provided `SensorContext` and
`SensorDescriptor` payload contracts remain the porting reference.

## Clock and SPI report

The current generated `SystemClock_Config()` enables HSI16 and configures a
PLL from HSI, but selects **HSI directly** as SYSCLK.  Consequently SYSCLK,
HCLK, PCLK1, and PCLK2 are 16 MHz.  The `.ioc` and generated code configure
MCO1 on PA8 as HSI divided by 4, producing the required 4 MHz ADS131M08
CLKIN.

SPI1 has no separately selected kernel source in the generated RCC setup, so
its 16 MHz PCLK2 source is inferred from the SPI1 `/2` prescaler and CubeMX's
reported 8 Mbit/s baud rate.  MCO and SPI1 therefore derive from the same HSI
oscillator, although HSI16 is not a crystal and the ADS output rate/notches
will drift with its tolerance.

The present generated SPI1 setup is **not ADS131M08-ready**: it is 4-bit data,
CPOL low/first edge (mode 0), hardware NSS with NSS pulse enabled.  The ADC
needs 8-bit data, CPOL low/second edge (mode 1), and CS held low for the
entire frame.  No CubeMX or clock changes are made in Phase 0; this is the
Phase 4 CubeMX work item.

## C++ readiness check

Ready without changes:

- `cmake/gcc-arm-none-eabi.cmake` defines `arm-none-eabi-g++`, C++ debug and
  release flags, and the embedded-profile flags `-fno-rtti`,
  `-fno-exceptions`, and `-fno-threadsafe-statics`.
- `STM32G473xx_FLASH.ld` retains both `.preinit_array` and `.init_array`.
- `startup_stm32g473xx.s` calls `__libc_init_array` before `main`.

Phase 1 still needs to enable C++ in the root CMake user section and add the
C++ app/board sources; no toolchain, linker, startup, or CubeMX change is
required for C++ constructor support.
