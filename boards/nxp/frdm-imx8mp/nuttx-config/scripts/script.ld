/****************************************************************************
 * boards/nxp/frdm-imx8mp/nuttx-config/scripts/script.ld
 *
 * PX4 on the i.MX 8M Plus Cortex-M7 (NXP FRDM-IMX8MPLUS), running from DDR.
 *
 * The M7's 16 MB DDR window 0x80000000-0x81000000 is reserved for the M7 in the Linux
 * device tree and write-protected against the Cortex-A53s by the RDC (bbz-image):
 *   0x80000000-0x80100000  bbz-m7-boot (M7 bootloader in TCM) working area
 *   0x80100000-0x80800000  PX4 code + read-only data ("flash", application slot)
 *   0x80800000-0x81000000  PX4 data, bss and heap ("sram")
 * TCM (128 KB ITCM + 128 KB DTCM) is used by bbz-m7-boot.
 ****************************************************************************/

MEMORY
{
  flash (rx)  : ORIGIN = 0x80100000, LENGTH = 7M
  sram  (rwx) : ORIGIN = 0x80800000, LENGTH = 8M
}

OUTPUT_ARCH(arm)
EXTERN(_vectors)
ENTRY(_stext)

SECTIONS
{
	.text : {
		_stext = ABSOLUTE(.);
		*(.vectors)
		*(.text .text.*)
		*(.fixup)
		*(.gnu.warning)
		*(.rodata .rodata.*)
		*(.gnu.linkonce.t.*)
		*(.glue_7)
		*(.glue_7t)
		*(.got)
		*(.gcc_except_table)
		*(.gnu.linkonce.r.*)
		_etext = ABSOLUTE(.);
	} > flash

	.resource_table : {
		. = ALIGN(8);
		KEEP(*(.resource_table))
		. = ALIGN(4);
	} > flash

	.init_section : {
		_sinit = ABSOLUTE(.);
		KEEP(*(.init_array .init_array.*))
		_einit = ABSOLUTE(.);
	} > flash

	.ARM.extab : {
		*(.ARM.extab*)
	} > flash

	__exidx_start = ABSOLUTE(.);
	.ARM.exidx : {
		*(.ARM.exidx*)
	} > flash
	__exidx_end = ABSOLUTE(.);

	_eronly = ABSOLUTE(.);

	/* functions copied to RAM at start-up (CONFIG_ARCH_RAMFUNCS) */
	.ramfunc ALIGN(4) : {
		_sramfuncs = ABSOLUTE(.);
		*(.ramfunc .ramfunc.*)
		_eramfuncs = ABSOLUTE(.);
	} > sram AT > flash

	_framfuncs = LOADADDR(.ramfunc);

	.data : {
		_sdata = ABSOLUTE(.);
		*(.data .data.*)
		*(.gnu.linkonce.d.*)
		CONSTRUCTORS
		. = ALIGN(4);
		_edata = ABSOLUTE(.);
	} > sram AT > flash

	.bss : {
		_sbss = ABSOLUTE(.);
		*(.bss .bss.*)
		*(.gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(8);
		_ebss = ABSOLUTE(.);
	} > sram

	/* Stabs debugging sections. */
	.stab 0 : { *(.stab) }
	.stabstr 0 : { *(.stabstr) }
	.stab.excl 0 : { *(.stab.excl) }
	.stab.exclstr 0 : { *(.stab.exclstr) }
	.stab.index 0 : { *(.stab.index) }
	.stab.indexstr 0 : { *(.stab.indexstr) }
	.comment 0 : { *(.comment) }
	.debug_abbrev 0 : { *(.debug_abbrev) }
	.debug_info 0 : { *(.debug_info) }
	.debug_line 0 : { *(.debug_line) }
	.debug_pubnames 0 : { *(.debug_pubnames) }
	.debug_aranges 0 : { *(.debug_aranges) }

	/* cacheable region for the MPU (mx8mp_mpuinit.c) */
	__CACHE_REGION_START = ORIGIN(flash);
	__CACHE_REGION_SIZE  = LENGTH(flash) + LENGTH(sram);
}
