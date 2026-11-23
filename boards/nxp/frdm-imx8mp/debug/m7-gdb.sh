#!/bin/sh
#
# GDB on the FRDM-IMX8MPLUS Cortex-M7 over SWD/JTAG (J-Link on the board's debug header).
#
#   boards/nxp/frdm-imx8mp/debug/m7-gdb.sh attach   # attach to the running image, no reset
#   boards/nxp/frdm-imx8mp/debug/m7-gdb.sh load     # load the ELF into DDR and start it halted
#
# The board must be powered and past DDR initialisation (U-Boot prompt or Linux running);
# the image runs from DDR 0x80100000, there is no flash to program. 'load' leaves the core
# halted at the reset handler: 'continue' to run. Environment: ELF=<path> (default: the
# nxp_frdm-imx8mp_default build), JLINK_DEVICE (default MIMX8ML8_M7), JLINK_IF (SWD|JTAG).

set -e
MODE=${1:-attach}
ROOT=$(cd "$(dirname "$0")/../../../.." && pwd)
ELF=${ELF:-$ROOT/build/nxp_frdm-imx8mp_default/nxp_frdm-imx8mp_default.elf}
JLINK_DEVICE=${JLINK_DEVICE:-MIMX8ML8_M7}
JLINK_IF=${JLINK_IF:-SWD}

GDB=$(command -v gdb-multiarch || command -v arm-none-eabi-gdb || true)
[ -n "$GDB" ] || { echo "gdb-multiarch (or arm-none-eabi-gdb) not found"; exit 1; }
SERVER=$(command -v JLinkGDBServerCLExe || command -v JLinkGDBServer || true)
[ -n "$SERVER" ] || { echo "SEGGER J-Link software not found (JLinkGDBServerCLExe)"; exit 1; }
[ -f "$ELF" ] || { echo "$ELF not found: make nxp_frdm-imx8mp_default first"; exit 1; }

# -noreset/-nohalt: attaching must not disturb the running M7 (or Linux on the A53s)
"$SERVER" -silent -device "$JLINK_DEVICE" -if "$JLINK_IF" -speed auto -port 2331 \
	-LocalhostOnly 1 -noreset -nohalt -nogui -singlerun &
SERVER_PID=$!
trap 'kill $SERVER_PID 2>/dev/null || true' EXIT
sleep 2

# WDOG3 (0x302A0000) keeps counting while the core is halted unless WCR.WDBG (bit 1) is set;
# PX4 and bbz-m7-boot set it. If the M7 resets while halted, WDOG3 is why.
if [ "$MODE" = load ]; then
	exec "$GDB" -q "$ELF" \
		-ex "target extended-remote localhost:2331" \
		-ex "monitor halt" \
		-ex "set mem inaccessible-by-default off" \
		-ex "load" \
		-ex "set *(unsigned short *)0x302A0000 = *(unsigned short *)0x302A0000 | 2" \
		-ex "set *(unsigned int *)0xE000ED08 = 0x80100000" \
		-ex "set \$msp = *(unsigned int *)0x80100000" \
		-ex "set \$pc = *(unsigned int *)0x80100004" \
		-ex "info registers pc msp"
else
	exec "$GDB" -q "$ELF" \
		-ex "target extended-remote localhost:2331" \
		-ex "set mem inaccessible-by-default off" \
		-ex "monitor halt" \
		-ex "bt"
fi
