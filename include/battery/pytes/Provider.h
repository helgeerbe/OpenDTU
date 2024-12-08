// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <memory>
#include <driver/twai.h>
#include <battery/CanReceiver.h>
#include <battery/pytes/Stats.h>

namespace BatteryNs::Pytes {

class Provider : public ::BatteryNs::CanReceiver {
public:
    bool init(bool verboseLogging) final;
    void onMessage(twai_message_t rx_message) final;

    std::shared_ptr<::BatteryNs::Stats> getStats() const final { return _stats; }

private:
    std::shared_ptr<Stats> _stats =
        std::make_shared<Stats>();
};

} // namespace BatteryNs::Pytes
