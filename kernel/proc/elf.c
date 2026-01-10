/*
 * elf.c - ELF64 loader implementation
 *
 * Loads statically linked ELF64 executables into user space.
 */

#include "elf.h"
#include "process.h"
#include "scheduler.h"
#include "user_space.h"
#include "usermode.h"
#include "syscall.h"
#include "../fs/vfs.h"
#include "../fs/fd.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Forward declaration for switch_and_jump */
extern void switch_and_jump_to_usermode(uint64_t pml4, uint64_t entry, uint64_t user_stack);

/* External function to set TSS RSP0 */
extern void tss_set_rsp0(uint64_t rsp0);

/* User process wrapper - jumps to user mode */
static void user_process_wrapper(void) {
    process_t *proc = current_proc;

    kprintf("[USER] Starting user process: %s (entry=0x%lx, stack=0x%lx)\n",
            proc->name, proc->user_entry, proc->user_stack);

    /* Set TSS RSP0 to this process's kernel stack
     * This is where the CPU will switch to when an interrupt
     * occurs while in user mode
     */
    tss_set_rsp0(proc->kernel_stack);

    /* Switch address space and jump to user mode */
    switch_and_jump_to_usermode((uint64_t)proc->page_table,
                                 proc->user_entry,
                                 proc->user_stack);

    /* Never reached */
    kprintf("[USER] ERROR: switch_and_jump_to_usermode returned!\n");
    process_exit(-1);
}

/*
 * Validate ELF header
 */
int elf_validate(Elf64_Ehdr *ehdr) {
    /* Check magic number */
    if (ehdr->e_ident[EI_MAG0] != 0x7F ||
        ehdr->e_ident[EI_MAG1] != 'E' ||
        ehdr->e_ident[EI_MAG2] != 'L' ||
        ehdr->e_ident[EI_MAG3] != 'F') {
        kprintf("[ELF] Invalid magic number\n");
        return -1;
    }

    /* Check class (64-bit) */
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        kprintf("[ELF] Not a 64-bit ELF\n");
        return -1;
    }

    /* Check endianness (little-endian) */
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        kprintf("[ELF] Not little-endian\n");
        return -1;
    }

    /* Check type (executable) */
    if (ehdr->e_type != ET_EXEC && ehdr->e_type != ET_DYN) {
        kprintf("[ELF] Not an executable (type=%d)\n", ehdr->e_type);
        return -1;
    }

    /* Check machine (x86_64) */
    if (ehdr->e_machine != EM_X86_64) {
        kprintf("[ELF] Not x86_64 (machine=%d)\n", ehdr->e_machine);
        return -1;
    }

    return 0;
}

/*
 * Load ELF from memory buffer into process
 */
int elf_load_from_memory(process_t *proc, void *data, size_t size) {
    Elf64_Ehdr *ehdr;
    Elf64_Phdr *phdr;
    int i;
    uint64_t max_addr = 0;

    if (!proc || !data || size < sizeof(Elf64_Ehdr)) {
        return -EINVAL;
    }

    ehdr = (Elf64_Ehdr *)data;

    /* Validate ELF header */
    if (elf_validate(ehdr) < 0) {
        return -ENOEXEC;
    }

    kprintf("[ELF] Loading: entry=0x%lx, phnum=%d\n",
            ehdr->e_entry, ehdr->e_phnum);

    /* Create user address space if needed */
    if (!proc->page_table) {
        proc->page_table = create_user_address_space();
        if (!proc->page_table) {
            kprintf("[ELF] Failed to create address space\n");
            return -ENOMEM;
        }
    }

    /* Validate program header offset and size */
    size_t phdr_end = ehdr->e_phoff + ehdr->e_phnum * sizeof(Elf64_Phdr);
    if (phdr_end > size) {
        kprintf("[ELF] Program headers extend beyond file\n");
        return -ENOEXEC;
    }

    /* Process program headers */
    phdr = (Elf64_Phdr *)((uint8_t *)data + ehdr->e_phoff);

    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        uint64_t vaddr = phdr[i].p_vaddr;
        uint64_t memsz = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;
        uint32_t flags = phdr[i].p_flags;

        kprintf("[ELF] Segment %d: vaddr=0x%lx, memsz=0x%lx, filesz=0x%lx, flags=%c%c%c\n",
                i, vaddr, memsz, filesz,
                (flags & PF_R) ? 'R' : '-',
                (flags & PF_W) ? 'W' : '-',
                (flags & PF_X) ? 'X' : '-');

        /* Validate segment */
        if (offset + filesz > size) {
            kprintf("[ELF] Segment extends beyond file\n");
            return -ENOEXEC;
        }

        /* Map pages for this segment */
        uint64_t seg_start = vaddr & ~(PAGE_SIZE - 1);
        uint64_t seg_end = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uint64_t addr = seg_start; addr < seg_end; addr += PAGE_SIZE) {
            /* Allocate physical page */
            uint64_t phys = (uint64_t)pmm_alloc_page();
            if (!phys) {
                kprintf("[ELF] Failed to allocate page\n");
                return -ENOMEM;
            }

            /* Clear the page */
            memset((void *)phys, 0, PAGE_SIZE);

            /* Copy data from file to this page */
            /* Calculate the range of virtual addresses this page covers */
            uint64_t page_vstart = addr;
            uint64_t page_vend = addr + PAGE_SIZE;

            /* Calculate the range of file data for this segment */
            uint64_t file_vstart = vaddr;
            uint64_t file_vend = vaddr + filesz;

            /* Find the overlap between this page and the file data */
            uint64_t copy_vstart = (page_vstart > file_vstart) ? page_vstart : file_vstart;
            uint64_t copy_vend = (page_vend < file_vend) ? page_vend : file_vend;

            if (copy_vstart < copy_vend) {
                /* There is data to copy */
                uint64_t dest_offset = copy_vstart - page_vstart;  /* Offset within the page */
                uint64_t src_offset = copy_vstart - vaddr;         /* Offset within segment data */
                uint64_t copy_size = copy_vend - copy_vstart;

                memcpy((void *)(phys + dest_offset),
                       (uint8_t *)data + offset + src_offset,
                       copy_size);
            }

            /* Map the page */
            uint64_t page_flags = PTE_USER;
            if (flags & PF_W) page_flags |= PTE_WRITABLE;

            vmm_map_page_in(proc->page_table, addr, phys, page_flags);
        }

        /* Track highest address for heap */
        if (vaddr + memsz > max_addr) {
            max_addr = vaddr + memsz;
        }
    }

    /* Set entry point */
    proc->user_entry = ehdr->e_entry;

    /* Set initial break (heap starts after last segment) */
    proc->brk = (max_addr + PAGE_SIZE) & ~(PAGE_SIZE - 1);
    if (proc->brk < 0x1000000) {
        proc->brk = 0x1000000;  /* Minimum 16MB */
    }

    /* Stack will be setup later with proper argc/argv */
    proc->is_user = 1;

    kprintf("[ELF] ELF loaded, entry=0x%lx, brk=0x%lx\n",
            proc->user_entry, proc->brk);

    return 0;
}

/*
 * Load ELF from file path
 */
int elf_load(process_t *proc, const char *path) {
    struct file *file;
    void *buf;
    size_t size;
    ssize_t read_size;
    int ret;

    if (!proc || !path) {
        return -EINVAL;
    }

    kprintf("[ELF] Loading file: %s\n", path);

    /* Open the file */
    file = vfs_open(path, O_RDONLY, 0);
    if (!file) {
        kprintf("[ELF] Failed to open file: %s\n", path);
        return -ENOENT;
    }

    /* Get file size */
    size = file->f_inode->i_size;
    if (size < sizeof(Elf64_Ehdr)) {
        vfs_close(file);
        return -ENOEXEC;
    }

    /* Allocate buffer */
    buf = kmalloc(size);
    if (!buf) {
        vfs_close(file);
        return -ENOMEM;
    }

    /* Read file contents */
    read_size = vfs_read(file, buf, size);
    vfs_close(file);

    if (read_size != (ssize_t)size) {
        kfree(buf);
        return -EIO;
    }

    /* Load from memory */
    ret = elf_load_from_memory(proc, buf, size);

    kfree(buf);
    return ret;
}

/*
 * Setup argc/argv/envp on user stack
 * Returns new stack pointer
 */
static uint64_t setup_stack_args(process_t *proc, char *const argv[], char *const envp[]) {
    uint64_t sp = proc->user_stack;
    int argc = 0;
    int envc = 0;

    (void)envc;  /* Suppress unused warning for now */

    /* Count arguments */
    if (argv) {
        while (argv[argc]) argc++;
    }
    if (envp) {
        while (envp[envc]) envc++;
    }

    /* For simplicity, we'll just push argc and leave argv/envp as NULL
     * A full implementation would copy the strings to user stack */

    /* Align stack to 16 bytes */
    sp &= ~0xF;

    /* Push NULL terminator for envp */
    sp -= 8;

    /* Push NULL terminator for argv */
    sp -= 8;

    /* Push argc */
    sp -= 8;
    /* Note: We can't directly write to user space here without mapping
     * For now, set argc = 0 */

    proc->user_stack = sp;
    return sp;
}

/*
 * Execute an ELF file - replaces current process
 */
int elf_exec(const char *path, char *const argv[], char *const envp[]) {
    process_t *proc = current_proc;
    int ret;

    if (!proc) {
        return -ESRCH;
    }

    kprintf("[ELF] exec: %s\n", path);

    /* Clear existing address space if any (but keep kernel mappings) */
    if (proc->page_table) {
        free_user_address_space(proc->page_table);
        proc->page_table = NULL;
    }

    /* Load the new executable */
    ret = elf_load(proc, path);
    if (ret < 0) {
        kprintf("[ELF] Failed to load: %d\n", ret);
        return ret;
    }

    /* Update process name */
    const char *name = path;
    const char *slash = strrchr(path, '/');
    if (slash) name = slash + 1;
    strncpy(proc->name, name, sizeof(proc->name) - 1);

    /* Setup stack arguments */
    setup_stack_args(proc, argv, envp);

    kprintf("[ELF] Ready to execute: %s (entry=0x%lx)\n", proc->name, proc->user_entry);

    return 0;
}

/*
 * Create and start a user process from an ELF file with arguments
 */
process_t *elf_create_process_with_args(const char *path, int argc, char *argv[], char *envp[]) {
    process_t *proc;
    context_t *ctx;
    int ret;

    /* Allocate process */
    proc = process_alloc();
    if (!proc) {
        return NULL;
    }

    /* Load ELF - this loads the binary but doesn't set up stack yet */
    ret = elf_load(proc, path);
    if (ret < 0) {
        process_free(proc);
        return NULL;
    }

    /* Set process name */
    const char *name = path;
    const char *slash = strrchr(path, '/');
    if (slash) name = slash + 1;
    strncpy(proc->name, name, sizeof(proc->name) - 1);

    /* Setup user stack with proper argc/argv */
    if (setup_user_stack_with_args(proc, argc, argv, envp) < 0) {
        kprintf("[ELF] Failed to setup user stack\n");
        process_free(proc);
        return NULL;
    }
    kprintf("[ELF] User stack pointer: 0x%lx\n", proc->user_stack);

    /* Set up context to call user_process_wrapper when scheduled */
    proc->kernel_stack -= sizeof(context_t);
    ctx = (context_t *)proc->kernel_stack;
    memset(ctx, 0, sizeof(context_t));
    ctx->rip = (uint64_t)user_process_wrapper;
    ctx->rbp = 0;
    proc->context = ctx;

    /* Create file descriptor table for the process */
    proc->fd_table = fd_table_create();
    if (proc->fd_table) {
        /* Set up stdio */
        struct file *console = vfs_open("/dev/console", O_RDWR, 0);
        if (console) {
            fd_alloc(proc->fd_table, console);  /* stdin = 0 */
            fd_alloc(proc->fd_table, console);  /* stdout = 1 */
            fd_alloc(proc->fd_table, console);  /* stderr = 2 */
        }
    }

    return proc;
}

/*
 * Create and start a user process from an ELF file
 * Uses the path as argv[0] with argc=1
 */
process_t *elf_create_process(const char *path) {
    char *argv[2];
    argv[0] = (char *)path;
    argv[1] = NULL;
    return elf_create_process_with_args(path, 1, argv, NULL);
}
