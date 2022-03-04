/*
 * iPhone 11 - t8030
 *
 * Copyright (c) 2019 Jonathan Afek <jonyafek@me.com>
 * Copyright (c) 2021 Nguyen Hoang Trung (TrungNguyen1909)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef HW_ARM_T8030_H
#define HW_ARM_T8030_H

#include "qemu-common.h"
#include "exec/hwaddr.h"
#include "hw/boards.h"
#include "hw/arm/boot.h"
#include "hw/arm/apple/xnu.h"
#include "exec/memory.h"
#include "cpu.h"
#include "sysemu/kvm.h"
#include "hw/arm/apple/t8030_cpu.h"

#define TYPE_T8030 "t8030"

#define TYPE_T8030_MACHINE MACHINE_TYPE_NAME(TYPE_T8030)

#define T8030_MACHINE(obj) \
    OBJECT_CHECK(T8030MachineState, (obj), TYPE_T8030_MACHINE)

typedef struct
{
    MachineClass parent;
} T8030MachineClass;

typedef enum BootMode {
    kBootModeAuto = 0,
    kBootModeManual,
    kBootModeEnterRecovery,
    kBootModeExitRecovery,
} BootMode;

typedef struct
{
    MachineState parent;
    hwaddr soc_base_pa;
    hwaddr soc_size;

    unsigned long dram_size;
    T8030CPUState *cpus[T8030_MAX_CPU];
    T8030CPUCluster clusters[T8030_MAX_CLUSTER];
    SysBusDevice *aic;
    MemoryRegion *sysmem;
    struct mach_header_64 *kernel;
    DTBNode *device_tree;
    struct macho_boot_info bootinfo;
    video_boot_args video;
    char *trustcache_filename;
    char *ticket_filename;
    BootMode boot_mode;
    uint32_t build_version;
    hwaddr panic_base;
    hwaddr panic_size;
} T8030MachineState;
#endif
