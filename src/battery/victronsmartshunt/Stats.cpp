// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <battery/victronsmartshunt/Stats.h>

namespace BatteryNs::VictronSmartShunt {

void Stats::updateFrom(VeDirectShuntController::data_t const& shuntData) {
    ::BatteryNs::Stats::setVoltage(shuntData.batteryVoltage_V_mV / 1000.0, millis());
    ::BatteryNs::Stats::setSoC(static_cast<float>(shuntData.SOC) / 10, 1/*precision*/, millis());
    ::BatteryNs::Stats::setCurrent(static_cast<float>(shuntData.batteryCurrent_I_mA) / 1000, 2/*precision*/, millis());
    _fwversion = shuntData.getFwVersionFormatted();

    _chargeCycles = shuntData.H4;
    _timeToGo = shuntData.TTG / 60;
    _chargedEnergy = static_cast<float>(shuntData.H18) / 100;
    _dischargedEnergy = static_cast<float>(shuntData.H17) / 100;
    setManufacturer(String("Victron ") + shuntData.getPidAsString().data());
    _temperature = shuntData.T;
    _tempPresent = shuntData.tempPresent;
    _midpointVoltage = static_cast<float>(shuntData.VM) / 1000;
    _midpointDeviation = static_cast<float>(shuntData.DM) / 10;
    _instantaneousPower = shuntData.P;
    _consumedAmpHours = static_cast<float>(shuntData.CE) / 1000;
    _lastFullCharge = shuntData.H9 / 60;
    // shuntData.AR is a bitfield, so we need to check each bit individually
    _alarmLowVoltage = shuntData.alarmReason_AR & 1;
    _alarmHighVoltage = shuntData.alarmReason_AR & 2;
    _alarmLowSOC = shuntData.alarmReason_AR & 4;
    _alarmLowTemperature = shuntData.alarmReason_AR & 32;
    _alarmHighTemperature = shuntData.alarmReason_AR & 64;

    _lastUpdate = VeDirectShunt.getLastUpdate();
}

void Stats::getLiveViewData(JsonVariant& root) const {
    ::BatteryNs::Stats::getLiveViewData(root);

    // values go into the "Status" card of the web application
    addLiveViewValue(root, "chargeCycles", _chargeCycles, "", 0);
    addLiveViewValue(root, "chargedEnergy", _chargedEnergy, "kWh", 2);
    addLiveViewValue(root, "dischargedEnergy", _dischargedEnergy, "kWh", 2);
    addLiveViewValue(root, "instantaneousPower", _instantaneousPower, "W", 0);
    addLiveViewValue(root, "consumedAmpHours", _consumedAmpHours, "Ah", 3);
    addLiveViewValue(root, "midpointVoltage", _midpointVoltage, "V", 2);
    addLiveViewValue(root, "midpointDeviation", _midpointDeviation, "%", 1);
    addLiveViewValue(root, "lastFullCharge", _lastFullCharge, "min", 0);
    if (_tempPresent) {
        addLiveViewValue(root, "temperature", _temperature, "°C", 0);
    }

    addLiveViewAlarm(root, "lowVoltage", _alarmLowVoltage);
    addLiveViewAlarm(root, "highVoltage", _alarmHighVoltage);
    addLiveViewAlarm(root, "lowSOC", _alarmLowSOC);
    addLiveViewAlarm(root, "lowTemperature", _alarmLowTemperature);
    addLiveViewAlarm(root, "highTemperature", _alarmHighTemperature);
}

void Stats::mqttPublish() const {
    ::BatteryNs::Stats::mqttPublish();

    MqttSettings.publish("battery/chargeCycles", String(_chargeCycles));
    MqttSettings.publish("battery/chargedEnergy", String(_chargedEnergy));
    MqttSettings.publish("battery/dischargedEnergy", String(_dischargedEnergy));
    MqttSettings.publish("battery/instantaneousPower", String(_instantaneousPower));
    MqttSettings.publish("battery/consumedAmpHours", String(_consumedAmpHours));
    MqttSettings.publish("battery/lastFullCharge", String(_lastFullCharge));
    MqttSettings.publish("battery/midpointVoltage", String(_midpointVoltage));
    MqttSettings.publish("battery/midpointDeviation", String(_midpointDeviation));
}

} // namespace BatteryNs::VictronSmartShunt
