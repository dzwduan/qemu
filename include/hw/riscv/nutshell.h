#ifndef HW_RISCV_NUTSHELL_H
#define HW_RISCV_NUTSHELL_H

#include "hw/block/flash.h"
#include "hw/boards.h"
#include "hw/cpu/cluster.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/sysbus.h"
#include "qemu/typedefs.h"
#include "stdint.h"
#include "hw/block/flash.h"

#define NUTSHELL_CPUS_MAX 8
#define NUTSHELL_CPUS_MIN 1
#define NUTSHELL_SOCKETS_MAX 8


#define TYPE_RISCV_NUTSHELL_MACHINE MACHINE_TYPE_NAME("nutshell")
typedef struct NUTSHELLState NUTSHELLState;
#define RISCV_NUTSHELL_MACHINE(obj)     \
  OBJECT_CHECK(NUTSHELLState, (obj), TYPE_RISCV_NUTSHELL_MACHINE)


struct NUTSHELLState {
  /*< private >*/
  MachineState parent;

  /*< public >*/
  // CPUClusterState c_cluster;
  RISCVHartArrayState soc[NUTSHELL_SOCKETS_MAX];
  DeviceState *plic;
  PFlashCFI01 *flash;
};

enum {
  UART0_IRQ = 10,
  UART1_IRQ = 11,
  UART2_IRQ = 12,
  RTC_IRQ = 11,
  VIRTIO_IRQ = 1, /* 1 to 8 */
  VIRTIO_COUNT = 8,
  PCIE_IRQ = 0x20,            /* 32 to 35 */
  VIRT_PLATFORM_BUS_IRQ = 64, /* 64 to 95 */
};

enum {
  NUTSHELL_VGA,
  NUTSHELL_VMEM,
  NUTSHELL_PLIC,
  NUTSHELL_CLINT,
  NUTSHELL_UARTLITE,
  NUTSHELL_FLASH,
  NUTSHELL_SD,
  NUTSHELL_DMA,
  NUTSHELL_DRAM,
  NUTSHELL_MROM,
  NUTSHELL_SRAM,
  NUTSHELL_UART0,
  NUTSHELL_UART1,
  NUTSHELL_UART2
};

/*
 * Freedom E310 G002 and G003 supports 52 interrupt sources while
 * Freedom E310 G000 supports 51 interrupt sources. We use the value
 * of G002 and G003, so it is 53 (including interrupt source 0).
 */
#define PLIC_HART_CONFIG "MS"
#define PLIC_NUM_SOURCES 53
#define PLIC_NUM_PRIORITIES 7
#define PLIC_PRIORITY_BASE 0x00
#define PLIC_PENDING_BASE 0x1000
#define PLIC_ENABLE_BASE 0x2000
#define PLIC_ENABLE_STRIDE 0x80
#define PLIC_CONTEXT_BASE 0x200000
#define PLIC_CONTEXT_STRIDE 0x1000

#endif