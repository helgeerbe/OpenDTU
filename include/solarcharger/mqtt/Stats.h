// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <solarcharger/Stats.h>

namespace SolarChargers::Mqtt {

class Stats : public ::SolarChargers::Stats {
friend class Provider;

public:
    // the last time *any* data was updated
    uint32_t getAgeMillis() const final { return millis() - _lastUpdate; }

    std::optional<float> getOutputPowerWatts() const final;
    std::optional<float> getOutputVoltage() const final;
    std::optional<uint16_t> getPanelPowerWatts() const final { return std::nullopt; }
    std::optional<float> getYieldTotal() const final { return std::nullopt; }
    std::optional<float> getYieldDay() const final { return std::nullopt; }
    std::optional<StateOfOperation> getStateOfOperation() const final { return std::nullopt; }
    std::optional<float> getFloatVoltage() const final { return std::nullopt; }
    std::optional<float> getAbsorptionVoltage() const final { return std::nullopt; }

    void getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const final;

    // no need to republish values received via mqtt
    void mqttPublish() const final {}

    // no need to republish values received via mqtt
    void mqttPublishSensors(const boolean forcePublish) const final {}

protected:
    std::optional<float> getOutputCurrent() const;

    void setOutputPowerWatts(const float powerWatts) {
        _outputPowerWatts = powerWatts;
        _lastUpdateOutputPowerWatts = _lastUpdate =  millis();
    }

    void setOutputVoltage(const float voltage);

    void setOutputCurrent(const float current);

private:
    uint32_t _lastUpdate = 0;

    float _outputPowerWatts = 0;
    uint32_t _lastUpdateOutputPowerWatts = 0;

    float _outputVoltage = 0;
    uint32_t _lastUpdateOutputVoltage = 0;

    float _outputCurrent = 0;
    uint32_t _lastUpdateOutputCurrent = 0;

    std::optional<float> getValueIfNotOutdated(const uint32_t lastUpdate, const float value) const;
};

} // namespace SolarChargers::Mqtt
