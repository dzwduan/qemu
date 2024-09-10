#ifndef HW_RISCV_NUTSHELL_H
#define HW_RISCV_NUTSHELL_H

#include "hw/boards.h"
#include "hw/cpu/cluster.h"
#include "hw/riscv/riscv_hart.h"
#include "qemu/typedefs.h"

#include "qom/object.h"

#define NUTSHELL_CPUS_MAX 2
#define NUTSHELL_CPUS_MIN 1

#define TYPE_NUTSHELL_MACHINE MACHINE_TYPE_NAME("nutshell")
typedef struct NUTSHELLState NUTSHELLState;
DECLARE_INSTANCE_CHECKER(NUTSHELLState, NUTSHELL_MACHINE,
                         TYPE_NUTSHELL_MACHINE)

typedef struct NUTSHELLConfig{} NUTSHELLConfig;

struct NUTSHELLState{
    /*< private >*/
    MachineState parent;

    /*< public >*/
    CPUClusterState c_cluster;
    RISCVHartArrayState c_cpus;
};


#endif