#include "MessageOutput.h"
#include "PowerLimiterOverscalingInverter.h"

PowerLimiterOverscalingInverter::PowerLimiterOverscalingInverter(bool verboseLogging, PowerLimiterInverterConfig const& config)
    : PowerLimiterInverter(verboseLogging, config) { }

uint16_t PowerLimiterOverscalingInverter::applyIncrease(uint16_t increase)
{
    if (isEligible() != Eligibility::Eligible) { return 0; }

    if (increase == 0) { return 0; }

    // do not wake inverter up if it would produce too much power
    if (!isProducing() && _config.LowerPowerLimit > increase) { return 0; }

    // the limit might be scaled, so we use the
    // current output as the baseline. inverters in standby have
    // no output (baseline is zero).
    auto baseline = getCurrentOutputAcWatts();

    auto actualIncrease = std::min(increase, getMaxIncreaseWatts());
    setAcOutput(baseline + actualIncrease);
    return actualIncrease;
}

uint16_t PowerLimiterOverscalingInverter::scaleLimit(uint16_t expectedOutputWatts)
{
    // overscalling allows us to compensate for shaded panels by increasing the
    // total power limit, if the inverter is solar powered.
    // this feature should not be used when homyiles 'Power Distribution Logic' is available
    // as the inverter will take care of the power distribution across the MPPTs itself.
    // (added in inverter firmware 01.01.12 on supported models (HMS-1600/1800/2000))
    // When disabled we return the expected output.
    if (!_config.UseOverscaling || _spInverter->supportsPowerDistributionLogic()) { return expectedOutputWatts; }

    // prevent scaling if inverter is not producing, as input channels are not
    // producing energy and hence are detected as not-producing, causing
    // unreasonable scaling.
    if (!isProducing()) { return expectedOutputWatts; }

    auto pStats = _spInverter->Statistics();
    std::vector<ChannelNum_t> dcChnls = _spInverter->getChannelsDC();
    std::vector<MpptNum_t> dcMppts = _spInverter->getMppts();
    size_t dcTotalChnls = dcChnls.size();
    size_t dcTotalMppts = dcMppts.size();

    // if there is only one MPPT available, there is nothing we can do
    if (dcTotalMppts <= 1) { return expectedOutputWatts; }

    // test for a reasonable power limit that allows us to assume that an input
    // channel with little energy is actually not producing, rather than
    // producing very little due to the very low limit.
    if (getCurrentLimitWatts() < dcTotalChnls * 10) { return expectedOutputWatts; }

    auto inverterOutputAC = pStats->getChannelFieldValue(TYPE_AC, CH0, FLD_PAC);

    float inverterEfficiencyFactor = pStats->getChannelFieldValue(TYPE_INV, CH0, FLD_EFF);

    // fall back to hoymiles peak efficiency as per datasheet if inverter
    // is currently not producing (efficiency is zero in that case)
    inverterEfficiencyFactor = (inverterEfficiencyFactor > 0) ? inverterEfficiencyFactor/100 : 0.967;

    auto scalingThreshold = static_cast<float>(_config.ScalingThreshold) / 100.0;
    auto expectedAcPowerPerMppt = (getCurrentLimitWatts() / dcTotalMppts) * scalingThreshold;

    if (_verboseLogging) {
        MessageOutput.printf(
            "%s\r\n"
            "    expected AC power per MPPT %.0f W\r\n",
            _logPrefix, expectedAcPowerPerMppt);
    }

    size_t dcShadedMppts = 0;
    auto shadedChannelACPowerSum = 0.0;

    for (auto& m : dcMppts) {
        float mpptPowerAC = 0.0;
        std::vector<ChannelNum_t> mpptChnls = _spInverter->getChannelsDCByMppt(m);

        for (auto& c : mpptChnls) {
            mpptPowerAC += pStats->getChannelFieldValue(TYPE_DC, c, FLD_PDC) * inverterEfficiencyFactor;
        }

        if (mpptPowerAC < expectedAcPowerPerMppt) {
            dcShadedMppts++;
            shadedChannelACPowerSum += mpptPowerAC;
        }

        if (_verboseLogging) {
            MessageOutput.printf("    MPPT-%c AC power %.0f W\r\n",
                    mpptName(m), mpptPowerAC);
        }
    }

    // no shading or the shaded channels provide more power than what
    // we currently need.
    if (dcShadedMppts == 0 || shadedChannelACPowerSum >= expectedOutputWatts) {
        return expectedOutputWatts;
    }

    if (dcShadedMppts == dcTotalMppts) {
        // keep the currentLimit when:
        // - all channels are shaded
        // - currentLimit >= expectedOutputWatts
        // - we get the expected AC power or less and
        if (getCurrentLimitWatts() >= expectedOutputWatts &&
                inverterOutputAC <= expectedOutputWatts) {
            if (_verboseLogging) {
                MessageOutput.printf("    all mppts are shaded, "
                        "keeping the current limit of %d W\r\n",
                        getCurrentLimitWatts());
            }

            return getCurrentLimitWatts();

        } else {
            return expectedOutputWatts;
        }
    }

    size_t dcNonShadedMppts = dcTotalMppts - dcShadedMppts;
    uint16_t overScaledLimit = (expectedOutputWatts - shadedChannelACPowerSum) / dcNonShadedMppts * dcTotalMppts;

    if (overScaledLimit <= expectedOutputWatts) { return expectedOutputWatts; }

    if (_verboseLogging) {
        MessageOutput.printf("    %d/%d mppts are not-producing/shaded, scaling %d W\r\n",
                dcShadedMppts, dcTotalMppts, overScaledLimit);
    }

    return overScaledLimit;
}

void PowerLimiterOverscalingInverter::setAcOutput(uint16_t expectedOutputWatts)
{
    // make sure to enforce the lower and upper bounds
    expectedOutputWatts = std::min(expectedOutputWatts, getConfiguredMaxPowerWatts());
    expectedOutputWatts = std::max(expectedOutputWatts, _config.LowerPowerLimit);

    setExpectedOutputAcWatts(expectedOutputWatts);
    setTargetPowerLimitWatts(scaleLimit(expectedOutputWatts));
    setTargetPowerState(true);
}
