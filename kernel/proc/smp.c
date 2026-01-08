/*
 * smp.c - Symmetric Multiprocessing support
 *
 * Handles multi-processor initialization and management.
 */

#include "proc/smp.h"
#include "proc/gdt.h"
#include "drivers/apic.h"
#include "lib/kprintf.h"
#include "lib/string.h"
#include "mm/pmm.h"
#include "mm/vmm.h"

/* AP boot trampoline code (16-bit to 64-bit transition) */
extern uint8_t ap_trampoline_start[];
extern uint8_t ap_trampoline_end[];

/* AP startup flag - set when AP is ready */
static volatile uint32_t ap_started = 0;

/* Kernel page table for APs */
extern uint64_t *kernel_pml4;

/* ACPI MADT signatures */
#define ACPI_SIG_MADT   "APIC"

/* MADT entry types */
#define MADT_LOCAL_APIC     0
#define MADT_IO_APIC        1
#define MADT_INT_OVERRIDE   2
#define MADT_NMI            4
#define MADT_LOCAL_APIC_64  9

/* ACPI table header */
typedef struct __attribute__((packed)) {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} acpi_header_t;

/* MADT header */
typedef struct __attribute__((packed)) {
    acpi_header_t header;
    uint32_t local_apic_addr;
    uint32_t flags;
} madt_header_t;

/* MADT entry header */
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
} madt_entry_t;

/* Local APIC entry */
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint8_t acpi_processor_id;
    uint8_t apic_id;
    uint32_t flags;
} madt_local_apic_t;

/* I/O APIC entry */
typedef struct __attribute__((packed)) {
    uint8_t type;
    uint8_t length;
    uint8_t io_apic_id;
    uint8_t reserved;
    uint32_t address;
    uint32_t gsi_base;
} madt_io_apic_t;

/* Detected CPUs from ACPI */
static uint32_t detected_apic_ids[MAX_CPUS];
static uint32_t detected_cpu_count = 0;

/* Parse MADT to detect CPUs */
static void parse_madt(madt_header_t *madt) {
    uint8_t *ptr = (uint8_t *)madt + sizeof(madt_header_t);
    uint8_t *end = (uint8_t *)madt + madt->header.length;

    /* Get Local APIC base address */
    lapic_base = madt->local_apic_addr;

    kprintf("[SMP] MADT: Local APIC at 0x%x\n", madt->local_apic_addr);

    while (ptr < end) {
        madt_entry_t *entry = (madt_entry_t *)ptr;

        switch (entry->type) {
        case MADT_LOCAL_APIC: {
            madt_local_apic_t *lapic = (madt_local_apic_t *)ptr;
            if (lapic->flags & 1) {  /* Processor enabled */
                if (detected_cpu_count < MAX_CPUS) {
                    detected_apic_ids[detected_cpu_count++] = lapic->apic_id;
                    kprintf("[SMP] Found CPU: APIC ID %d\n", lapic->apic_id);
                }
            }
            break;
        }
        case MADT_IO_APIC: {
            madt_io_apic_t *ioapic = (madt_io_apic_t *)ptr;
            ioapic_base = ioapic->address;
            kprintf("[SMP] Found I/O APIC: ID %d at 0x%x, GSI base %d\n",
                    ioapic->io_apic_id, ioapic->address, ioapic->gsi_base);
            break;
        }
        case MADT_INT_OVERRIDE:
            /* Interrupt source override - for ISA IRQ remapping */
            break;
        default:
            break;
        }

        ptr += entry->length;
        if (entry->length == 0) break;  /* Avoid infinite loop */
    }
}

/* Simple ACPI table search (looks in first 1MB) */
static void *find_acpi_table(const char *sig) {
    /* Search EBDA and BIOS ROM area for RSDP */
    /* Simplified: Just search known locations */

    /* For now, we'll detect CPUs using CPUID if ACPI not found */
    (void)sig;
    return NULL;
}

/* Detect CPUs using CPUID (fallback) */
static void detect_cpus_cpuid(void) {
    uint32_t eax, ebx, ecx, edx;

    /* Check if CPUID leaf 0x1 is supported */
    __asm__ volatile ("cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0));

    if (eax >= 1) {
        __asm__ volatile ("cpuid"
            : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
            : "a"(1));

        /* EBX[23:16] contains initial APIC ID */
        uint8_t initial_apic_id = (ebx >> 24) & 0xFF;

        /* EBX[23:16] logical processor count (if HTT supported) */
        uint8_t logical_count = (ebx >> 16) & 0xFF;

        /* Check for HTT (Hyper-Threading) support */
        if (edx & (1 << 28)) {  /* HTT bit */
            if (logical_count > 1) {
                detected_cpu_count = logical_count;
            } else {
                detected_cpu_count = 1;
            }
        } else {
            detected_cpu_count = 1;
        }

        /* Record BSP APIC ID */
        detected_apic_ids[0] = initial_apic_id;

        /* Assume sequential APIC IDs for other CPUs */
        for (uint32_t i = 1; i < detected_cpu_count && i < MAX_CPUS; i++) {
            detected_apic_ids[i] = initial_apic_id + i;
        }

        kprintf("[SMP] Detected %d CPU(s) via CPUID (BSP APIC ID: %d)\n",
                detected_cpu_count, initial_apic_id);
    }
}

/* Copy AP trampoline to low memory */
static void setup_ap_trampoline(void) {
    /* The trampoline must be in the first 1MB (16-bit real mode accessible) */
    uint64_t trampoline_addr = SMP_TRAMPOLINE_ADDR;
    size_t trampoline_size = (size_t)(ap_trampoline_end - ap_trampoline_start);

    /* Map and copy trampoline code */
    vmm_map_page(trampoline_addr, trampoline_addr, PTE_WRITABLE);
    memcpy((void *)trampoline_addr, ap_trampoline_start, trampoline_size);

    kprintf("[SMP] AP trampoline at 0x%lx (%d bytes)\n",
            trampoline_addr, (int)trampoline_size);
}

/* Initialize SMP subsystem */
void smp_init(void) {
    kprintf("[SMP] Initializing SMP subsystem...\n");

    /* Initialize Local APIC on BSP */
    lapic_init();

    /* Initialize I/O APIC */
    ioapic_init();

    /* Try to find ACPI MADT */
    madt_header_t *madt = find_acpi_table(ACPI_SIG_MADT);
    if (madt) {
        parse_madt(madt);
    } else {
        /* Fallback to CPUID detection */
        detect_cpus_cpuid();
    }

    /* Record detected CPUs */
    for (uint32_t i = 0; i < detected_cpu_count && i < MAX_CPUS; i++) {
        cpus[i].id = i;
        cpus[i].apic_id = detected_apic_ids[i];
        cpus[i].state = (i == 0) ? CPU_ONLINE : CPU_OFFLINE;
        cpus[i].is_bsp = (i == 0);
    }
    cpu_count = detected_cpu_count;

    kprintf("[SMP] Total CPUs detected: %d\n", cpu_count);

    /* Setup AP trampoline if we have multiple CPUs */
    if (cpu_count > 1) {
        setup_ap_trampoline();
    }
}

/* Start all Application Processors */
void smp_start_aps(void) {
    if (cpu_count <= 1) {
        kprintf("[SMP] Single CPU system, no APs to start\n");
        return;
    }

    kprintf("[SMP] Starting %d Application Processor(s)...\n", cpu_count - 1);

    for (uint32_t i = 1; i < cpu_count; i++) {
        uint32_t apic_id = cpus[i].apic_id;

        kprintf("[SMP] Starting CPU %d (APIC ID %d)...\n", i, apic_id);

        /* Mark as starting */
        cpus[i].state = CPU_STARTING;
        ap_started = 0;

        /* Send INIT IPI */
        lapic_send_init(apic_id);

        /* Send two SIPI (as per Intel spec) */
        /* Vector is trampoline address / 4KB */
        uint8_t vector = SMP_TRAMPOLINE_ADDR >> 12;
        lapic_send_sipi(apic_id, vector);
        lapic_send_sipi(apic_id, vector);

        /* Wait for AP to signal it's ready (with timeout) */
        uint32_t timeout = 100000;
        while (!ap_started && timeout > 0) {
            __asm__ volatile ("pause");
            timeout--;
        }

        if (ap_started) {
            cpus[i].state = CPU_ONLINE;
            kprintf("[SMP] CPU %d online\n", i);
        } else {
            kprintf("[SMP] CPU %d failed to start\n", i);
        }
    }

    /* Count online CPUs */
    uint32_t online = 0;
    for (uint32_t i = 0; i < cpu_count; i++) {
        if (cpus[i].state == CPU_ONLINE) {
            online++;
        }
    }

    kprintf("[SMP] %d/%d CPUs online\n", online, cpu_count);
}

/* AP initialization (called on each AP after startup) */
void ap_init(void) {
    /* Get our APIC ID */
    uint32_t apic_id = lapic_get_id();
    uint32_t cpu_id = 0;

    /* Find our CPU structure */
    for (uint32_t i = 0; i < cpu_count; i++) {
        if (cpus[i].apic_id == apic_id) {
            cpu_id = i;
            break;
        }
    }

    kprintf("[AP] CPU %d (APIC ID %d) initializing...\n", cpu_id, apic_id);

    /* Initialize local APIC */
    lapic_write(LAPIC_TPR, 0);
    lapic_write(LAPIC_SVR, SVR_ENABLE | 0xFF);

    /* Load GDT and IDT (AP needs its own GDT/TSS) */
    /* For now, use the same GDT as BSP */
    gdt_reload();

    /* Signal that we're ready */
    __asm__ volatile ("" ::: "memory");
    ap_started = 1;

    /* Enable interrupts and halt (will be woken by scheduler) */
    __asm__ volatile ("sti");

    kprintf("[AP] CPU %d ready, entering idle loop\n", cpu_id);

    /* Idle loop - wait for work */
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* Check if SMP is enabled */
int smp_enabled(void) {
    return cpu_count > 1;
}

/* Get number of CPUs online */
uint32_t smp_get_cpu_count(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < cpu_count; i++) {
        if (cpus[i].state == CPU_ONLINE) {
            count++;
        }
    }
    return count;
}

/* Send IPI to specific CPU */
void smp_send_ipi(uint32_t cpu_id, uint32_t vector) {
    if (cpu_id < cpu_count) {
        lapic_send_ipi(cpus[cpu_id].apic_id, vector);
    }
}

/* Broadcast IPI to all CPUs except self */
void smp_broadcast_ipi(uint32_t vector) {
    /* Wait for any previous IPI to complete */
    while (lapic_read(LAPIC_ICR_LO) & ICR_SEND_PENDING) {
        __asm__ volatile ("pause");
    }

    /* Send to all excluding self */
    lapic_write(LAPIC_ICR_LO, ICR_ALL_EXCLUDING | ICR_FIXED | ICR_ASSERT |
                ICR_EDGE | vector);
}
