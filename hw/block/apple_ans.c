#include "qemu/osdep.h"
#include "qemu/bitops.h"
#include "qemu/log.h"
#include "qemu/module.h"
#include "qapi/error.h"
#include "hw/irq.h"
#include "hw/block/block.h"
#include "hw/pci/pci.h"
#include "hw/pci/pcie_host.h"
#include "hw/pci/msi.h"
#include "hw/pci/msix.h"
#include "sysemu/dma.h"
#include "hw/nvme/nvme.h"
#include "migration/vmstate.h"
#include "hw/arm/apple/xnu.h"
#include "hw/arm/apple/xnu_dtb.h"
#include "hw/misc/apple_mbox.h"
#include "hw/block/apple_ans.h"

#define TYPE_APPLE_ANS "apple.ans"
OBJECT_DECLARE_SIMPLE_TYPE(AppleANSState, APPLE_ANS)

#define ANS_LOG_MSG(ep, msg) \
do { qemu_log_mask(LOG_GUEST_ERROR, "ANS2: message:" \
                   " ep=%u msg=0x" TARGET_FMT_plx, \
                   ep, msg); } while (0)

#define APPLE_BOOT_STATUS       0x1300
#define   APPLE_BOOT_STATUS_OK  0xde71ce55

typedef struct QEMU_PACKED {
    uint32_t NSID;
    uint32_t NSType;
    uint32_t numBlocks;
} NVMeCreateNamespacesEntryStruct;

struct AppleANSState {
    PCIExpressHost parent_obj;
    MemoryRegion *iomems[4];
    MemoryRegion io_mmio;
    MemoryRegion io_ioport;
    MemoryRegion msix;
    AppleMboxState *mbox;
    qemu_irq irq;

    NvmeCtrl nvme;
    uint32_t nvme_interrupt_idx;
    bool started;
};

static void ascv2_core_reg_write(void *opaque, hwaddr addr,
                  uint64_t data,
                  unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "ANS2: AppleASCWrapV2 core reg WRITE @ 0x"
                  TARGET_FMT_plx " value: 0x" TARGET_FMT_plx "\n", addr, data);
}

static uint64_t ascv2_core_reg_read(void *opaque,
                     hwaddr addr,
                     unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "ANS2: AppleASCWrapV2 core reg READ @ 0x"
                  TARGET_FMT_plx "\n", addr);
    return 0;
}

static const MemoryRegionOps ascv2_core_reg_ops = {
    .write = ascv2_core_reg_write,
    .read = ascv2_core_reg_read,
    .endianness = DEVICE_NATIVE_ENDIAN,
    .impl.min_access_size = 8,
    .impl.max_access_size = 8,
    .valid.min_access_size = 8,
    .valid.max_access_size = 8,
    .valid.unaligned = false,
};

static void iop_autoboot_reg_write(void *opaque,
                  hwaddr addr,
                  uint64_t data,
                  unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "ANS2: AppleA7IOP autoboot reg WRITE @ 0x"
                  TARGET_FMT_plx " value: 0x" TARGET_FMT_plx "\n", addr, data);
}

static uint64_t iop_autoboot_reg_read(void *opaque, hwaddr addr, unsigned size)
{
    qemu_log_mask(LOG_UNIMP, "ANS2: AppleA7IOP autoboot reg READ @ 0x"
                  TARGET_FMT_plx "\n", addr);
    return 0;
}

static const MemoryRegionOps iop_autoboot_reg_ops = {
    .write = iop_autoboot_reg_write,
    .read = iop_autoboot_reg_read,
};

static void apple_ans_set_irq(void *opaque, int irq_num, int level)
{
    AppleANSState *s = APPLE_ANS(opaque);
    qemu_set_irq(s->irq, level);
}

static void apple_ans_start(void *opaque)
{
    AppleANSState *s = APPLE_ANS(opaque);
    uint32_t config;

    config = pci_default_read_config(PCI_DEVICE(&s->nvme),
                                     PCI_COMMAND, 4);
    config |= 0x0002 | 0x0004; /* memory | bus */
    pci_default_write_config(PCI_DEVICE(&s->nvme),
                             PCI_COMMAND, config, 4);
    s->started = true;
    assert(PCI_DEVICE(&s->nvme)->bus_master_enable_region.enabled);
}

static void apple_ans_ep_handler(void *opaque, uint32_t ep, uint64_t msg)
{
    ANS_LOG_MSG(ep, msg);
}

static const struct AppleMboxOps ans_mailbox_ops = {
    .start = apple_ans_start,
    .wakeup = apple_ans_start,
};

SysBusDevice *apple_ans_create(DTBNode *node, uint32_t build_version)
{
    DeviceState  *dev;
    AppleANSState *s;
    PCIHostState *pci;
    SysBusDevice *sbd;
    PCIExpressHost *pex;
    DTBNode *child;
    DTBProp *prop;
    uint64_t *reg;
    uint32_t protocol_version = 0;
    int i;
    uint32_t data;
    struct segment_range {
        uint64_t phys;
        uint64_t virt;
        uint64_t remap;
        uint32_t size;
        uint32_t flag;
    };
    struct segment_range segrange[2] = { 0 };

    dev = qdev_new(TYPE_APPLE_ANS);
    s = APPLE_ANS(dev);
    pci = PCI_HOST_BRIDGE(dev);
    sbd = SYS_BUS_DEVICE(dev);
    pex = PCIE_HOST_BRIDGE(dev);

    switch (BUILD_VERSION_MAJOR(build_version)) {
        case 14:
            protocol_version = 11;
            break;
        case 15:
            protocol_version = 12;
            break;
        default:
            break;
    }
    prop = find_dtb_prop(node, "reg");
    assert(prop);

    reg = (uint64_t *)prop->value;

    /*
     * 0: AppleA7IOP akfRegMap
     * 1: AppleASCWrapV2 coreRegisterMap
     * 2: AppleA7IOP autoBootRegMap
     */
    s->mbox = apple_mbox_create("ANS2", s, reg[1], protocol_version,
                                &ans_mailbox_ops);
    object_property_add_child(OBJECT(s), "mbox", OBJECT(s->mbox));
    apple_mbox_register_endpoint(s->mbox, 1, apple_ans_ep_handler);
    sysbus_init_mmio(sbd, sysbus_mmio_get_region(SYS_BUS_DEVICE(s->mbox), 0));

    s->iomems[1] = g_new(MemoryRegion, 1);
    memory_region_init_io(s->iomems[1], OBJECT(dev), &ascv2_core_reg_ops, s,
                          TYPE_APPLE_ANS ".ascv2-core-reg", reg[3]);
    sysbus_init_mmio(sbd, s->iomems[1]);

    s->iomems[2] = g_new(MemoryRegion, 1);
    memory_region_init_io(s->iomems[2], OBJECT(dev), &iop_autoboot_reg_ops, s,
                          TYPE_APPLE_ANS ".iop-autoboot-reg", reg[5]);
    sysbus_init_mmio(sbd, s->iomems[2]);

    sysbus_pass_irq(sbd, SYS_BUS_DEVICE(s->mbox));
    sysbus_init_irq(sbd, &s->irq);

    child = get_dtb_node(node, "iop-ans-nub");
    assert(child);

    data = 1;
    set_dtb_prop(child, "pre-loaded", 4, (uint8_t *)&data);
    set_dtb_prop(child, "running", 4, (uint8_t *)&data);

    prop = find_dtb_prop(child, "region-base");
    *(uint64_t *)prop->value = 0x8fc400000;

    prop = find_dtb_prop(child, "region-size");
    *(uint64_t *)prop->value = 0x3c00000;

    set_dtb_prop(child, "segment-names", 14, (uint8_t *)"__TEXT;__DATA");

    segrange[0].phys = 0x800024000;
    segrange[0].virt = 0x0;
    segrange[0].remap = 0x800024000;
    segrange[0].size = 0x124000;
    segrange[0].flag = 0x1;

    segrange[1].phys = 0x8fc400000;
    segrange[1].virt = 0x124000;
    segrange[1].remap = 0x8fc400000;
    segrange[1].size = 0x3c00000;
    segrange[1].flag = 0x0;
    set_dtb_prop(child, "segment-ranges", 64, (uint8_t *)segrange);

    object_initialize_child(OBJECT(dev), "nvme", &s->nvme, TYPE_NVME);

    object_property_set_str(OBJECT(&s->nvme), "serial",
                            "QEMUT8030ANS", &error_fatal);
    object_property_set_bool(OBJECT(&s->nvme), "is-apple-ans",
                             true, &error_fatal);
    object_property_set_uint(OBJECT(&s->nvme), "max_ioqpairs", 8, &error_fatal);
    object_property_set_uint(OBJECT(&s->nvme), "mdts", 8, &error_fatal);
    object_property_set_uint(OBJECT(&s->nvme), "logical_block_size", 4096, &error_fatal);
    object_property_set_uint(OBJECT(&s->nvme), "physical_block_size", 4096, &error_fatal);

    pcie_host_mmcfg_init(pex, PCIE_MMCFG_SIZE_MAX);
    memory_region_init(&s->io_mmio, OBJECT(s), "ans_pci_mmio", UINT64_MAX);
    memory_region_init(&s->io_ioport, OBJECT(s), "ans_pci_ioport", 64 * 1024);

    pci->bus = pci_register_root_bus(dev, "anspcie.0", apple_ans_set_irq,
                                     pci_swizzle_map_irq_fn, s, &s->io_mmio,
                                     &s->io_ioport, 0, 4, TYPE_PCIE_BUS);

    s->iomems[3] = g_new(MemoryRegion, 1);
    memory_region_init_alias(s->iomems[3], OBJECT(dev), TYPE_APPLE_ANS ".nvme",
                             &s->nvme.iomem, 0, reg[7]);
    sysbus_init_mmio(sbd, s->iomems[3]);

    return sbd;
}

static void apple_ans_realize(DeviceState *dev, Error **errp)
{
    AppleANSState *s = APPLE_ANS(dev);
    PCIHostState *pci = PCI_HOST_BRIDGE(dev);

    pci_realize_and_unref(PCI_DEVICE(&s->nvme), pci->bus, &error_fatal);

    sysbus_realize(SYS_BUS_DEVICE(s->mbox), errp);
}

static void apple_ans_unrealize(DeviceState *dev)
{
    AppleANSState *s = APPLE_ANS(dev);

    qdev_unrealize(DEVICE(s->mbox));
}

static int apple_ans_post_load(void *opaque, int version_id)
{
    AppleANSState *s = APPLE_ANS(opaque);
    if (s->started) {
        apple_ans_start(s);
    }
    return 0;
}

static const VMStateDescription vmstate_apple_ans = {
    .name = "apple_ans",
    .post_load = apple_ans_post_load,
    .fields = (VMStateField[]) {
        VMSTATE_UINT32(nvme_interrupt_idx, AppleANSState),
        VMSTATE_BOOL(started, AppleANSState),

        VMSTATE_END_OF_LIST()
    }
};

static void apple_ans_class_init(ObjectClass *klass, void *data)
{
    DeviceClass *dc = DEVICE_CLASS(klass);

    dc->realize = apple_ans_realize;
    dc->unrealize = apple_ans_unrealize;
    /* dc->reset = apple_ans_reset; */
    dc->desc = "Apple ANS NVMe";
    dc->vmsd = &vmstate_apple_ans;
    set_bit(DEVICE_CATEGORY_BRIDGE, dc->categories);
    dc->fw_name = "pci";
}

static const TypeInfo apple_ans_info = {
    .name = TYPE_APPLE_ANS,
    .parent = TYPE_PCIE_HOST_BRIDGE,
    .instance_size = sizeof(AppleANSState),
    .class_init = apple_ans_class_init,
};

static void apple_ans_register_types(void)
{
    type_register_static(&apple_ans_info);
}

type_init(apple_ans_register_types);
