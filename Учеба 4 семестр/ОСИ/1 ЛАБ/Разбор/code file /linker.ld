ENTRY(kmain)

SECTIONS
{
    . = 0x10000;

    .text :
    {
        *(.text)
    }

    .rodata :
    {
        *(.rodata)
    }

    /DISCARD/ :
    {
        *(.eh_frame)
        *(.comment)
        *(.note*)
    }
}
