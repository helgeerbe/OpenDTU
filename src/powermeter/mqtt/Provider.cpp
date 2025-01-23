// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/mqtt/Provider.h>
#include <MqttSettings.h>
#include <MessageOutput.h>
#include <ArduinoJson.h>
#include <Utils.h>

namespace PowerMeters::Mqtt {

bool Provider::init()
{
    auto subscribe = [this](PowerMeterMqttValue const& val, float* targetVariable) {
        *targetVariable = 0;
        char const* topic = val.Topic;
        if (strlen(topic) == 0) { return; }
        MqttSettings.subscribe(topic, 0,
                std::bind(&Provider::onMessage,
                    this, std::placeholders::_1, std::placeholders::_2,
                    std::placeholders::_3, std::placeholders::_4,
                    std::placeholders::_5, std::placeholders::_6,
                    targetVariable, &val)
                );
        _mqttSubscriptions.push_back(topic);
    };

    for (size_t i = 0; i < _powerValues.size(); ++i) {
        subscribe(_cfg.Values[i], &_powerValues[i]);
    }

    return _mqttSubscriptions.size() > 0;
}

Provider::~Provider()
{
    for (auto const& t: _mqttSubscriptions) { MqttSettings.unsubscribe(t); }
    _mqttSubscriptions.clear();
}

void Provider::onMessage(Provider::MsgProperties const& properties,
        char const* topic, uint8_t const* payload, size_t len, size_t index,
        size_t total, float* targetVariable, PowerMeterMqttValue const* cfg)
{
    auto extracted = Utils::getNumericValueFromMqttPayload<float>("PowerMeters::Mqtt",
            std::string(reinterpret_cast<const char*>(payload), len), topic,
            cfg->JsonPath);

    if (!extracted.has_value()) { return; }

    float newValue = *extracted;

    using Unit_t = PowerMeterMqttValue::Unit;
    switch (cfg->PowerUnit) {
        case Unit_t::MilliWatts:
            newValue /= 1000;
            break;
        case Unit_t::KiloWatts:
            newValue *= 1000;
            break;
        default:
            break;
    }

    if (cfg->SignInverted) { newValue *= -1; }

    {
        std::lock_guard<std::mutex> l(_mutex);
        *targetVariable = newValue;
    }

    if (_verboseLogging) {
        MessageOutput.printf("[PowerMeters::Mqtt] Topic '%s': new value: %5.2f, "
                "total: %5.2f\r\n", topic, newValue, getPowerTotal());
    }

    gotUpdate();
}

float Provider::getPowerTotal() const
{
    float sum = 0.0;
    std::unique_lock<std::mutex> lock(_mutex);
    for (auto v: _powerValues) { sum += v; }
    return sum;
}

} // namespace PowerMeters::Mqtt
