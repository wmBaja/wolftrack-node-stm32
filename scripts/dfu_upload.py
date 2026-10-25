#!/usr/bin/env python3
"""Touch the application CDC port at 1200 baud, then flash STM32 ROM DFU."""

import argparse
from pathlib import Path
import subprocess
import sys
import time

APP_VID = 0x0483
APP_PID = 0x5740
DFU_PID = 0xDF11


def require_usb_modules():
    try:
        import serial
        from serial.tools import list_ports
        import usb.core
    except ImportError as error:
        raise SystemExit(
            "This helper needs pyserial and pyusb: "
            "python3 -m pip install pyserial pyusb"
        ) from error
    return serial, list_ports, usb.core


def application_ports(list_ports):
    ports = [port.device for port in list_ports.comports()
             if port.vid == APP_VID and port.pid == APP_PID]
    if sys.platform.startswith("linux"):
        by_id = Path("/dev/serial/by-id")
        if by_id.is_dir():
            resolved = {str(path.resolve()): str(path) for path in by_id.iterdir()}
            ports = [resolved.get(port, port) for port in ports]
    return ports


def exactly_one_dfu(usb_core):
    devices = list(usb_core.find(find_all=True, idVendor=APP_VID, idProduct=DFU_PID))
    if len(devices) != 1:
        raise SystemExit(
            f"Expected exactly one STM32 ROM DFU device (0483:df11); found {len(devices)}."
        )
    return devices[0]


def wait_for_dfu(usb_core, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        devices = list(usb_core.find(find_all=True, idVendor=APP_VID, idProduct=DFU_PID))
        if devices:
            return exactly_one_dfu(usb_core)
        time.sleep(0.25)
    return exactly_one_dfu(usb_core)


def wait_for_application(list_ports, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if application_ports(list_ports):
            return
        time.sleep(0.25)
    raise SystemExit("Flash completed, but the application CDC device (0483:5740) did not re-enumerate.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="ELF for STM32_Programmer_CLI, BIN for dfu-util")
    parser.add_argument("--port", help="application CDC port; bypass automatic discovery")
    parser.add_argument("--skip-touch", action="store_true", help="device is already in ROM DFU")
    parser.add_argument("--tool", choices=("stm32-programmer", "dfu-util"), default="stm32-programmer")
    parser.add_argument("--timeout", type=float, default=10.0, help="enumeration timeout in seconds")
    args = parser.parse_args()

    if not args.image.is_file():
        raise SystemExit(f"Image not found: {args.image}")
    if args.timeout <= 0:
        raise SystemExit("--timeout must be positive")

    serial, list_ports, usb_core = require_usb_modules()

    if not args.skip_touch:
        ports = [args.port] if args.port else application_ports(list_ports)
        if len(ports) != 1:
            raise SystemExit(
                f"Expected exactly one application CDC port (0483:5740); found {len(ports)}. "
                "Pass --port to select one."
            )
        with serial.Serial(ports[0], baudrate=1200, timeout=1):
            pass

    wait_for_dfu(usb_core, args.timeout)
    if args.tool == "stm32-programmer":
        command = ["STM32_Programmer_CLI", "-c", "port=usb1", "-w", str(args.image), "-rst"]
    else:
        if args.image.suffix.lower() != ".bin":
            raise SystemExit("dfu-util requires a .bin image.")
        command = [
            "dfu-util", "-d", "0483:df11", "-a", "0", "-s",
            "0x08000000:leave", "-D", str(args.image),
        ]
    subprocess.run(command, check=True)
    wait_for_application(list_ports, args.timeout)
    print("Update complete; application CDC device re-enumerated.")


if __name__ == "__main__":
    main()
