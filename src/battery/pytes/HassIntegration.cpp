// SPDX-License-Identifier: GPL-2.0-or-later

#include <battery/pytes/HassIntegration.h>

namespace BatteryNs::Pytes {

void HassIntegration::publishSensors() const
{
    ::BatteryNs::HassIntegration::publishSensors();

    publishSensor("Charge voltage (BMS)", NULL, "settings/chargeVoltage", "voltage", "measurement", "V");
    publishSensor("Charge current limit", NULL, "settings/chargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge current limit", NULL, "settings/dischargeCurrentLimitation", "current", "measurement", "A");
    publishSensor("Discharge voltage limit", NULL, "settings/dischargeVoltageLimitation", "voltage", "measurement", "V");

    publishSensor("Voltage", "mdi:battery-charging", "voltage", "voltage", "measurement", "V");
    publishSensor("Current", "mdi:current-dc", "current", "current", "measurement", "A");
    publishSensor("State of Health (SOH)", "mdi:heart-plus", "stateOfHealth", NULL, "measurement", "%");
    publishSensor("Temperature", "mdi:thermometer", "temperature", "temperature", "measurement", "°C");
    publishSensor("Charge Cycles", "mdi:counter", "chargeCycles");

    publishSensor("Charged Energy", NULL, "chargedEnergy", "energy", "total_increasing", "kWh");
    publishSensor("Discharged Energy", NULL, "dischargedEnergy", "energy", "total_increasing", "kWh");

    publishSensor("Total Capacity", NULL, "capacity");
    publishSensor("Available Capacity", NULL, "availableCapacity");

    publishSensor("Cell Min Voltage", NULL, "CellMinMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Max Voltage", NULL, "CellMaxMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Voltage Diff", "mdi:battery-alert", "CellDiffMilliVolt", "voltage", "measurement", "mV");
    publishSensor("Cell Min Temperature", NULL, "CellMinTemperature", "temperature", "measurement", "°C");
    publishSensor("Cell Max Temperature", NULL, "CellMaxTemperature", "temperature", "measurement", "°C");

    publishSensor("Cell Min Voltage Label", NULL, "CellMinVoltageName");
    publishSensor("Cell Max Voltage Label", NULL, "CellMaxVoltageName");
    publishSensor("Cell Min Temperature Label", NULL, "CellMinTemperatureName");
    publishSensor("Cell Max Temperature Label", NULL, "CellMaxTemperatureName");

    publishSensor("Modules Online", "mdi:counter", "modulesOnline");
    publishSensor("Modules Offline", "mdi:counter", "modulesOffline");
    publishSensor("Modules Blocking Charge", "mdi:counter", "modulesBlockingCharge");
    publishSensor("Modules Blocking Discharge", "mdi:counter", "modulesBlockingDischarge");

    publishBinarySensor("Alarm Discharge current", "mdi:alert", "alarm/overCurrentDischarge", "1", "0");
    publishBinarySensor("Alarm High charge current", "mdi:alert", "alarm/overCurrentCharge", "1", "0");
    publishBinarySensor("Alarm Voltage low", "mdi:alert", "alarm/underVoltage", "1", "0");
    publishBinarySensor("Alarm Voltage high", "mdi:alert", "alarm/overVoltage", "1", "0");
    publishBinarySensor("Alarm Temperature low", "mdi:thermometer-low", "alarm/underTemperature", "1", "0");
    publishBinarySensor("Alarm Temperature high", "mdi:thermometer-high", "alarm/overTemperature", "1", "0");
    publishBinarySensor("Alarm Temperature low (charge)", "mdi:thermometer-low", "alarm/underTemperatureCharge", "1", "0");
    publishBinarySensor("Alarm Temperature high (charge)", "mdi:thermometer-high", "alarm/overTemperatureCharge", "1", "0");
    publishBinarySensor("Alarm BMS internal", "mdi:alert", "alarm/bmsInternal", "1", "0");
    publishBinarySensor("Alarm Cell Imbalance", "mdi:alert-outline", "alarm/cellImbalance", "1", "0");

    publishBinarySensor("Warning Discharge current", "mdi:alert-outline", "warning/highCurrentDischarge", "1", "0");
    publishBinarySensor("Warning High charge current", "mdi:alert-outline", "warning/highCurrentCharge", "1", "0");
    publishBinarySensor("Warning Voltage low", "mdi:alert-outline", "warning/lowVoltage", "1", "0");
    publishBinarySensor("Warning Voltage high", "mdi:alert-outline", "warning/highVoltage", "1", "0");
    publishBinarySensor("Warning Temperature low", "mdi:thermometer-low", "warning/lowTemperature", "1", "0");
    publishBinarySensor("Warning Temperature high", "mdi:thermometer-high", "warning/highTemperature", "1", "0");
    publishBinarySensor("Warning Temperature low (charge)", "mdi:thermometer-low", "warning/lowTemperatureCharge", "1", "0");
    publishBinarySensor("Warning Temperature high (charge)", "mdi:thermometer-high", "warning/highTemperatureCharge", "1", "0");
    publishBinarySensor("Warning BMS internal", "mdi:alert-outline", "warning/bmsInternal", "1", "0");
    publishBinarySensor("Warning Cell Imbalance", "mdi:alert-outline", "warning/cellImbalance", "1", "0");

    publishBinarySensor("Balancing Active", "mdi:scale-balance", "balancingActive", "1", "0");
    publishBinarySensor("Charge immediately", "mdi:alert", "charging/chargeImmediately", "1", "0");
}

} // namespace BatteryNs::Pytes
