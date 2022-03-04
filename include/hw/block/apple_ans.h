#ifndef APPLE_ANS_H
#define APPLE_ANS_H

#include "qemu/queue.h"
#include "hw/sysbus.h"
#include "qom/object.h"
#include "hw/arm/apple/xnu_dtb.h"

SysBusDevice *apple_ans_create(DTBNode* node, uint32_t build_version);

#endif /* APPLE_ANS_H */
