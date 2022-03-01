// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Logger.hpp>

#include <cassert>
#include <stdexcept>

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
        "Connecting to %s",
        _spyServerURL.c_str());

    _sdrppClient = spyserver::connect(
        hostIter->second,
        SoapySDR::StringToSetting<uint16_t>(portIter->second),
        &_sdrppStream);
    SoapySDR::log(
        SOAPY_SDR_INFO,
        "Waiting for device information...");

    if(not _sdrppClient or not _sdrppClient->isOpen() or
       not _sdrppClient->waitForDevInfo(1000) or not _sdrppClient->waitForClientSync(1000))
        throw std::runtime_error("SoapySpyServer: failed to connect to client with args: "+SoapySDR::KwargsToString(args));

    SoapySDR::log(
        SOAPY_SDR_INFO,
        "Ready.");

    if(not _sdrppClient->clientSync.CanControl)
        SoapySDR::log(
            SOAPY_SDR_WARNING,
            "This device only supports a limited subset of hardware control. Some setters will not work.");
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
    };
}
