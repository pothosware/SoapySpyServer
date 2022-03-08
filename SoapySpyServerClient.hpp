// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "spyserver_client.h"

#include <SoapySDR/Constants.h>
#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>

#include <volk/volk_alloc.hh>

#include <atomic>
#include <cassert>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

struct SDRPPClient
{
    static constexpr size_t TimeoutMs = 1000;

    std::unique_ptr<DSPComplexBufferQueue> bufferQueue;
    spyserver::SpyServerClient client;

    inline bool syncFields(void) const
    {
        assert(client);
        assert(client->isOpen());

        return (client->waitForDevInfo(TimeoutMs) and client->waitForClientSync(TimeoutMs));
    }
};

struct SoapySpyServerStream
{
    std::atomic_bool active{false};
};

class SoapySpyServerClient: public SoapySDR::Device
{
public:
    SoapySpyServerClient(const SoapySDR::Kwargs &args);
    virtual ~SoapySpyServerClient(void) = default;

    /*******************************************************************
     * Utility
     ******************************************************************/

    static SDRPPClient makeSDRPPClient(const SoapySDR::Kwargs &args);

    static std::string ParamsToSpyServerURL(
        const std::string &host,
        const std::string &port);

    static std::string DeviceEnumToName(const uint32_t deviceType);

    /*******************************************************************
     * Identification API
     ******************************************************************/

    std::string getDriverKey(void) const;

    std::string getHardwareKey(void) const;

    SoapySDR::Kwargs getHardwareInfo(void) const;

    /*******************************************************************
     * Channels API
     ******************************************************************/

    inline bool validChannelParams(const int direction, const size_t channel) const
    {
        return (direction == SOAPY_SDR_RX) and (channel == 0);
    }

    size_t getNumChannels(const int direction) const;

    SoapySDR::Kwargs getChannelInfo(const int direction, const size_t channel) const;

    /*******************************************************************
     * Stream API
     ******************************************************************/

    inline bool validStream(SoapySDR::Stream* stream)
    {
        return (stream == (SoapySDR::Stream*)_stream.get());
    }

    std::vector<std::string> getStreamFormats(const int direction, const size_t channel) const;

    SoapySDR::Stream *setupStream(
        const int direction,
        const std::string &format,
        const std::vector<size_t> &channels,
        const SoapySDR::Kwargs &args);

    void closeStream(SoapySDR::Stream *stream);

    int activateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs,
        const size_t numElems);

    int deactivateStream(
        SoapySDR::Stream *stream,
        const int flags,
        const long long timeNs);

    int readStream(
        SoapySDR::Stream *stream,
        void * const *buffs,
        const size_t numElems,
        int &flags,
        long long &timeNs,
        const long timeoutUs);

    /*******************************************************************
     * Antenna API
     ******************************************************************/

    static const std::string AntennaName;

    std::vector<std::string> listAntennas(const int direction, const size_t channel) const;

    void setAntenna(const int direction, const size_t channel, const std::string &name);

    std::string getAntenna(const int direction, const size_t channel) const;

    /*******************************************************************
     * Gain API
     ******************************************************************/

    static const std::string GainName;

    inline bool validGainParams(const int direction, const size_t channel, const std::string &name) const
    {
        return validChannelParams(direction, channel) and (name == GainName);
    }

    std::vector<std::string> listGains(const int direction, const size_t channel) const;

    void setGain(const int direction, const size_t channel, const std::string &name, const double value);

    double getGain(const int direction, const size_t channel, const std::string &name) const;

    SoapySDR::Range getGainRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Frequency API
     ******************************************************************/

    static const std::string FrequencyName;

    inline bool validFrequencyParams(const int direction, const size_t channel, const std::string &name) const
    {
        return validChannelParams(direction, channel) and (name == FrequencyName);
    }

    void setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args);

    double getFrequency(const int direction, const size_t channel, const std::string &name) const;

    std::vector<std::string> listFrequencies(const int direction, const size_t channel) const;

    SoapySDR::RangeList getFrequencyRange(const int direction, const size_t channel, const std::string &name) const;

    /*******************************************************************
     * Sample Rate API
     ******************************************************************/

    void setSampleRate(const int direction, const size_t channel, const double rate);

    double getSampleRate(const int direction, const size_t channel) const;

    // Intentionally using deprecated API, no guaranteed step. Let Soapy deal with it.
    std::vector<double> listSampleRates(const int direction, const size_t channel) const;

private:
    //
    // Fields
    //

    static constexpr int SDRPP_TIMEOUT_MS = 1000;

    std::string _spyServerURL;

    SDRPPClient _sdrppClient;

    volk::vector<dsp::complex_t> _currentBuffer;
    size_t _startIndex{0};

    double _sampleRate{0.0};
    std::vector<std::pair<uint32_t, double>> _sampleRates;

    std::unique_ptr<SoapySpyServerStream> _stream;
    mutable std::mutex _streamMutex;
};
