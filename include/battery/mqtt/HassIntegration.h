// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>

namespace BatteryNs::Mqtt {

class HassIntegration : public ::BatteryNs::HassIntegration {
public:
    void publishSensors() const final
    {
        ::BatteryNs::HassIntegration::publishSensors();
    }
};

} // namespace BatteryNs::Mqtt
