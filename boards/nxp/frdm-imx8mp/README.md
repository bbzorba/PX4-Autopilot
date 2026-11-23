# NXP FRDM-IMX8MPLUS — PX4 on the Cortex-M7 (`nxp_frdm-imx8mp_default`)

PX4 (NuttX, flat build) for the i.MX 8M Plus **Cortex-M7**, next to Linux on the Cortex-A53s.
It was started as a copy of `px4/fmu-v6c`, with everything STM32-specific removed.

## Build

```sh
make nxp_frdm-imx8mp_default            # build only
make nxp_frdm-imx8mp_default menuconfig # NuttX config (or boardguiconfig for PX4 modules)
```

Output in `build/nxp_frdm-imx8mp_default/`:

| File | Use |
|---|---|
| `nxp_frdm-imx8mp_default.bin` | raw image for DDR `0x80100000` (vector table first) — what the board runs |
| `nxp_frdm-imx8mp_default.elf` | symbols for GDB / VS Code |
| `nxp_frdm-imx8mp_default.px4` | PX4 container (QGC); unused here, there is no PX4 USB bootloader |

`make … upload` does not apply: the M7 has no flash and no PX4 bootloader (so no `hw_config.h`,
no `bootloader.px4board`). U-Boot loads the image into DDR and `bbz-m7-boot` starts it.

## Memory and boot

| Range | What |
|---|---|
| `0x80000000–0x800FFFFF` | `bbz-m7-boot` DDR area (test pattern) |
| `0x80100000–0x807FFFFF` | PX4 code + read-only data (`flash` in `scripts/script.ld`, 7 MB) |
| `0x80800000–0x80FFFFFF` | PX4 RAM: .data/.bss/heap (`sram`, 8 MB) |
| `0x55000000–0x550FFFFF` | RPMsg vrings + resource table (`0x550FF000`), non-cacheable (MPU) |
| `0x55400000–0x554FFFFF` | Linux RPMsg buffers (`vdevbuffer`) |

The 16 MB window `0x80000000` is `m7_reserved` in the Linux DT and locked by the RDC (A53
read-only) in U-Boot's `bootcmd`. Boot: U-Boot `preboot` loads `m7-app.bin` (this `.bin`) to
`0x80100000`, then starts `bbz-m7-boot` in ITCM, which jumps to the vector table at `0x80100000`
(VTOR, MSP, reset handler) after 0.5 s. Then Linux boots and attaches to the running M7.

## Peripherals / ports

| Function | Where |
|---|---|
| NuttX console / NSH | UART4 = J19 second port, 115200 8N1 (`make m7-console` in the Yocto repo) |
| Link to the A53 (Linux) | RPMsg over MU1 + shared DDR. M7: `/dev/ttyRPMSG0` = PX4 `TEL1` (MAVLink, `MAV_0_CONFIG`). A53: `/dev/ttyRPMSG<n>` (`imx_rpmsg_tty`) |
| HRT | GPT1, 24 MHz |
| Watchdog | WDOG3, 2 s (`CONFIG_BOARD_FRDM_IMX8MP_WDOG3_TIMEOUT_MS`), fed every 100 ms from HPWORK; resets **only the M7** (Linux keeps running) |
| Sensors / actuators | none yet: `SYS_AUTOSTART=1100`, SIH simulation (`SYS_HITL=2`) |

The RPMsg TTY (`src/rpmsg_tty.c`) speaks the raw protocol of NXP's Linux `imx_rpmsg_tty`
driver (name service `rpmsg-virtual-tty-channel`). NuttX's own `uart_rpmsg` only talks to
another NuttX. Until Linux binds the channel, the bytes written are dropped, as on a UART with
nothing connected.

WDOG3: `bbz-m7-boot` arms it before the jump. NuttX refreshes it and widens it to 10 s in
`mx8mp_board_initialize()`, and `board_app_initialize()` sets the configured timeout and starts
feeding. WCR.WDBG is set, so it pauses while a debugger halts the core.

## Deploy

```sh
# from the Yocto repo (board on the network): copies the .bin as m7-app.bin to the boot partition
make m7-app-deploy M7_APP=~/PX4-Autopilot/build/nxp_frdm-imx8mp_default/nxp_frdm-imx8mp_default.bin
make m7-app-off      # back to bbz-m7-boot only
```

Try a new build by hand first, so a broken image can't loop the board. At the U-Boot prompt:
`loady 0x80100000` (YMODEM the `.bin`), `dcache flush`, `boot`; then `j` on the M7 console.
`bbz-m7-boot` stops auto-starting an image that WDOG3-resets the M7 3 times in a row.

## Debug

Hardware: J-Link (SEGGER software installed) on the board's JTAG/SWD header. Device
`MIMX8ML8_M7`. Linux keeps running while the M7 is halted.

**Terminal (GDB):**

```sh
boards/nxp/frdm-imx8mp/debug/m7-gdb.sh attach   # attach to the running image (no reset)
boards/nxp/frdm-imx8mp/debug/m7-gdb.sh load     # load the ELF into DDR, halt at the reset handler
```

PX4's J-Link helpers work too, once J-Link is installed and CMake has been re-run:
`make nxp_frdm-imx8mp_default jlink_gdb_backtrace`.

**VS Code:** install `marus25.cortex-debug` (listed in `.vscode/extensions.json`). Building this
target with J-Link installed makes CMake write `.vscode/launch.json` from
`platforms/nuttx/Debug/launch.json.in`, the same as for the other NuttX boards. Pick
"jlink (nxp_frdm-imx8mp)" and press F5. It loads the ELF into DDR and resets the M7;
`bbz-m7-boot` then auto-starts the freshly loaded image, so breakpoints in PX4 hit. To attach
without reloading, copy that entry and set `"request": "attach"`.

`gdb-multiarch` is used because `arm-none-eabi-gdb` 13.3 needs `libncursesw5`.

## Files

- Chip: NuttX's in-tree `arch/arm/src/mx8mp` (`CONFIG_ARCH_CHIP_MX8MP`, NuttX 12.12), unmodified.
- `nuttx-config/Kconfig`: board options (WDOG3, RPMsg TTY).
- `src/init.c`: board init, extra MPU regions (RPMsg shared memory non-cacheable, DDR window
  cacheable), WDOG3, RPMsg start. `src/rpmsg_tty.c`: A53 serial link.
- `platforms/nuttx/src/px4/nxp/mx8mp/`: PX4 platform layer (HRT on GPT1, reset, version/UUID).

## Status

Builds and links. **Not yet run on hardware.** I2C (`CONFIG_MX8MP_I2Cn`) and SPI
(`CONFIG_MX8MP_SPIn`) drivers exist in the chip but are not enabled or routed yet.
