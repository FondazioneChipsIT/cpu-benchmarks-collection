OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  . = 0x10000000;

  _start_text = .;
  .text.init : { *(.text.init) }
  . = ALIGN(16);
  .text : { *(.text) }
  . = ALIGN(16);
  _end_text = .;
  . = ALIGN(16);
  .rodata : { *(.rodata*) }
  . = ALIGN(0x8);
  . = ALIGN(0x1000);

  .data : { *(.data) } :data

  .sdata :
  {
    __global_pointer$ = . + 0x800;
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  }

  /* BSS unificata */
  .bss :
  {
    . = ALIGN(16);
    _bss_start = .;
    *(.bss)
    *(.bss.*)
    *(.sbss)
    *(.sbss.*)
    *(COMMON)
    . = ALIGN(16);
    _bss_end = .;
  }

  _end = .;
}

PHDRS {
  text PT_LOAD FLAGS(5); /* R + X */
  data PT_LOAD FLAGS(6); /* R + W */
}