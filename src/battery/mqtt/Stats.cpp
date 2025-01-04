// SPDX-License-Identifier: GPL-2.0-or-later
#include <MqttSettings.h>
#include <battery/mqtt/Stats.h>

namespace BatteryNs::Mqtt {

void Stats::getLiveViewData(JsonVariant& root) const
{
    // as we don't want to repeat the data that is already shown in the live data card
    // we only add the live view data here when the discharge current limit can be shown
    if (isDischargeCurrentLimitValid()) {
        ::BatteryNs::Stats::getLiveViewData(root);
    }
}

} // namespace BatteryNs::Mqtt
