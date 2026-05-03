MEMORY
{
  RAM(RWX) : ORIGIN = 0x00000800, LENGTH = 1M
}

SECTIONS
{
  . = 0x00000800;
  . = ALIGN(2);
  
  .text : {
    *(.text)
  } > RAM
  . = ALIGN(2);
  
  .rodata : {
    *(.rodata)
  } > RAM
  . = ALIGN(2);

  .data : {
    *(.data)
  } > RAM
  . = ALIGN(2);

  .bss : {
    *(.bss)
  } > RAM
}
