// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/jkbms/HassIntegration.h>

namespace BatteryNs::JkBms {

void HassIntegration::publishSensors() const
{
    ::BatteryNs::HassIntegration::publishSensors();

    //            caption              icon                    topic                       dev. class     state class    unit
    publishSensor("Voltage",           "mdi:battery-charging", "BatteryVoltageMilliVolt",  "voltage",     "measurement", "mV");
    publishSensor("Current",           "mdi:current-dc",       "BatteryCurrentMilliAmps",  "current",     "measurement", "mA");
    publishSensor("BMS Temperature",   "mdi:thermometer",      "BmsTempCelsius",           "temperature", "measurement", "°C");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert",    "CellDiffMilliVolt",        "voltage",     "measurement", "mV");
    publishSensor("Battery Temperature 1", "mdi:thermometer",  "BatteryTempOneCelsius",    "temperature", "measurement", "°C");
    publishSensor("Battery Temperature 2", "mdi:thermometer",  "BatteryTempTwoCelsius",    "temperature", "measurement", "°C");
    publishSensor("Charge Cycles",     "mdi:counter",          "BatteryCycles");
    publishSensor("Cycle Capacity",    "mdi:battery-sync",     "BatteryCycleCapacity");

    publishBinarySensor("Charging Possible",    "mdi:battery-arrow-up",   "status/ChargingActive",    "1", "0");
    publishBinarySensor("Discharging Possible", "mdi:battery-arrow-down", "status/DischargingActive", "1", "0");
    publishBinarySensor("Balancing Active",     "mdi:scale-balance",      "status/BalancingActive",   "1", "0");

#define PBS(a, b, c) publishBinarySensor("Alarm: " a, "mdi:" b, "alarms/" c, "1", "0")
    PBS("Low Capacity",                "battery-alert-variant-outline", "LowCapacity");
    PBS("BMS Overtemperature",         "thermometer-alert",             "BmsOvertemperature");
    PBS("Charging Overvoltage",        "fuse-alert",                    "ChargingOvervoltage");
    PBS("Discharge Undervoltage",      "fuse-alert",                    "DischargeUndervoltage");
    PBS("Battery Overtemperature",     "thermometer-alert",             "BatteryOvertemperature");
    PBS("Charging Overcurrent",        "fuse-alert",                    "ChargingOvercurrent");
    PBS("Discharging Overcurrent",     "fuse-alert",                    "DischargeOvercurrent");
    PBS("Cell Voltage Difference",     "battery-alert",                 "CellVoltageDifference");
    PBS("Battery Box Overtemperature", "thermometer-alert",             "BatteryBoxOvertemperature");
    PBS("Battery Undertemperature",    "thermometer-alert",             "BatteryUndertemperature");
    PBS("Cell Overvoltage",            "battery-alert",                 "CellOvervoltage");
    PBS("Cell Undervoltage",           "battery-alert",                 "CellUndervoltage");
#undef PBS
}

} // namespace BatteryNs::JkBms
