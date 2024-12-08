// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <battery/Provider.h>
#include <battery/victronsmartshunt/Stats.h>

namespace BatteryNs::VictronSmartShunt {

class Provider : public ::BatteryNs::Provider {
public:
    bool init(bool verboseLogging) final;
    void deinit() final;
    void loop() final;
    std::shared_ptr<::BatteryNs::Stats> getStats() const final { return _stats; }

private:
    static char constexpr _serialPortOwner[] = "SmartShunt";

    uint32_t _lastUpdate = 0;
    std::shared_ptr<Stats> _stats =
        std::make_shared<Stats>();
};

} // namespace BatteryNs::VictronSmartShunt
