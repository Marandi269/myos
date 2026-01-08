/*
 * dynlink.c - Dynamic linker implementation
 *
 * Loads shared libraries (.so) and performs runtime relocations.
 */

#include "dynlink.h"
#include "elf.h"
#include "process.h"
#include "syscall.h"
#include "user_space.h"
#include "../fs/vfs.h"
#include "../mm/pmm.h"
#include "../mm/vmm.h"
#include "../mm/heap.h"
#include "../lib/kprintf.h"
#include "../lib/string.h"

/* Maximum uint64 value */
#define UINT64_MAX_VAL 0xFFFFFFFFFFFFFFFFUL

/* Default base address for loading shared objects */
#define SO_BASE_ADDR 0x7f0000000000UL

/* ELF hash function (standard ELF hash) */
uint32_t elf_hash(const char *name) {
    uint32_t h = 0, g;
    while (*name) {
        h = (h << 4) + (uint8_t)*name++;
        if ((g = h & 0xf0000000))
            h ^= g >> 24;
        h &= ~g;
    }
    return h;
}

/* Initialize dynamic linker for a process */
dynlink_ctx_t *dynlink_init(process_t *proc) {
    dynlink_ctx_t *ctx;

    ctx = kmalloc(sizeof(dynlink_ctx_t));
    if (!ctx) return NULL;

    memset(ctx, 0, sizeof(dynlink_ctx_t));
    ctx->proc = proc;
    ctx->next_load_addr = SO_BASE_ADDR;

    kprintf("[dynlink] Initialized for process %d\n", proc->pid);
    return ctx;
}

/* Clean up dynamic linker context */
void dynlink_cleanup(dynlink_ctx_t *ctx) {
    shared_object_t *so, *next;

    if (!ctx) return;

    /* Free all shared objects */
    so = ctx->objects;
    while (so) {
        next = so->next;
        if (so->data) {
            kfree(so->data);
        }
        kfree(so);
        so = next;
    }

    kfree(ctx);
}

/* Find library file in search paths */
static struct file *find_library(const char *name, char *fullpath, size_t pathlen) {
    struct file *file;

    /* Check if it's an absolute path */
    if (name[0] == '/') {
        strncpy(fullpath, name, pathlen);
        return vfs_open(name, O_RDONLY, 0);
    }

    /* Search in /lib */
    if (strlen(LIB_PATH_1) + strlen(name) + 2 < pathlen) {
        strcpy(fullpath, LIB_PATH_1);
        strcat(fullpath, "/");
        strcat(fullpath, name);
        file = vfs_open(fullpath, O_RDONLY, 0);
        if (file) return file;
    }

    /* Search in /usr/lib */
    if (strlen(LIB_PATH_2) + strlen(name) + 2 < pathlen) {
        strcpy(fullpath, LIB_PATH_2);
        strcat(fullpath, "/");
        strcat(fullpath, name);
        file = vfs_open(fullpath, O_RDONLY, 0);
        if (file) return file;
    }

    return NULL;
}

/* Check if library is already loaded */
static shared_object_t *find_loaded(dynlink_ctx_t *ctx, const char *name) {
    shared_object_t *so;

    for (so = ctx->objects; so; so = so->next) {
        if (strcmp(so->name, name) == 0) {
            return so;
        }
    }
    return NULL;
}

/* Load a shared library */
shared_object_t *dynlink_load(dynlink_ctx_t *ctx, const char *name) {
    shared_object_t *so;
    struct file *file;
    char fullpath[256];
    void *buf;
    size_t size;
    ssize_t read_size;
    Elf64_Ehdr *ehdr;
    Elf64_Phdr *phdr;
    uint64_t base_addr;
    uint64_t load_bias = 0;
    int i;

    /* Check if already loaded */
    so = find_loaded(ctx, name);
    if (so) {
        so->ref_count++;
        return so;
    }

    kprintf("[dynlink] Loading: %s\n", name);

    /* Find and open the library file */
    file = find_library(name, fullpath, sizeof(fullpath));
    if (!file) {
        kprintf("[dynlink] Library not found: %s\n", name);
        return NULL;
    }

    /* Get file size */
    size = file->f_inode->i_size;
    if (size < sizeof(Elf64_Ehdr)) {
        vfs_close(file);
        return NULL;
    }

    /* Allocate buffer and read file */
    buf = kmalloc(size);
    if (!buf) {
        vfs_close(file);
        return NULL;
    }

    read_size = vfs_read(file, buf, size);
    vfs_close(file);

    if (read_size != (ssize_t)size) {
        kfree(buf);
        return NULL;
    }

    /* Validate ELF */
    ehdr = (Elf64_Ehdr *)buf;
    if (elf_validate(ehdr) < 0) {
        kfree(buf);
        return NULL;
    }

    /* Must be a shared object */
    if (ehdr->e_type != ET_DYN) {
        kprintf("[dynlink] Not a shared object\n");
        kfree(buf);
        return NULL;
    }

    /* Allocate shared object structure */
    so = kmalloc(sizeof(shared_object_t));
    if (!so) {
        kfree(buf);
        return NULL;
    }
    memset(so, 0, sizeof(shared_object_t));

    strncpy(so->name, name, sizeof(so->name) - 1);
    so->data = buf;
    so->data_size = size;
    so->ref_count = 1;

    /* Determine load address */
    base_addr = ctx->next_load_addr;

    /* Calculate size needed and find lowest vaddr */
    uint64_t min_vaddr = UINT64_MAX_VAL;
    uint64_t max_vaddr = 0;

    phdr = (Elf64_Phdr *)((uint8_t *)buf + ehdr->e_phoff);
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_LOAD) {
            if (phdr[i].p_vaddr < min_vaddr) {
                min_vaddr = phdr[i].p_vaddr;
            }
            uint64_t end = phdr[i].p_vaddr + phdr[i].p_memsz;
            if (end > max_vaddr) {
                max_vaddr = end;
            }
        }
    }

    /* Load bias for PIE/shared objects */
    load_bias = base_addr - (min_vaddr & ~(PAGE_SIZE - 1));
    so->base = base_addr;
    so->size = (max_vaddr - min_vaddr + PAGE_SIZE) & ~(PAGE_SIZE - 1);

    kprintf("[dynlink] %s: base=0x%lx, bias=0x%lx, size=0x%lx\n",
            name, so->base, load_bias, so->size);

    /* Load PT_LOAD segments */
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) {
            continue;
        }

        uint64_t vaddr = phdr[i].p_vaddr + load_bias;
        uint64_t memsz = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;
        uint32_t flags = phdr[i].p_flags;

        kprintf("[dynlink] Segment: vaddr=0x%lx, memsz=0x%lx, flags=%c%c%c\n",
                vaddr, memsz,
                (flags & PF_R) ? 'R' : '-',
                (flags & PF_W) ? 'W' : '-',
                (flags & PF_X) ? 'X' : '-');

        /* Map pages */
        uint64_t seg_start = vaddr & ~(PAGE_SIZE - 1);
        uint64_t seg_end = (vaddr + memsz + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);

        for (uint64_t addr = seg_start; addr < seg_end; addr += PAGE_SIZE) {
            uint64_t phys = (uint64_t)pmm_alloc_page();
            if (!phys) {
                kprintf("[dynlink] Failed to allocate page\n");
                goto fail;
            }

            memset((void *)phys, 0, PAGE_SIZE);

            /* Copy data from file */
            uint64_t page_offset = addr - vaddr;
            if (page_offset < filesz) {
                uint64_t copy_size = PAGE_SIZE;
                if (page_offset + copy_size > filesz) {
                    copy_size = filesz - page_offset;
                }
                memcpy((void *)phys, (uint8_t *)buf + offset + page_offset, copy_size);
            }

            /* Map into process address space */
            uint64_t page_flags = PTE_USER;
            if (flags & PF_W) page_flags |= PTE_WRITABLE;

            vmm_map_page_in(ctx->proc->page_table, addr, phys, page_flags);
        }
    }

    /* Find and process PT_DYNAMIC */
    for (i = 0; i < ehdr->e_phnum; i++) {
        if (phdr[i].p_type == PT_DYNAMIC) {
            so->dynamic = (Elf64_Dyn *)((uint8_t *)buf + phdr[i].p_offset);
            dynlink_process_dynamic(so);
            break;
        }
    }

    /* Update next load address */
    ctx->next_load_addr = (so->base + so->size + PAGE_SIZE) & ~(PAGE_SIZE - 1);

    /* Add to list */
    so->next = ctx->objects;
    ctx->objects = so;
    ctx->num_objects++;

    kprintf("[dynlink] Loaded %s at 0x%lx\n", name, so->base);
    return so;

fail:
    kfree(so);
    kfree(buf);
    return NULL;
}

/* Process PT_DYNAMIC segment */
int dynlink_process_dynamic(shared_object_t *so) {
    Elf64_Dyn *dyn;

    if (!so->dynamic) {
        return -1;
    }

    for (dyn = so->dynamic; dyn->d_tag != DT_NULL; dyn++) {
        switch (dyn->d_tag) {
        case DT_STRTAB:
            so->strtab = (char *)((uint8_t *)so->data + dyn->d_un.d_ptr);
            break;
        case DT_SYMTAB:
            so->symtab = (Elf64_Sym *)((uint8_t *)so->data + dyn->d_un.d_ptr);
            break;
        case DT_HASH:
            so->hash = (uint32_t *)((uint8_t *)so->data + dyn->d_un.d_ptr);
            break;
        case DT_RELA:
            so->rela = (Elf64_Rela *)((uint8_t *)so->data + dyn->d_un.d_ptr);
            break;
        case DT_RELASZ:
            so->rela_size = dyn->d_un.d_val;
            break;
        case DT_JMPREL:
            so->jmprel = (Elf64_Rela *)((uint8_t *)so->data + dyn->d_un.d_ptr);
            break;
        case DT_PLTRELSZ:
            so->jmprel_size = dyn->d_un.d_val;
            break;
        case DT_PLTGOT:
            so->pltgot = dyn->d_un.d_ptr;
            break;
        case DT_NEEDED:
            /* Will be processed by dynlink_load_needed */
            break;
        }
    }

    return 0;
}

/* Look up symbol in a shared object using hash table */
Elf64_Sym *dynlink_lookup_sym(shared_object_t *so, const char *name) {
    uint32_t hash, nbucket, nchain;
    uint32_t *bucket, *chain;
    uint32_t idx;

    if (!so->hash || !so->symtab || !so->strtab) {
        return NULL;
    }

    /* Hash table format: nbucket, nchain, bucket[nbucket], chain[nchain] */
    nbucket = so->hash[0];
    nchain = so->hash[1];
    bucket = &so->hash[2];
    chain = &so->hash[2 + nbucket];

    (void)nchain;  /* Used for bounds checking in real impl */

    hash = elf_hash(name);
    idx = bucket[hash % nbucket];

    while (idx != 0) {
        Elf64_Sym *sym = &so->symtab[idx];
        const char *symname = so->strtab + sym->st_name;

        if (strcmp(symname, name) == 0) {
            /* Found it */
            if (sym->st_shndx != 0) {  /* Not undefined */
                return sym;
            }
        }

        idx = chain[idx];
    }

    return NULL;
}

/* Resolve a symbol by name across all loaded objects */
uint64_t dynlink_resolve(dynlink_ctx_t *ctx, const char *name) {
    shared_object_t *so;
    Elf64_Sym *sym;

    for (so = ctx->objects; so; so = so->next) {
        sym = dynlink_lookup_sym(so, name);
        if (sym && sym->st_value != 0) {
            /* Return absolute address */
            return so->base + sym->st_value;
        }
    }

    kprintf("[dynlink] Undefined symbol: %s\n", name);
    return 0;
}

/* Perform relocations for a shared object */
int dynlink_relocate(dynlink_ctx_t *ctx, shared_object_t *so) {
    size_t i, num_rela;
    uint64_t load_bias;

    /* Calculate load bias */
    load_bias = so->base;

    /* Process RELA relocations */
    if (so->rela && so->rela_size > 0) {
        num_rela = so->rela_size / sizeof(Elf64_Rela);
        kprintf("[dynlink] Processing %d RELA relocations\n", (int)num_rela);

        for (i = 0; i < num_rela; i++) {
            Elf64_Rela *rel = &so->rela[i];
            uint32_t type = ELF64_R_TYPE(rel->r_info);
            uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
            uint64_t *target = (uint64_t *)(load_bias + rel->r_offset);
            uint64_t sym_val = 0;

            /* Get symbol value if needed */
            if (sym_idx != 0 && so->symtab && so->strtab) {
                Elf64_Sym *sym = &so->symtab[sym_idx];
                const char *name = so->strtab + sym->st_name;

                if (sym->st_shndx == 0) {
                    /* Undefined - look up in other objects */
                    sym_val = dynlink_resolve(ctx, name);
                } else {
                    sym_val = load_bias + sym->st_value;
                }
            }

            switch (type) {
            case R_X86_64_NONE:
                break;

            case R_X86_64_64:
                *target = sym_val + rel->r_addend;
                break;

            case R_X86_64_RELATIVE:
                *target = load_bias + rel->r_addend;
                break;

            case R_X86_64_GLOB_DAT:
            case R_X86_64_JUMP_SLOT:
                *target = sym_val;
                break;

            case R_X86_64_COPY:
                /* Copy symbol data - needs special handling */
                break;

            default:
                kprintf("[dynlink] Unknown reloc type: %d\n", type);
                break;
            }
        }
    }

    /* Process PLT relocations (JMPREL) */
    if (so->jmprel && so->jmprel_size > 0) {
        num_rela = so->jmprel_size / sizeof(Elf64_Rela);
        kprintf("[dynlink] Processing %d PLT relocations\n", (int)num_rela);

        for (i = 0; i < num_rela; i++) {
            Elf64_Rela *rel = &so->jmprel[i];
            uint32_t type = ELF64_R_TYPE(rel->r_info);
            uint32_t sym_idx = ELF64_R_SYM(rel->r_info);
            uint64_t *target = (uint64_t *)(load_bias + rel->r_offset);

            if (type == R_X86_64_JUMP_SLOT && sym_idx != 0) {
                Elf64_Sym *sym = &so->symtab[sym_idx];
                const char *name = so->strtab + sym->st_name;
                uint64_t sym_val;

                if (sym->st_shndx == 0) {
                    sym_val = dynlink_resolve(ctx, name);
                } else {
                    sym_val = load_bias + sym->st_value;
                }

                *target = sym_val;
            }
        }
    }

    return 0;
}

/* Load all needed libraries for a shared object */
int dynlink_load_needed(dynlink_ctx_t *ctx, shared_object_t *so) {
    Elf64_Dyn *dyn;

    if (!so->dynamic || !so->strtab) {
        return 0;
    }

    for (dyn = so->dynamic; dyn->d_tag != DT_NULL; dyn++) {
        if (dyn->d_tag == DT_NEEDED) {
            const char *name = so->strtab + dyn->d_un.d_val;
            kprintf("[dynlink] Needed library: %s\n", name);

            shared_object_t *dep = dynlink_load(ctx, name);
            if (!dep) {
                kprintf("[dynlink] Failed to load dependency: %s\n", name);
                /* Continue anyway - might be optional */
            }
        }
    }

    return 0;
}
