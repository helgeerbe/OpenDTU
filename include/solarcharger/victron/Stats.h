// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <solarcharger/Stats.h>
#include <solarcharger/victron/HassIntegration.h>
#include <VeDirectMpptController.h>
#include <map>

namespace SolarChargers::Victron {

class Stats : public ::SolarChargers::Stats {
public:
    uint32_t getAgeMillis() const final;
    std::optional<float> getOutputPowerWatts() const final;
    std::optional<float> getOutputVoltage() const final;
    std::optional<uint16_t> getPanelPowerWatts() const final;
    std::optional<float> getYieldTotal() const final;
    std::optional<float> getYieldDay() const final;
    std::optional<StateOfOperation> getStateOfOperation() const final;
    std::optional<float> getFloatVoltage() const final;
    std::optional<float> getAbsorptionVoltage() const final;

    void getLiveViewData(JsonVariant& root, const boolean fullUpdate, const uint32_t lastPublish) const final;
    void mqttPublish() const final;
    void mqttPublishSensors(const boolean forcePublish) const final;

    void update(const String serial, const std::optional<VeDirectMpptController::data_t> mpptData, uint32_t lastUpdate) const;

private:
    // TODO(andreasboehm): _data and _lastUpdate in two different structures is not ideal and needs to change
    mutable std::map<String, std::optional<VeDirectMpptController::data_t>> _data;
    mutable std::map<String, uint32_t> _lastUpdate;

    mutable std::map<String, VeDirectMpptController::data_t> _previousData;

    // point of time in millis() when updated values will be published
    mutable uint32_t _nextPublishUpdatesOnly = 0;

    // point of time in millis() when all values will be published
    mutable uint32_t _nextPublishFull = 1;

    mutable bool _PublishFull;

    HassIntegration _hassIntegration;

    void populateJsonWithInstanceStats(const JsonObject &root, const VeDirectMpptController::data_t &mpptData) const;

    void publishMpptData(const VeDirectMpptController::data_t &mpptData, const VeDirectMpptController::data_t &frame) const;
};

} // namespace SolarChargers::Victron
