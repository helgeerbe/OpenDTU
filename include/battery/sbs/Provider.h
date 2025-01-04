// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <driver/twai.h>
#include <battery/CanReceiver.h>
#include <battery/sbs/Stats.h>
#include <battery/sbs/HassIntegration.h>

namespace BatteryNs::SBS {

class Provider : public ::BatteryNs::CanReceiver {
public:
    bool init(bool verboseLogging) final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<::BatteryNs::Stats> getStats() const final { return _stats; }
    ::BatteryNs::HassIntegration const& getHassIntegration() const final { return _hassIntegration; }

private:
    void dummyData();
    std::shared_ptr<Stats> _stats =
        std::make_shared<Stats>();
    HassIntegration _hassIntegration;
};

} // namespace BatteryNs::SBS
