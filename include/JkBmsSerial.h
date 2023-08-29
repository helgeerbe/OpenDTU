#pragma once

#include <memory>
#include <vector>

#include "JkBmsSerialMessage.h"

typedef struct {
    uint8_t start0;
    uint8_t start1;
    uint16_t length;
} JkBmsData;

class JkBmsSerial {
    public:
        JkBmsSerial() = default;

        void init(int8_t rx, int8_t tx);
        void loop();

        enum class Status : unsigned {
            Initializing,
            DisabledByConfig,
            Timeout,
            WaitingForPollInterval,
            HwSerialNotAvailableForWrite,
            BusyReading,
            RequestSent,
            FrameCompleted
        };

        void setPollInterval(uint32_t interval) {
            _pollInterval = interval;
        }

        // timestamp in millis() when the last data was received
        uint32_t getLastMessageTimestamp() {
            return _lastMessage;
        }

        bool isDataValid();

        JkBmsData data{};

    private:
        std::string const& getStatusText(Status status);
        void announceStatus(Status status);
        void sendRequest();
        void rxData(uint8_t inbyte);
        void reset();
        void frameComplete();

        enum class ReadState : unsigned {
            Idle,
            WaitingForFrameStart,
            FrameStartReceived,
            StartMarkerReceived,
            FrameLengthMsbReceived,
            ReadingFrame
        };
        ReadState _readState;
        void setReadState(ReadState state) {
            _readState = state;
        }

        Status _lastStatus = Status::Initializing;
        uint32_t _lastStatusPrinted = 0;
        std::vector<uint8_t> _buffer = {};
        uint32_t _pollInterval = 5;
        uint32_t _lastRequest = 0;
        uint32_t _lastMessage = 0;
        uint16_t _frameLength = 0;
        std::unique_ptr<JkBmsSerialMessage const> _pData = nullptr;
};

extern JkBmsSerial JkBms;
