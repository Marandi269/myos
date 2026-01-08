/*
 * xhci.h - xHCI (USB 3.x) host controller driver
 */

#ifndef _XHCI_H
#define _XHCI_H

#include "usb.h"

/* xHCI Capability Registers */
#define XHCI_CAP_CAPLENGTH      0x00
#define XHCI_CAP_HCSPARAMS1     0x04
#define XHCI_CAP_HCSPARAMS2     0x08
#define XHCI_CAP_HCSPARAMS3     0x0C
#define XHCI_CAP_HCCPARAMS1     0x10
#define XHCI_CAP_DBOFF          0x14
#define XHCI_CAP_RTSOFF         0x18

/* xHCI Operational Registers (offset from cap_base + cap_length) */
#define XHCI_OP_USBCMD          0x00
#define XHCI_OP_USBSTS          0x04
#define XHCI_OP_PAGESIZE        0x08
#define XHCI_OP_DNCTRL          0x14
#define XHCI_OP_CRCR            0x18
#define XHCI_OP_DCBAAP          0x30
#define XHCI_OP_CONFIG          0x38

/* Port registers (offset from op_base + 0x400 + port*0x10) */
#define XHCI_PORT_SC            0x00
#define XHCI_PORT_PMSC          0x04
#define XHCI_PORT_LI            0x08
#define XHCI_PORT_HLPMC         0x0C

/* USBCMD bits */
#define XHCI_CMD_RUN            (1 << 0)
#define XHCI_CMD_HCRST          (1 << 1)
#define XHCI_CMD_INTE           (1 << 2)
#define XHCI_CMD_HSEE           (1 << 3)

/* USBSTS bits */
#define XHCI_STS_HCH            (1 << 0)    /* HC Halted */
#define XHCI_STS_HSE            (1 << 2)    /* Host System Error */
#define XHCI_STS_EINT           (1 << 3)    /* Event Interrupt */
#define XHCI_STS_PCD            (1 << 4)    /* Port Change Detect */
#define XHCI_STS_CNR            (1 << 11)   /* Controller Not Ready */

/* Port status bits */
#define XHCI_PORT_CCS           (1 << 0)    /* Current Connect Status */
#define XHCI_PORT_PED           (1 << 1)    /* Port Enabled/Disabled */
#define XHCI_PORT_OCA           (1 << 3)    /* Overcurrent Active */
#define XHCI_PORT_PR            (1 << 4)    /* Port Reset */
#define XHCI_PORT_PLS_MASK      (0xF << 5)  /* Port Link State */
#define XHCI_PORT_PP            (1 << 9)    /* Port Power */
#define XHCI_PORT_SPEED_MASK    (0xF << 10) /* Port Speed */
#define XHCI_PORT_LWS           (1 << 16)   /* Port Link State Write Strobe */
#define XHCI_PORT_CSC           (1 << 17)   /* Connect Status Change */
#define XHCI_PORT_PEC           (1 << 18)   /* Port Enabled/Disabled Change */
#define XHCI_PORT_WRC           (1 << 19)   /* Warm Port Reset Change */
#define XHCI_PORT_OCC           (1 << 20)   /* Overcurrent Change */
#define XHCI_PORT_PRC           (1 << 21)   /* Port Reset Change */
#define XHCI_PORT_PLC           (1 << 22)   /* Port Link State Change */
#define XHCI_PORT_CEC           (1 << 23)   /* Config Error Change */
#define XHCI_PORT_WDE           (1 << 26)   /* Wake on Disconnect Enable */
#define XHCI_PORT_WCE           (1 << 25)   /* Wake on Connect Enable */
#define XHCI_PORT_WOE           (1 << 27)   /* Wake on Overcurrent Enable */

/* Port speed values */
#define XHCI_SPEED_FULL         1
#define XHCI_SPEED_LOW          2
#define XHCI_SPEED_HIGH         3
#define XHCI_SPEED_SUPER        4

/* TRB types */
#define TRB_TYPE_NORMAL         1
#define TRB_TYPE_SETUP          2
#define TRB_TYPE_DATA           3
#define TRB_TYPE_STATUS         4
#define TRB_TYPE_LINK           6
#define TRB_TYPE_EVENT_DATA     7
#define TRB_TYPE_NOOP           8
#define TRB_TYPE_ENABLE_SLOT    9
#define TRB_TYPE_DISABLE_SLOT   10
#define TRB_TYPE_ADDRESS_DEV    11
#define TRB_TYPE_CONFIG_EP      12
#define TRB_TYPE_EVAL_CTX       13
#define TRB_TYPE_RESET_EP       14
#define TRB_TYPE_STOP_EP        15
#define TRB_TYPE_SET_TR_DEQUEUE 16
#define TRB_TYPE_RESET_DEV      17
#define TRB_TYPE_NOOP_CMD       23
#define TRB_TYPE_TRANSFER       32
#define TRB_TYPE_COMMAND_COMP   33
#define TRB_TYPE_PORT_STATUS    34

/* TRB completion codes */
#define TRB_COMP_SUCCESS        1
#define TRB_COMP_DATA_BUFFER    2
#define TRB_COMP_BABBLE         3
#define TRB_COMP_USB_TRANS      4
#define TRB_COMP_TRB            5
#define TRB_COMP_STALL          6
#define TRB_COMP_SHORT_PKT      13

/* Transfer Ring Block (TRB) - 16 bytes */
typedef struct __attribute__((packed)) {
    uint64_t param;
    uint32_t status;
    uint32_t control;
} xhci_trb_t;

/* Slot Context */
typedef struct __attribute__((packed)) {
    uint32_t info1;
    uint32_t info2;
    uint32_t tt_info;
    uint32_t state;
    uint32_t reserved[4];
} xhci_slot_ctx_t;

/* Endpoint Context */
typedef struct __attribute__((packed)) {
    uint32_t info1;
    uint32_t info2;
    uint64_t deq_ptr;
    uint32_t tx_info;
    uint32_t reserved[3];
} xhci_ep_ctx_t;

/* Input Control Context */
typedef struct __attribute__((packed)) {
    uint32_t drop_flags;
    uint32_t add_flags;
    uint32_t reserved[6];
} xhci_input_ctrl_ctx_t;

/* Device Context - includes slot + 31 endpoint contexts */
typedef struct __attribute__((packed)) {
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t ep[31];
} xhci_dev_ctx_t;

/* Input Context - control + slot + 31 endpoints */
typedef struct __attribute__((packed)) {
    xhci_input_ctrl_ctx_t ctrl;
    xhci_slot_ctx_t slot;
    xhci_ep_ctx_t ep[31];
} xhci_input_ctx_t;

/* Event Ring Segment Table Entry */
typedef struct __attribute__((packed)) {
    uint64_t base;
    uint32_t size;
    uint32_t reserved;
} xhci_erst_entry_t;

/* Per-slot device info */
typedef struct xhci_slot {
    uint8_t slot_id;
    uint8_t port;
    uint8_t speed;
    uint8_t enabled;

    /* Device context */
    xhci_dev_ctx_t *dev_ctx;
    uint64_t dev_ctx_phys;

    /* Input context */
    xhci_input_ctx_t *input_ctx;
    uint64_t input_ctx_phys;

    /* Transfer rings for each endpoint (0 = control EP) */
    xhci_trb_t *ep_ring[32];
    uint64_t ep_ring_phys[32];
    uint32_t ep_enqueue[32];
    uint32_t ep_cycle[32];

    /* USB device reference */
    usb_device_t *usb_dev;
} xhci_slot_t;

/* xHCI controller state */
typedef struct xhci_controller {
    uint64_t cap_base;      /* Capability registers base */
    uint64_t op_base;       /* Operational registers base */
    uint64_t rt_base;       /* Runtime registers base */
    uint64_t db_base;       /* Doorbell registers base */

    uint32_t max_slots;
    uint32_t max_ports;
    uint32_t page_size;
    uint32_t context_size;  /* 32 or 64 bytes */

    /* Device Context Base Address Array */
    uint64_t *dcbaa;
    uint64_t dcbaa_phys;

    /* Command Ring */
    xhci_trb_t *cmd_ring;
    uint64_t cmd_ring_phys;
    uint32_t cmd_enqueue;
    uint32_t cmd_cycle;

    /* Event Ring */
    xhci_trb_t *event_ring;
    uint64_t event_ring_phys;
    uint32_t event_dequeue;
    uint32_t event_cycle;
    xhci_erst_entry_t *erst;
    uint64_t erst_phys;

    /* Scratchpad */
    uint64_t *scratchpad;
    uint64_t scratchpad_phys;

    /* Slots */
    xhci_slot_t slots[256];

} xhci_controller_t;

/* Initialize xHCI controller */
int xhci_init(uint64_t mmio_base);

/* Get xHCI HCD */
usb_hcd_t *xhci_get_hcd(void);

/* Configure endpoint for interrupt transfers */
int xhci_configure_endpoint(uint32_t slot_id, uint8_t ep_num, uint8_t ep_type,
                            uint16_t max_packet, uint8_t interval);

/* Setup interrupt transfer ring and start polling */
int xhci_setup_interrupt_ep(uint32_t slot_id, uint8_t ep_addr,
                            uint16_t max_packet, uint8_t interval);

/* Poll for interrupt transfer completion (non-blocking) */
int xhci_poll_interrupt(uint32_t slot_id, uint8_t ep_num,
                        void *buf, int buf_len);

/* Start an async interrupt transfer */
int xhci_start_interrupt_transfer(uint32_t slot_id, uint8_t ep_idx, int len);

#endif /* _XHCI_H */
