// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#include "spyserver_client.h"

#include "SoapySpyServerClient.hpp"

#include <SoapySDR/Constants.h>
#include <SoapySDR/Formats.h>

#include <cassert>
#include <cstring>
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

    assert(_sdrppClient.client);
    assert(_sdrppClient.client->isOpen());

    if(_stream->active)
        _sdrppClient.client->stopStream();

    _stream.reset(nullptr);
}

int SoapySpyServerClient::activateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs,
    const size_t numElems)
{
    std::lock_guard<std::mutex> lock(_streamMutex);
    if(not validStream(stream))
        throw std::invalid_argument("Invalid stream");
    if(_stream->active)
        throw std::runtime_error("Stream is already active");

    if((flags != 0) or (timeNs != 0) or (numElems != 0))
        return SOAPY_SDR_NOT_SUPPORTED;

    _sdrppClient.client->startStream();
    _stream->active = true;

    return 0;
}

int SoapySpyServerClient::deactivateStream(
    SoapySDR::Stream *stream,
    const int flags,
    const long long timeNs)
{
    std::lock_guard<std::mutex> lock(_streamMutex);
    if(not validStream(stream))
        throw std::invalid_argument("Invalid stream");
    if(not _stream->active)
        throw std::runtime_error("Stream is already inactive");

    if((flags != 0) or (timeNs != 0))
        return SOAPY_SDR_NOT_SUPPORTED;

    _sdrppClient.client->stopStream();
    _stream->active = false;

    return 0;
}

int SoapySpyServerClient::readStream(
    SoapySDR::Stream *stream,
    void * const *buffs,
    const size_t numElems,
    int &,
    long long &,
    const long timeoutUs)
{
    std::lock_guard<std::mutex> lock(_streamMutex);

    assert(_sdrppClient.bufferQueue);

    // As a policy, don't throw.
    if(not validStream(stream))
        return SOAPY_SDR_NOT_SUPPORTED;
    if(not _stream->active)
        return SOAPY_SDR_NOT_SUPPORTED;
    if(not buffs or not buffs[0])
        return SOAPY_SDR_NOT_SUPPORTED;

    // The SpyServer client asychronously adds buffers to a queue as
    // it receives data. If we haven't consumed the entirety of the
    // latest buffer, we'll grab the next one here.
    if(_currentBuffer.empty())
    {
        const auto timeoutS = static_cast<double>(timeoutUs * 1e6);
        if(not _sdrppClient.bufferQueue->dequeue(timeoutS, _currentBuffer))
            return SOAPY_SDR_TIMEOUT;
    }

    assert(not _currentBuffer.empty());

    static constexpr size_t elemSize = sizeof(std::complex<float>);

    const auto actualNumElems = std::min(
        numElems,
        (_currentBuffer.size() - _startIndex));
    assert((_startIndex + actualNumElems) <= _currentBuffer.size());

    std::memcpy(
        buffs[0],
        &_currentBuffer[_startIndex],
        actualNumElems * elemSize);

    _startIndex += actualNumElems;
    assert(_startIndex <= _currentBuffer.size());

    if(_startIndex == _currentBuffer.size())
    {
        _currentBuffer.clear();
        _startIndex = 0;
    }

    return static_cast<int>(actualNumElems);
}
