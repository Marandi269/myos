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

/* Section header types */
#define SHT_NULL     0   /* Inactive */
#define SHT_PROGBITS 1   /* Program data */
#define SHT_SYMTAB   2   /* Symbol table */
#define SHT_STRTAB   3   /* String table */
#define SHT_RELA     4   /* Relocation with addend */
#define SHT_HASH     5   /* Symbol hash table */
#define SHT_DYNAMIC  6   /* Dynamic linking info */
#define SHT_NOTE     7   /* Notes */
#define SHT_NOBITS   8   /* No space in file */
#define SHT_REL      9   /* Relocation without addend */
#define SHT_DYNSYM   11  /* Dynamic symbol table */

/* Dynamic section tags */
#define DT_NULL      0   /* End of dynamic section */
#define DT_NEEDED    1   /* Name of needed library */
#define DT_PLTRELSZ  2   /* Size of PLT relocs */
#define DT_PLTGOT    3   /* Address of PLT/GOT */
#define DT_HASH      4   /* Address of symbol hash table */
#define DT_STRTAB    5   /* Address of string table */
#define DT_SYMTAB    6   /* Address of symbol table */
#define DT_RELA      7   /* Address of Rela relocs */
#define DT_RELASZ    8   /* Total size of Rela relocs */
#define DT_RELAENT   9   /* Size of one Rela reloc */
#define DT_STRSZ     10  /* Size of string table */
#define DT_SYMENT    11  /* Size of one symbol entry */
#define DT_INIT      12  /* Address of init function */
#define DT_FINI      13  /* Address of fini function */
#define DT_SONAME    14  /* Name of shared object */
#define DT_RPATH     15  /* Library search path */
#define DT_SYMBOLIC  16  /* Alter symbol resolution */
#define DT_REL       17  /* Address of Rel relocs */
#define DT_RELSZ     18  /* Total size of Rel relocs */
#define DT_RELENT    19  /* Size of one Rel reloc */
#define DT_PLTREL    20  /* Type of PLT relocs */
#define DT_DEBUG     21  /* Reserved for debugger */
#define DT_TEXTREL   22  /* Relocs in non-writable seg */
#define DT_JMPREL    23  /* Address of PLT relocs */
#define DT_BIND_NOW  24  /* Process relocations now */
#define DT_INIT_ARRAY 25 /* Array of init functions */
#define DT_FINI_ARRAY 26 /* Array of fini functions */

/* Symbol binding */
#define STB_LOCAL  0   /* Local symbol */
#define STB_GLOBAL 1   /* Global symbol */
#define STB_WEAK   2   /* Weak symbol */

/* Symbol types */
#define STT_NOTYPE  0   /* No type */
#define STT_OBJECT  1   /* Data object */
#define STT_FUNC    2   /* Function */
#define STT_SECTION 3   /* Section */
#define STT_FILE    4   /* File name */

/* Symbol visibility */
#define STV_DEFAULT   0
#define STV_INTERNAL  1
#define STV_HIDDEN    2
#define STV_PROTECTED 3

/* Extract symbol info */
#define ELF64_ST_BIND(i)    ((i) >> 4)
#define ELF64_ST_TYPE(i)    ((i) & 0xf)
#define ELF64_ST_INFO(b,t)  (((b) << 4) + ((t) & 0xf))

/* x86_64 relocation types */
#define R_X86_64_NONE       0   /* No reloc */
#define R_X86_64_64         1   /* Direct 64-bit */
#define R_X86_64_PC32       2   /* PC relative 32-bit signed */
#define R_X86_64_GOT32      3   /* 32-bit GOT entry */
#define R_X86_64_PLT32      4   /* 32-bit PLT address */
#define R_X86_64_COPY       5   /* Copy symbol at runtime */
#define R_X86_64_GLOB_DAT   6   /* Create GOT entry */
#define R_X86_64_JUMP_SLOT  7   /* Create PLT entry */
#define R_X86_64_RELATIVE   8   /* Adjust by program base */
#define R_X86_64_GOTPCREL   9   /* 32-bit PC-rel GOT offset */
#define R_X86_64_32        10   /* Direct 32-bit zero-extended */
#define R_X86_64_32S       11   /* Direct 32-bit sign-extended */

/* Extract relocation info */
#define ELF64_R_SYM(i)     ((i) >> 32)
#define ELF64_R_TYPE(i)    ((i) & 0xffffffff)
#define ELF64_R_INFO(s,t)  (((uint64_t)(s) << 32) + (uint64_t)(t))

/* ELF64 symbol table entry */
typedef struct {
    uint32_t st_name;   /* Symbol name (string tbl index) */
    uint8_t  st_info;   /* Symbol type and binding */
    uint8_t  st_other;  /* Symbol visibility */
    uint16_t st_shndx;  /* Section index */
    uint64_t st_value;  /* Symbol value */
    uint64_t st_size;   /* Symbol size */
} __attribute__((packed)) Elf64_Sym;

/* ELF64 relocation entry (without addend) */
typedef struct {
    uint64_t r_offset;  /* Address */
    uint64_t r_info;    /* Relocation type and symbol */
} __attribute__((packed)) Elf64_Rel;

/* ELF64 relocation entry (with addend) */
typedef struct {
    uint64_t r_offset;  /* Address */
    uint64_t r_info;    /* Relocation type and symbol */
    int64_t  r_addend;  /* Addend */
} __attribute__((packed)) Elf64_Rela;

/* ELF64 dynamic section entry */
typedef struct {
    int64_t d_tag;      /* Dynamic entry type */
    union {
        uint64_t d_val; /* Integer value */
        uint64_t d_ptr; /* Address value */
    } d_un;
} __attribute__((packed)) Elf64_Dyn;

/* Load an ELF file into a process */
int elf_load(process_t *proc, const char *path);

/* Load ELF from memory buffer */
int elf_load_from_memory(process_t *proc, void *data, size_t size);

/* Validate ELF header */
int elf_validate(Elf64_Ehdr *ehdr);

/* Execute an ELF file (load and start) */
int elf_exec(const char *path, char *const argv[], char *const envp[]);

#endif /* _ELF_H */
