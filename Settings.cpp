// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

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
