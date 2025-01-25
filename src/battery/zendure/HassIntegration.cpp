// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/zendure/HassIntegration.h>

namespace BatteryNs::Zendure {

void HassIntegration::publishSensors() const
{
    ::BatteryNs::HassIntegration::publishSensors();

    publishSensor("Voltage", "mdi:battery-charging", "voltage", "voltage", "measurement", "V");
    publishSensor("Current", "mdi:current-dc", "current", "current", "measurement", "A");

    publishSensor("Cell Min Voltage", NULL, "CellMinMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Average Voltage", NULL, "CellAvgMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Voltage", NULL, "CellMaxMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Temperature", NULL, "CellMaxTemperature", "temperature", "measurement", "°C");
    publishSensor("Charge Power", "mdi:battery-charging", "chargePower", "power", "measurement", "W");
    publishSensor("Discharge Power", "mdi:battery-discharging", "dischargePower", "power", "measurement", "W");
    publishBinarySensor("Battery Heating", NULL, "heating", "1", "0");
    publishSensor("State", NULL, "state");
    publishSensor("Number of Batterie Packs", "mdi:counter", "numPacks");
    publishSensor("Efficiency", NULL, "efficiency", NULL, "measurement", "%");
    publishSensor("Last Full Charge", "mdi:timelapse", "lastFullCharge", NULL, NULL, "h");

    // auto stats = std::reinterpret_pointer_cast<const ZendureBatteryStats>(Battery.getStats());
    // if (stats)
    // {
    //     for (const auto& [i, value] : stats->getPackDataList()) {
    //         if (!value) {
    //             continue;
    //         }
    //         auto id = String(i) + "/";
    //         publishSensor("Cell Min Voltage", NULL, String(id + "CellMinMilliVolt").c_str(), "voltage", "measurement", "mV");
    //         publishSensor("Cell Average Voltage", NULL, String(id + "CellAvgMilliVolt").c_str(), "voltage", "measurement", "mV");
    //         publishSensor("Cell Max Voltage", NULL, String(id + "CellMaxMilliVolt").c_str(), "voltage", "measurement", "mV");
    //         publishSensor("Cell Voltage Diff", "mdi:battery-alert", String(id + "CellDiffMilliVolt").c_str(), "voltage", "measurement", "mV");
    //         publishSensor("Cell Max Temperature", NULL, String(id + "CellMaxTemperature").c_str(), "temperature", "measurement", "°C");
    //         publishSensor("Power", NULL, String(id + "power").c_str(), "power", "measurement", "W");
    //         publishSensor("Voltage", NULL, String(id + "voltage").c_str(), "voltage", "measurement", "V");
    //         publishSensor("Current", NULL, String(id + "current").c_str(), "current", "measurement", "A");
    //         publishSensor("State Of Charge", NULL, String(id + "stateOfCharge").c_str(), NULL, "measurement", "%");
    //         publishSensor("State Of Health", NULL, String(id + "stateOfHealth").c_str(), NULL, "measurement", "%");
    //         publishSensor("State", NULL, "state");
    //     }
    // }

    publishSensor("Solar Power MPPT 1", "mdi:solar-power", "solarPowerMppt1", "power", "measurement", "W");
    publishSensor("Solar Power MPPT 2", "mdi:solar-power", "solarPowerMppt2", "power", "measurement", "W");
    publishSensor("Total Output Power", NULL, "outputPower", "power", "measurement", "W");
    publishSensor("Total Input Power", NULL, "inputPower", "power", "measurement", "W");
    publishBinarySensor("Bypass State", NULL, "bypass", "1", "0");

    publishSensor("Output Power Limit", NULL, "settings/outputLimitPower", "power", "settings", "W");
    publishSensor("Input Power Limit", NULL, "settings/inputLimitPower", "power", "settings", "W");
    publishSensor("Minimum State of Charge", NULL, "settings/stateOfChargeMin", NULL, "settings", "%");
    publishSensor("Maximum State of Charge", NULL, "settings/stateOfChargeMax", NULL, "settings", "%");
    publishSensor("Bypass Mode", NULL, "settings/bypassMode", "settings");
}

} // namespace BatteryNs::Zendure
