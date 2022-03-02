// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Constants.h>
#include <SoapySDR/Formats.h>

#include <cassert>
#include <stdexcept>

std::vector<std::string> SoapySpyServerClient::getStreamFormats(const int direction, const size_t channel) const
{
    return ((direction == SOAPY_SDR_RX) and (channel == 0)) ? std::vector<std::string>{SOAPY_SDR_CF32}
                                                            : SoapySDR::Device::getStreamFormats(direction, channel);
}

SoapySDR::Stream *SoapySpyServerClient::setupStream(
    const int direction,
    const std::string &format,
    const std::vector<size_t> &channels,
    const SoapySDR::Kwargs &)
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    if(_stream)
        throw std::runtime_error("Stream already active");
    if(direction != SOAPY_SDR_RX)
        throw std::invalid_argument("SoapySpyServerClient only supports RX");
    if(format != SOAPY_SDR_CF32)
        throw std::invalid_argument("Invalid format: "+format);
    if((channels.size() != 1) or (channels[0] != 0))
        throw std::invalid_argument("SoapySpyServerClient only accepts RX channel 0");

    _stream.reset(new SoapySpyServerStream);

    return (SoapySDR::Stream*)_stream.get();
}

void SoapySpyServerClient::closeStream(SoapySDR::Stream *stream)
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    if(not stream)
        throw std::invalid_argument("Null stream");
    if(stream != (SoapySDR::Stream*)_stream.get())
        throw std::invalid_argument("Invalid stream");

    assert(_sdrppClient);
    assert(_sdrppClient->isOpen());

    if(_stream->active)
        _sdrppClient->stopStream();

    _stream.reset(nullptr);
}

size_t SoapySpyServerClient::getStreamMTU(SoapySDR::Stream *stream) const
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    if(not stream)
        throw std::invalid_argument("Null stream");
    if(stream != (SoapySDR::Stream*)_stream.get())
        throw std::invalid_argument("Invalid stream");

    // The SpyServer client asynchronously reads into a buffer that we
    // consume here, so there's no real MTU per se, so we'll just return
    // the stream's buffer size.
    return STREAM_BUFFER_SIZE;
}

int SoapySpyServerClient::activateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs,
    const size_t numElems)
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    if(not stream)
        throw std::invalid_argument("Null stream");
    if(stream != (SoapySDR::Stream*)_stream.get())
        throw std::invalid_argument("Invalid stream");

    if((flags != 0) or (timeNs != 0) or (numElems != 0))
        return SOAPY_SDR_NOT_SUPPORTED;

    _sdrppClient->startStream();
    _stream->active = true;

    return 0;
}

int SoapySpyServerClient::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    if(not stream)
        throw std::invalid_argument("Null stream");
    if(stream != (SoapySDR::Stream*)_stream.get())
        throw std::invalid_argument("Invalid stream");

    if((flags != 0) or (timeNs != 0))
        return SOAPY_SDR_NOT_SUPPORTED;

    _sdrppClient->stopStream();
    _stream->active = false;

    return 0;
}

int SoapySpyServerClient::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    const size_t numElems,
    int &flags,
    long long &timeNs,
    const long timeoutUs)
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    // As a policy, don't throw.
    if(not stream or (stream != (SoapySDR::Stream*)_stream.get()))
        return SOAPY_SDR_NOT_SUPPORTED;
    if(not _stream->active)
        return SOAPY_SDR_NOT_SUPPORTED;

    // The SpyServer client asychronously writes to its buffer as
    // it receives data.

    return 0;
}
