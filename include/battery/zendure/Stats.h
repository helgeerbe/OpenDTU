// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <battery/Stats.h>
#include <map>

namespace BatteryNs::Zendure {

enum class State : uint8_t {
    Idle        = 0,
    Charging    = 1,
    Discharging = 2,
    Invalid     = 255
};

enum class BypassMode : uint8_t {
    Automatic   = 0,
    AlwaysOff   = 1,
    AlwaysOn    = 2,
    Invalid     = 255
};

class PackStats;

class Stats : public ::BatteryNs::Stats {
    friend class Provider;

    template <typename T>
    static T stateToString(State state) {
        switch (state) {
            case State::Idle:
                return "idle";
            case State::Charging:
                return "charging";
            case State::Discharging:
                return "discharging";
            default:
                return "invalid";
        }
    }
    template <typename T>
    static T bypassModeToString(BypassMode state) {
        switch (state) {
            case BypassMode::Automatic:
                return "automatic";
            case BypassMode::AlwaysOff:
                return "alwaysoff";
            case BypassMode::AlwaysOn:
                return "alwayson";
            default:
                return "invalid";
        }
    }
    inline static bool isDischarging(State state) {
        return state == State::Discharging;
    }
    inline static bool isCharging(State state) {
        return state == State::Charging;
    }

public:
    void getLiveViewData(JsonVariant& root) const final;
    void mqttPublish() const final;

    bool supportsAlarmsAndWarnings() const final { return false; }

    std::map<size_t, std::shared_ptr<PackStats>> getPackDataList() const { return _packData; }

protected:
    std::shared_ptr<PackStats> getPackData(size_t index) const;
    std::shared_ptr<PackStats> addPackData(size_t index, String serial);

    uint16_t getCapacity() const { return _capacity; };
    uint16_t getUseableCapacity() const { return _capacity_avail * (static_cast<float>(_soc_max - _soc_min) / 100.0); };

private:
    void setLastUpdate(uint32_t ts) { _lastUpdate = ts; }

    void setHwVersion(String&& version) {
        if (!version.isEmpty()) {
            _hwversion = _device + " (" + std::move(version) + ")";
        }else{
            _hwversion = _device;
        }
    }
    void setFwVersion(String&& version) { _fwversion = std::move(version); }

    void setSerial(String serial) {
        _serial = serial;
    }
    void setSerial(std::optional<String> serial) {
        if (serial.has_value()) {
            setSerial(*serial);
        }
    }

    void setDevice(String&& device) {
        _device = std::move(device);
    }

    String _device = String("Unkown");

    std::map<size_t, std::shared_ptr<PackStats>> _packData = std::map<size_t, std::shared_ptr<PackStats> >();

    int16_t _cellTemperature = 0;
    uint16_t _cellMinMilliVolt = 0;
    uint16_t _cellMaxMilliVolt = 0;
    uint16_t _cellDeltaMilliVolt = 0;
    uint16_t _cellAvgMilliVolt = 0;

    float _soc_max = 0.0;
    float _soc_min = 0.0;

    uint16_t _inverse_max = 0;
    uint16_t _input_limit = 0;
    uint16_t _output_limit = 0;

    float _efficiency = 0.0;
    uint16_t _capacity = 0;
    uint16_t _capacity_avail = 0;

    uint16_t _charge_power = 0;
    uint16_t _discharge_power = 0;
    uint16_t _output_power = 0;
    uint16_t _input_power = 0;
    uint16_t _solar_power_1 = 0;
    uint16_t _solar_power_2 = 0;

    int16_t _remain_out_time = 0;
    int16_t _remain_in_time = 0;

    State _state = State::Invalid;
    uint8_t _num_batteries = 0;
    BypassMode _bypass_mode = BypassMode::Invalid;
    bool _bypass_state = false;
    bool _auto_recover = false;
    bool _heat_state = false;
    bool _auto_shutdown = false;
    bool _buzzer = false;

    std::optional<uint64_t> _last_full_timestamp = std::nullopt;
    std::optional<uint32_t> _last_full_charge_hours = std::nullopt;
    std::optional<uint64_t> _last_empty_timestamp = std::nullopt;
    std::optional<bool>  _charge_through_state = std::nullopt;
};

class PackStats {
    friend class Stats;
    friend class Provider;

    public:
        PackStats() {}
        explicit PackStats(String serial) : _serial(serial) {}
        virtual ~PackStats() {}

        String getSerial() const { return _serial; }

        inline uint8_t getCellCount() const { return _cellCount; }
        inline uint16_t getCapacity() const { return _capacity; }
        inline uint16_t getAvailableCapacity() const { return _capacity_avail; };
        inline String getName() const { return _name; }

        static std::shared_ptr<PackStats> fromSerial(String serial) {
            if (serial.length() == 15) {
                if (serial.startsWith("AO4H")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB1000", 960));
                }
                if (serial.startsWith("CO4H")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AB2000", 1920));
                }
                if (serial.startsWith("R04Y")) {
                    return std::make_shared<PackStats>(PackStats(serial, "AIO2400", 2400));
                }
                return std::make_shared<PackStats>(PackStats(serial));
            }
            return nullptr;
        };

    protected:
        explicit PackStats(String serial, String name, uint16_t capacity, uint8_t cellCount = 15) :
            _serial(serial), _name(name), _capacity(capacity), _cellCount(cellCount) {}
        void setSerial(String serial) { _serial = serial; }
        void setHwVersion(String&& version) { _hwversion = std::move(version); }
        void setFwVersion(String&& version) { _fwversion = std::move(version); }

    private:
        String _serial = String();
        String _name = String("UNKOWN");
        uint16_t _capacity = 0;
        uint8_t _cellCount = 15;

        String _fwversion = String();
        String _hwversion = String();

        uint16_t _cell_voltage_min = 0;
        uint16_t _cell_voltage_max = 0;
        uint16_t _cell_voltage_spread = 0;
        uint16_t _cell_voltage_avg = 0;
        int16_t _cell_temperature_max = 0;

        float _state_of_health = 1;
        uint16_t _capacity_avail = 0;

        float _voltage_total = 0.0;
        float _current = 0.0;
        int16_t _power = 0;
        float _soc_level = 0.0;
        State _state = State::Invalid;

        uint32_t _lastUpdate = 0;
};


} // namespace BatteryNs::Zendure
