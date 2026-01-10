/*
 * acpi.c - ACPI Driver Implementation
 *
 * Provides basic ACPI support for power management
 */

#include "drivers/acpi.h"
#include "lib/kprintf.h"
#include "lib/string.h"

/* I/O port functions */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ACPI state */
static bool acpi_available = false;
static acpi_rsdp2_t *rsdp = NULL;
static acpi_rsdt_t *rsdt = NULL;
static acpi_xsdt_t *xsdt = NULL;
static acpi_fadt_t *fadt = NULL;
static char oem_id[7] = {0};

/* S5 sleep type values (from DSDT \_S5 object) */
static uint16_t slp_typa = 0;
static uint16_t slp_typb = 0;
static bool s5_found = false;

/* Validate ACPI table checksum */
static bool acpi_validate_checksum(void *table, size_t length) {
    uint8_t sum = 0;
    uint8_t *ptr = (uint8_t *)table;

    for (size_t i = 0; i < length; i++) {
        sum += ptr[i];
    }

    return sum == 0;
}

/* Find RSDP in memory range */
static acpi_rsdp_t *acpi_find_rsdp_in_range(uint64_t start, uint64_t end) {
    /* Search on 16-byte boundaries */
    for (uint64_t addr = start; addr < end; addr += 16) {
        acpi_rsdp_t *rsdp = (acpi_rsdp_t *)addr;

        /* Check signature */
        if (memcmp(rsdp->signature, RSDP_SIGNATURE, 8) != 0) {
            continue;
        }

        /* Validate ACPI 1.0 checksum */
        if (!acpi_validate_checksum(rsdp, sizeof(acpi_rsdp_t))) {
            continue;
        }

        /* If ACPI 2.0+, validate extended checksum */
        if (rsdp->revision >= 2) {
            acpi_rsdp2_t *rsdp2 = (acpi_rsdp2_t *)rsdp;
            if (!acpi_validate_checksum(rsdp2, rsdp2->length)) {
                continue;
            }
        }

        return rsdp;
    }

    return NULL;
}

/* Find RSDP */
static acpi_rsdp_t *acpi_find_rsdp(void) {
    acpi_rsdp_t *found = NULL;

    /* Search Extended BIOS Data Area (EBDA) */
    uint16_t ebda_seg = *(uint16_t *)0x40E;
    uint64_t ebda_addr = (uint64_t)ebda_seg << 4;
    if (ebda_addr) {
        found = acpi_find_rsdp_in_range(ebda_addr, ebda_addr + 1024);
        if (found) return found;
    }

    /* Search BIOS ROM area (0xE0000 - 0xFFFFF) */
    found = acpi_find_rsdp_in_range(0xE0000, 0x100000);

    return found;
}

/* Find ACPI table by signature */
static acpi_header_t *acpi_find_table(const char *signature) {
    if (xsdt) {
        /* Use XSDT (64-bit addresses) */
        int entries = (xsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint64_t);
        for (int i = 0; i < entries; i++) {
            acpi_header_t *table = (acpi_header_t *)xsdt->entries[i];
            if (memcmp(table->signature, signature, 4) == 0) {
                return table;
            }
        }
    } else if (rsdt) {
        /* Use RSDT (32-bit addresses) */
        int entries = (rsdt->header.length - sizeof(acpi_header_t)) / sizeof(uint32_t);
        for (int i = 0; i < entries; i++) {
            acpi_header_t *table = (acpi_header_t *)(uint64_t)rsdt->entries[i];
            if (memcmp(table->signature, signature, 4) == 0) {
                return table;
            }
        }
    }

    return NULL;
}

/* Parse DSDT to find \_S5 sleep type values */
static void acpi_parse_s5(acpi_header_t *dsdt) {
    if (!dsdt) return;

    /* Search for "_S5_" in DSDT AML bytecode */
    uint8_t *ptr = (uint8_t *)dsdt;
    uint8_t *end = ptr + dsdt->length;

    while (ptr < end - 5) {
        /* Look for "_S5_" name */
        if (memcmp(ptr, "_S5_", 4) == 0 || memcmp(ptr, "\\_S5_", 4) == 0) {
            /* Skip name */
            if (*ptr == '\\') ptr++;
            ptr += 4;

            /* Skip if DefPackage follows (0x12 = PackageOp) */
            if (*ptr == 0x12) {
                ptr++;  /* Skip PackageOp */

                /* Skip PkgLength (can be 1-4 bytes) */
                uint8_t len_byte = *ptr++;
                if (len_byte & 0xC0) {
                    ptr += (len_byte >> 6);
                }

                /* Skip NumElements */
                ptr++;

                /* Read SLP_TYPa */
                if (*ptr == 0x0A) {  /* BytePrefix */
                    ptr++;
                    slp_typa = *ptr++;
                } else if (*ptr < 0x40) {  /* Small integer */
                    slp_typa = *ptr++;
                }

                /* Read SLP_TYPb */
                if (*ptr == 0x0A) {
                    ptr++;
                    slp_typb = *ptr++;
                } else if (*ptr < 0x40) {
                    slp_typb = *ptr++;
                }

                s5_found = true;
                kprintf("[ACPI] Found \\_S5: SLP_TYPa=%d SLP_TYPb=%d\n", slp_typa, slp_typb);
                return;
            }
        }
        ptr++;
    }

    /* If not found, use common default values */
    kprintf("[ACPI] \\_S5 not found in DSDT, using defaults\n");
    slp_typa = 5;  /* Common S5 value */
    slp_typb = 0;
    s5_found = true;
}

/* Enable ACPI mode if running in legacy mode */
static void acpi_enable(void) {
    if (!fadt) return;

    /* Check if already in ACPI mode */
    if (fadt->smi_command == 0 || fadt->acpi_enable == 0) {
        return;  /* Already in ACPI mode or not supported */
    }

    /* Check if SCI_EN is already set */
    uint16_t pm1_ctrl = inw(fadt->pm1a_control_block);
    if (pm1_ctrl & ACPI_PM1_SCI_EN) {
        return;  /* Already enabled */
    }

    /* Send ACPI enable command to SMI command port */
    outb(fadt->smi_command, fadt->acpi_enable);

    /* Wait for ACPI to be enabled */
    for (int i = 0; i < 3000; i++) {
        pm1_ctrl = inw(fadt->pm1a_control_block);
        if (pm1_ctrl & ACPI_PM1_SCI_EN) {
            kprintf("[ACPI] ACPI mode enabled\n");
            return;
        }
        /* Small delay */
        for (volatile int j = 0; j < 1000; j++);
    }

    kprintf("[ACPI] Warning: Failed to enable ACPI mode\n");
}

/* Initialize ACPI */
int acpi_init(void) {
    kprintf("[ACPI] Initializing ACPI...\n");

    /* Find RSDP */
    acpi_rsdp_t *rsdp1 = acpi_find_rsdp();
    if (!rsdp1) {
        kprintf("[ACPI] RSDP not found\n");
        return -1;
    }

    /* Copy OEM ID */
    memcpy(oem_id, rsdp1->oem_id, 6);
    oem_id[6] = '\0';

    kprintf("[ACPI] RSDP found at 0x%p, revision %d, OEM: %s\n",
            rsdp1, rsdp1->revision, oem_id);

    /* Get RSDT/XSDT */
    if (rsdp1->revision >= 2) {
        rsdp = (acpi_rsdp2_t *)rsdp1;
        if (rsdp->xsdt_address) {
            xsdt = (acpi_xsdt_t *)rsdp->xsdt_address;
            kprintf("[ACPI] Using XSDT at 0x%lx\n", rsdp->xsdt_address);
        }
    }

    if (!xsdt) {
        rsdt = (acpi_rsdt_t *)(uint64_t)rsdp1->rsdt_address;
        kprintf("[ACPI] Using RSDT at 0x%x\n", rsdp1->rsdt_address);
    }

    /* Find FADT */
    fadt = (acpi_fadt_t *)acpi_find_table(FADT_SIGNATURE);
    if (!fadt) {
        kprintf("[ACPI] FADT not found\n");
        return -1;
    }

    kprintf("[ACPI] FADT found, PM1a control block: 0x%x\n", fadt->pm1a_control_block);

    /* Parse DSDT for S5 values */
    acpi_header_t *dsdt = NULL;
    if (fadt->header.length >= 148 && fadt->x_dsdt) {
        dsdt = (acpi_header_t *)fadt->x_dsdt;
    } else {
        dsdt = (acpi_header_t *)(uint64_t)fadt->dsdt;
    }

    if (dsdt) {
        kprintf("[ACPI] DSDT at 0x%p, length %d\n", dsdt, dsdt->length);
        acpi_parse_s5(dsdt);
    }

    /* Enable ACPI mode */
    acpi_enable();

    acpi_available = true;
    kprintf("[ACPI] Initialization complete\n");

    return 0;
}

/* Shutdown (enter S5 state) */
void acpi_shutdown(void) {
    if (!acpi_available || !fadt) {
        kprintf("[ACPI] Shutdown not available, trying fallback...\n");

        /* Fallback: try keyboard controller reset */
        outb(0x64, 0xFE);  /* Pulse CPU reset line */

        /* Fallback: try QEMU shutdown port */
        outw(0x604, 0x2000);

        /* If all else fails, halt */
        kprintf("[ACPI] Halting CPU...\n");
        __asm__ volatile("cli; hlt");
        return;
    }

    kprintf("[ACPI] Entering S5 (shutdown) state...\n");

    /* Write SLP_TYP and SLP_EN to PM1a control register */
    uint16_t pm1_ctrl = inw(fadt->pm1a_control_block);
    pm1_ctrl &= ~(7 << 10);  /* Clear SLP_TYP bits */
    pm1_ctrl |= ACPI_PM1_SLP_TYP(slp_typa) | ACPI_PM1_SLP_EN;
    outw(fadt->pm1a_control_block, pm1_ctrl);

    /* If PM1b exists, write there too */
    if (fadt->pm1b_control_block) {
        pm1_ctrl = inw(fadt->pm1b_control_block);
        pm1_ctrl &= ~(7 << 10);
        pm1_ctrl |= ACPI_PM1_SLP_TYP(slp_typb) | ACPI_PM1_SLP_EN;
        outw(fadt->pm1b_control_block, pm1_ctrl);
    }

    /* If we reach here, something went wrong */
    kprintf("[ACPI] Shutdown failed, trying QEMU fallback...\n");
    outw(0x604, 0x2000);  /* QEMU specific */

    /* Halt */
    __asm__ volatile("cli; hlt");
}

/* Reboot system */
void acpi_reboot(void) {
    kprintf("[ACPI] Rebooting...\n");

    /* Try ACPI reset register first */
    if (acpi_available && fadt &&
        (fadt->flags & ACPI_FADT_RESET_REG_SUP) &&
        fadt->reset_reg.address) {

        if (fadt->reset_reg.address_space == ACPI_GAS_IO) {
            outb((uint16_t)fadt->reset_reg.address, fadt->reset_value);
        } else if (fadt->reset_reg.address_space == ACPI_GAS_MEMORY) {
            *(volatile uint8_t *)fadt->reset_reg.address = fadt->reset_value;
        }

        /* Wait a bit */
        for (volatile int i = 0; i < 1000000; i++);
    }

    /* Fallback: keyboard controller reset */
    kprintf("[ACPI] Trying keyboard controller reset...\n");

    /* Wait for keyboard controller to be ready */
    while (inb(0x64) & 0x02);

    /* Send reset command */
    outb(0x64, 0xFE);

    /* Wait a bit */
    for (volatile int i = 0; i < 1000000; i++);

    /* If that failed, try triple fault */
    kprintf("[ACPI] Trying triple fault...\n");
    __asm__ volatile(
        "lidt (%%rax)"
        : : "a"(0)
    );
    __asm__ volatile("int $0");

    /* Should not reach here */
    __asm__ volatile("cli; hlt");
}

/* Check if ACPI is available */
bool acpi_is_available(void) {
    return acpi_available;
}

/* Get OEM ID */
const char *acpi_get_oem_id(void) {
    return oem_id;
}
