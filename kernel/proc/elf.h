/*
 * elf.h - ELF64 format definitions and loader interface
 */

#ifndef _ELF_H
#define _ELF_H

#include "types.h"
#include "process.h"

/* ELF Magic number */
#define ELF_MAGIC 0x464C457F  /* "\x7FELF" */

/* ELF Class */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* ELF Data encoding */
#define ELFDATA2LSB 1  /* Little-endian */
#define ELFDATA2MSB 2  /* Big-endian */

/* ELF Type */
#define ET_NONE   0  /* No file type */
#define ET_REL    1  /* Relocatable file */
#define ET_EXEC   2  /* Executable file */
#define ET_DYN    3  /* Shared object file */
#define ET_CORE   4  /* Core file */

/* ELF Machine */
#define EM_X86_64 62  /* AMD x86-64 */

/* Program header types */
#define PT_NULL    0  /* Unused */
#define PT_LOAD    1  /* Loadable segment */
#define PT_DYNAMIC 2  /* Dynamic linking info */
#define PT_INTERP  3  /* Interpreter */
#define PT_NOTE    4  /* Auxiliary info */
#define PT_SHLIB   5  /* Reserved */
#define PT_PHDR    6  /* Program header table */
#define PT_TLS     7  /* Thread-local storage */

/* Program header flags */
#define PF_X 0x1  /* Executable */
#define PF_W 0x2  /* Writable */
#define PF_R 0x4  /* Readable */

/* ELF64 header */
typedef struct {
    uint8_t  e_ident[16];   /* ELF identification */
    uint16_t e_type;        /* Object file type */
    uint16_t e_machine;     /* Machine type */
    uint32_t e_version;     /* Object file version */
    uint64_t e_entry;       /* Entry point address */
    uint64_t e_phoff;       /* Program header offset */
    uint64_t e_shoff;       /* Section header offset */
    uint32_t e_flags;       /* Processor-specific flags */
    uint16_t e_ehsize;      /* ELF header size */
    uint16_t e_phentsize;   /* Program header entry size */
    uint16_t e_phnum;       /* Number of program headers */
    uint16_t e_shentsize;   /* Section header entry size */
    uint16_t e_shnum;       /* Number of section headers */
    uint16_t e_shstrndx;    /* Section name string table index */
} __attribute__((packed)) Elf64_Ehdr;

/* ELF64 program header */
typedef struct {
    uint32_t p_type;    /* Segment type */
    uint32_t p_flags;   /* Segment flags */
    uint64_t p_offset;  /* Offset in file */
    uint64_t p_vaddr;   /* Virtual address */
    uint64_t p_paddr;   /* Physical address */
    uint64_t p_filesz;  /* Size in file */
    uint64_t p_memsz;   /* Size in memory */
    uint64_t p_align;   /* Alignment */
} __attribute__((packed)) Elf64_Phdr;

/* ELF64 section header */
typedef struct {
    uint32_t sh_name;       /* Section name index */
    uint32_t sh_type;       /* Section type */
    uint64_t sh_flags;      /* Section flags */
    uint64_t sh_addr;       /* Virtual address */
    uint64_t sh_offset;     /* Offset in file */
    uint64_t sh_size;       /* Section size */
    uint32_t sh_link;       /* Link to another section */
    uint32_t sh_info;       /* Additional info */
    uint64_t sh_addralign;  /* Alignment */
    uint64_t sh_entsize;    /* Entry size if table */
} __attribute__((packed)) Elf64_Shdr;

/* ELF ident indices */
#define EI_MAG0    0   /* Magic byte 0 */
#define EI_MAG1    1   /* Magic byte 1 */
#define EI_MAG2    2   /* Magic byte 2 */
#define EI_MAG3    3   /* Magic byte 3 */
#define EI_CLASS   4   /* File class */
#define EI_DATA    5   /* Data encoding */
#define EI_VERSION 6   /* File version */
#define EI_OSABI   7   /* OS/ABI identification */
#define EI_PAD     8   /* Start of padding */

/* Load an ELF file into a process */
int elf_load(process_t *proc, const char *path);

/* Load ELF from memory buffer */
int elf_load_from_memory(process_t *proc, void *data, size_t size);

/* Validate ELF header */
int elf_validate(Elf64_Ehdr *ehdr);

/* Execute an ELF file (load and start) */
int elf_exec(const char *path, char *const argv[], char *const envp[]);

#endif /* _ELF_H */
