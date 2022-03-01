// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "spyserver_client.h"

#include <SoapySDR/Device.hpp>
#include <SoapySDR/Types.hpp>

#include <string>

struct SoapySpyServerStream
{
};

class SoapySpyServerClient: public SoapySDR::Device
{
public:
    SoapySpyServerClient(const SoapySDR::Kwargs &args);
    virtual ~SoapySpyServerClient(void) = default;

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

private:
    std::string _spyServerURL;

    spyserver::SpyServerClient _sdrppClient;
    dsp::stream<dsp::complex_t> _sdrppStream;
};
