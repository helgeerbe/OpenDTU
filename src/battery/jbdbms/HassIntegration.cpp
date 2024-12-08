// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/jbdbms/HassIntegration.h>

namespace BatteryNs::JbdBms {

void HassIntegration::publishSensors() const
{
    ::BatteryNs::HassIntegration::publishSensors();

    //            caption                  icon                    topic                    dev. class     state class    unit
    publishSensor("Voltage",               "mdi:battery-charging", "voltage",               "voltage",     "measurement", "V");
    publishSensor("Current",               "mdi:current-dc",       "current",               "current",     "measurement", "A");
    publishSensor("Cell Min Voltage",      NULL,                   "CellMinMilliVolt",      "voltage",     "measurement", "mV");
    publishSensor("Cell Average Voltage",  NULL,                   "CellAvgMilliVolt",      "voltage",     "measurement", "mV");
    publishSensor("Cell Max Voltage",      NULL,                   "CellMaxMilliVolt",      "voltage",     "measurement", "mV");
    publishSensor("Cell Voltage Diff",     "mdi:battery-alert",    "CellDiffMilliVolt",     "voltage",     "measurement", "mV");
    publishSensor("Battery Temperature 1", "mdi:thermometer",      "BatteryTempOneCelsius", "temperature", "measurement", "°C");
    publishSensor("Battery Temperature 2", "mdi:thermometer",      "BatteryTempTwoCelsius", "temperature", "measurement", "°C");
    publishSensor("Alarms Bitmask",        NULL,                   "AlarmsBitmask");
    publishSensor("Charge Cycles",         "mdi:counter",          "BatteryCycles");
    publishSensor("Available Capacity",    NULL,                   "ActualBatteryCapacityAmpHours");

    publishBinarySensor("Charging enabled",    "mdi:battery-arrow-up",   "BatteryChargeEnabled",    "yes", "no");
    publishBinarySensor("Discharging enabled", "mdi:battery-arrow-down", "BatteryDischargeEnabled", "yes", "no");
    publishBinarySensor("Balancing enabled",   "mdi:scale-balance",      "BatteryBalancingEnabled", "yes", "no");

#define PBS(a, b, c) publishBinarySensor("Alarm: " a, "mdi:" b, "alarms/" c, "1", "0")
    PBS("Cell Overvoltage",             "battery-alert",                 "CellOverVoltage");
    PBS("Cell Undervoltage",            "battery-alert",                 "CellUnderVoltage");
    PBS("Pack Overvoltage",             "fuse-alert",                    "PackOverVoltage");
    PBS("Pack Undervoltage",            "fuse-alert",                    "PackUnderVoltage");
    PBS("Charging Overtemperature",     "thermometer-alert",             "ChargingOverTemperature");
    PBS("Charging Undertemperature",    "thermometer-alert",             "ChargingLowTemperature");
    PBS("Discharging Overtemperature",  "thermometer-alert",             "DischargingOverTemperature");
    PBS("Discharging Undertemperature", "thermometer-alert",             "DischargingLowTemperature");
    PBS("Charging Overcurrent",         "fuse-alert",                    "ChargingOverCurrent");
    PBS("Discharging Overcurrent",      "fuse-alert",                    "DischargeOverCurrent");
    PBS("Short Circuit",                "fuse-alert",                    "ShortCircuit");
    PBS("IC Frontend Error",            "battery-alert-variant-outline", "IcFrontEndError");
    PBS("MOS Software Lock",            "battery-alert-variant-outline", "MosSoftwareLock");
    PBS("Reserved 1",                   "battery-alert-variant-outline", "Reserved1");
    PBS("Reserved 2",                   "battery-alert-variant-outline", "Reserved2");
    PBS("Reserved 3",                   "battery-alert-variant-outline", "Reserved3");
#undef PBS
}

} // namespace BatteryNs::JbdBms
