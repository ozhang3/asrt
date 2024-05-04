#ifndef A5AA74F2_19F7_4D46_969C_8B21749B1A6A
#define A5AA74F2_19F7_4D46_969C_8B21749B1A6A

#include <cstdint>
#include "asrt/reactor/types.hpp"

namespace ExecutorNS::Types{

    enum class UnblockReason : std::uint8_t{
        kTimeout,
        kSignal,
        kSoftwareEvent,
        kUnblocked,
        kEventsHandled
    };

    using ScheduledJobId = ReactorNS::Types::HandlerTag;
}

#endif /* A5AA74F2_19F7_4D46_969C_8B21749B1A6A */
