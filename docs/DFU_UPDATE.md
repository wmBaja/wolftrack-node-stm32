# Updating through USB DFU

The firmware enters the STM32 ROM USB DFU bootloader when its application CDC
port is set to 1200 baud. It disconnects as application USB (`0483:5740`) and
re-enumerates as ROM DFU (`0483:df11`).

## One-time board setup

Using SWD, program the option bytes once as `nSWBOOT0=0` and `nBOOT0=1`.
This prevents the BOOT0 function on `PB8` (also FDCAN1 RX) from affecting normal
boot. Normal reset and power cycling then always boot application Flash. Do not
change option bytes as part of a firmware update.

## Trigger DFU without extra software

Close any program using the application serial port, then set it to 1200 baud.
Replace the example port name with the board's port.

Linux:

```sh
stty -F /dev/ttyACM* 1200
```

Windows (CMD or PowerShell):

```text
mode COM* BAUD=1200
```

macOS:

```sh
stty -f /dev/cu.usbmodem* 1200
```

## Upload helper

`scripts/dfu_upload.py` discovers a single `0483:5740` application port,
touches it at 1200 baud, requires exactly one `0483:df11` DFU device, flashes
it, and returns after the transfer. It needs Python 3 plus `pyserial` and
`pyusb`:

```sh
python3 -m pip install pyserial pyusb
python3 scripts/dfu_upload.py build/Debug/wolftrack-node.bin
python3 scripts/dfu_upload.py --tool stm32-programmer build/Debug/wolftrack-node.elf
```

The helper automatically chooses `dfu-util` for a `.bin` image and STM32
Programmer for other image types. STM32 ROM DFU resets immediately after a
successful `:leave` request, which causes `dfu-util` 0.11 to return status 74
while attempting its final status check; the helper recognizes that completed
transfer and reports it as successful. Its output remains live while the
transfer runs. The helper waits for the application CDC device to re-enumerate
by default; add `--no-verify-application` to skip that check.

To flash an already-running ROM bootloader, add `--skip-touch`. Specify
`--port COM5` (or its Unix device path) if automatic application-port discovery
finds zero or multiple devices.

Equivalent direct flashing commands are:

```sh
STM32_Programmer_CLI -c port=usb1 -w build/Debug/wolftrack-node.elf -rst
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D build/Debug/wolftrack-node.bin
```
