#!/usr/bin/env python3
"""Touch the application CDC port at 1200 baud, then flash STM32 ROM DFU."""

import argparse
from pathlib import Path
import shutil
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
    print(f"Waiting up to {timeout:g} seconds for application CDC device (0483:5740)...", flush=True)
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if application_ports(list_ports):
            return
        time.sleep(0.25)
    raise SystemExit("Flash completed, but the application CDC device (0483:5740) did not re-enumerate.")


def select_tool(image, requested_tool):
    if requested_tool:
        return requested_tool
    return "dfu-util" if image.suffix.lower() == ".bin" else "stm32-programmer"


def build_flash_command(image, tool):
    if tool == "stm32-programmer":
        return ["STM32_Programmer_CLI", "-c", "port=usb1", "-w", str(image), "-rst"]
    if image.suffix.lower() != ".bin":
        raise SystemExit("dfu-util requires a .bin image.")
    return [
        "dfu-util", "-d", "0483:df11", "-a", "0", "-s",
        "0x08000000:leave", "-D", str(image),
    ]


def require_flash_tool(command):
    if shutil.which(command[0]) is None:
        raise SystemExit(
            f"Required flashing tool not found: {command[0]}. "
            "Install it or select the other tool with --tool."
        )


def is_expected_dfu_leave_reset(result):
    return (
        result.returncode == 74
        and "File downloaded successfully" in result.stdout
        and "Error during download get_status" in result.stdout
    )


def run_dfu_util(command):
    output = []
    deferred_error_lines = []
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
        text=True, bufsize=1,
    )
    assert process.stdout is not None
    for line in process.stdout:
        output.append(line)
        if "Error during download get_status" in line:
            deferred_error_lines.append(line)
        else:
            print(line, end="", flush=True)
    return subprocess.CompletedProcess(command, process.wait(), "".join(output)), deferred_error_lines


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("image", type=Path, help="firmware image (.bin uses dfu-util by default)")
    parser.add_argument("--port", help="application CDC port; bypass automatic discovery")
    parser.add_argument("--skip-touch", action="store_true", help="device is already in ROM DFU")
    parser.add_argument(
        "--tool", choices=("stm32-programmer", "dfu-util"),
        help="flashing tool (default: dfu-util for .bin, STM32 Programmer for other images)",
    )
    parser.add_argument(
        "--no-verify-app", dest="verify_application", action="store_false",
        help="do not wait for the application CDC device after flashing",
    )
    parser.set_defaults(verify_application=True)
    parser.add_argument("--timeout", type=float, default=10.0, help="enumeration timeout in seconds")
    args = parser.parse_args()

    if not args.image.is_file():
        raise SystemExit(f"Image not found: {args.image}")
    if args.timeout <= 0:
        raise SystemExit("--timeout must be positive")

    args.tool = select_tool(args.image, args.tool)
    command = build_flash_command(args.image, args.tool)
    require_flash_tool(command)
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
    if args.tool == "dfu-util":
        result, deferred_error_lines = run_dfu_util(command)
        expected_leave_reset = is_expected_dfu_leave_reset(result)
        if not expected_leave_reset:
            for line in deferred_error_lines:
                print(line, end="", file=sys.stderr)
        if expected_leave_reset:
            if args.verify_application:
                wait_for_application(list_ports, args.timeout)
                print("Update complete; application CDC device re-enumerated.")
            else:
                print("Update complete; target reset during the final DFU status check.")
            return
    else:
        result = subprocess.run(command, check=False)
    if result.returncode:
        raise SystemExit(f"{command[0]} failed with exit status {result.returncode}.")
    if args.verify_application:
        wait_for_application(list_ports, args.timeout)
        print("Update complete; application CDC device re-enumerated.")
    else:
        print("Update complete.")


if __name__ == "__main__":
    main()
