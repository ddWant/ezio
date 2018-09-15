/*
 @ 0xCCCCCCCC
*/

#ifndef EZIO_IO_CONTEXT_H_
#define EZIO_IO_CONTEXT_H_

#include <cstdint>
#include <type_traits>

#include "kbase/basic_macros.h"

#if defined(OS_POSIX)
#include <sys/epoll.h>
#else
#include <Windows.h>
#endif

namespace ezio {

enum IOEvent : uint32_t {
    None = 0,
#if defined(OS_POSIX)
    Read = EPOLLIN | EPOLLPRI,
    Write = EPOLLOUT
#else
    Probe = 1 << 0,
    Read = 1 << 1,
    Write = 1 << 2
#endif
};

using IOEventType = std::underlying_type_t<IOEvent>;

#if defined(OS_POSIX)

struct IOContext {
    IOEventType event;

    // Details on Linux is currently no use.
    struct Details {
        constexpr Details() noexcept = default;
        ~Details() = default;
    };

    explicit IOContext(IOEventType event) noexcept
        : event(event)
    {}

    constexpr Details ToDetails() const noexcept
    {
        return Details{};
    }
};

#else

struct IORequest : OVERLAPPED {
    IOEventType events;

    explicit IORequest(IOEventType events)
        : events(events)
    {
        Reset();
    }

    void Reset()
    {
        Offset = 0;
        OffsetHigh = 0;
        Internal = 0;
        InternalHigh = 0;
        hEvent = nullptr;
    }
};

struct IOContext {
    IORequest* io_req;
    DWORD bytes_transferred;

    struct Details {
        IOEventType events;
        DWORD bytes_transferred;

        Details(IOEventType events, DWORD bytes)
            : events(events), bytes_transferred(bytes)
        {}
    };

    IOContext(IORequest* req, DWORD bytes)
        : io_req(req), bytes_transferred(bytes)
    {}

    Details ToDetails() const noexcept
    {
        return {io_req->events, bytes_transferred};
    }
};

#endif

}   // namespace ezio

#endif  // EZIO_IO_CONTEXT_H_