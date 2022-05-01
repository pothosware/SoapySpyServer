// Copyright (c) 2022 Nicholas Corgan
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <ThreadSafeQueue.h>

#include <atomic>
#include <cassert>

template <class T>
class CappedSizeQueue: public codepi::ThreadSafeQueue<T>
{
public:
    using Base = codepi::ThreadSafeQueue<T>;

    CappedSizeQueue(const size_t maxSize):
        _maxSize(maxSize)
    {
        assert(_maxSize > 0);
    }
    virtual ~CappedSizeQueue(void) = default;

    void enqueue(T t) override
    {
        assert(Base::size() <= _maxSize);

        if(Base::size() == _maxSize)
        {
            (void)Base::dequeue();
            _overflow = true;
        }

        Base::enqueue(std::forward<T>(t));
    }

    inline bool dequeue(double timeout_sec, T &rVal) override
    {
        _overflow = false;

        return Base::dequeue(timeout_sec, rVal);
    }

    inline bool overflow(void) const noexcept
    {
        return _overflow;
    }

    inline void resetOverflow(void) noexcept
    {
        _overflow = false;
    }

private:
    size_t _maxSize{0};
    std::atomic_bool _overflow{false};
};
