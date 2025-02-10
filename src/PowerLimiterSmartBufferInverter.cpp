#include "MessageOutput.h"
#include "PowerLimiterSmartBufferInverter.h"

PowerLimiterSmartBufferInverter::PowerLimiterSmartBufferInverter(bool verboseLogging, PowerLimiterInverterConfig const& config)
    : PowerLimiterOverscalingInverter(verboseLogging, config) { }

uint16_t PowerLimiterSmartBufferInverter::getMaxReductionWatts(bool allowStandby) const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (!isProducing()) { return 0; }

    if (allowStandby) { return getCurrentOutputAcWatts(); }

    if (getCurrentOutputAcWatts() <= _config.LowerPowerLimit) { return 0; }

    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSmartBufferInverter::getMaxIncreaseWatts() const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (!isProducing()) {
        return getConfiguredMaxPowerWatts();
    }

    // when overscaling is in use we must not substract the current limit
    // because it might be scaled and higher than the configured max power.
    if (_config.UseOverscaling && !_spInverter->supportsPowerDistributionLogic()) {
        return getConfiguredMaxPowerWatts() - getCurrentOutputAcWatts();
    }

    // we must not substract the current AC output here, but the current
    // limit value, so we avoid trying to produce even more even if the
    // inverter is already at the maximum limit value (the actual AC
    // output may be less than the inverter's current power limit).
    return std::max(0, getConfiguredMaxPowerWatts() - getCurrentLimitWatts());
}

uint16_t PowerLimiterSmartBufferInverter::applyReduction(uint16_t reduction, bool allowStandby)
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (reduction == 0) { return 0; }

    auto low = std::min(getCurrentLimitWatts(), getCurrentOutputAcWatts());
    if (low <= _config.LowerPowerLimit) {
        if (allowStandby) {
            standby();
            return std::min(reduction, getCurrentOutputAcWatts());
        }
        return 0;
    }

    if ((getCurrentLimitWatts() - _config.LowerPowerLimit) >= reduction) {
        setAcOutput(getCurrentLimitWatts() - reduction);
        return reduction;
    }

    if (allowStandby) {
        standby();
        return std::min(reduction, getCurrentOutputAcWatts());
    }

    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSmartBufferInverter::standby()
{
    setTargetPowerState(false);
    setExpectedOutputAcWatts(0);
    return getCurrentOutputAcWatts();
}
