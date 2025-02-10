// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <solarcharger/Stats.h>

namespace SolarChargers {

class DummyStats : public Stats {
public:
    uint32_t getAgeMillis() const final { return 0; }
    std::optional<float> getOutputPowerWatts() const final { return std::nullopt; }
    std::optional<float> getOutputVoltage() const final { return std::nullopt; }
    std::optional<uint16_t> getPanelPowerWatts() const final { return std::nullopt; }
    std::optional<float> getYieldTotal() const final { return std::nullopt; }
    std::optional<float> getYieldDay() const final { return std::nullopt; }
    std::optional<StateOfOperation> getStateOfOperation() const final { return std::nullopt; }
    std::optional<float> getFloatVoltage() const final { return std::nullopt; }
    std::optional<float> getAbsorptionVoltage() const final { return std::nullopt; }
    void getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const final {}
    void mqttPublish() const final {}
    void mqttPublishSensors(const boolean forcePublish) const final {}
};

} // namespace SolarChargers
