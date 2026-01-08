/*
 * xhci.c - xHCI (USB 3.x) host controller driver
 */

#include "xhci.h"
#include "usb.h"
#include "../pci.h"
#include "../../lib/kprintf.h"
#include "../../lib/string.h"
#include "../../mm/pmm.h"
#include "../../mm/vmm.h"
#include "../../mm/heap.h"

/* xHCI controller instance */
static xhci_controller_t xhci;
static usb_hcd_t xhci_hcd;

/* Command ring size */
#define CMD_RING_SIZE   256
#define EVENT_RING_SIZE 256
#define TRANSFER_RING_SIZE 256

/* TRB flags */
#define TRB_CYCLE       (1 << 0)
#define TRB_ENT         (1 << 1)    /* Evaluate Next TRB */
#define TRB_ISP         (1 << 2)    /* Interrupt on Short Packet */
#define TRB_NO_SNOOP    (1 << 3)
#define TRB_CHAIN       (1 << 4)
#define TRB_IOC         (1 << 5)    /* Interrupt on Completion */
#define TRB_IDT         (1 << 6)    /* Immediate Data */
#define TRB_DIR_IN      (1 << 16)   /* Direction: IN */

/* Read capability register */
static uint32_t xhci_cap_read32(uint32_t reg) {
    return *(volatile uint32_t *)(xhci.cap_base + reg);
}

/* Read/write operational register */
static uint32_t xhci_op_read32(uint32_t reg) {
    return *(volatile uint32_t *)(xhci.op_base + reg);
}

static void xhci_op_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(xhci.op_base + reg) = val;
}

static uint64_t xhci_op_read64(uint32_t reg) {
    return *(volatile uint64_t *)(xhci.op_base + reg);
}

static void xhci_op_write64(uint32_t reg, uint64_t val) {
    *(volatile uint64_t *)(xhci.op_base + reg) = val;
}

/* Read/write doorbell */
static void xhci_doorbell(uint32_t slot, uint32_t val) {
    *(volatile uint32_t *)(xhci.db_base + slot * 4) = val;
}

/* Read/write runtime register */
static uint32_t xhci_rt_read32(uint32_t reg) {
    return *(volatile uint32_t *)(xhci.rt_base + reg);
}

static void xhci_rt_write32(uint32_t reg, uint32_t val) {
    *(volatile uint32_t *)(xhci.rt_base + reg) = val;
}

static uint64_t xhci_rt_read64(uint32_t reg) {
    return *(volatile uint64_t *)(xhci.rt_base + reg);
}

static void xhci_rt_write64(uint32_t reg, uint64_t val) {
    *(volatile uint64_t *)(xhci.rt_base + reg) = val;
}

/* Read port status */
static uint32_t xhci_port_read(int port) {
    return *(volatile uint32_t *)(xhci.op_base + 0x400 + port * 0x10);
}

static void xhci_port_write(int port, uint32_t val) {
    *(volatile uint32_t *)(xhci.op_base + 0x400 + port * 0x10) = val;
}

/* Delay */
static void xhci_delay(int count) {
    for (volatile int i = 0; i < count * 1000; i++);
}

/* Wait for controller not ready to clear */
static int xhci_wait_ready(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (!(xhci_op_read32(XHCI_OP_USBSTS) & XHCI_STS_CNR)) {
            return 0;
        }
        xhci_delay(1);
    }
    return -1;
}

/* Wait for controller halt */
static int xhci_wait_halt(void) {
    int timeout = 100000;
    while (timeout-- > 0) {
        if (xhci_op_read32(XHCI_OP_USBSTS) & XHCI_STS_HCH) {
            return 0;
        }
        xhci_delay(1);
    }
    return -1;
}

/* Allocate aligned memory for xHCI structures */
static void *xhci_alloc(size_t size, size_t align, uint64_t *phys) {
    (void)align;
    size_t pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    void *virt = NULL;
    uint64_t paddr = 0;

    for (size_t i = 0; i < pages; i++) {
        uint64_t page = (uint64_t)pmm_alloc_page();
        if (!page) return NULL;

        if (i == 0) {
            paddr = page;
            virt = (void *)page;
        }

        memset((void *)page, 0, PAGE_SIZE);
    }

    if (phys) *phys = paddr;
    return virt;
}

/* Queue a TRB to the command ring */
static void xhci_queue_command(uint64_t param, uint32_t status, uint32_t control) {
    xhci_trb_t *trb = &xhci.cmd_ring[xhci.cmd_enqueue];

    trb->param = param;
    trb->status = status;
    trb->control = control | (xhci.cmd_cycle ? TRB_CYCLE : 0);

    xhci.cmd_enqueue++;
    if (xhci.cmd_enqueue >= CMD_RING_SIZE - 1) {
        /* Link TRB to wrap around */
        xhci_trb_t *link = &xhci.cmd_ring[CMD_RING_SIZE - 1];
        link->param = xhci.cmd_ring_phys;
        link->status = 0;
        link->control = (TRB_TYPE_LINK << 10) | TRB_CYCLE;
        xhci.cmd_enqueue = 0;
        xhci.cmd_cycle ^= 1;
    }
}

/* Wait for command completion event */
static int xhci_wait_event(uint32_t *completion_code, uint32_t *slot_id) {
    int timeout = 100000;

    while (timeout-- > 0) {
        xhci_trb_t *event = &xhci.event_ring[xhci.event_dequeue];
        uint32_t cycle = event->control & TRB_CYCLE;

        if (cycle == xhci.event_cycle) {
            uint32_t trb_type = (event->control >> 10) & 0x3F;

            if (trb_type == TRB_TYPE_COMMAND_COMP) {
                if (completion_code) {
                    *completion_code = (event->status >> 24) & 0xFF;
                }
                if (slot_id) {
                    *slot_id = (event->control >> 24) & 0xFF;
                }

                /* Advance dequeue pointer */
                xhci.event_dequeue++;
                if (xhci.event_dequeue >= EVENT_RING_SIZE) {
                    xhci.event_dequeue = 0;
                    xhci.event_cycle ^= 1;
                }

                /* Update ERDP */
                uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
                xhci_rt_write64(0x38, erdp | (1 << 3));  /* EHB bit */

                return 0;
            } else if (trb_type == TRB_TYPE_TRANSFER) {
                /* Transfer event */
                if (completion_code) {
                    *completion_code = (event->status >> 24) & 0xFF;
                }

                xhci.event_dequeue++;
                if (xhci.event_dequeue >= EVENT_RING_SIZE) {
                    xhci.event_dequeue = 0;
                    xhci.event_cycle ^= 1;
                }

                uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
                xhci_rt_write64(0x38, erdp | (1 << 3));

                return 0;
            } else if (trb_type == TRB_TYPE_PORT_STATUS) {
                /* Port status change - skip */
                xhci.event_dequeue++;
                if (xhci.event_dequeue >= EVENT_RING_SIZE) {
                    xhci.event_dequeue = 0;
                    xhci.event_cycle ^= 1;
                }
                uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
                xhci_rt_write64(0x38, erdp | (1 << 3));
                continue;
            }
        }
        xhci_delay(1);
    }

    return -1;
}

/* Send Enable Slot command */
static int xhci_enable_slot(uint32_t *slot_id) {
    xhci_queue_command(0, 0, (TRB_TYPE_ENABLE_SLOT << 10));
    xhci_doorbell(0, 0);  /* Ring command doorbell */

    uint32_t cc;
    if (xhci_wait_event(&cc, slot_id) < 0) {
        kprintf("[xHCI] Enable slot timeout\n");
        return -1;
    }

    if (cc != TRB_COMP_SUCCESS) {
        kprintf("[xHCI] Enable slot failed: %d\n", cc);
        return -1;
    }

    kprintf("[xHCI] Enabled slot %d\n", *slot_id);
    return 0;
}

/* Initialize a slot for a device */
static int xhci_init_slot(uint32_t slot_id, uint32_t port, uint32_t speed) {
    xhci_slot_t *slot = &xhci.slots[slot_id];

    memset(slot, 0, sizeof(xhci_slot_t));
    slot->slot_id = slot_id;
    slot->port = port;
    slot->speed = speed;

    /* Allocate device context */
    slot->dev_ctx = xhci_alloc(sizeof(xhci_dev_ctx_t), 64, &slot->dev_ctx_phys);
    if (!slot->dev_ctx) {
        kprintf("[xHCI] Failed to allocate device context\n");
        return -1;
    }

    /* Allocate input context */
    slot->input_ctx = xhci_alloc(sizeof(xhci_input_ctx_t), 64, &slot->input_ctx_phys);
    if (!slot->input_ctx) {
        kprintf("[xHCI] Failed to allocate input context\n");
        return -1;
    }

    /* Allocate transfer ring for EP0 (control endpoint) */
    slot->ep_ring[0] = xhci_alloc(TRANSFER_RING_SIZE * sizeof(xhci_trb_t), 64, &slot->ep_ring_phys[0]);
    if (!slot->ep_ring[0]) {
        kprintf("[xHCI] Failed to allocate EP0 transfer ring\n");
        return -1;
    }
    slot->ep_enqueue[0] = 0;
    slot->ep_cycle[0] = 1;

    /* Set DCBAA entry */
    xhci.dcbaa[slot_id] = slot->dev_ctx_phys;

    /* Setup input context */
    xhci_input_ctx_t *input = slot->input_ctx;

    /* Add slot and EP0 */
    input->ctrl.add_flags = (1 << 0) | (1 << 1);  /* Slot + EP0 */
    input->ctrl.drop_flags = 0;

    /* Slot context */
    uint32_t route_string = 0;
    uint32_t ctx_entries = 1;  /* Only EP0 for now */

    /* Speed mapping for slot context */
    uint32_t slot_speed;
    switch (speed) {
    case XHCI_SPEED_LOW:   slot_speed = 2; break;
    case XHCI_SPEED_FULL:  slot_speed = 1; break;
    case XHCI_SPEED_HIGH:  slot_speed = 3; break;
    case XHCI_SPEED_SUPER: slot_speed = 4; break;
    default:               slot_speed = 3; break;
    }

    input->slot.info1 = (ctx_entries << 27) | (slot_speed << 20) | route_string;
    input->slot.info2 = (port + 1) << 16;  /* Root hub port number (1-based) */

    /* EP0 context */
    uint32_t max_packet;
    switch (speed) {
    case XHCI_SPEED_LOW:
    case XHCI_SPEED_FULL:  max_packet = 8; break;
    case XHCI_SPEED_HIGH:  max_packet = 64; break;
    case XHCI_SPEED_SUPER: max_packet = 512; break;
    default:               max_packet = 64; break;
    }

    /* EP type: Control = 4, Error count = 3 */
    input->ep[0].info1 = 0;
    input->ep[0].info2 = (max_packet << 16) | (4 << 3) | (3 << 1);
    input->ep[0].deq_ptr = slot->ep_ring_phys[0] | slot->ep_cycle[0];
    input->ep[0].tx_info = 8;  /* Average TRB length */

    slot->enabled = 1;
    return 0;
}

/* Address Device command */
static int xhci_address_device(uint32_t slot_id, int bsr) {
    xhci_slot_t *slot = &xhci.slots[slot_id];

    uint32_t control = (TRB_TYPE_ADDRESS_DEV << 10) | (slot_id << 24);
    if (bsr) {
        control |= (1 << 9);  /* Block Set Address Request */
    }

    xhci_queue_command(slot->input_ctx_phys, 0, control);
    xhci_doorbell(0, 0);

    uint32_t cc;
    if (xhci_wait_event(&cc, NULL) < 0) {
        kprintf("[xHCI] Address device timeout\n");
        return -1;
    }

    if (cc != TRB_COMP_SUCCESS) {
        kprintf("[xHCI] Address device failed: %d\n", cc);
        return -1;
    }

    return 0;
}

/* Queue a TRB to a transfer ring */
static void xhci_queue_transfer(xhci_slot_t *slot, int ep_idx,
                                 uint64_t param, uint32_t status, uint32_t control) {
    xhci_trb_t *ring = slot->ep_ring[ep_idx];
    uint32_t idx = slot->ep_enqueue[ep_idx];

    xhci_trb_t *trb = &ring[idx];
    trb->param = param;
    trb->status = status;
    trb->control = control | (slot->ep_cycle[ep_idx] ? TRB_CYCLE : 0);

    slot->ep_enqueue[ep_idx]++;
    if (slot->ep_enqueue[ep_idx] >= TRANSFER_RING_SIZE - 1) {
        /* Link TRB */
        xhci_trb_t *link = &ring[TRANSFER_RING_SIZE - 1];
        link->param = slot->ep_ring_phys[ep_idx];
        link->status = 0;
        link->control = (TRB_TYPE_LINK << 10) | (slot->ep_cycle[ep_idx] ? TRB_CYCLE : 0);
        slot->ep_enqueue[ep_idx] = 0;
        slot->ep_cycle[ep_idx] ^= 1;
    }
}

/* Wait for transfer completion on a specific slot/endpoint */
static int xhci_wait_transfer(uint32_t *completion_code, int *transferred) {
    int timeout = 50000;  /* Shorter timeout for transfers */

    while (timeout-- > 0) {
        xhci_trb_t *event = &xhci.event_ring[xhci.event_dequeue];
        uint32_t cycle = event->control & TRB_CYCLE;

        if (cycle == xhci.event_cycle) {
            uint32_t trb_type = (event->control >> 10) & 0x3F;

            if (trb_type == TRB_TYPE_TRANSFER) {
                if (completion_code) {
                    *completion_code = (event->status >> 24) & 0xFF;
                }
                if (transferred) {
                    /* Get transfer length from status field */
                    *transferred = event->status & 0xFFFFFF;
                }

                /* Advance dequeue pointer */
                xhci.event_dequeue++;
                if (xhci.event_dequeue >= EVENT_RING_SIZE) {
                    xhci.event_dequeue = 0;
                    xhci.event_cycle ^= 1;
                }

                /* Update ERDP with EHB bit to clear interrupt */
                uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
                xhci_rt_write64(0x38, erdp | (1 << 3));

                return 0;
            } else if (trb_type == TRB_TYPE_PORT_STATUS) {
                /* Port status change - skip and continue */
                xhci.event_dequeue++;
                if (xhci.event_dequeue >= EVENT_RING_SIZE) {
                    xhci.event_dequeue = 0;
                    xhci.event_cycle ^= 1;
                }
                uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
                xhci_rt_write64(0x38, erdp | (1 << 3));
                continue;
            } else {
                /* Other event types - skip */
                xhci.event_dequeue++;
                if (xhci.event_dequeue >= EVENT_RING_SIZE) {
                    xhci.event_dequeue = 0;
                    xhci.event_cycle ^= 1;
                }
                uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
                xhci_rt_write64(0x38, erdp | (1 << 3));
                continue;
            }
        }
        xhci_delay(1);
    }

    return -1;
}

/* Sync software enqueue with hardware dequeue */
static void xhci_sync_ep_ring(xhci_slot_t *slot, int ep_idx) {
    xhci_dev_ctx_t *ctx = slot->dev_ctx;
    uint64_t hw_deq = ctx->ep[ep_idx].deq_ptr;
    uint32_t hw_cycle = hw_deq & 1;
    uint64_t hw_ptr = hw_deq & ~0xF;

    /* Calculate TRB index from physical address */
    if (hw_ptr >= slot->ep_ring_phys[ep_idx]) {
        uint64_t offset = hw_ptr - slot->ep_ring_phys[ep_idx];
        uint32_t idx = offset / sizeof(xhci_trb_t);

        if (idx < TRANSFER_RING_SIZE) {
            slot->ep_enqueue[ep_idx] = idx;
            slot->ep_cycle[ep_idx] = hw_cycle;
        }
    }
}

/* Control transfer implementation */
static int xhci_do_control_transfer(uint32_t slot_id, usb_setup_packet_t *setup,
                                     void *data, int data_len) {
    xhci_slot_t *slot = &xhci.slots[slot_id];
    int has_data = (data_len > 0) && (data != NULL);
    int dir_in = (setup->bmRequestType & 0x80) ? 1 : 0;

    if (!slot->enabled || !slot->ep_ring[0]) {
        kprintf("[xHCI] Slot %d not initialized\n", slot_id);
        return -1;
    }

    /* Sync with hardware EP state before each transfer */
    xhci_sync_ep_ring(slot, 0);

    /* Allocate DMA buffer for data if needed */
    uint64_t data_phys = 0;
    void *data_buf = NULL;
    if (has_data) {
        data_buf = xhci_alloc(data_len, 64, &data_phys);
        if (!data_buf) {
            return -1;
        }
        if (!dir_in) {
            memcpy(data_buf, data, data_len);
        }
    }

    /* Setup TRB - no IOC, we wait for Status stage */
    uint64_t setup_data = 0;
    memcpy(&setup_data, setup, 8);

    uint32_t setup_control = (TRB_TYPE_SETUP << 10) | TRB_IDT;
    if (has_data) {
        setup_control |= (dir_in ? 3 : 2) << 16;  /* TRT: IN=3, OUT=2 */
    } else {
        setup_control |= (0 << 16);  /* TRT: No Data */
    }

    xhci_queue_transfer(slot, 0, setup_data, 8, setup_control);

    /* Data TRB (if any) - no IOC */
    if (has_data) {
        uint32_t data_control = (TRB_TYPE_DATA << 10);
        if (dir_in) {
            data_control |= TRB_DIR_IN;
        }
        xhci_queue_transfer(slot, 0, data_phys, data_len, data_control);
    }

    /* Status TRB - with IOC */
    uint32_t status_control = (TRB_TYPE_STATUS << 10) | TRB_IOC;
    if (!has_data || !dir_in) {
        status_control |= TRB_DIR_IN;  /* Status stage direction opposite to data */
    }
    xhci_queue_transfer(slot, 0, 0, 0, status_control);

    /* Memory barrier to ensure TRBs are written before doorbell */
    __asm__ volatile ("mfence" ::: "memory");

    /* Ring doorbell for EP0 (doorbell target = 1) */
    xhci_doorbell(slot_id, 1);

    /* Wait for transfer completion event */
    uint32_t cc;
    if (xhci_wait_transfer(&cc, NULL) < 0) {
        kprintf("[xHCI] Control transfer timeout\n");
        if (data_buf) pmm_free_page(data_buf);
        return -1;
    }

    if (cc != TRB_COMP_SUCCESS && cc != TRB_COMP_SHORT_PKT) {
        kprintf("[xHCI] Control transfer failed: %d\n", cc);
        if (data_buf) pmm_free_page(data_buf);
        return -1;
    }

    /* Copy data back for IN transfers */
    if (has_data && dir_in) {
        memcpy(data, data_buf, data_len);
    }

    if (data_buf) pmm_free_page(data_buf);
    return data_len;
}

/* HCD control transfer callback */
static int xhci_control_transfer(usb_hcd_t *hcd, usb_device_t *dev,
                                  usb_setup_packet_t *setup, void *data) {
    (void)hcd;

    if (!dev || dev->slot_id == 0) {
        kprintf("[xHCI] Invalid device for control transfer\n");
        return -1;
    }

    return xhci_do_control_transfer(dev->slot_id, setup, data, setup->wLength);
}

/* Enumerate device on a port */
static int xhci_enumerate_port(uint32_t port, uint32_t speed) {
    uint32_t slot_id;

    /* Enable a slot */
    if (xhci_enable_slot(&slot_id) < 0) {
        return -1;
    }

    /* Initialize slot structures */
    if (xhci_init_slot(slot_id, port, speed) < 0) {
        return -1;
    }

    /* Address device (BSR=1 first to get slot state) */
    if (xhci_address_device(slot_id, 0) < 0) {
        return -1;
    }

    kprintf("[xHCI] Device addressed, slot %d\n", slot_id);

    /* Create USB device */
    usb_device_t *dev = usb_alloc_device(&xhci_hcd);
    if (!dev) {
        return -1;
    }

    dev->port = port;
    dev->speed = speed;
    dev->slot_id = slot_id;
    dev->address = slot_id;  /* xHCI assigns address automatically */
    dev->state = USB_STATE_ADDRESS;

    /* Update max packet size based on speed */
    switch (speed) {
    case XHCI_SPEED_LOW:
    case XHCI_SPEED_FULL:  dev->ep0.max_packet = 8; break;
    case XHCI_SPEED_HIGH:  dev->ep0.max_packet = 64; break;
    case XHCI_SPEED_SUPER: dev->ep0.max_packet = 512; break;
    default:               dev->ep0.max_packet = 64; break;
    }

    xhci.slots[slot_id].usb_dev = dev;

    /* Now enumerate the device */
    if (usb_enumerate_device(dev) < 0) {
        kprintf("[xHCI] Device enumeration failed\n");
        return -1;
    }

    return 0;
}

/* Initialize command ring */
static int xhci_init_cmd_ring(void) {
    xhci.cmd_ring = xhci_alloc(CMD_RING_SIZE * sizeof(xhci_trb_t), 64, &xhci.cmd_ring_phys);
    if (!xhci.cmd_ring) {
        kprintf("[xHCI] Failed to allocate command ring\n");
        return -1;
    }

    xhci.cmd_enqueue = 0;
    xhci.cmd_cycle = 1;

    xhci_op_write64(XHCI_OP_CRCR, xhci.cmd_ring_phys | xhci.cmd_cycle);

    kprintf("[xHCI] Command ring at 0x%lx\n", xhci.cmd_ring_phys);
    return 0;
}

/* Initialize event ring */
static int xhci_init_event_ring(void) {
    xhci.event_ring = xhci_alloc(EVENT_RING_SIZE * sizeof(xhci_trb_t), 64, &xhci.event_ring_phys);
    if (!xhci.event_ring) {
        return -1;
    }

    xhci.erst = xhci_alloc(sizeof(xhci_erst_entry_t), 64, &xhci.erst_phys);
    if (!xhci.erst) {
        return -1;
    }

    xhci.erst[0].base = xhci.event_ring_phys;
    xhci.erst[0].size = EVENT_RING_SIZE;

    xhci.event_dequeue = 0;
    xhci.event_cycle = 1;

    /* Interrupter 0 registers (offset 0x20 from runtime base) */
    xhci_rt_write32(0x28, 1);                     /* ERSTSZ */
    xhci_rt_write64(0x30, xhci.erst_phys);        /* ERSTBA */
    xhci_rt_write64(0x38, xhci.event_ring_phys);  /* ERDP */

    kprintf("[xHCI] Event ring at 0x%lx\n", xhci.event_ring_phys);
    return 0;
}

/* Initialize DCBAA */
static int xhci_init_dcbaa(void) {
    size_t dcbaa_size = (xhci.max_slots + 1) * sizeof(uint64_t);
    xhci.dcbaa = xhci_alloc(dcbaa_size, 64, &xhci.dcbaa_phys);
    if (!xhci.dcbaa) {
        return -1;
    }

    xhci_op_write64(XHCI_OP_DCBAAP, xhci.dcbaa_phys);

    kprintf("[xHCI] DCBAA at 0x%lx (%d slots)\n", xhci.dcbaa_phys, xhci.max_slots);
    return 0;
}

/* Reset xHCI controller */
static int xhci_reset(void) {
    uint32_t cmd = xhci_op_read32(XHCI_OP_USBCMD);
    xhci_op_write32(XHCI_OP_USBCMD, cmd & ~XHCI_CMD_RUN);

    if (xhci_wait_halt() < 0) {
        kprintf("[xHCI] Failed to halt controller\n");
        return -1;
    }

    xhci_op_write32(XHCI_OP_USBCMD, XHCI_CMD_HCRST);

    int timeout = 100000;
    while (timeout-- > 0) {
        if (!(xhci_op_read32(XHCI_OP_USBCMD) & XHCI_CMD_HCRST)) {
            break;
        }
        xhci_delay(1);
    }

    if (timeout <= 0) {
        kprintf("[xHCI] Reset timeout\n");
        return -1;
    }

    if (xhci_wait_ready() < 0) {
        kprintf("[xHCI] Controller not ready after reset\n");
        return -1;
    }

    kprintf("[xHCI] Controller reset complete\n");
    return 0;
}

/* Start xHCI controller */
static int xhci_start(usb_hcd_t *hcd) {
    (void)hcd;

    xhci_op_write32(XHCI_OP_CONFIG, xhci.max_slots);

    uint32_t cmd = xhci_op_read32(XHCI_OP_USBCMD);
    cmd |= XHCI_CMD_RUN | XHCI_CMD_INTE;
    xhci_op_write32(XHCI_OP_USBCMD, cmd);

    int timeout = 100000;
    while (timeout-- > 0) {
        if (!(xhci_op_read32(XHCI_OP_USBSTS) & XHCI_STS_HCH)) {
            break;
        }
        xhci_delay(1);
    }

    if (timeout <= 0) {
        kprintf("[xHCI] Failed to start controller\n");
        return -1;
    }

    kprintf("[xHCI] Controller started\n");
    return 0;
}

/* Stop xHCI controller */
static void xhci_stop(usb_hcd_t *hcd) {
    (void)hcd;

    uint32_t cmd = xhci_op_read32(XHCI_OP_USBCMD);
    xhci_op_write32(XHCI_OP_USBCMD, cmd & ~XHCI_CMD_RUN);
    xhci_wait_halt();
}

/* Reset port */
static int xhci_reset_port(usb_hcd_t *hcd, int port) {
    (void)hcd;

    uint32_t status = xhci_port_read(port);

    if (!(status & XHCI_PORT_CCS)) {
        return -1;
    }

    /* Clear status change bits first */
    xhci_port_write(port, status | XHCI_PORT_CSC | XHCI_PORT_PEC | XHCI_PORT_PRC);
    xhci_delay(10);

    /* Reset port */
    status = xhci_port_read(port);
    xhci_port_write(port, (status & ~XHCI_PORT_PED) | XHCI_PORT_PR);

    /* Wait for reset complete */
    int timeout = 500;
    while (timeout-- > 0) {
        xhci_delay(10);
        status = xhci_port_read(port);
        if (status & XHCI_PORT_PRC) {
            xhci_port_write(port, status | XHCI_PORT_PRC);
            break;
        }
    }

    if (timeout <= 0) {
        kprintf("[xHCI] Port %d reset timeout\n", port);
        return -1;
    }

    xhci_delay(10);
    status = xhci_port_read(port);
    if (!(status & XHCI_PORT_PED)) {
        kprintf("[xHCI] Port %d not enabled after reset\n", port);
        return -1;
    }

    kprintf("[xHCI] Port %d reset complete, speed=%d\n", port, (status >> 10) & 0xF);
    return 0;
}

/* Bulk transfer */
static int xhci_bulk_transfer(usb_hcd_t *hcd, usb_device_t *dev,
                               usb_endpoint_t *ep, void *data, int len) {
    (void)hcd;
    (void)dev;
    (void)ep;
    (void)data;
    (void)len;
    return -1;  /* TODO */
}

/* Interrupt transfer */
static int xhci_int_transfer(usb_hcd_t *hcd, usb_device_t *dev,
                              usb_endpoint_t *ep, void *data, int len) {
    (void)hcd;
    (void)dev;
    (void)ep;
    (void)data;
    (void)len;
    return -1;  /* TODO */
}

/* Scan ports for connected devices */
static void xhci_scan_ports(void) {
    kprintf("[xHCI] Scanning %d ports...\n", xhci.max_ports);

    for (uint32_t i = 0; i < xhci.max_ports; i++) {
        uint32_t status = xhci_port_read(i);

        if (status & XHCI_PORT_CCS) {
            int speed = (status >> 10) & 0xF;
            const char *speed_str = "unknown";

            switch (speed) {
            case XHCI_SPEED_FULL:  speed_str = "Full (12 Mbps)"; break;
            case XHCI_SPEED_LOW:   speed_str = "Low (1.5 Mbps)"; break;
            case XHCI_SPEED_HIGH:  speed_str = "High (480 Mbps)"; break;
            case XHCI_SPEED_SUPER: speed_str = "Super (5 Gbps)"; break;
            }

            kprintf("[xHCI] Port %d: Device connected, speed=%s\n", i, speed_str);

            /* Reset and enumerate device */
            if (xhci_reset_port(&xhci_hcd, i) == 0) {
                xhci_enumerate_port(i, speed);
            }
        }
    }
}

/* Initialize xHCI controller */
int xhci_init(uint64_t mmio_base) {
    uint32_t hcs1, hcs2, hcc1;
    uint8_t cap_length;

    memset(&xhci, 0, sizeof(xhci));

    kprintf("[xHCI] Initializing controller at 0x%lx\n", mmio_base);

    /* Map MMIO region */
    for (int i = 0; i < 16; i++) {
        vmm_map_page(mmio_base + i * PAGE_SIZE,
                     mmio_base + i * PAGE_SIZE,
                     PTE_WRITABLE | PTE_PCD);
    }

    xhci.cap_base = mmio_base;

    /* Read capability registers */
    cap_length = xhci_cap_read32(XHCI_CAP_CAPLENGTH) & 0xFF;
    hcs1 = xhci_cap_read32(XHCI_CAP_HCSPARAMS1);
    hcs2 = xhci_cap_read32(XHCI_CAP_HCSPARAMS2);
    hcc1 = xhci_cap_read32(XHCI_CAP_HCCPARAMS1);

    xhci.max_slots = hcs1 & 0xFF;
    xhci.max_ports = (hcs1 >> 24) & 0xFF;
    xhci.context_size = (hcc1 & (1 << 2)) ? 64 : 32;

    xhci.op_base = mmio_base + cap_length;
    xhci.rt_base = mmio_base + (xhci_cap_read32(XHCI_CAP_RTSOFF) & ~0x1F);
    xhci.db_base = mmio_base + (xhci_cap_read32(XHCI_CAP_DBOFF) & ~0x3);

    kprintf("[xHCI] Max slots: %d, Max ports: %d\n", xhci.max_slots, xhci.max_ports);
    kprintf("[xHCI] HCS2=0x%x, HCC1=0x%x\n", hcs2, hcc1);

    xhci.page_size = xhci_op_read32(XHCI_OP_PAGESIZE);
    kprintf("[xHCI] Page size: %d\n", xhci.page_size << 12);

    if (xhci_reset() < 0) {
        return -1;
    }

    if (xhci_init_dcbaa() < 0) {
        return -1;
    }

    if (xhci_init_cmd_ring() < 0) {
        return -1;
    }

    if (xhci_init_event_ring() < 0) {
        return -1;
    }

    /* Setup HCD structure */
    xhci_hcd.name = "xHCI";
    xhci_hcd.priv = &xhci;
    xhci_hcd.start = xhci_start;
    xhci_hcd.stop = xhci_stop;
    xhci_hcd.reset_port = xhci_reset_port;
    xhci_hcd.control_transfer = xhci_control_transfer;
    xhci_hcd.bulk_transfer = xhci_bulk_transfer;
    xhci_hcd.int_transfer = xhci_int_transfer;

    /* Register HCD */
    usb_register_hcd(&xhci_hcd);

    /* Scan for connected devices */
    xhci_scan_ports();

    kprintf("[xHCI] Initialization complete\n");
    return 0;
}

/* Get xHCI HCD */
usb_hcd_t *xhci_get_hcd(void) {
    return &xhci_hcd;
}

/* Configure Endpoint command */
static int xhci_configure_ep_cmd(uint32_t slot_id) {
    xhci_slot_t *slot = &xhci.slots[slot_id];

    uint32_t control = (TRB_TYPE_CONFIG_EP << 10) | (slot_id << 24);
    xhci_queue_command(slot->input_ctx_phys, 0, control);
    xhci_doorbell(0, 0);

    uint32_t cc;
    if (xhci_wait_event(&cc, NULL) < 0) {
        kprintf("[xHCI] Configure endpoint timeout\n");
        return -1;
    }

    if (cc != TRB_COMP_SUCCESS) {
        kprintf("[xHCI] Configure endpoint failed: %d\n", cc);
        return -1;
    }

    return 0;
}

/* Setup interrupt endpoint */
int xhci_setup_interrupt_ep(uint32_t slot_id, uint8_t ep_addr,
                            uint16_t max_packet, uint8_t interval) {
    xhci_slot_t *slot = &xhci.slots[slot_id];

    if (!slot->enabled) {
        kprintf("[xHCI] Slot %d not enabled\n", slot_id);
        return -1;
    }

    /* Calculate endpoint index: IN endpoints are odd, OUT are even */
    /* ep_addr bit 7 = direction (1=IN), bits 0-3 = endpoint number */
    uint8_t ep_num = ep_addr & 0x0F;
    uint8_t ep_dir = (ep_addr & 0x80) ? 1 : 0;  /* 1 = IN */
    uint8_t ep_idx = ep_num * 2 + ep_dir;       /* DCI = ep_num*2 + dir */

    if (ep_idx >= 32) {
        kprintf("[xHCI] Invalid endpoint index %d\n", ep_idx);
        return -1;
    }

    kprintf("[xHCI] Setting up interrupt EP%d %s (DCI=%d)\n",
            ep_num, ep_dir ? "IN" : "OUT", ep_idx);

    /* Allocate transfer ring for this endpoint */
    if (!slot->ep_ring[ep_idx]) {
        slot->ep_ring[ep_idx] = xhci_alloc(TRANSFER_RING_SIZE * sizeof(xhci_trb_t),
                                            64, &slot->ep_ring_phys[ep_idx]);
        if (!slot->ep_ring[ep_idx]) {
            kprintf("[xHCI] Failed to allocate EP%d transfer ring\n", ep_idx);
            return -1;
        }
        slot->ep_enqueue[ep_idx] = 0;
        slot->ep_cycle[ep_idx] = 1;
    }

    /* Setup input context for Configure Endpoint */
    xhci_input_ctx_t *input = slot->input_ctx;
    memset(input, 0, sizeof(xhci_input_ctx_t));

    /* Add this endpoint (bit 0 = slot, bit 1 = EP0, bit 2 = EP1, etc) */
    input->ctrl.add_flags = (1 << 0) | (1 << (ep_idx + 1));
    input->ctrl.drop_flags = 0;

    /* Copy slot context from device context */
    memcpy(&input->slot, &slot->dev_ctx->slot, sizeof(xhci_slot_ctx_t));

    /* Update context entries to include this endpoint */
    uint32_t ctx_entries = ep_idx;
    input->slot.info1 = (input->slot.info1 & ~(0x1F << 27)) | (ctx_entries << 27);

    /* Setup endpoint context */
    /* EP type: 7=Interrupt IN, 3=Interrupt OUT */
    uint8_t ep_type = ep_dir ? 7 : 3;

    /* Calculate interval (xHCI uses 2^(interval-1) for HS/SS) */
    uint8_t xhci_interval = interval;
    if (slot->speed >= XHCI_SPEED_HIGH) {
        /* For HS/SS, interval is already in 125us units (2^(bInterval-1)) */
        xhci_interval = interval;
    } else {
        /* For LS/FS, convert ms to 125us frames */
        xhci_interval = interval + 3;
    }
    if (xhci_interval > 16) xhci_interval = 16;
    if (xhci_interval < 1) xhci_interval = 1;

    /* Endpoint context */
    /* info1: Interval, LSA=0, MaxPStreams=0, Mult=0, EPState=0 */
    input->ep[ep_idx - 1].info1 = (xhci_interval << 16);

    /* info2: MaxPacketSize, MaxBurstSize=0, EPType, CErr=3 */
    input->ep[ep_idx - 1].info2 = (max_packet << 16) | (ep_type << 3) | (3 << 1);

    /* Dequeue pointer with cycle bit */
    input->ep[ep_idx - 1].deq_ptr = slot->ep_ring_phys[ep_idx] | slot->ep_cycle[ep_idx];

    /* Average TRB length */
    input->ep[ep_idx - 1].tx_info = max_packet;

    /* Issue Configure Endpoint command */
    if (xhci_configure_ep_cmd(slot_id) < 0) {
        return -1;
    }

    kprintf("[xHCI] Interrupt EP%d configured\n", ep_num);
    return ep_idx;
}

/* Queue interrupt transfer TRB */
static void xhci_queue_interrupt_trb(xhci_slot_t *slot, int ep_idx,
                                      uint64_t buf_phys, uint32_t len) {
    xhci_trb_t *ring = slot->ep_ring[ep_idx];
    uint32_t idx = slot->ep_enqueue[ep_idx];

    xhci_trb_t *trb = &ring[idx];
    trb->param = buf_phys;
    trb->status = len;
    trb->control = (TRB_TYPE_NORMAL << 10) | TRB_IOC |
                   (slot->ep_cycle[ep_idx] ? TRB_CYCLE : 0);

    slot->ep_enqueue[ep_idx]++;
    if (slot->ep_enqueue[ep_idx] >= TRANSFER_RING_SIZE - 1) {
        /* Link TRB */
        xhci_trb_t *link = &ring[TRANSFER_RING_SIZE - 1];
        link->param = slot->ep_ring_phys[ep_idx];
        link->status = 0;
        link->control = (TRB_TYPE_LINK << 10) | TRB_CHAIN |
                        (slot->ep_cycle[ep_idx] ? TRB_CYCLE : 0);
        slot->ep_enqueue[ep_idx] = 0;
        slot->ep_cycle[ep_idx] ^= 1;
    }
}

/* Interrupt transfer data buffer */
static uint8_t int_transfer_buf[64] __attribute__((aligned(64)));
static uint64_t int_transfer_buf_phys = 0;
static int int_transfer_pending = 0;
static uint32_t int_transfer_slot = 0;
static uint32_t int_transfer_ep = 0;

/* Start an interrupt transfer (async) */
int xhci_start_interrupt_transfer(uint32_t slot_id, uint8_t ep_idx, int len) {
    xhci_slot_t *slot = &xhci.slots[slot_id];

    if (!slot->enabled || !slot->ep_ring[ep_idx]) {
        return -1;
    }

    /* Get physical address of buffer */
    if (int_transfer_buf_phys == 0) {
        int_transfer_buf_phys = (uint64_t)int_transfer_buf;
    }

    /* Queue the transfer */
    xhci_queue_interrupt_trb(slot, ep_idx, int_transfer_buf_phys, len);

    /* Memory barrier */
    __asm__ volatile ("mfence" ::: "memory");

    /* Ring doorbell (doorbell target = ep_idx + 1) */
    xhci_doorbell(slot_id, ep_idx + 1);

    int_transfer_pending = 1;
    int_transfer_slot = slot_id;
    int_transfer_ep = ep_idx;

    return 0;
}

/* Poll for interrupt transfer completion (non-blocking) */
int xhci_poll_interrupt(uint32_t slot_id, uint8_t ep_idx,
                        void *buf, int buf_len) {
    (void)slot_id;
    (void)ep_idx;

    if (!int_transfer_pending) {
        return 0;  /* No transfer pending */
    }

    /* Check event ring for completion */
    xhci_trb_t *event = &xhci.event_ring[xhci.event_dequeue];
    uint32_t cycle = event->control & TRB_CYCLE;

    if (cycle != xhci.event_cycle) {
        return 0;  /* No event yet */
    }

    uint32_t trb_type = (event->control >> 10) & 0x3F;

    if (trb_type == TRB_TYPE_TRANSFER) {
        uint32_t cc = (event->status >> 24) & 0xFF;
        int transferred = event->status & 0xFFFFFF;

        /* Advance event dequeue */
        xhci.event_dequeue++;
        if (xhci.event_dequeue >= EVENT_RING_SIZE) {
            xhci.event_dequeue = 0;
            xhci.event_cycle ^= 1;
        }
        uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
        xhci_rt_write64(0x38, erdp | (1 << 3));

        int_transfer_pending = 0;

        if (cc == TRB_COMP_SUCCESS || cc == TRB_COMP_SHORT_PKT) {
            /* Copy data to user buffer */
            int copy_len = (buf_len < transferred) ? buf_len : transferred;
            if (copy_len < 0) copy_len = buf_len;  /* SHORT_PKT may have weird length */
            memcpy(buf, int_transfer_buf, copy_len > 0 ? copy_len : buf_len);

            /* Queue next transfer */
            xhci_start_interrupt_transfer(int_transfer_slot, int_transfer_ep, buf_len);

            return copy_len > 0 ? copy_len : buf_len;
        } else {
            kprintf("[xHCI] Interrupt transfer error: %d\n", cc);
            return -1;
        }
    } else if (trb_type == TRB_TYPE_PORT_STATUS) {
        /* Skip port status events */
        xhci.event_dequeue++;
        if (xhci.event_dequeue >= EVENT_RING_SIZE) {
            xhci.event_dequeue = 0;
            xhci.event_cycle ^= 1;
        }
        uint64_t erdp = xhci.event_ring_phys + xhci.event_dequeue * sizeof(xhci_trb_t);
        xhci_rt_write64(0x38, erdp | (1 << 3));
    }

    return 0;
}
