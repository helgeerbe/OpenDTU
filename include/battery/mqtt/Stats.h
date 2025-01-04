// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>

namespace BatteryNs::Mqtt {

class Stats : public ::BatteryNs::Stats {
friend class Provider;

public:
    // since the source of information was MQTT in the first place,
    // we do NOT publish the same data under a different topic.
    void mqttPublish() const final { }

    void getLiveViewData(JsonVariant& root) const final;

    bool supportsAlarmsAndWarnings() const final { return false; }
};

} // namespace BatteryNs::Mqtt
