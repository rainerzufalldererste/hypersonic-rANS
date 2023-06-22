#ifndef rans_h__
#define rans_h__

#include <stdint.h>
#include <stddef.h>

constexpr uint32_t TotalSymbolCountBits = 15;
constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);
constexpr size_t DecodeConsumePoint = 1 << 23;
constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint >> TotalSymbolCountBits) << 8);

static_assert(TotalSymbolCountBits < 16);

#endif // rans_h__
