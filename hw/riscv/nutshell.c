/*
 * QEMU RISC-V Nutshell Board
 * Copyright (c)
 * Copyright (c) 2016-2017 Sagar Karandikar, sagark@eecs.berkeley.edu
 * Copyright (c) 2017-2018 SiFive, Inc.
 *
 * This provides a RISC-V Board with the following devices:
 *
 * 0)
 * 1)
 * 2)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2 or later, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "qemu/osdep.h"
#include "qemu/typedefs.h"
#include "qemu/units.h"
#include "qemu/log.h"
#include "qemu/error-report.h"
#include "qapi/error.h"
#include "hw/boards.h"
#include "hw/loader.h"
#include "hw/sysbus.h"
#include "hw/qdev-properties.h"
#include "hw/char/serial.h"
#include "target/riscv/cpu.h"
#include "hw/riscv/riscv_hart.h"
#include "hw/riscv/nutshell.h"
#include "hw/riscv/boot.h"
#include "hw/riscv/numa.h"
#include "chardev/char.h"
#include "sysemu/arch_init.h"
#include "sysemu/device_tree.h"
#include "sysemu/sysemu.h"
#include "chardev/char-serial.h"
#include "hw/intc/riscv_aclint.h"
#include "hw/intc/sifive_plic.h"
#include "hw/block/flash.h"

// refer src/main/scala/sim/SimMMIO.scala
// refer src/main/scala/system/NutShell.scala
// https://github.com/OpenXiangShan/NEMU/blob/master/configs/riscv64-nutshell_defconfig
// TODO: 额外加的mrom sram

static const MemMapEntry memmap[] = {
    [NUTSHELL_MROM] = {0x00000000, 0x20000},
    [NUTSHELL_SRAM] = {0x00020000, 0xe0000},
    [NUTSHELL_UART0] = {0x10000000, 0x100},
    [NUTSHELL_UART1] = {0x10001000, 0x100},
    [NUTSHELL_UART2] = {0x10002000, 0x100},
    [NUTSHELL_CLINT] = {0x38000000, 0x00010000},
    [NUTSHELL_PLIC] = {0x3c000000, 0x04000000},
    [NUTSHELL_FLASH] = {0x40000000, 0x1000},
    [NUTSHELL_SD] = {0x40002000, 0x1000},
    [NUTSHELL_DMA] = {0x40003000, 0x1000},
    [NUTSHELL_UARTLITE] = {0x40600000, 0x10},
    [NUTSHELL_DRAM] = {0x80000000, 0x0}};


static void nutshell_setup_rom_reset_vec(MachineState *machine,
                                         RISCVHartArrayState *harts,
                                         hwaddr start_addr, hwaddr rom_base,
                                         hwaddr rom_size, uint64_t kernel_entry,
                                         uint64_t fdt_load_addr) {
  int i;
  uint32_t start_addr_hi32 = 0x00000000;
  uint32_t fdt_load_addr_hi32 = 0x00000000;

  if (!riscv_is_32bit(harts)) {
    start_addr_hi32 = start_addr >> 32;
    fdt_load_addr_hi32 = fdt_load_addr >> 32;
  }
  /* reset vector */
  uint32_t reset_vec[10] = {
      0x00000297, /* 1:  auipc  t0, %pcrel_hi(fw_dyn) */
      0x02828613, /*     addi   a2, t0, %pcrel_lo(1b) */
      0xf1402573, /*     csrr   a0, mhartid  */
      0,
      0,
      0x00028067, /*     jr     t0 */
      start_addr, /* start: .dword */
      start_addr_hi32,
      fdt_load_addr, /* fdt_laddr: .dword */
      fdt_load_addr_hi32,
      /* fw_dyn: */
  };
  if (riscv_is_32bit(harts)) {
    reset_vec[3] = 0x0202a583; /*     lw     a1, 32(t0) */
    reset_vec[4] = 0x0182a283; /*     lw     t0, 24(t0) */
  } else {
    reset_vec[3] = 0x0202b583; /*     ld     a1, 32(t0) */
    reset_vec[4] = 0x0182b283; /*     ld     t0, 24(t0) */
  }

  if (!harts->harts[0].cfg.ext_zicsr) {
    /*
     * The Zicsr extension has been disabled, so let's ensure we don't
     * run the CSR instruction. Let's fill the address with a non
     * compressed nop.
     */
    reset_vec[2] = 0x00000013; /*     addi   x0, x0, 0 */
  }

  /* copy in the reset vector in little_endian byte order */
  for (i = 0; i < ARRAY_SIZE(reset_vec); i++) {
    reset_vec[i] = cpu_to_le32(reset_vec[i]);
  }
}

static void nutshell_flash_create(MachineState *machine)
{ 
    MemoryRegion *system_memory = get_system_memory();
    NUTSHELLState *s = RISCV_NUTSHELL_MACHINE(machine);
    uint64_t flash_sector_size = 256 * KiB;
    DeviceState *dev = qdev_new(TYPE_PFLASH_CFI01);

    qdev_prop_set_uint64(dev, "sector-length", flash_sector_size);
    qdev_prop_set_uint8(dev, "width", 4);
    qdev_prop_set_uint8(dev, "device-width", 2);
    qdev_prop_set_bit(dev, "big-endian", false);
    qdev_prop_set_uint16(dev, "id0", 0x89);
    qdev_prop_set_uint16(dev, "id1", 0x18);
    qdev_prop_set_uint16(dev, "id2", 0x00);
    qdev_prop_set_uint16(dev, "id3", 0x00);
    qdev_prop_set_string(dev, "name", "nutshell.flash0");
    object_property_add_child(OBJECT(s), "nutshell.flash0", OBJECT(dev));
    object_property_add_alias(OBJECT(s), "pflash0",
                              OBJECT(dev), "drive");
    s->flash = PFLASH_CFI01(dev);
    pflash_cfi01_legacy_drive(s->flash, drive_get(IF_PFLASH, 0, 0));

    assert(QEMU_IS_ALIGNED(memmap[NUTSHELL_FLASH].size, 
                                flash_sector_size));
    assert(memmap[NUTSHELL_FLASH].size/flash_sector_size <= UINT32_MAX);
    qdev_prop_set_uint32(dev, "num-blocks", 
                    memmap[NUTSHELL_FLASH].size / flash_sector_size);
    sysbus_realize_and_unref(SYS_BUS_DEVICE(dev), &error_fatal);

    memory_region_add_subregion(system_memory, 
                            memmap[NUTSHELL_FLASH].base,
                            sysbus_mmio_get_region(SYS_BUS_DEVICE(dev), 0));
}

static void nutshell_interrupt_controller_create(MachineState *machine) {
    NUTSHELLState *s = RISCV_NUTSHELL_MACHINE(machine);
    char *plic_hart_config;

    riscv_aclint_swi_create(
        memmap[NUTSHELL_CLINT].base,
        0, machine->smp.cpus, false);
    riscv_aclint_mtimer_create(memmap[NUTSHELL_CLINT].base + RISCV_ACLINT_SWI_SIZE,
        RISCV_ACLINT_DEFAULT_MTIMER_SIZE, 0, machine->smp.cpus,
        RISCV_ACLINT_DEFAULT_MTIMECMP, RISCV_ACLINT_DEFAULT_MTIME,
        RISCV_ACLINT_DEFAULT_TIMEBASE_FREQ, true);

    plic_hart_config = riscv_plic_hart_config_string(machine->smp.cpus);
    s->plic = sifive_plic_create(
        memmap[NUTSHELL_PLIC].base,
        plic_hart_config,  machine->smp.cpus, 0,
        PLIC_NUM_SOURCES,
        PLIC_NUM_PRIORITIES,
        PLIC_PRIORITY_BASE,
        PLIC_PENDING_BASE,
        PLIC_ENABLE_BASE,
        PLIC_ENABLE_STRIDE,
        PLIC_CONTEXT_BASE,
        PLIC_CONTEXT_STRIDE,
        memmap[NUTSHELL_PLIC].size);
    g_free(plic_hart_config);
}

static void nutshell_serial_create(MachineState *machine)
{    
    MemoryRegion *system_memory = get_system_memory();
    NUTSHELLState *s = RISCV_NUTSHELL_MACHINE(machine);

    serial_mm_init(system_memory, memmap[NUTSHELL_UART0].base,
        0, qdev_get_gpio_in(DEVICE(s->plic), UART0_IRQ), 399193,
        serial_hd(0), DEVICE_LITTLE_ENDIAN);
    serial_mm_init(system_memory, memmap[NUTSHELL_UART1].base,
        0, qdev_get_gpio_in(DEVICE(s->plic), UART1_IRQ), 399193,
        serial_hd(1), DEVICE_LITTLE_ENDIAN);
    serial_mm_init(system_memory, memmap[NUTSHELL_UART2].base,
        0, qdev_get_gpio_in(DEVICE(s->plic), UART2_IRQ), 399193,
        serial_hd(2), DEVICE_LITTLE_ENDIAN);
}

static void nutshell_cpu_create(MachineState *machine) {
  NUTSHELLState *s = RISCV_NUTSHELL_MACHINE(machine);
  int base_hartid, hart_count;
  char *soc_name;

  if (NUTSHELL_SOCKETS_MAX < riscv_socket_count(machine)) {
    error_report("number of sockets/nodes should be less than %d",
                 NUTSHELL_SOCKETS_MAX);
    exit(1);
  }

//TODO: change soc to cpus
  for (int i = 0; i < riscv_socket_count(machine); i++) {
    if (!riscv_socket_check_hartids(machine, i)) {
      error_report("discontinuous hartids in socket%d", i);
      exit(1);
    }

    base_hartid = riscv_socket_first_hartid(machine, i);
    if (base_hartid < 0) {
      error_report("can't find hartid base for socket%d", i);
      exit(1);
    }

    hart_count = riscv_socket_hart_count(machine, i);
    if (hart_count < 0) {
      error_report("can't find hart count for socket%d", i);
      exit(1);
    }

    soc_name = g_strdup_printf("soc%d", i);
    object_initialize_child(OBJECT(machine), soc_name, &s->soc[i],
                            TYPE_RISCV_HART_ARRAY);
    g_free(soc_name);
    object_property_set_str(OBJECT(&s->soc[i]), "cpu-type", machine->cpu_type,
                            &error_abort);
    object_property_set_int(OBJECT(&s->soc[i]), "hartid-base", base_hartid,
                            &error_abort);
    object_property_set_int(OBJECT(&s->soc[i]), "num-harts", hart_count,
                            &error_abort);
    sysbus_realize(SYS_BUS_DEVICE(&s->soc[i]), &error_abort);
  }
}

static void nutshell_memory_create(MachineState *machine) {
  NUTSHELLState *s = RISCV_NUTSHELL_MACHINE(machine);
  MemoryRegion *system_memory = get_system_memory();
  MemoryRegion *main_mem = g_new(MemoryRegion, 1);
  MemoryRegion *sram_mem = g_new(MemoryRegion, 1);
  MemoryRegion *mask_rom = g_new(MemoryRegion, 1);

  memory_region_init_ram(main_mem, NULL, "riscv_nutshell_board.dram",
                         machine->ram_size, &error_fatal);
  memory_region_add_subregion(system_memory, memmap[NUTSHELL_DRAM].base,
                              main_mem);

  memory_region_init_ram(sram_mem, NULL, "riscv_nutshell_board.sram",
                         memmap[NUTSHELL_SRAM].size, &error_fatal);
  memory_region_add_subregion(system_memory, memmap[NUTSHELL_SRAM].base,
                              sram_mem);

  memory_region_init_rom(mask_rom, NULL, "riscv_nutshell_board.mrom",
                         memmap[NUTSHELL_MROM].size, &error_fatal);
  memory_region_add_subregion(system_memory, memmap[NUTSHELL_MROM].base,
                              mask_rom);

  nutshell_setup_rom_reset_vec(machine, &s->soc[0], memmap[NUTSHELL_FLASH].base,
                               memmap[NUTSHELL_MROM].base,
                               memmap[NUTSHELL_MROM].size, 0, 0);
}


static void nutshell_machine_init(MachineState *machine) {
  nutshell_cpu_create(machine);
  nutshell_interrupt_controller_create(machine);
  nutshell_memory_create(machine);
  nutshell_flash_create(machine);
  nutshell_serial_create(machine);
}



static void nutshell_machine_class_init(ObjectClass *oc, void *data) {
  MachineClass *mc = MACHINE_CLASS(oc);

  mc->desc = "RISC-V Nutshell board";
  mc->init = nutshell_machine_init;
  mc->max_cpus = NUTSHELL_CPUS_MAX;
  mc->min_cpus = NUTSHELL_CPUS_MIN;
  mc->default_cpu_type = TYPE_RISCV_CPU_NUTSHELL;
  mc->pci_allow_0_address = true;
  mc->possible_cpu_arch_ids = riscv_numa_possible_cpu_arch_ids;
  mc->cpu_index_to_instance_props = riscv_numa_cpu_index_to_props;
  mc->get_default_cpu_node_id = riscv_numa_get_default_cpu_node_id;
  mc->numa_mem_supported = true;
}

static void nutshell_machine_instance_init(Object *obj) {}

static const TypeInfo nutshell_machine_typeinfo = {
    .name = TYPE_RISCV_NUTSHELL_MACHINE,
    .parent = TYPE_MACHINE,
    .class_init = nutshell_machine_class_init,
    .instance_init = nutshell_machine_instance_init,
    .instance_size = sizeof(NUTSHELLState),
};

static void nutshell_machine_init_register_types(void) {
  type_register_static(&nutshell_machine_typeinfo);
}

type_init(nutshell_machine_init_register_types)