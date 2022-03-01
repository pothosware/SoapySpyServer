// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Version.hpp>

/***********************************************************************
 * Find
 **********************************************************************/
static std::vector<SoapySDR::Kwargs> findSpyServerClient(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    auto hostIter = args.find("host");
    if(hostIter == args.end()) return results;

    auto portIter = args.find("port");
    if(portIter == args.end()) return results;

    try
    {
        dsp::stream<dsp::complex_t> stream;
        auto client = spyserver::connect(
            hostIter->second,
            SoapySDR::StringToSetting<uint16_t>(portIter->second),
            &stream);

        if(client and client->isOpen() and client->waitForDevInfo(1000))
        {
            results.emplace_back();
            auto &result = results.front();

            const auto &devInfo = client->devInfo;
            result["device"] = SoapySpyServerClient::DeviceEnumToName(devInfo.DeviceType);
            result["serial"] = std::to_string(devInfo.DeviceSerial);
            result["url"] = SoapySpyServerClient::ParamsToSpyServerURL(hostIter->second, portIter->second);
        }
    }
    catch(...){}

    return results;
}

/***********************************************************************
 * Make
 **********************************************************************/
static SoapySDR::Device *makeSpyServerClient(const SoapySDR::Kwargs &args)
{
    return new SoapySpyServerClient(args);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerSpyServerClient(
    "spyserver",
    &findSpyServerClient,
    &makeSpyServerClient,
    SOAPY_SDR_ABI_VERSION);
