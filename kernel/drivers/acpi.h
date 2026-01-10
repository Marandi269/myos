/*
 * acpi.h - ACPI (Advanced Configuration and Power Interface) Driver
 *
 * Provides basic ACPI support for power management (shutdown, reboot)
 */

#ifndef _ACPI_H
#define _ACPI_H

#include "types.h"

/* RSDP (Root System Description Pointer) signatures */
#define RSDP_SIGNATURE "RSD PTR "

/* ACPI table signatures */
#define RSDT_SIGNATURE "RSDT"
#define XSDT_SIGNATURE "XSDT"
#define FADT_SIGNATURE "FACP"
#define DSDT_SIGNATURE "DSDT"
#define MADT_SIGNATURE "APIC"

/* RSDP structure (ACPI 1.0) */
typedef struct {
    char signature[8];          /* "RSD PTR " */
    uint8_t checksum;
    char oem_id[6];
    uint8_t revision;           /* 0 = ACPI 1.0, 2 = ACPI 2.0+ */
    uint32_t rsdt_address;      /* Physical address of RSDT */
} __attribute__((packed)) acpi_rsdp_t;

/* RSDP structure (ACPI 2.0+) */
typedef struct {
    acpi_rsdp_t v1;             /* ACPI 1.0 part */
    uint32_t length;            /* Total length of table */
    uint64_t xsdt_address;      /* Physical address of XSDT (64-bit) */
    uint8_t ext_checksum;
    uint8_t reserved[3];
} __attribute__((packed)) acpi_rsdp2_t;

/* Generic ACPI table header */
typedef struct {
    char signature[4];
    uint32_t length;
    uint8_t revision;
    uint8_t checksum;
    char oem_id[6];
    char oem_table_id[8];
    uint32_t oem_revision;
    uint32_t creator_id;
    uint32_t creator_revision;
} __attribute__((packed)) acpi_header_t;

/* RSDT (Root System Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t entries[];         /* Array of 32-bit physical addresses */
} __attribute__((packed)) acpi_rsdt_t;

/* XSDT (Extended System Description Table) */
typedef struct {
    acpi_header_t header;
    uint64_t entries[];         /* Array of 64-bit physical addresses */
} __attribute__((packed)) acpi_xsdt_t;

/* Generic Address Structure */
typedef struct {
    uint8_t address_space;      /* 0 = system memory, 1 = system I/O */
    uint8_t bit_width;
    uint8_t bit_offset;
    uint8_t access_size;        /* 0 = undefined, 1 = byte, 2 = word, 3 = dword, 4 = qword */
    uint64_t address;
} __attribute__((packed)) acpi_gas_t;

/* Address space IDs */
#define ACPI_GAS_MEMORY     0
#define ACPI_GAS_IO         1

/* FADT (Fixed ACPI Description Table) */
typedef struct {
    acpi_header_t header;
    uint32_t firmware_ctrl;     /* Physical address of FACS */
    uint32_t dsdt;              /* Physical address of DSDT */
    uint8_t reserved1;
    uint8_t preferred_pm_profile;
    uint16_t sci_interrupt;
    uint32_t smi_command;       /* SMI command port */
    uint8_t acpi_enable;
    uint8_t acpi_disable;
    uint8_t s4bios_req;
    uint8_t pstate_control;
    uint32_t pm1a_event_block;
    uint32_t pm1b_event_block;
    uint32_t pm1a_control_block;
    uint32_t pm1b_control_block;
    uint32_t pm2_control_block;
    uint32_t pm_timer_block;
    uint32_t gpe0_block;
    uint32_t gpe1_block;
    uint8_t pm1_event_length;
    uint8_t pm1_control_length;
    uint8_t pm2_control_length;
    uint8_t pm_timer_length;
    uint8_t gpe0_length;
    uint8_t gpe1_length;
    uint8_t gpe1_base;
    uint8_t cstate_control;
    uint16_t worst_c2_latency;
    uint16_t worst_c3_latency;
    uint16_t flush_size;
    uint16_t flush_stride;
    uint8_t duty_offset;
    uint8_t duty_width;
    uint8_t day_alarm;
    uint8_t month_alarm;
    uint8_t century;
    uint16_t boot_arch_flags;   /* IA-PC Boot Architecture Flags */
    uint8_t reserved2;
    uint32_t flags;
    acpi_gas_t reset_reg;       /* Reset register */
    uint8_t reset_value;
    uint16_t arm_boot_arch;
    uint8_t fadt_minor_version;
    /* ACPI 2.0+ fields */
    uint64_t x_firmware_ctrl;
    uint64_t x_dsdt;
    acpi_gas_t x_pm1a_event_block;
    acpi_gas_t x_pm1b_event_block;
    acpi_gas_t x_pm1a_control_block;
    acpi_gas_t x_pm1b_control_block;
    acpi_gas_t x_pm2_control_block;
    acpi_gas_t x_pm_timer_block;
    acpi_gas_t x_gpe0_block;
    acpi_gas_t x_gpe1_block;
    acpi_gas_t sleep_control_reg;
    acpi_gas_t sleep_status_reg;
} __attribute__((packed)) acpi_fadt_t;

/* FADT flags */
#define ACPI_FADT_RESET_REG_SUP     (1 << 10)  /* Reset register supported */

/* PM1 Control Register bits */
#define ACPI_PM1_SCI_EN         (1 << 0)    /* SCI Enable */
#define ACPI_PM1_BM_RLD         (1 << 1)    /* Bus Master Reload */
#define ACPI_PM1_GBL_RLS        (1 << 2)    /* Global Lock Release */
#define ACPI_PM1_SLP_TYP(x)     ((x) << 10) /* Sleep Type (bits 10-12) */
#define ACPI_PM1_SLP_EN         (1 << 13)   /* Sleep Enable */

/* Sleep states */
#define ACPI_SLEEP_S0           0   /* Working */
#define ACPI_SLEEP_S1           1   /* Sleeping with processor context maintained */
#define ACPI_SLEEP_S2           2   /* Sleeping without processor context */
#define ACPI_SLEEP_S3           3   /* Suspend to RAM */
#define ACPI_SLEEP_S4           4   /* Suspend to Disk (Hibernate) */
#define ACPI_SLEEP_S5           5   /* Soft Off (Shutdown) */

/* Function declarations */
int acpi_init(void);
void acpi_shutdown(void);
void acpi_reboot(void);

/* Get ACPI info */
bool acpi_is_available(void);
const char *acpi_get_oem_id(void);

#endif /* _ACPI_H */
