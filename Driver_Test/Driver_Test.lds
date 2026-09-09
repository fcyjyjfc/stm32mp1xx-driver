OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm") 
/*OUTPUT_FORMAT("elf32-arm", "elf32-arm", "elf32-arm")*/ 
OUTPUT_ARCH(arm) 
ENTRY(_start) 
SECTIONS 
{ 
    . = 0xc2000040; 
    . = ALIGN(4); 
    .text      : 
    {
        start.o(.text)
        *(.text) 
    } 
    . = ALIGN(4); 
    .rodata :  
    { *(.rodata) } 
    . = ALIGN(4); 
    .data :  
    { *(.data) } 
    . = ALIGN(4);
    __exidx_start = .;
    .ARM.exidx : { *(.ARM.exidx*) }
    __exidx_end = .;

    . = ALIGN(4);
    __bss_start = .;
    .bss :
    { *(.bss) }
    __bss_end = .;

    . = ALIGN(4);
    _end = .;
}