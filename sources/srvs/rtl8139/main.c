#include <ahci/ahci.h>
#include <ahci/device.h>
#include <protos/pci.h>
#include <brutal-alloc>
#include <brutal-debug>
#include "ipc/ipc.h"
#include "rtl8139.h"

int ipc_component_main(IpcComponent *self)
{
    IpcCap pci_bus = ipc_component_require(self, IPC_PCI_BUS_PROTO);

    PciIdentifier id = {
        .vendor = 0x10EC, /* Realtek */
        .device = 0x8139, /* RTL-8100/8101L/8139 PCI Fast Eth Adapter */
        .class = 0x02, /* network controller */
        .subclass = 0,
    };

    PciAddr rtl8139_device;
    if (pci_bus_query_rpc(self, pci_bus, &id, &rtl8139_device, alloc_global()) != IPC_SUCCESS)
    {
        log$("No RTL8139 network controller found!");
        return 0;
    }

    log$("RTL8139 network controller found on bus: {} func: {} seg: {} slot: {}",
        rtl8139_device.bus, rtl8139_device.func, rtl8139_device.seg, rtl8139_device.slot);

    PciBarInfo pci_bar = {};
    for (int num = 0; num < 6; num++)
    {
        pci_bus_bar_rpc(self, pci_bus, &(PciBusBarRequest){.addr = rtl8139_device, .num = num}, &pci_bar, alloc_global());
        if (pci_bar.type == PCI_BAR_PIO)
        {
            log$("bar[{}] {#x}-{#x} type: {}", num, pci_bar.base, pci_bar.base + pci_bar.size, pci_bar.type);
            break;
        }
    }

    uint8_t int_line = 0xFF;
    pci_bus_enable_irq_rpc(self, pci_bus, &(PciBusEnableIrqRequest){.addr = rtl8139_device}, &int_line, alloc_global());

    if (int_line == 0xFF)
    {
        return 0;
    }

    return ipc_component_run(self);
}
