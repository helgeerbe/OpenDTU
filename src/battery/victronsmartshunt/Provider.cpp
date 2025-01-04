// SPDX-License-Identifier: GPL-2.0-or-later
#include <battery/victronsmartshunt/Provider.h>
#include <PinMapping.h>
#include <MessageOutput.h>
#include <SerialPortManager.h>

namespace BatteryNs::VictronSmartShunt {

void Provider::deinit()
{
    SerialPortManager.freePort(_serialPortOwner);
}

bool Provider::init(bool verboseLogging)
{
    MessageOutput.println("[VictronSmartShunt] Initialize interface...");

    const PinMapping_t& pin = PinMapping.get();
    MessageOutput.printf("[VictronSmartShunt] Interface rx = %d, tx = %d\r\n",
            pin.battery_rx, pin.battery_tx);

    if (pin.battery_rx < 0) {
        MessageOutput.println("[VictronSmartShunt] Invalid pin config");
        return false;
    }

    auto tx = static_cast<gpio_num_t>(pin.battery_tx);
    auto rx = static_cast<gpio_num_t>(pin.battery_rx);

    auto oHwSerialPort = SerialPortManager.allocatePort(_serialPortOwner);
    if (!oHwSerialPort) { return false; }

    VeDirectShunt.init(rx, tx, &MessageOutput, verboseLogging, *oHwSerialPort);
    return true;
}

void Provider::loop()
{
    VeDirectShunt.loop();

    if (VeDirectShunt.getLastUpdate() <= _lastUpdate) { return; }

    _stats->updateFrom(VeDirectShunt.getData());
    _lastUpdate = VeDirectShunt.getLastUpdate();
}

} // namespace BatteryNs::VictronSmartShunt
