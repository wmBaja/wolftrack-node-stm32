# wolftrack-node-stm32: Agent Instructions & Architecture Context

Repo: `wolftrack-node-stm32` (CubeMX CMake project, STM32G473CEU6, FreeRTOS via CMSIS-RTOS v2).
Reference: Local directory `reference/` (specifically `reference/wolftrack-embedded/bajacan/`, cloned from https://github.com/wmBaja/wolftrack-embedded), an Arduino/C++ AVR128DB32 node, plus any datasheets/reference notes placed in `reference/`.
Goal: reproduce its structure (board config tables, sensor descriptors, message groups, CAN control) as idiomatic FreeRTOS C++ on this MCU (CubeMX-generated files stay C). Front and rear boards use the same PCB and the same `.ioc`; they differ only in the sensors connected, selected at build time.

**Workflow Rule:** Work one phase at a time from `PLAN.md`. After each phase: build, commit, report (using the format at the bottom of this file), then wait for me.

---

## 1. Ground rules

**Environment**
- Do not flash or run on target. "Verified" means: firmware builds, host unit tests pass, memory use is sane.
- Reference materials live locally in `reference/` (git-ignored). Read the reference codebase and any datasheets from `reference/` first before attempting web fetches. Do not modify or commit anything inside `reference/`.
- Build: `cube-cmake --preset Debug`, then `cube-cmake --build build/Debug`. In my VS Code session it lives at `~/.vscode/extensions/stmicroelectronics.stm32cube-ide-build-cmake-*/resources/cube-cmake/linux/x86_64`. If it is not on PATH, try that path. If that fails, stop and tell me. Do not install packages or change `.vscode/settings.json`.
- Baseline from my last build: target `projects` (or `wolftrack-node-stm32` if renamed in `.ioc`), output `build/Debug/*.elf`, FLASH 30896 B ($5.89\%$), RAM 9792 B ($7.47\%$). FreeRTOS uses `heap_4` and the `ARM_CM4F` port. The HAL timebase is a TIM (`stm32g4xx_hal_timebase_tim.c`), not SysTick.
- Compiled HAL modules include ADC, FDCAN, I2C, SPI, TIM, DMA, PCD. There is no UART HAL. Debug output goes through ITM/SWO (PB3).

**Do not touch**
- `Drivers/`, `Middlewares/`, `cmake/stm32cubemx/`, `startup_stm32g473xx.s`, `STM32G473xx_FLASH.ld`, and the `.ioc` file (`projects.ioc` / `wolftrack-node-stm32.ioc`).
- `reference/` (read-only local reference files; keep git-ignored).
- Anything in `Core/` outside `/* USER CODE BEGIN ... */` / `END` blocks.
- Never hand-edit the `.ioc`. If a phase needs a CubeMX change, add exact GUI steps to `CUBEMX_TODO.md` and stop that phase so I can apply them and regenerate.
- Do not change the clock tree (MCO on PA8 feeds the ADC clock).

**Where code goes**
- App code in `App/Inc`, `App/Src`. Board variants in `Board/`. Host tests in `tests/`.
- CMake edits only in the user-marked sections of the root `CMakeLists.txt`. If you cannot find those markers, stop and ask.
- Language: C++17 for `App/` and `Board/`. Everything CubeMX generates, including `main.c`, stays plain C. See the C++ subset and boundary rules under "Code rules".

**Code rules**
- Create all tasks, queues and buffers at boot. No `malloc` or dynamic task creation at runtime.
- ISRs only use `...FromISR` APIs. Any interrupt that calls one must have a NVIC preemption priority number **greater than or equal to** `configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY` (that is, lower urgency). CubeMX shows these numbers in the NVIC tab.
- Check `configTICK_RATE_HZ`. Code below assumes 1000; otherwise convert with `pdMS_TO_TICKS`.
- Tick math must be wrap-safe: `(int32_t)(now - deadline) >= 0`.
- Add a compile-time `APP_DEBUG` flag for ITM prints, default off.
- **C++ subset (embedded profile):** compile `App/` and `Board/` with `-fno-exceptions -fno-rtti -fno-threadsafe-statics`. No `new`/`delete`, no STL containers, `std::string`, iostream, `std::thread` or `std::mutex`; use FreeRTOS APIs. Avoid virtual functions. Global constructors run before `HAL_Init` and the scheduler, so they must not touch HAL, RTOS or hardware (plain `constexpr` data is fine).
- **C/C++ boundary:** `main.c` and all CubeMX-generated files stay C. App code exposes `extern "C"` entry points (for example `app_init()`) that are called from USER CODE sections.

---

## 2. Confirmed design decisions

| Topic | Decision |
|---|---|
| Boards | Same PCB, same `.ioc`. Board config differs only in which sensors are connected (plus their IDs and calibration constants). |
| CAN | Hardware FDCAN1 (PB9 TX, PB8 RX) into a TCAN1044V-Q1 transceiver. No MCP251863. Defaults carried over from bajacan: 500 kbit/s arbitration, 1 Mbit/s data ($\times 2$), extended IDs on. |
| ADC | ADS131M08 on SPI1. Each ADC channel is its own `SensorDescriptor` in a message group. |
| ADC rate | About 1.95 kSPS: OSR 1024 with CLKIN 4 MHz ($\text{modulator} = \text{CLKIN} / 2$, $\text{ODR} = 2\text{ MHz} / \text{OSR}$). Default to low-power mode (4 MHz is inside its 0.3 to 4.15 MHz range). |
| Channel to CAN | A group poll reports the **average of all samples since the last poll**. |
| Data-ready | `ADC_nDR` is PC14, changed to EXTI falling edge in CubeMX. |
| Calibration | Compile-time constants in each board config, written to the ADS131M08 offset/gain registers at boot. |
| Sleep | Real MCU Stop mode. TCAN in standby (STB high) with remote wake via the bus wake-up pattern. The Raspberry Pi logger sends the wake. |
| Open | CAN IDs (use `TODO_ID`), payload layout per channel (default: 3 bytes, 24-bit two's complement, MSB first; confirm against dashboard decoding), which channels and gains each board uses (ADC sheet shows only AIN6 and AIN7 wired). Do not invent these. |

---

## 3. Hardware map (from schematics and CubeMX; verify against the `.ioc`)

| Function | Pins |
|---|---|
| FDCAN1 | TX PB9, RX PB8 |
| TCAN1044V STB | PA3 (GPIO_Output; looks like `TCAN_STB`). STB high = standby, low = normal. |
| SPI1 to ADS131M08 | NSS PA4, SCK PA5, MISO PB4, MOSI PB5 |
| ADC reset/sync | PC13 (GPIO_Output, `ADC_nRST`, wired to SYNC/RESET) |
| ADC data-ready | PC14 (`ADC_nDR`, must become EXTI falling edge) |
| ADC clock | PA8 MCO, HSI16/4 = 4 MHz into CLKIN |
| I2C1 / I2C2 | SDA PB7 / SCL PA15, SDA PF0 / SCL PC4 |
| Internal ADC pins | PA0, PA1, PA6, PB1, PB12, PA9 (some behind TLV9062 buffers) |
| USB | PA11 / PA12 |
| SWD + SWO | PA13, PA14, PB3. Never remap. |

---

## 4. ADS131M08 facts for the driver (check `reference/` first, or datasheet SBAS950B: https://www.ti.com/lit/ds/symlink/ads131m08.pdf)

- **SPI:** mode 1 (CPOL 0, CPHA 1). Min SCLK period 40 ns at DVDD 2.7 to 3.6 V. Start at 8 MHz or less. CS must change only while SCLK is low and stay low for the whole frame.
- **Frame:** default 24-bit words, 10 words = 30 bytes. DIN: command word, input-CRC word (zeros if disabled), 8 zero words. DOUT: response (STATUS for a NULL command), 8 channel words, output CRC. The output CRC is always present.
- **Data:** 24-bit two's complement. $1\text{ LSB} = (2.4 / \text{gain}) / 2^{24}\text{ V}$ with the internal reference.
- **CRC:** CCITT, $x^{16} + x^{12} + x^5 + 1$, seed `FFFFh` (section 8.3.12). Cross-check your implementation against an independent one (for example a short Python script), since there is no hardware.
- **Commands:** NULL `0000h`, RESET `0011h` (ack `FF28h`), STANDBY `0022h`, WAKEUP `0033h`, RREG `101a aaaa annn nnnn`, WREG `011a aaaa annn nnnn`. A command is answered in the following frame.
- **Registers:** MODE `02h`, CLOCK `03h` (channel enables, OSR, power mode), GAIN1 `04h`, GAIN2 `05h`, per-channel CFG/OCAL/GCAL. GCAL defaults to `800000h` (gain 1). Read the bit layouts from the register map (Table 8-12); do not assume bit positions.
- **Startup:** after power-up or reset, a DRDY low-to-high edge means SPI is ready ($t_{\text{POR}} = 250\text{ }\mu\text{s}$). Wait $t_{\text{REGACQ}}$ ($5\text{ }\mu\text{s}$) after RESET. CLKIN must be running. The first two samples after reset use the noisier fast-settling filter. Sinc3 settling at OSR 1024 is 2648 CLKIN cycles (about 0.66 ms at 4 MHz). Discard samples until settling completes (Table 8-3).
- **FIFO:** the device holds 2 samples per channel. After a pause, read two frames back to back or pulse SYNC to clear it.
- **SYNC/RESET (PC13):** a low pulse of 1 to 2047 CLKIN cycles (under about $512\text{ }\mu\text{s}$ at 4 MHz) re-syncs. A pulse of 2048 cycles or more resets. Pulse timing needs microsecond accuracy, so use a timer or DWT cycle counter, not `HAL_Delay`.
- **Standby:** send STANDBY, then stop CLKIN. Exit with WAKEUP once CLKIN is toggling; modulators resume after 8 modulator periods.
- **Clock:** the datasheet wants the modulator clock synchronous with SCLK for best noise. Read the RCC config from the `.ioc` and report SYSCLK source, SPI1 kernel clock and MCO source. Flag any mismatch but do not change it. HSI16 is not a crystal, so ODR and filter notches drift with its tolerance.

---

## 5. Reference behavior to port (local path: `reference/`)

All reference code is located locally inside `reference/` (specifically `reference/wolftrack-embedded/bajacan/src/main.cpp` and `reference/wolftrack-embedded/bajacan/include/config.h`, or directly under `reference/bajacan/` depending on how it was placed—inspect `reference/` first). The code is the source of truth; the reference README is out of date (it mentions a sensor table and a wake command ID the code no longer has).

- **Contracts:** `SensorContext {name, payloadSize}`; `SensorDescriptor {context, begin, sample, suspend, resume}`; `MessageGroupConfig {name, canId, pollIntervalMs, sensors, sensorCount}`; `ControlMessageConfig {sleepCommandId, sleepCommandByte, commandByteIndex}`; `BoardHooks {preSetup, beforeSleep, afterWake}`; `BoardConfig` aggregating CAN settings, startup delay, control, hooks and groups.
- **One CAN frame per group.** Payload is the concatenation of each sensor's fixed `payloadSize`. A group is invalid (never transmits, logs an error) if it has no sensors, a null context, or total payload over 64 bytes.
- **Stagger:** first poll of active group $i$ of $n$ is offset by $\text{interval} \times i / n$.
- **No burst catch-up:** if the next deadline is already past, reschedule to `now + interval`.
- **Failed sample or wrong length:** zero-fill that sensor's bytes, still send, log it. (Exception for ADC channels, see Phase 5 in `PLAN.md`.)
- **Control match:** ID type equals `useExtendedIds`, ID matches, `commandByteIndex < len`, byte matches. A sleep match suspends sensors, runs `beforeSleep`, puts the CAN path in low power. In the original, wake came from the MCP251863 wake interrupt (bus activity), not a command frame.
- **Known gaps to fix, not copy:** sensor `begin` failures are ignored, and CAN init failure spins forever with no status.

Sensor libraries to read in `reference/` (under `reference/wolftrack-embedded/bajacan/lib/` or `reference/`) for payload layout and scaling: `analog_sensor`, `analog_rpm_sensor`, `pulse_rpm_sensor`, `as5600_sensor`, `mlx90614_sensor`, `bno085_sensor`, `i2c_sensor`, `can_driver`.

---

## 6. Things not verified (flag these, do not paper over them)
- Whether PA3 is `TCAN_STB`, PC13 is `ADC_nRST` and PC14 is `ADC_nDR` (read from schematic screenshots).
- Whether a normal CAN frame produces a valid wake-up pattern; the exact timing is in the TCAN1044V datasheet and needs a bench test.
- The FDCAN clock-stop handshake and the wake path through EXTI on the RX pin (the approach comes from ST community answers, not the reference manual).
- Whether the HSI16-derived CLKIN is acceptable against SCLK synchronicity for your noise target.
- Everything that needs a board.

---

## 7. Report format after each phase
1. What changed (files).
2. Build result for `front-debug` and `rear-debug`, with the FLASH/RAM lines.
3. Host test result.
4. Anything unverified because there is no hardware.
5. Any CubeMX changes I need to make.