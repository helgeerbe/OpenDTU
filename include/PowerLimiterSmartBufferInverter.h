// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include "PowerLimiterOverscalingInverter.h"

class PowerLimiterSmartBufferInverter : public PowerLimiterOverscalingInverter {
public:
    PowerLimiterSmartBufferInverter(bool verboseLogging, PowerLimiterInverterConfig const& config);

    uint16_t getMaxReductionWatts(bool allowStandby) const final;
    uint16_t getMaxIncreaseWatts() const final;
    uint16_t applyReduction(uint16_t reduction, bool allowStandby) final;
    uint16_t standby() final;
};
