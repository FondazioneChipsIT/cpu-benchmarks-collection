OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS
{
  .text 0x80000000 :
  {
    _start_text = .;
    *(.text.init)
    *(.text)
    _end_text = .;
  }

  . = ALIGN(16);
  .rodata : { *(.rodata*) }

  . = ALIGN(0x8);
  /* Force next PT_LOAD segment onto a page boundary so text is not merged with writable data. */
  . = ALIGN(0x1000);

  .data : { *(.data) } :data

  .sdata :
  {
    __global_pointer$ = . + 0x800;
    *(.srodata.cst16) *(.srodata.cst8) *(.srodata.cst4) *(.srodata.cst2) *(.srodata*)
    *(.sdata .sdata.* .gnu.linkonce.s.*)
  }

  /* BSS unificata: .bss, .bss.*, .sbss, .sbss.*, COMMON */
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
