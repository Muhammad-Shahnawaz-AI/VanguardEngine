#pragma once
#include <cstdint>
#include <cstddef>

namespace vg {

using u8  = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8  = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

constexpr u32 TARGET_TICK_HZ = 60;
constexpr f64 FIXED_DT       = 1.0 / static_cast<f64>(TARGET_TICK_HZ);
constexpr f32 FIXED_DT_F     = 1.0f / static_cast<f32>(TARGET_TICK_HZ);

} // namespace vg
