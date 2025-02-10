<template>
    <BasePage :title="$t('acchargeradmin.ChargerSettings')" :isLoading="dataLoading">
        <BootstrapAlert v-model="showAlert" dismissible :variant="alertType">
            {{ alertMessage }}
        </BootstrapAlert>

        <form @submit="saveChargerConfig">
            <CardElement :text="$t('acchargeradmin.Configuration')" textVariant="text-bg-primary">
                <InputElement
                    :label="$t('acchargeradmin.EnableHuawei')"
                    v-model="acChargerConfigList.enabled"
                    type="checkbox"
                    wide
                />
                <InputElement
                    :label="$t('acchargeradmin.EnableShelly')"
                    v-model="acChargerShellyConfigList.enabled"
                    type="checkbox"
                    wide
                />

                <template v-if="acChargerConfigList.enabled">
                    <InputElement
                        :label="$t('acchargeradmin.VerboseLogging')"
                        v-model="acChargerConfigList.verbose_logging"
                        type="checkbox"
                        wide
                    />

                    <div class="row mb-3">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('acchargeradmin.HardwareInterface') }}
                        </label>
                        <div class="col-sm-8">
                            <select class="form-select" v-model="acChargerConfigList.hardware_interface">
                                <option v-for="type in hardwareInterfaceList" :key="type.key" :value="type.key">
                                    {{ $t('acchargeradmin.HardwareInterface' + type.value) }}
                                </option>
                            </select>
                        </div>
                    </div>

                    <div class="row mb-3" v-if="acChargerConfigList.hardware_interface === 0">
                        <label class="col-sm-4 col-form-label">
                            {{ $t('acchargeradmin.CanControllerFrequency') }}
                        </label>
                        <div class="col-sm-8">
                            <select class="form-select" v-model="acChargerConfigList.can_controller_frequency">
                                <option
                                    v-for="frequency in frequencyTypeList"
                                    :key="frequency.key"
                                    :value="frequency.value"
                                >
                                    {{ frequency.key }} MHz
                                </option>
                            </select>
                        </div>
                    </div>

                    <InputElement
                        :label="$t('acchargeradmin.EnableAutoPower')"
                        v-model="acChargerConfigList.auto_power_enabled"
                        type="checkbox"
                        wide
                    />

                    <InputElement
                        v-if="acChargerConfigList.auto_power_enabled"
                        :label="$t('acchargeradmin.EnableBatterySoCLimits')"
                        v-model="acChargerConfigList.auto_power_batterysoc_limits_enabled"
                        type="checkbox"
                        wide
                    />

                    <InputElement
                        :label="$t('acchargeradmin.EnableEmergencyCharge')"
                        :tooltip="$t('acchargeradmin.EnableEmergencyChargeHint')"
                        v-model="acChargerConfigList.emergency_charge_enabled"
                        type="checkbox"
                        wide
                    />
                </template>
            </CardElement>

            <CardElement
                :text="$t('acchargeradmin.Limits')"
                textVariant="text-bg-primary"
                add-space
                v-if="acChargerConfigList.auto_power_enabled || acChargerConfigList.emergency_charge_enabled"
            >
                <InputElement
                    :label="$t('acchargeradmin.VoltageLimit')"
                    :tooltip="$t('acchargeradmin.stopVoltageLimitHint')"
                    v-model="acChargerConfigList.voltage_limit"
                    postfix="V"
                    type="number"
                    wide
                    required
                    step="0.01"
                    min="42"
                    max="58.5"
                />
                <InputElement
                    :label="$t('acchargeradmin.enableVoltageLimit')"
                    :tooltip="$t('acchargeradmin.enableVoltageLimitHint')"
                    v-model="acChargerConfigList.enable_voltage_limit"
                    postfix="V"
                    type="number"
                    wide
                    required
                    step="0.01"
                    min="42"
                    max="58.5"
                />
                <InputElement
                    :label="$t('acchargeradmin.lowerPowerLimit')"
                    v-model="acChargerConfigList.lower_power_limit"
                    postfix="W"
                    type="number"
                    wide
                    required
                    min="50"
                    max="3000"
                />
                <InputElement
                    :label="$t('acchargeradmin.upperPowerLimit')"
                    :tooltip="$t('acchargeradmin.upperPowerLimitHint')"
                    v-model="acChargerConfigList.upper_power_limit"
                    postfix="W"
                    type="number"
                    wide
                    required
                    min="100"
                    max="3000"
                />
                <InputElement
                    :label="$t('acchargeradmin.targetPowerConsumption')"
                    :tooltip="$t('acchargeradmin.targetPowerConsumptionHint')"
                    v-model="acChargerConfigList.target_power_consumption"
                    postfix="W"
                    type="number"
                    wide
                    required
                />
            </CardElement>

            <CardElement
                :text="$t('acchargeradmin.BatterySoCLimits')"
                textVariant="text-bg-primary"
                add-space
                v-if="
                    acChargerConfigList.auto_power_enabled && acChargerConfigList.auto_power_batterysoc_limits_enabled
                "
            >
                <InputElement
                    :label="$t('acchargeradmin.StopBatterySoCThreshold')"
                    :tooltip="$t('acchargeradmin.StopBatterySoCThresholdHint')"
                    v-model="acChargerConfigList.stop_batterysoc_threshold"
                    postfix="%"
                    type="number"
                    wide
                    required
                    min="2"
                    max="99"
                />
            </CardElement>

            <CardElement
                :text="$t('acchargeradmin.ShellySettings')"
                textVariant="text-bg-primary"
                add-space
                v-show="acChargerShellyConfigList.enabled"
            >
                <InputElement
                    :label="$t('acchargeradmin.VerboseLogging')"
                    v-model="acChargerShellyConfigList.verbose_logging"
                    type="checkbox"
                    wide
                />
                <InputElement
                    :label="$t('acchargeradmin.EnableEmergencyCharge')"
                    v-model="acChargerShellyConfigList.emergency_charge_enabled"
                    type="checkbox"
                    wide
                />
                <InputElement
                    :label="$t('acchargeradmin.EnableBatterySoCLimits')"
                    v-model="acChargerShellyConfigList.auto_power_batterysoc_limits_enabled"
                    type="checkbox"
                    wide
                />
                <div class="row mb-3">
                    <label for="ip" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyAddress') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyAddressHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="string"
                                class="form-control"
                                id="url"
                                placeholder="http://192.168.2.100"
                                v-model="acChargerShellyConfigList.url"
                                aria-describedby="urlDescription"
                                required
                            />
                            <span class="input-group-text" id="urlDescription"></span>
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label for="ip" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyuriON') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyuriONHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="string"
                                class="form-control"
                                id="uri_on"
                                placeholder="/relay/0?turn=on"
                                v-model="acChargerShellyConfigList.uri_on"
                                aria-describedby="uriDescription"
                                required
                            />
                            <span class="input-group-text" id="uriDescription"></span>
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label for="ip" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyuriOFF') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyuriOFFHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="string"
                                class="form-control"
                                id="uri_off"
                                placeholder="/relay/0?turn=off"
                                v-model="acChargerShellyConfigList.uri_off"
                                aria-describedby="uriDescription"
                                required
                            />
                            <span class="input-group-text" id="uriDescription"></span>
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label for="ip" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyuriSTATS') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyuriSTATSHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="string"
                                class="form-control"
                                id="uri_stats"
                                placeholder="/relay/0?turn=stats"
                                v-model="acChargerShellyConfigList.uri_stats"
                                aria-describedby="uriDescription"
                                required
                            />
                            <span class="input-group-text" id="uriDescription"></span>
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label for="ip" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyuriPOWERPARAM') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyuriPOWERPARAMHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="string"
                                class="form-control"
                                id="uri_powerparam"
                                placeholder="apower"
                                v-model="acChargerShellyConfigList.uri_powerparam"
                                aria-describedby="uriDescription"
                                required
                            />
                            <span class="input-group-text" id="uriDescription"></span>
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label for="ShellyStartThreshold" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyStartThreshold') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyStartThresholdHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="number"
                                class="form-control"
                                id="ShellyStartThreshold"
                                placeholder="-500"
                                v-model="acChargerShellyConfigList.power_on_threshold"
                                aria-describedby="ShellyStartThresholdDescription"
                                required
                            />
                            <span class="input-group-text" id="ShellyStartThresholdDescription">W</span>
                        </div>
                    </div>
                </div>
                <div class="row mb-3">
                    <label for="ShellyStopThreshold" class="col-sm-2 col-form-label"
                        >{{ $t('acchargeradmin.ShellyStopThreshold') }}:
                        <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.ShellyStopThresholdHint')" />
                    </label>
                    <div class="col-sm-10">
                        <div class="input-group">
                            <input
                                type="number"
                                class="form-control"
                                id="ShellyStopThreshold"
                                placeholder="-100"
                                v-model="acChargerShellyConfigList.power_off_threshold"
                                aria-describedby="ShellyStopThresholdDescription"
                                required
                            />
                            <span class="input-group-text" id="ShellyStopThresholdDescription">W</span>
                        </div>
                    </div>
                </div>
                <CardElement
                    :text="$t('acchargeradmin.BatterySoCLimits')"
                    textVariant="text-bg-primary"
                    add-space
                    v-show="acChargerShellyConfigList.auto_power_batterysoc_limits_enabled"
                >
                    <div class="row mb-3">
                        <label for="stopBatterySoCThreshold" class="col-sm-2 col-form-label"
                            >{{ $t('acchargeradmin.StopBatterySoCThreshold') }}:
                            <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.StopBatterySoCThresholdHint')" />
                        </label>
                        <div class="col-sm-10">
                            <div class="input-group">
                                <input
                                    type="number"
                                    class="form-control"
                                    id="Shelly_stopBatterySoCThreshold"
                                    placeholder="95"
                                    v-model="acChargerShellyConfigList.stop_batterysoc_threshold"
                                    aria-describedby="Shelly_stopBatterySoCThresholdDescription"
                                    min="2"
                                    max="99"
                                    required
                                />
                                <span class="input-group-text" id="Shelly_stopBatterySoCThresholdDescription">%</span>
                            </div>
                        </div>
                    </div>
                    <div class="row mb-3">
                        <label for="startBatterySoCThreshold" class="col-sm-2 col-form-label"
                            >{{ $t('acchargeradmin.StartBatterySoCThreshold') }}:
                            <BIconInfoCircle v-tooltip :title="$t('acchargeradmin.StartBatterySoCThresholdHint')" />
                        </label>
                        <div class="col-sm-10">
                            <div class="input-group">
                                <input
                                    type="number"
                                    class="form-control"
                                    id="Shelly_startBatterySoCThreshold"
                                    placeholder="90"
                                    v-model="acChargerShellyConfigList.start_batterysoc_threshold"
                                    aria-describedby="Shelly_startBatterySoCThresholdDescription"
                                    min="2"
                                    max="99"
                                    required
                                />
                                <span class="input-group-text" id="Shelly_startBatterySoCThresholdDescription">%</span>
                            </div>
                        </div>
                    </div>
                </CardElement>
            </CardElement>

            <FormFooter @reload="getChargerConfig" />
        </form>
    </BasePage>
</template>

<script lang="ts">
import BasePage from '@/components/BasePage.vue';
import BootstrapAlert from '@/components/BootstrapAlert.vue';
import CardElement from '@/components/CardElement.vue';
import FormFooter from '@/components/FormFooter.vue';
import InputElement from '@/components/InputElement.vue';
import type { AcChargerConfig } from '@/types/AcChargerConfig';
import type { AcChargerShellyConfig } from '@/types/AcChargerConfig';
import { authHeader, handleResponse } from '@/utils/authentication';
import { defineComponent } from 'vue';

export default defineComponent({
    components: {
        BasePage,
        BootstrapAlert,
        CardElement,
        FormFooter,
        InputElement,
    },
    data() {
        return {
            dataLoading: true,
            acChargerConfigList: {} as AcChargerConfig,
            acChargerShellyConfigList: {} as AcChargerShellyConfig,
            alertMessage: '',
            alertType: 'info',
            showAlert: false,
            frequencyTypeList: [
                { key: 8, value: 8000000 },
                { key: 16, value: 16000000 },
            ],
            hardwareInterfaceList: [
                { key: 0, value: 'MCP2515' },
                { key: 1, value: 'TWAI' },
            ],
        };
    },
    created() {
        this.getChargerConfig();
    },
    methods: {
        getChargerConfig() {
            this.dataLoading = true;
            fetch('/api/huawei/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.acChargerConfigList = data;
                    this.dataLoading = false;
                });
            fetch('/api/shelly/config', { headers: authHeader() })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((data) => {
                    this.acChargerShellyConfigList = data;
                    this.dataLoading = false;
                });
        },
        saveChargerConfig(e: Event) {
            e.preventDefault();

            const formData = new FormData();
            const formDataShelly = new FormData();
            formData.append('data', JSON.stringify(this.acChargerConfigList));
            formDataShelly.append('data', JSON.stringify(this.acChargerShellyConfigList));

            fetch('/api/shelly/config', {
                method: 'POST',
                headers: authHeader(),
                body: formDataShelly,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                    this.alertType = response.type;
                    this.showAlert = true;
                });

            fetch('/api/huawei/config', {
                method: 'POST',
                headers: authHeader(),
                body: formData,
            })
                .then((response) => handleResponse(response, this.$emitter, this.$router))
                .then((response) => {
                    this.alertMessage = this.$t('apiresponse.' + response.code, response.param);
                    this.alertType = response.type;
                    this.showAlert = true;
                });
        },
    },
});
</script>
