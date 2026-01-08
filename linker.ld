/* Linker script for MyOS 64-bit kernel */

ENTRY(_start)

SECTIONS
{
    /* 内核加载到 1MB 处 */
    . = 1M;

    /* Multiboot header 必须在最前面 */
    .multiboot ALIGN(8) :
    {
        *(.multiboot)
    }

    /* 代码段 */
    .text ALIGN(4K) :
    {
        *(.text)
        *(.text.*)
    }

    /* 只读数据 */
    .rodata ALIGN(4K) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* 已初始化数据 */
    .data ALIGN(4K) :
    {
        *(.data)
        *(.data.*)
    }

    /* 未初始化数据 */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* 丢弃不需要的段 */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
    }
}
