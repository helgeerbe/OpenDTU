// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/HassIntegration.h>

namespace BatteryNs::Pytes {

class HassIntegration : public ::BatteryNs::HassIntegration {
public:
    void publishSensors() const final;
};

} // namespace BatteryNs::Pytes
