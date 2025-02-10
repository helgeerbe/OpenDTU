#include "MessageOutput.h"
#include "PowerLimiterSolarInverter.h"

PowerLimiterSolarInverter::PowerLimiterSolarInverter(bool verboseLogging, PowerLimiterInverterConfig const& config)
    : PowerLimiterOverscalingInverter(verboseLogging, config) { }

uint16_t PowerLimiterSolarInverter::getMaxReductionWatts(bool) const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    auto low = std::min(getCurrentLimitWatts(), getCurrentOutputAcWatts());
    if (low <= _config.LowerPowerLimit) { return 0; }

    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::getMaxIncreaseWatts() const
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (!isProducing()) {
        // the inverter is not producing, we don't know how much we can increase
        // the power, so we return the maximum possible increase
        return getConfiguredMaxPowerWatts();
    }

    // the maximum increase possible for this inverter
    int16_t maxTotalIncrease = getConfiguredMaxPowerWatts() - getCurrentOutputAcWatts();

    auto pStats = _spInverter->Statistics();
    std::vector<MpptNum_t> dcMppts = _spInverter->getMppts();
    size_t dcTotalMppts = dcMppts.size();

    float inverterEfficiencyFactor = pStats->getChannelFieldValue(TYPE_INV, CH0, FLD_EFF) / 100;

    // with 97% we are a bit less strict than when we scale the limit
    auto expectedPowerPercentage = 0.97;

    // use the scaling threshold as the expected power percentage if lower,
    // but only when overscaling is enabled and the inverter does not support PDL
    if (_config.UseOverscaling && !_spInverter->supportsPowerDistributionLogic()) {
        expectedPowerPercentage = std::min(expectedPowerPercentage, static_cast<float>(_config.ScalingThreshold) / 100.0);
    }

    // x% of the expected power is good enough
    auto expectedAcPowerPerMppt = (getCurrentLimitWatts() / dcTotalMppts) * expectedPowerPercentage;

    size_t dcNonShadedMppts = 0;
    auto nonShadedMpptACPowerSum = 0.0;

    for (auto& m : dcMppts) {
        float mpptPowerAC = 0.0;
        std::vector<ChannelNum_t> mpptChnls = _spInverter->getChannelsDCByMppt(m);

        for (auto& c : mpptChnls) {
            mpptPowerAC += pStats->getChannelFieldValue(TYPE_DC, c, FLD_PDC) * inverterEfficiencyFactor;
        }

        if (mpptPowerAC >= expectedAcPowerPerMppt) {
            nonShadedMpptACPowerSum += mpptPowerAC;
            dcNonShadedMppts++;
        }
    }

    if (dcNonShadedMppts == 0) {
        // all mppts are shaded, we can't increase the power
        return 0;
    }

    if (dcNonShadedMppts == dcTotalMppts) {
        // no MPPT is shaded, we assume that we can increase the power by the maximum
        return maxTotalIncrease;
    }

    // for inverters without PDL we use the configured max power, because the limit will be divided equally across the MPPTs by the inverter.
    int16_t inverterMaxPower = getConfiguredMaxPowerWatts();

    // for inverter with PDL or when overscaling is enabled we use the max power of the inverter because each MPPT can deliver its max power.
    if (_spInverter->supportsPowerDistributionLogic() || _config.UseOverscaling) {
        inverterMaxPower = getInverterMaxPowerWatts();
    }

    int16_t maxPowerPerMppt = inverterMaxPower / dcTotalMppts;

    int16_t currentPowerPerNonShadedMppt = nonShadedMpptACPowerSum / dcNonShadedMppts;

    int16_t maxIncreasePerNonShadedMppt = maxPowerPerMppt - currentPowerPerNonShadedMppt;

    // maximum increase based on the non-shaded mppts, can be higher than maxTotalIncrease for inverters
    // with PDL when getConfiguredMaxPowerWatts() is less than getInverterMaxPowerWatts() divided by
    // the number of used/unshaded MPPTs.
    int16_t maxIncreaseNonShadedMppts = maxIncreasePerNonShadedMppt * dcNonShadedMppts;

    // maximum increase should not exceed the max total increase
    return std::min(maxTotalIncrease, maxIncreaseNonShadedMppts);
}

uint16_t PowerLimiterSolarInverter::applyReduction(uint16_t reduction, bool)
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (reduction == 0) { return 0; }

    if ((getCurrentOutputAcWatts() - _config.LowerPowerLimit) >= reduction) {
        setAcOutput(getCurrentOutputAcWatts() - reduction);
        return reduction;
    }

    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}

uint16_t PowerLimiterSolarInverter::standby()
{
    // solar-powered inverters are never actually put into standby (by the
    // DPL), but only set to the configured lower power limit instead.
    setAcOutput(_config.LowerPowerLimit);
    return getCurrentOutputAcWatts() - _config.LowerPowerLimit;
}
