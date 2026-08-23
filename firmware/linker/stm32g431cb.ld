/*
 * stm32g431cb.ld — PERDICAN V1 / STM32G431CBT6
 *
 * FLASH  128 KB @ 0x08000000
 * SRAM1   16 KB @ 0x20000000  \ contiguous, treated as one 22 KB region
 * SRAM2    6 KB @ 0x20004000  /
 * CCMRAM  10 KB @ 0x10000000    separate bus, left free for the user
 */

ENTRY(Reset_Handler)

MEMORY
{
  FLASH  (rx)  : ORIGIN = 0x08000000, LENGTH = 128K
  RAM    (rw)  : ORIGIN = 0x20000000, LENGTH = 22K
  CCMRAM (rw)  : ORIGIN = 0x10000000, LENGTH = 10K
}

/* Main stack lives at the top of RAM and grows down. */
_stack_size = 4K;
_estack     = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS
{
  .isr_vector :
  {
    . = ALIGN(4);
    KEEP(*(.isr_vector))
    . = ALIGN(4);
  } >FLASH

  .text :
  {
    . = ALIGN(4);
    *(.text)
    *(.text*)
    *(.glue_7)
    *(.glue_7t)
    *(.eh_frame)
    KEEP(*(.init))
    KEEP(*(.fini))
    . = ALIGN(4);
    _etext = .;
  } >FLASH

  .rodata :
  {
    . = ALIGN(4);
    *(.rodata)
    *(.rodata*)
    . = ALIGN(4);
  } >FLASH

  .ARM.extab : { *(.ARM.extab* .gnu.linkonce.armextab.*) } >FLASH
  .ARM       : {
    __exidx_start = .;
    *(.ARM.exidx*)
    __exidx_end = .;
  } >FLASH

  .preinit_array :
  {
    PROVIDE_HIDDEN(__preinit_array_start = .);
    KEEP(*(.preinit_array*))
    PROVIDE_HIDDEN(__preinit_array_end = .);
  } >FLASH
  .init_array :
  {
    PROVIDE_HIDDEN(__init_array_start = .);
    KEEP(*(SORT(.init_array.*)))
    KEEP(*(.init_array*))
    PROVIDE_HIDDEN(__init_array_end = .);
  } >FLASH
  .fini_array :
  {
    PROVIDE_HIDDEN(__fini_array_start = .);
    KEEP(*(SORT(.fini_array.*)))
    KEEP(*(.fini_array*))
    PROVIDE_HIDDEN(__fini_array_end = .);
  } >FLASH

  _sidata = LOADADDR(.data);

  .data :
  {
    . = ALIGN(4);
    _sdata = .;
    *(.data)
    *(.data*)
    . = ALIGN(4);
    _edata = .;
  } >RAM AT>FLASH

  .bss :
  {
    . = ALIGN(4);
    _sbss = .;
    __bss_start__ = _sbss;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    _ebss = .;
    __bss_end__ = _ebss;
  } >RAM

  /*
   * .noinit is deliberately NOT cleared by the startup code. The DFU reboot
   * magic word lives here so it survives a warm reset — PERDICAN V1 cannot
   * use the TAMP backup registers because VBAT is left floating (see board.h).
   */
  .noinit (NOLOAD) :
  {
    . = ALIGN(4);
    _snoinit = .;
    *(.noinit)
    *(.noinit*)
    . = ALIGN(4);
    _enoinit = .;
  } >RAM

  .heap (NOLOAD) :
  {
    . = ALIGN(8);
    _sheap = .;
    PROVIDE(end = .);
    PROVIDE(_end = .);
    . = ALIGN(8);
  } >RAM

  /* Fail the link rather than the board if we ever overflow into the stack. */
  ._stack_guard (NOLOAD) :
  {
    . = ALIGN(8);
    . = . + _stack_size;
    . = ALIGN(8);
  } >RAM

  .ccmram (NOLOAD) :
  {
    . = ALIGN(4);
    _sccmram = .;
    *(.ccmram)
    *(.ccmram*)
    . = ALIGN(4);
    _eccmram = .;
  } >CCMRAM

  .ARM.attributes 0 : { *(.ARM.attributes) }
}
