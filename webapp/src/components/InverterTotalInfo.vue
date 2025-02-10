<template>
    <BootstrapAlert :show="noTotals" variant="info">
        <div class="d-flex">
            <div class="align-content-center"><BIconGear class="fs-4" /></div>
            <div class="align-content-center ms-3">{{ $t('hints.NoTotals') }}</div>
        </div>
    </BootstrapAlert>
    <div class="row row-cols-1 row-cols-md-3 g-3" ref="totals-container">
        <template v-if="solarChargerData.enabled">
            <div class="col" v-if="solarChargerData.yieldTotal">
                <CardElement
                    centerContent
                    textVariant="text-bg-primary"
                    :text="$t('invertertotalinfo.MpptTotalYieldTotal')"
                >
                    <h2>
                        {{
                            $n(solarChargerData.yieldTotal.v, 'decimal', {
                                minimumFractionDigits: solarChargerData.yieldTotal.d,
                                maximumFractionDigits: solarChargerData.yieldTotal.d,
                            })
                        }}
                        <small class="text-muted">{{ solarChargerData.yieldTotal.u }}</small>
                    </h2>
                </CardElement>
            </div>
            <div class="col" v-if="solarChargerData.yieldDay">
                <CardElement
                    centerContent
                    textVariant="text-bg-primary"
                    :text="$t('invertertotalinfo.MpptTotalYieldDay')"
                >
                    <h2>
                        {{
                            $n(solarChargerData.yieldDay.v, 'decimal', {
                                minimumFractionDigits: solarChargerData.yieldDay.d,
                                maximumFractionDigits: solarChargerData.yieldDay.d,
                            })
                        }}
                        <small class="text-muted">{{ solarChargerData.yieldDay.u }}</small>
                    </h2>
                </CardElement>
            </div>
            <div class="col" v-if="solarChargerData.power">
                <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.MpptTotalPower')">
                    <h2>
                        {{
                            $n(solarChargerData.power.v, 'decimal', {
                                minimumFractionDigits: solarChargerData.power.d,
                                maximumFractionDigits: solarChargerData.power.d,
                            })
                        }}
                        <small class="text-muted">{{ solarChargerData.power.u }}</small>
                    </h2>
                </CardElement>
            </div>
        </template>
        <div class="col" v-if="hasInverters">
            <CardElement
                centerContent
                textVariant="text-bg-primary"
                :text="$t('invertertotalinfo.InverterTotalYieldTotal')"
            >
                <h2>
                    {{
                        $n(totalData.YieldTotal.v, 'decimal', {
                            minimumFractionDigits: totalData.YieldTotal.d,
                            maximumFractionDigits: totalData.YieldTotal.d,
                        })
                    }}
                    <small class="text-muted">{{ totalData.YieldTotal.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="hasInverters">
            <CardElement
                centerContent
                textVariant="text-bg-primary"
                :text="$t('invertertotalinfo.InverterTotalYieldDay')"
            >
                <h2>
                    {{
                        $n(totalData.YieldDay.v, 'decimal', {
                            minimumFractionDigits: totalData.YieldDay.d,
                            maximumFractionDigits: totalData.YieldDay.d,
                        })
                    }}
                    <small class="text-muted">{{ totalData.YieldDay.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="hasInverters">
            <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.InverterTotalPower')">
                <h2>
                    {{
                        $n(totalData.Power.v, 'decimal', {
                            minimumFractionDigits: totalData.Power.d,
                            maximumFractionDigits: totalData.Power.d,
                        })
                    }}
                    <small class="text-muted">{{ totalData.Power.u }}</small>
                </h2>
            </CardElement>
        </div>
        <template v-if="totalBattData.enabled">
            <div class="col">
                <CardElement
                    centerContent
                    flexChildren
                    textVariant="text-bg-primary"
                    :text="$t('invertertotalinfo.BatteryCharge')"
                >
                    <div class="flex-fill" v-if="totalBattData.soc">
                        <h2 class="mb-0">
                            {{
                                $n(totalBattData.soc.v, 'decimal', {
                                    minimumFractionDigits: totalBattData.soc.d,
                                    maximumFractionDigits: totalBattData.soc.d,
                                })
                            }}
                            <small class="text-muted">{{ totalBattData.soc.u }}</small>
                        </h2>
                    </div>

                    <div class="flex-fill" v-if="totalBattData.voltage">
                        <h2 class="mb-0">
                            {{
                                $n(totalBattData.voltage.v, 'decimal', {
                                    minimumFractionDigits: totalBattData.voltage.d,
                                    maximumFractionDigits: totalBattData.voltage.d,
                                })
                            }}
                            <small class="text-muted">{{ totalBattData.voltage.u }}</small>
                        </h2>
                    </div>
                </CardElement>
            </div>
            <div class="col" v-if="totalBattData.power || totalBattData.current">
                <CardElement
                    centerContent
                    flexChildren
                    textVariant="text-bg-primary"
                    :text="$t('invertertotalinfo.BatteryPower')"
                >
                    <div class="flex-fill" v-if="totalBattData.power">
                        <h2 class="mb-0">
                            {{
                                $n(totalBattData.power.v, 'decimal', {
                                    minimumFractionDigits: totalBattData.power.d,
                                    maximumFractionDigits: totalBattData.power.d,
                                })
                            }}
                            <small class="text-muted">{{ totalBattData.power.u }}</small>
                        </h2>
                    </div>

                    <div class="flex-fill" v-if="totalBattData.current">
                        <h2 class="mb-0">
                            {{
                                $n(totalBattData.current.v, 'decimal', {
                                    minimumFractionDigits: totalBattData.current.d,
                                    maximumFractionDigits: totalBattData.current.d,
                                })
                            }}
                            <small class="text-muted">{{ totalBattData.current.u }}</small>
                        </h2>
                    </div>
                </CardElement>
            </div>
        </template>
        <div class="col" v-if="powerMeterData.enabled">
            <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.HomePower')">
                <h2>
                    {{
                        $n(powerMeterData.Power.v, 'decimal', {
                            minimumFractionDigits: powerMeterData.Power.d,
                            maximumFractionDigits: powerMeterData.Power.d,
                        })
                    }}
                    <small class="text-muted">{{ powerMeterData.Power.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="huaweiData.enabled">
            <CardElement centerContent textVariant="text-bg-primary" :text="$t('invertertotalinfo.HuaweiPower')">
                <h2>
                    {{
                        $n(huaweiData.Power.v, 'decimal', {
                            minimumFractionDigits: huaweiData.Power.d,
                            maximumFractionDigits: huaweiData.Power.d,
                        })
                    }}
                    <small class="text-muted">{{ huaweiData.Power.u }}</small>
                </h2>
            </CardElement>
        </div>
        <div class="col" v-if="shellyData.enabled">
            <CardElement centerContent textVariant="text-bg-success" :text="$t('invertertotalinfo.ShellyPower')">
                <h2>
                    {{
                        $n(shellyData.Power.v, 'decimal', {
                            minimumFractionDigits: shellyData.Power.d,
                            maximumFractionDigits: shellyData.Power.d,
                        })
                    }}
                    <small class="text-muted">{{ shellyData.Power.u }}</small>
                </h2>
            </CardElement>
        </div>
    </div>
</template>

<script lang="ts">
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import { BIconGear } from 'bootstrap-icons-vue';
import type { Battery, Total, SolarCharger, Huawei, PowerMeter, Shelly } from '@/types/LiveDataStatus';
import CardElement from './CardElement.vue';
import { defineComponent, type PropType, useTemplateRef } from 'vue';

export default defineComponent({
    components: {
        BootstrapAlert,
        BIconGear,
        CardElement,
    },
    props: {
        totalData: { type: Object as PropType<Total>, required: true },
        hasInverters: { type: Boolean, required: true },
        solarChargerData: { type: Object as PropType<SolarCharger>, required: true },
        totalBattData: { type: Object as PropType<Battery>, required: true },
        powerMeterData: { type: Object as PropType<PowerMeter>, required: true },
        huaweiData: { type: Object as PropType<Huawei>, required: true },
        shellyData: { type: Object as PropType<Shelly>, required: true },
    },
    data() {
        return {
            totalsContainer: useTemplateRef<HTMLDivElement>('totals-container'),
            noTotals: false,
        };
    },
    mounted() {
        this.noTotals = this.totalsContainer?.children.length === 0 || false;
    },
});
</script>
