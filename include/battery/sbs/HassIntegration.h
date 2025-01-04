// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>

namespace BatteryNs::SBS {

class HassIntegration : public ::BatteryNs::HassIntegration {
public:
    void publishSensors() const final;
};

} // namespace BatteryNs::SBS
