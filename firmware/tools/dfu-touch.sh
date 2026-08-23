#!/usr/bin/env bash
#
# dfu-touch.sh — reboot a running PERDICAN V1 into its DFU bootloader.
#
# Opens the board's CDC port at 1200 baud and drops DTR. The firmware treats
# that as a request to reboot into the STM32 system bootloader, so the board
# can be reflashed over the same USB-C cable with no debugger and without
# reaching for the BOOT button.
#
# Usage:  tools/dfu-touch.sh [/dev/port]

set -euo pipefail

PORT="${1:-}"

find_port() {
    case "$(uname -s)" in
        Darwin)
            # PERDICAN enumerates as a plain CDC ACM device.
            ls /dev/cu.usbmodem* 2>/dev/null | head -1
            ;;
        Linux)
            ls /dev/ttyACM* 2>/dev/null | head -1
            ;;
        *)
            echo "" ;;
    esac
}

if [ -z "$PORT" ]; then
    PORT="$(find_port)"
fi

if [ -z "$PORT" ] || [ ! -e "$PORT" ]; then
    echo "dfu-touch: no CDC port found." >&2
    echo "           Pass one explicitly, or use the hardware route instead:" >&2
    echo "           hold BOOT, tap RESET, release BOOT." >&2
    exit 1
fi

echo "dfu-touch: knocking on $PORT at 1200 baud"

case "$(uname -s)" in
    Darwin) stty -f "$PORT" 1200 || true ;;
    Linux)  stty -F "$PORT" 1200 || true ;;
esac

# Give the board time to reset and the host time to enumerate the ROM
# bootloader as 0483:df11.
sleep 2

if command -v dfu-util >/dev/null 2>&1; then
    if dfu-util -l 2>/dev/null | grep -q "0483:df11"; then
        echo "dfu-touch: bootloader is up, ready for 'make dfu'"
        exit 0
    fi
    echo "dfu-touch: warning — did not see 0483:df11 in 'dfu-util -l'" >&2
fi
