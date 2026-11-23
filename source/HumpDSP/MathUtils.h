#pragma once

#include <functional>

namespace hump
{

// Exact float comparison for change detection, where bit-identical values
// are exactly what we mean (silences -Wfloat-equal at the call sites).
inline bool exactlyEqual (float a, float b) noexcept
{
    return std::equal_to<float>() (a, b);
}

} // namespace hump
