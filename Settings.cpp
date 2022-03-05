// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Constants.h>
#include <SoapySDR/Formats.h>
#include <SoapySDR/Logger.hpp>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iterator>
#include <stdexcept>

//
// Non-class utility
//

static inline bool almostEqual(const double lhs, const double rhs)
{
    constexpr double EPSILON = 1e-6;
    return (std::abs(lhs-rhs) <= EPSILON);
}

//
// Static utility functions
//

SDRPPClient SoapySpyServerClient::makeSDRPPClient(const SoapySDR::Kwargs &args)
{
    auto hostIter = args.find("host");
    if(hostIter == args.end())
        throw std::runtime_error("SoapySpyServer: missing required key \"host\"");

    auto portIter = args.find("port");
    if(portIter == args.end())
        throw std::runtime_error("SoapySpyServer: missing required key \"port\"");

    const auto &host = hostIter->second;
    const auto &port = portIter->second;

    SDRPPClient client;
    client.bufferQueue.reset(new DSPComplexBufferQueue);

    const auto spyServerURL = ParamsToSpyServerURL(host, port);
    SoapySDR::logf(
        SOAPY_SDR_INFO,
        "Connecting to %s...",
        spyServerURL.c_str());
    client.client = spyserver::connect(
        hostIter->second,
        SoapySDR::StringToSetting<uint16_t>(portIter->second),
        *client.bufferQueue);

    if(not client.client or not client.client->isOpen() or not client.syncFields())
        throw std::runtime_error("SoapySpyServer: failed to connect to client with args: "+SoapySDR::KwargsToString(args));

    if(client.client->devInfo.ForcedIQFormat != static_cast<uint32_t>(SPYSERVER_STREAM_FORMAT_INVALID))
    {
        switch(static_cast<SpyServerStreamFormat>(client.client->devInfo.ForcedIQFormat))
        {
        case SPYSERVER_STREAM_FORMAT_INT24:
            throw std::runtime_error("Conversion from internal stream format INT24 unsupported.");

        case SPYSERVER_STREAM_FORMAT_DINT4:
            throw std::runtime_error("Conversion from internal stream format DINT4 unsupported.");

        default:
            break;
        }
    }

    SoapySDR::log(
        SOAPY_SDR_INFO,
        "Ready.");

    return client;
}

std::string SoapySpyServerClient::ParamsToSpyServerURL(
    const std::string &host,
    const std::string &port)
{
    return "sdr://"+host+":"+port;
}

std::string SoapySpyServerClient::DeviceEnumToName(const uint32_t deviceType)
{
    switch(static_cast<SpyServerDeviceType>(deviceType))
    {
    case SPYSERVER_DEVICE_AIRSPY_ONE:
        return "AirSpy One";

    case SPYSERVER_DEVICE_AIRSPY_HF:
        return "AirSpy HF+";

    case SPYSERVER_DEVICE_RTLSDR:
        return "RTL-SDR";

    default:
        return "Unknown";
    }
}

//
// Construction
//

SoapySpyServerClient::SoapySpyServerClient(const SoapySDR::Kwargs &args):
    _sdrppClient(makeSDRPPClient(args))
{
    assert(args.count("host"));
    assert(args.count("port"));

    if(not _sdrppClient.client->clientSync.CanControl)
        SoapySDR::logf(
            SOAPY_SDR_WARNING,
            "This device restricts changing gain. %s gain is set to %f.",
            GainName.c_str(),
            this->getGain(SOAPY_SDR_RX, 0, GainName));

    // Derive sample rates from associated fields.
    for(uint32_t i = _sdrppClient.client->devInfo.MinimumIQDecimation;
        i <= _sdrppClient.client->devInfo.DecimationStageCount;
        ++i)
    {
        const auto rate = static_cast<double>(_sdrppClient.client->devInfo.MaximumSampleRate / (1 << i));
        _sampleRates.emplace_back(i, rate);
    }
    assert(not _sampleRates.empty());

    // Ugly workaround: there doesn't seem to be a way to query the sample rate, so
    // each implementation just stores the sample rate passed into the setter. We'll
    // quietly set the sample rate so we have an initial value.
    this->setSampleRate(SOAPY_SDR_RX, 0, _sampleRates[0].second);

    _spyServerURL = ParamsToSpyServerURL(args.at("host"), args.at("port"));
}

/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapySpyServerClient::getDriverKey(void) const
{
    return "spyserver";
}

std::string SoapySpyServerClient::getHardwareKey(void) const
{
    return _spyServerURL;
}

SoapySDR::Kwargs SoapySpyServerClient::getHardwareInfo(void) const
{
    assert(_sdrppClient.client);
    assert(_sdrppClient.client->isOpen());

    return
    {
        {"device", DeviceEnumToName(_sdrppClient.client->devInfo.DeviceType)},
        {"serial", SoapySDR::SettingToString(_sdrppClient.client->devInfo.DeviceSerial)},
        {"protocol_version", SoapySDR::SettingToString(SPYSERVER_PROTOCOL_VERSION)},
    };
}

/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapySpyServerClient::getNumChannels(const int direction) const
{
    return (direction == SOAPY_SDR_RX) ? 1 : 0;
}

SoapySDR::Kwargs SoapySpyServerClient::getChannelInfo(const int direction, const size_t channel) const
{
    SoapySDR::Kwargs channelInfo;
    if(validChannelParams(direction, channel))
    {
        _sdrppClient.syncFields();
        channelInfo["full_control"] = SoapySDR::SettingToString(_sdrppClient.client->clientSync.CanControl > 0);
    }
    else channelInfo = SoapySDR::Device::getChannelInfo(direction, channel);

    return channelInfo;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

const std::string SoapySpyServerClient::AntennaName("RX");

std::vector<std::string> SoapySpyServerClient::listAntennas(const int direction, const size_t channel) const
{
    return validChannelParams(direction, channel) ? std::vector<std::string>{AntennaName}
                                                  : SoapySDR::Device::listAntennas(direction, channel);
}

void SoapySpyServerClient::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    if(validChannelParams(direction, channel))
    {
        if(name != AntennaName)
            throw std::invalid_argument("Invalid antenna: "+name);
    }
    else SoapySDR::Device::setAntenna(direction, channel, name);
}

std::string SoapySpyServerClient::getAntenna(const int direction, const size_t channel) const
{
    return validChannelParams(direction, channel) ? AntennaName
                                                  : SoapySDR::Device::getAntenna(direction, channel);
}

/*******************************************************************
 * Gain API
 ******************************************************************/

const std::string SoapySpyServerClient::GainName("Full");

std::vector<std::string> SoapySpyServerClient::listGains(const int direction, const size_t channel) const
{
    return validChannelParams(direction, channel) ? std::vector<std::string>{GainName}
                                                  : SoapySDR::Device::listGains(direction, channel);
}

void SoapySpyServerClient::setGain(const int direction, const size_t channel, const std::string &name, const double value)
{
    if(validGainParams(direction, channel, name))
    {
        _sdrppClient.syncFields();
        if(_sdrppClient.client->clientSync.CanControl)
        {
            _sdrppClient.client->setSetting(
                static_cast<uint32_t>(SPYSERVER_SETTING_GAIN),
                static_cast<uint32_t>(value));

            _sdrppClient.syncFields();
        }
        else throw std::runtime_error("This device does not allow setting gain.");
    }
    else SoapySDR::Device::setGain(direction, channel, value);
}

double SoapySpyServerClient::getGain(const int direction, const size_t channel, const std::string &name) const
{
    if(validGainParams(direction, channel, name))
    {
        _sdrppClient.syncFields();

        return static_cast<double>(_sdrppClient.client->clientSync.Gain);
    }
    else return SoapySDR::Device::getGain(direction, channel);
}

SoapySDR::Range SoapySpyServerClient::getGainRange(const int direction, const size_t channel, const std::string &name) const
{
    if(validGainParams(direction, channel, name))
    {
        _sdrppClient.syncFields();

        if(_sdrppClient.client->clientSync.CanControl)
        {
            return SoapySDR::Range(
                0.0,
                static_cast<double>(_sdrppClient.client->devInfo.MaximumGainIndex),
                1.0);
        }
        else
        {
            return SoapySDR::Range(
                static_cast<double>(_sdrppClient.client->clientSync.Gain),
                static_cast<double>(_sdrppClient.client->clientSync.Gain),
                1.0);
        }

    }
    else return SoapySDR::Device::getGainRange(direction, channel);
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

const std::string SoapySpyServerClient::FrequencyName("RF");

void SoapySpyServerClient::setFrequency(const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args)
{
    if(validFrequencyParams(direction, channel, name))
    {
        _sdrppClient.client->setSetting(
            static_cast<uint32_t>(SPYSERVER_SETTING_IQ_FREQUENCY),
            static_cast<uint32_t>(frequency));

        _sdrppClient.syncFields();
    }
    else SoapySDR::Device::setFrequency(direction, channel, frequency, args);
}

double SoapySpyServerClient::getFrequency(const int direction, const size_t channel, const std::string &name) const
{
    if(validFrequencyParams(direction, channel, name))
    {
        _sdrppClient.syncFields();

        return static_cast<double>(_sdrppClient.client->clientSync.IQCenterFrequency);
    }
    else return SoapySDR::Device::getFrequency(direction, channel, name);
}

std::vector<std::string> SoapySpyServerClient::listFrequencies(const int direction, const size_t channel) const
{
    return validChannelParams(direction, channel) ? std::vector<std::string>{FrequencyName}
                                                  : SoapySDR::Device::listFrequencies(direction, channel);
}

SoapySDR::RangeList SoapySpyServerClient::getFrequencyRange(const int direction, const size_t channel, const std::string &name) const
{
    if(validFrequencyParams(direction, channel, name))
    {
        _sdrppClient.syncFields();

        return SoapySDR::RangeList{{
            static_cast<double>(_sdrppClient.client->clientSync.MinimumIQCenterFrequency),
            static_cast<double>(_sdrppClient.client->clientSync.MaximumIQCenterFrequency),
            1.0}};
    }
    else return SoapySDR::Device::getFrequencyRange(direction, channel, name);
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySpyServerClient::setSampleRate(const int direction, const size_t channel, const double rate)
{
    if(validChannelParams(direction, channel))
    {
        auto sampleRateIter = std::find_if(
            _sampleRates.begin(),
            _sampleRates.end(),
            [&](const std::pair<uint32_t, double> &ratePair)
            {
                return almostEqual(ratePair.second, rate);
            });

        if(sampleRateIter != _sampleRates.end())
        {
            // SpyServer takes in sample rate by the decimation index.
            _sdrppClient.client->setSetting(
                static_cast<uint32_t>(SPYSERVER_SETTING_IQ_DECIMATION),
                sampleRateIter->first);

            _sampleRate = rate;

            _sdrppClient.syncFields();
        }
        else throw std::invalid_argument("Invalid sample rate: "+SoapySDR::SettingToString(rate));
    }
    else SoapySDR::Device::setSampleRate(direction, channel, rate);
}

double SoapySpyServerClient::getSampleRate(const int direction, const size_t channel) const
{
    if(validChannelParams(direction, channel))
    {
        _sdrppClient.syncFields();

        return _sampleRate;
    }
    else return SoapySDR::Device::getSampleRate(direction, channel);
}

std::vector<double> SoapySpyServerClient::listSampleRates(const int direction, const size_t channel) const
{
    if(validChannelParams(direction, channel))
    {
        std::vector<double> sampleRates;
        std::transform(
            _sampleRates.begin(),
            _sampleRates.end(),
            std::back_inserter(sampleRates),
            [](const std::pair<uint32_t, double> &ratePair)
            {
                return ratePair.second;
            });

        return sampleRates;
    }
    else return SoapySDR::Device::listSampleRates(direction, channel);
}
