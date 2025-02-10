// SPDX-License-Identifier: GPL-2.0-or-later
#include <powermeter/Provider.h>
#include <MqttSettings.h>

namespace PowerMeters {

bool Provider::isDataValid() const
{
    return _lastUpdate > 0 && ((millis() - _lastUpdate) < (30 * 1000));
}

void Provider::mqttPublish(String const& topic, float const& value) const
{
    MqttSettings.publish("powermeter/" + topic, String(value));
}

void Provider::mqttLoop() const
{
    if (!MqttSettings.getConnected()) { return; }

    if (!isDataValid()) { return; }

    auto constexpr halfOfAllMillis = std::numeric_limits<uint32_t>::max() / 2;
    if ((_lastUpdate - _lastMqttPublish) > halfOfAllMillis) { return; }

    mqttPublish("powertotal", getPowerTotal());

    doMqttPublish();

    _lastMqttPublish = millis();
}

} // namespace PowerMeters
