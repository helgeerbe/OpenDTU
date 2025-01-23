// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <Configuration.h>
#include <powermeter/Provider.h>
#include <espMqttClient.h>
#include <vector>
#include <mutex>
#include <array>

namespace PowerMeters::Mqtt {

class Provider : public ::PowerMeters::Provider {
public:
    explicit Provider(PowerMeterMqttConfig const& cfg)
        : _cfg(cfg) { }

    ~Provider();

    bool init() final;
    void loop() final { }
    float getPowerTotal() const final;

private:
    using MsgProperties = espMqttClientTypes::MessageProperties;
    void onMessage(MsgProperties const& properties, char const* topic,
            uint8_t const* payload, size_t len, size_t index,
            size_t total, float* targetVariable, PowerMeterMqttValue const* cfg);

    // we don't need to republish data received from MQTT
    void doMqttPublish() const final { };

    PowerMeterMqttConfig const _cfg;

    using power_values_t = std::array<float, POWERMETER_MQTT_MAX_VALUES>;
    power_values_t _powerValues;

    std::vector<String> _mqttSubscriptions;

    mutable std::mutex _mutex;
};

} // namespace PowerMeters::Mqtt
