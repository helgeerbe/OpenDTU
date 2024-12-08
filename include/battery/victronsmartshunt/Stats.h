// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <VeDirectShuntController.h>
#include <battery/Stats.h>

namespace BatteryNs::VictronSmartShunt {

class Stats : public ::BatteryNs::Stats {
public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;

    void updateFrom(VeDirectShuntController::data_t const& shuntData);

private:
    float _temperature;
    bool _tempPresent;
    uint8_t _chargeCycles;
    uint32_t _timeToGo;
    float _chargedEnergy;
    float _dischargedEnergy;
    int32_t _instantaneousPower;
    float _midpointVoltage;
    float _midpointDeviation;
    float _consumedAmpHours;
    int32_t _lastFullCharge;

    bool _alarmLowVoltage;
    bool _alarmHighVoltage;
    bool _alarmLowSOC;
    bool _alarmLowTemperature;
    bool _alarmHighTemperature;
};

} // namespace BatteryNs::VictronSmartShunt
