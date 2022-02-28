// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

//#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Registry.hpp>
#include <SoapySDR/Types.hpp>
#include <SoapySDR/Version.hpp>

/***********************************************************************
 * Find
 **********************************************************************/
static std::vector<SoapySDR::Kwargs> findSpyServerClient(const SoapySDR::Kwargs &args)
{
    std::vector<SoapySDR::Kwargs> results;
/*
    auto ipIter = args.find("ip");
    if(ipIter == args.end()) return results;

    auto portIter = args.find("port");
    if(portIter == args.end()) return results;

    try
    {
        SoapySpyServerClient client(args);

        results.emplace_back();
        auto &result = results.front();

        result["device"] = client.getHardwareKey();
        result["serial"] = client.getHardwareInfo()["serial"];
        result["url"] = client.getHardwareInfo()["url"];
    }
    catch(...){}
*/
    return results;
}

/***********************************************************************
 * Make
 **********************************************************************/
static SoapySDR::Device *makeSpyServerClient(const SoapySDR::Kwargs &args)
{
//    return new SoapySpyServerClient(args);
}

/***********************************************************************
 * Registration
 **********************************************************************/
static SoapySDR::Registry registerSpyServerClient(
    "spyserver",
    &findSpyServerClient,
    &makeSpyServerClient,
    SOAPY_SDR_ABI_VERSION);
