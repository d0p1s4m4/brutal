#pragma once

#include <brutal/base.h>
#include <stdint.h>

/* vendors ------------------------------------------------------------------ */

#define VENDOR_REALTEK_SEMI 0x10EC

/* devices ID --------------------------------------------------------------- */

#define DEVID_RTL8100 0x8139 /* RTL-8100/8101L/8139 PCI Fast Eth Adapter */

/* nic drivers abstraction -------------------------------------------------- */

typedef struct {
    void *(*init)(uint32_t, uint16_t);
    void (*deinit)(void *);
    void (*handle)(void *, uint16_t);
    void (*send)(void *, void *, size_t);
    uint8_t *(*get_mac)(void *);
} NicDriver;

typedef struct{
    uint16_t vendor;
    uint16_t device;
    void *driver;
} NicDevice;

extern NicDriver rtl8139_driver;

extern NicDevice nic_device_list[];
