/*
 * dynlink.h - Dynamic linker interface
 *
 * Handles loading shared libraries and performing relocations.
 */

#ifndef _DYNLINK_H
#define _DYNLINK_H

#include "types.h"
#include "elf.h"
#include "process.h"

/* Maximum shared libraries per process */
#define MAX_SHARED_LIBS 32

/* Library search paths */
#define LIB_PATH_1 "/lib"
#define LIB_PATH_2 "/usr/lib"

/* Loaded shared object */
typedef struct shared_object {
    char name[64];              /* Library name (e.g., "libc.so") */
    uint64_t base;              /* Load base address */
    uint64_t size;              /* Mapped size */
    void *data;                 /* File contents in memory */
    size_t data_size;           /* File size */

    /* Dynamic section info */
    Elf64_Dyn *dynamic;         /* .dynamic section */
    char *strtab;               /* String table */
    Elf64_Sym *symtab;          /* Symbol table */
    uint32_t *hash;             /* Symbol hash table */
    Elf64_Rela *rela;           /* RELA relocations */
    size_t rela_size;           /* Size of RELA */
    Elf64_Rela *jmprel;         /* PLT relocations */
    size_t jmprel_size;         /* Size of JMPREL */
    uint64_t pltgot;            /* PLT/GOT address */

    int ref_count;              /* Reference count */
    struct shared_object *next; /* Next in list */
} shared_object_t;

/* Dynamic linker context for a process */
typedef struct dynlink_ctx {
    process_t *proc;
    shared_object_t *objects;   /* Loaded shared objects */
    int num_objects;
    uint64_t next_load_addr;    /* Next address to load SO */
} dynlink_ctx_t;

/* Initialize dynamic linker for a process */
dynlink_ctx_t *dynlink_init(process_t *proc);

/* Clean up dynamic linker context */
void dynlink_cleanup(dynlink_ctx_t *ctx);

/* Load a shared library */
shared_object_t *dynlink_load(dynlink_ctx_t *ctx, const char *name);

/* Resolve a symbol by name */
uint64_t dynlink_resolve(dynlink_ctx_t *ctx, const char *name);

/* Perform relocations for a shared object */
int dynlink_relocate(dynlink_ctx_t *ctx, shared_object_t *so);

/* Process PT_DYNAMIC segment */
int dynlink_process_dynamic(shared_object_t *so);

/* Load all needed libraries for an executable */
int dynlink_load_needed(dynlink_ctx_t *ctx, shared_object_t *so);

/* ELF hash function */
uint32_t elf_hash(const char *name);

/* Look up symbol in a shared object */
Elf64_Sym *dynlink_lookup_sym(shared_object_t *so, const char *name);

#endif /* _DYNLINK_H */
