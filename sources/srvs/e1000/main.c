#include <ahci/ahci.h>
#include <ahci/device.h>
#include <protos/pci.h>
#include <brutal-alloc>
#include <brutal-debug>

#define VENDOR_INTEL 0x8086

#define DEVID_E1000_QEMU 0x100E
#define DEVID_E1000_82544GC 0x100C
#define DEVID_E1000_82545EM 0x100F
#define DEVID_E1000_82574L 0x10d3

static PciIdentifier compatible_nic[] = {
    {.vendor = 0x8086 /* intel */, .device = 0x10d3 /* 82574L */, .class = 0x2, .subclass = 0},
    {.vendor = 0x8086 /* intel */, .device = 0x100F /* 82545EM */, .class = 0x2, .subclass = 0},
    {.vendor = 0x8086 /* intel */, .device = 0x100C /* 82544GC */, .class = 0x2, .subclass = 0},
    {.vendor = 0x8086 /* intel */, .device = 0x100E /* QEMU */, .class = 0x2, .subclass = 0}
};
static size_t compatible_length = sizeof(compatible_nic)/sizeof(PciIdentifier);

int ipc_component_main(IpcComponent *self)
{
    IpcCap pci_bus = ipc_component_require(self, IPC_PCI_BUS_PROTO);

    PciAddr e1000_device;
    size_t idx;
    for (idx = 0; idx < compatible_length; idx++)
    {
        if (pci_bus_query_rpc(self, pci_bus, &compatible_nic[idx], &e1000_device, alloc_global()) == IPC_SUCCESS)
        {
            log$("E1000 network controller found on bus: {} func: {} seg: {} slot: {}",
                e1000_device.bus, e1000_device.func, e1000_device.seg, e1000_device.slot);
            break;
        }
    }

    if (idx >= compatible_length)
    {
        log$("No E1000 network controller found!");
        return 0;
    }

    PciBarInfo pci_bar = {};
    pci_bus_bar_rpc(self, pci_bus, &(PciBusBarRequest){.addr = e1000_device, .num = 5}, &pci_bar, alloc_global());

    log$("bar[5] {#x}-{#x} type: {}", pci_bar.base, pci_bar.base + pci_bar.size, pci_bar.type);

    return ipc_component_run(self);
}
