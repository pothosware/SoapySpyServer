// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Version.hpp>

#include <cassert>

/***********************************************************************
 * Find
 **********************************************************************/
static std::vector<SoapySDR::Kwargs> findSpyServerClient(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;

    try
    {
        const auto client = SoapySpyServerClient::makeSDRPPClient(args);
        assert(args.count("host"));
        assert(args.count("port"));
        assert(client.client);
        assert(client.client->isOpen());

        results.emplace_back();
        auto &result = results.front();

        const auto &devInfo = client.client->devInfo;
        result["device"] = SoapySpyServerClient::DeviceEnumToName(devInfo.DeviceType);
        result["serial"] = std::to_string(devInfo.DeviceSerial);
        result["url"] = SoapySpyServerClient::ParamsToSpyServerURL(args.at("host"), args.at("port"));
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
