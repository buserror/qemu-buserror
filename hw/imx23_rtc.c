/*
 * imx23_rtc.c
 *
 * Copyright: Michel Pollet <buserror@gmail.com>
 *
 * QEMU Licence
 */
/*
 * Implement the RTC block of the imx23. At least, the real time stuff.
 * TODO Alarm, and watchdog ?
 */

#include "sysbus.h"
#include "imx23.h"

#define D(w)

enum {
    RTC_CTRL = 0x0,
    RTC_STAT = 0x1,
    RTC_MS = 0x2,
    RTC_SECONDS = 0x3,
    RTC_ALARM = 0x4,
    RTC_WATCHDOG = 0x5,
    RTC_PERSISTENT0 = 0x6,
    RTC_PERSISTENT1 = 0x7,
    RTC_PERSISTENT2 = 0x8,
    RTC_PERSISTENT3 = 0x9,
    RTC_PERSISTENT4 = 0xa,
    RTC_PERSISTENT5 = 0xa,
    RTC_DEBUG = 0xc,
    RTC_VERSION = 0xd,

    RTC_MAX,
};
typedef struct imx23_rtc_state {
    SysBusDevice busdev;
    MemoryRegion iomem;
    uint32_t base_ms, base_s;
    uint32_t r[RTC_MAX];
    qemu_irq alarm_irq;
    CharDriverState *chr;
} imx23_rtc_state;

static uint64_t imx23_rtc_read(
        void *opaque, hwaddr offset, unsigned size)
{
    imx23_rtc_state *s = (imx23_rtc_state *) opaque;
    uint32_t res = 0;

    D(printf("%s %04x (%d) = ", __func__, (int)offset, size);)
    switch (offset >> 4) {
        case 0 ... RTC_MAX:
            if ((offset >> 4) == RTC_MS || (offset >> 4) == RTC_SECONDS) {
                struct timeval tv;
                gettimeofday(&tv, NULL);
                s->r[RTC_MS] = (tv.tv_usec / 1000) - s->base_ms;
                s->r[RTC_SECONDS] = tv.tv_sec - s->base_s;
            }
            res = s->r[offset >> 4];
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                    "%s: bad offset 0x%x\n", __func__, (int) offset);
            break;
    }
    D(printf("%08x\n", res);)

    return res;
}

static void imx23_rtc_write(void *opaque, hwaddr offset,
        uint64_t value, unsigned size)
{
    imx23_rtc_state *s = (imx23_rtc_state *) opaque;
    uint32_t oldvalue = 0;

    D(printf("%s %04x %08x(%d)\n", __func__, (int)offset, (int)value, size);)
    switch (offset >> 4) {
        case 0 ... RTC_MAX:
            imx23_write(&s->r[offset >> 4], offset, value, size);
            break;
        default:
            qemu_log_mask(LOG_GUEST_ERROR,
                    "%s: bad offset 0x%x\n", __func__, (int) offset);
            break;
    }
    switch (offset >> 4) {
        case RTC_MS:
            s->base_ms = s->r[RTC_MS];
            break;
        case RTC_SECONDS:
            s->base_s = s->r[RTC_SECONDS];
            break;
        case RTC_CTRL:
            if ((oldvalue ^ s->r[RTC_CTRL]) == 0x80000000
                    && !(oldvalue & 0x80000000)) {
                D(printf("%s reseting, anding clockgate\n", __func__);)
                s->r[RTC_CTRL] |= 0x40000000;
            }
            break;
    }
}

static const MemoryRegionOps imx23_rtc_ops = {
    .read = imx23_rtc_read,
    .write = imx23_rtc_write,
    .endianness = DEVICE_NATIVE_ENDIAN,
};

static int imx23_rtc_init(SysBusDevice *dev)
{
    imx23_rtc_state *s = FROM_SYSBUS(imx23_rtc_state, dev);

    sysbus_init_irq(dev, &s->alarm_irq);
    memory_region_init_io(&s->iomem, &imx23_rtc_ops, s,
            "imx23_rtc", 0x2000);
    sysbus_init_mmio(dev, &s->iomem);

    s->r[RTC_CTRL] = 0xc0000000;
    s->r[RTC_STAT] = 0xe80f0000;
    s->r[RTC_VERSION] = 0x02000000;
 //   s->base_s = time(NULL);
    return 0;
}


static void imx23_rtc_class_init(ObjectClass *klass, void *data)
{
    SysBusDeviceClass *sdc = SYS_BUS_DEVICE_CLASS(klass);

    sdc->init = imx23_rtc_init;
}

static TypeInfo rtc_info = {
    .name          = "imx23_rtc",
    .parent        = TYPE_SYS_BUS_DEVICE,
    .instance_size = sizeof(imx23_rtc_state),
    .class_init    = imx23_rtc_class_init,
};

static void imx23_rtc_register(void)
{
    type_register_static(&rtc_info);
}

type_init(imx23_rtc_register)
