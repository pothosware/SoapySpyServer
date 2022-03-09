#include <SoapySDR/Logger.hpp>
#include <spyserver_client.h>
#include <volk/volk.h>
#include <chrono>
#include <cstring>

namespace spyserver {
    SpyServerClientClass::SpyServerClientClass(net::Conn conn, DSPComplexBufferQueue& out): outputQueue(out) {
        readBuf = new uint8_t[SPYSERVER_MAX_MESSAGE_BODY_SIZE];
        writeBuf = new uint8_t[SPYSERVER_MAX_MESSAGE_BODY_SIZE];
        client = std::move(conn);

        sendHandshake("SoapySDR");

        client->readAsync(sizeof(SpyServerMessageHeader), (uint8_t*)&receivedHeader, dataHandler, this);
    }

    SpyServerClientClass::~SpyServerClientClass() {
        close();
        delete[] readBuf;
        delete[] writeBuf;
    }

    void SpyServerClientClass::startStream() {
        setSetting(SPYSERVER_SETTING_STREAMING_ENABLED, true);
    }

    void SpyServerClientClass::stopStream() {
        setSetting(SPYSERVER_SETTING_STREAMING_ENABLED, false);
    }

    void SpyServerClientClass::close() {
        client->close();
    }

    bool SpyServerClientClass::isOpen() {
        return client->isOpen();
    }

    int SpyServerClientClass::computeDigitalGain(int /*serverBits*/, int deviceGain, int decimationId) {
        if (devInfo.DeviceType == SPYSERVER_DEVICE_AIRSPY_ONE) {
            return (devInfo.MaximumGainIndex - deviceGain) + (decimationId * 3.01f);
        }
        else if (devInfo.DeviceType == SPYSERVER_DEVICE_AIRSPY_HF) {
            return decimationId * 3.01f;
        }
        else if (devInfo.DeviceType == SPYSERVER_DEVICE_RTLSDR) {
            return decimationId * 3.01f;
        }
        else {
            // Error, unknown device
            return -1;
        }
    }

    bool SpyServerClientClass::waitForDevInfo(int timeoutMS) {
        std::unique_lock<std::mutex> lck(deviceInfoMtx);
        auto now = std::chrono::system_clock::now();
        deviceInfoCnd.wait_until(lck, now + std::chrono::milliseconds(timeoutMS), [this]() { return deviceInfoAvailable; });
        return deviceInfoAvailable;
    }

    bool SpyServerClientClass::waitForClientSync(int timeoutMS) {
        std::unique_lock<std::mutex> lck(clientSyncMtx);
        auto now = std::chrono::system_clock::now();
        clientSyncCnd.wait_until(lck, now + std::chrono::milliseconds(timeoutMS), [this]() { return clientSyncAvailable; });
        return clientSyncAvailable;
    }

    void SpyServerClientClass::sendCommand(uint32_t command, void* data, int len) {
        SpyServerCommandHeader* hdr = (SpyServerCommandHeader*)writeBuf;
        hdr->CommandType = command;
        hdr->BodySize = len;
        memcpy(&writeBuf[sizeof(SpyServerCommandHeader)], data, len);
        client->write(sizeof(SpyServerCommandHeader) + len, writeBuf);
    }

    void SpyServerClientClass::sendHandshake(std::string appName) {
        int totSize = int(sizeof(SpyServerClientHandshake) + appName.size());
        uint8_t* buf = new uint8_t[totSize];

        SpyServerClientHandshake* cmdHandshake = (SpyServerClientHandshake*)buf;
        cmdHandshake->ProtocolVersion = SPYSERVER_PROTOCOL_VERSION;

        memcpy(&buf[sizeof(SpyServerClientHandshake)], appName.c_str(), appName.size());
        sendCommand(SPYSERVER_CMD_HELLO, buf, totSize);

        delete[] buf;
    }

    void SpyServerClientClass::setSetting(uint32_t setting, uint32_t arg) {
        SpyServerSettingTarget target;
        target.Setting = setting;
        target.Value = arg;
        sendCommand(SPYSERVER_CMD_SET_SETTING, &target, sizeof(SpyServerSettingTarget));
    }

    int SpyServerClientClass::readSize(int count, uint8_t* buffer) {
        int read = 0;
        int len = 0;
        while (read < count) {
            len = client->read(count - read, &buffer[read]);
            if (len <= 0) { return len; }
            read += len;
        }
        return read;
    }

    void SpyServerClientClass::dataHandler(int count, uint8_t* buf, void* ctx) {
        SpyServerClientClass* _this = (SpyServerClientClass*)ctx;

        if (size_t(count) < sizeof(SpyServerMessageHeader)) {
            _this->readSize(sizeof(SpyServerMessageHeader) - count, &buf[count]);
        }

        int size = _this->readSize(_this->receivedHeader.BodySize, _this->readBuf);
        if (size <= 0) {
            SoapySDR::log(SOAPY_SDR_INFO, "SpyServer device disconnected");
            return;
        }

        int mtype = _this->receivedHeader.MessageType & 0xFFFF;
        int mflags = (_this->receivedHeader.MessageType & 0xFFFF0000) >> 16;

        if (mtype == SPYSERVER_MSG_TYPE_DEVICE_INFO) {
            {
                std::lock_guard<std::mutex> lck(_this->deviceInfoMtx);
                SpyServerDeviceInfo* _devInfo = (SpyServerDeviceInfo*)_this->readBuf;
                _this->devInfo = *_devInfo;
                _this->deviceInfoAvailable = true;
            }
            _this->deviceInfoCnd.notify_all();
        }
        else if (mtype == SPYSERVER_MSG_TYPE_CLIENT_SYNC) {
            {
                std::lock_guard<std::mutex> lck(_this->clientSyncMtx);
                SpyServerClientSync* _clientSync = (SpyServerClientSync*)_this->readBuf;
                _this->clientSync = *_clientSync;
                _this->clientSyncAvailable = true;
            }
            _this->clientSyncCnd.notify_all();
        }
        else if (mtype == SPYSERVER_MSG_TYPE_UINT8_IQ) {
            int sampCount = _this->receivedHeader.BodySize / (sizeof(uint8_t) * 2);
            volk::vector<dsp::complex_t> output(sampCount);
            float gain = pow(10, (double)mflags / 20.0);
            float scale = 1.0f / (gain * 128.0f);
            for (int i = 0; i < sampCount; i++) {
                output[i].re = ((float)_this->readBuf[(2 * i)] - 128.0f) * scale;
                output[i].im = ((float)_this->readBuf[(2 * i) + 1] - 128.0f) * scale;
            }
            _this->outputQueue.enqueue(std::move(output));
        }
        else if (mtype == SPYSERVER_MSG_TYPE_INT16_IQ) {
            int sampCount = _this->receivedHeader.BodySize / (sizeof(int16_t) * 2);
            volk::vector<dsp::complex_t> output(sampCount);
            float gain = pow(10, (double)mflags / 20.0);
            volk_16i_s32f_convert_32f((float*)output.data(), (int16_t*)_this->readBuf, 32768.0 * gain, sampCount * 2);
            _this->outputQueue.enqueue(std::move(output));
        }
        else if (mtype == SPYSERVER_MSG_TYPE_FLOAT_IQ) {
            int sampCount = _this->receivedHeader.BodySize / sizeof(dsp::complex_t);
            volk::vector<dsp::complex_t> output(sampCount);
            float gain = pow(10, (double)mflags / 20.0);
            volk_32f_s32f_multiply_32f((float*)output.data(), (float*)_this->readBuf, gain, sampCount * 2);
            _this->outputQueue.enqueue(std::move(output));
        } else if (mtype == SPYSERVER_MSG_TYPE_INT24_IQ) {
            SoapySDR::log(
                SOAPY_SDR_FATAL,
                "SpyServer returned unsupported stream format INT24. We should have caught this.");
            return;
        }

        _this->client->readAsync(sizeof(SpyServerMessageHeader), (uint8_t*)&_this->receivedHeader, dataHandler, _this);
    }

    SpyServerClient connect(std::string host, uint16_t port, DSPComplexBufferQueue& out) {
        net::Conn conn = net::connect(host, port);
        if (!conn) {
            return NULL;
        }
        return SpyServerClient(new SpyServerClientClass(std::move(conn), out));
    }
}
