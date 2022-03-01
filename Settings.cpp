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

std::string SoapySpyServerClient::FormatEnumToName(const uint32_t format)
{
    switch(static_cast<SpyServerStreamFormat>(format))
    {
    case SPYSERVER_STREAM_FORMAT_UINT8:
        return SOAPY_SDR_CU8;

    case SPYSERVER_STREAM_FORMAT_INT16:
        return SOAPY_SDR_CS16;

    case SPYSERVER_STREAM_FORMAT_FLOAT:
        return SOAPY_SDR_CF32;

    default:
        return "Unknown";
    }
}

//
// Construction
//

SoapySpyServerClient::SoapySpyServerClient(const SoapySDR::Kwargs &args)
{
    auto hostIter = args.find("host");
    if(hostIter == args.end())
        throw std::runtime_error("SoapySpyServer: missing required key \"host\"");

    auto portIter = args.find("port");
    if(portIter == args.end())
        throw std::runtime_error("SoapySpyServer: missing required key \"port\"");

    _spyServerURL = ParamsToSpyServerURL(hostIter->second, portIter->second);

    SoapySDR::logf(
        SOAPY_SDR_INFO,
        "Connecting to %s...",
        _spyServerURL.c_str());

    _sdrppClient = spyserver::connect(
        hostIter->second,
        SoapySDR::StringToSetting<uint16_t>(portIter->second),
        &_sdrppStream);
    SoapySDR::log(
        SOAPY_SDR_INFO,
        "Waiting for device information...");

    if(not _sdrppClient or not _sdrppClient->isOpen() or not this->_syncSdrPPFields())
        throw std::runtime_error("SoapySpyServer: failed to connect to client with args: "+SoapySDR::KwargsToString(args));

    SoapySDR::log(
        SOAPY_SDR_INFO,
        "Ready.");

    if(not _sdrppClient->clientSync.CanControl)
        SoapySDR::log(
            SOAPY_SDR_WARNING,
            "This device only supports a limited subset of hardware control. Some setters will not work.");

    // This will throw if the device forces an unsupported internal stream format.
    (void)this->getChannelInfo(SOAPY_SDR_RX, 0);

    // Derive sample rates from associated fields.
    for(uint32_t i = _sdrppClient->devInfo.MinimumIQDecimation;
        i <= _sdrppClient->devInfo.DecimationStageCount;
        ++i)
    {
        const auto rate = static_cast<double>(_sdrppClient->devInfo.MaximumSampleRate / (1 << i));
        _sampleRates.emplace_back(i, rate);
    }
    assert(not _sampleRates.empty());

    // Ugly workaround: there doesn't seem to be a way to query the sample rate, so
    // each implementation just stores the sample rate passed into the setter. We'll
    // quietly set the sample rate so we have an initial value.
    this->setSampleRate(SOAPY_SDR_RX, 0, _sampleRates[0].second);
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
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    return _spyServerURL;
}

SoapySDR::Kwargs SoapySpyServerClient::getHardwareInfo(void) const
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    return
    {
        {"device", DeviceEnumToName(_sdrppClient->devInfo.DeviceType)},
        {"serial", SoapySDR::SettingToString(_sdrppClient->devInfo.DeviceSerial)},
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
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    SoapySDR::Kwargs channelInfo;
    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();

        if(_sdrppClient->devInfo.ForcedIQFormat != static_cast<uint32_t>(SPYSERVER_STREAM_FORMAT_INVALID))
        {
            switch(static_cast<SpyServerStreamFormat>(_sdrppClient->devInfo.ForcedIQFormat))
            {
            case SPYSERVER_STREAM_FORMAT_INT24:
                throw std::runtime_error("Conversion from internal stream format INT24 unsupported.");

            case SPYSERVER_STREAM_FORMAT_DINT4:
                throw std::runtime_error("Conversion from internal stream format DINT4 unsupported.");

            default:
                channelInfo["forced_internal_format"] = FormatEnumToName(_sdrppClient->devInfo.ForcedIQFormat);
                break;
            }
        }
    
        channelInfo["full_control"] = SoapySDR::SettingToString(_sdrppClient->clientSync.CanControl > 0);
    }
    else channelInfo = SoapySDR::Device::getChannelInfo(direction, channel);

    return channelInfo;
}

/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapySpyServerClient::listAntennas(const int direction, const size_t channel) const
{
    return ((direction == SOAPY_SDR_RX) and (channel == 0)) ? std::vector<std::string>{"RX"}
                                                            : SoapySDR::Device::listAntennas(direction, channel);
}

void SoapySpyServerClient::setAntenna(const int direction, const size_t channel, const std::string &name)
{
    // For the sake of consistency
    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        if(name != "RX")
            throw std::invalid_argument("Invalid antenna: "+name);
    }
    else SoapySDR::Device::setAntenna(direction, channel, name);
}

std::string SoapySpyServerClient::getAntenna(const int direction, const size_t channel) const
{
    return ((direction == SOAPY_SDR_RX) and (channel == 0)) ? "RX"
                                                            : SoapySDR::Device::getAntenna(direction, channel);
}

/*******************************************************************
 * Gain API
 ******************************************************************/

void SoapySpyServerClient::setGain(const int direction, const size_t channel, const double value)
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();
        if(_sdrppClient->clientSync.CanControl)
        {
            _sdrppClient->setSetting(
                static_cast<uint32_t>(SPYSERVER_SETTING_GAIN),
                static_cast<uint32_t>(value));

            this->_syncSdrPPFields();
        }
        else throw std::runtime_error("This device does not allow setting gain.");
    }
    else SoapySDR::Device::setGain(direction, channel, value);
}

double SoapySpyServerClient::getGain(const int direction, const size_t channel) const
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();

        return static_cast<double>(_sdrppClient->clientSync.Gain);
    }
    else return SoapySDR::Device::getGain(direction, channel);
}

SoapySDR::Range SoapySpyServerClient::getGainRange(const int direction, const size_t channel) const
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();

        return SoapySDR::Range(
            0.0,
            static_cast<double>(_sdrppClient->devInfo.MaximumGainIndex),
            1.0);
    }
    else return SoapySDR::Device::getGainRange(direction, channel);
}

/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapySpyServerClient::setFrequency(const int direction, const size_t channel, const double frequency, const SoapySDR::Kwargs &args)
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        _sdrppClient->setSetting(
            static_cast<uint32_t>(SPYSERVER_SETTING_IQ_FREQUENCY),
            static_cast<uint32_t>(frequency));

        this->_syncSdrPPFields();
    }
    else SoapySDR::Device::setFrequency(direction, channel, frequency, args);
}

double SoapySpyServerClient::getFrequency(const int direction, const size_t channel) const
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();

        return static_cast<double>(_sdrppClient->clientSync.IQCenterFrequency);
    }
    else return SoapySDR::Device::getFrequency(direction, channel);
}

SoapySDR::RangeList SoapySpyServerClient::getFrequencyRange(const int direction, const size_t channel) const
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();

        return SoapySDR::RangeList{{
            static_cast<double>(_sdrppClient->clientSync.MinimumIQCenterFrequency),
            static_cast<double>(_sdrppClient->clientSync.MaximumIQCenterFrequency),
            1.0}};
    }
    else return SoapySDR::Device::getFrequencyRange(direction, channel);
}

/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapySpyServerClient::setSampleRate(const int direction, const size_t channel, const double rate)
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
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
            _sdrppClient->setSetting(
                static_cast<uint32_t>(SPYSERVER_SETTING_IQ_DECIMATION),
                sampleRateIter->first);

            _sampleRate = rate;

            this->_syncSdrPPFields();
        }
        else throw std::invalid_argument("Invalid sample rate: "+SoapySDR::SettingToString(rate));
    }
    else SoapySDR::Device::setSampleRate(direction, channel, rate);
}

double SoapySpyServerClient::getSampleRate(const int direction, const size_t channel) const
{
    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if((direction == SOAPY_SDR_RX) and (channel == 0))
    {
        this->_syncSdrPPFields();

        return _sampleRate;
    }
    else return SoapySDR::Device::getSampleRate(direction, channel);
}

std::vector<double> SoapySpyServerClient::listSampleRates(const int direction, const size_t channel) const
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
