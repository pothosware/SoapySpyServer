#pragma once
#include <utils/networking.h>
#include <spyserver_protocol.h>
#include <dsp/types.h>

#include "CappedSizeQueue.hpp"
#include <volk/volk_alloc.hh>

using DSPComplexBufferQueue = CappedSizeQueue<volk::vector<dsp::complex_t>>;

/*
 * Originally written by Alexandre Rouma:
 *  * https://github.com/AlexandreRouma/SDRPlusPlus/tree/master/source_modules/spyserver_source
 *
 * Adapted by Nicholas Corgan:
 *  * Move samples into queue for caller instead of SDR++-specific stream class
 *  * Convert prints to SoapySDR logging
 *  * Compatibility with earlier C++ standard
 */
namespace spyserver {
    class SpyServerClientClass {
    public:
        SpyServerClientClass(net::Conn conn, DSPComplexBufferQueue& out);
        ~SpyServerClientClass();

        bool waitForDevInfo(int timeoutMS);
        bool waitForClientSync(int timeoutMS);

        void startStream();
        void stopStream();

        void setSetting(uint32_t setting, uint32_t arg);

        void close();
        bool isOpen();

        int computeDigitalGain(int serverBits, int deviceGain, int decimationId);

        SpyServerDeviceInfo devInfo;
        SpyServerClientSync clientSync;

    private:
        void sendCommand(uint32_t command, void* data, int len);
        void sendHandshake(std::string appName);

        int readSize(int count, uint8_t* buffer);

        static void dataHandler(int count, uint8_t* buf, void* ctx);

        net::Conn client;

        uint8_t* readBuf;
        uint8_t* writeBuf;

        bool deviceInfoAvailable = false;
        std::mutex deviceInfoMtx;
        std::condition_variable deviceInfoCnd;

        bool clientSyncAvailable = false;
        std::mutex clientSyncMtx;
        std::condition_variable clientSyncCnd;

        SpyServerMessageHeader receivedHeader;

        DSPComplexBufferQueue& outputQueue;
    };

    typedef std::unique_ptr<SpyServerClientClass> SpyServerClient;

    SpyServerClient connect(std::string host, uint16_t port, DSPComplexBufferQueue& out);

}
