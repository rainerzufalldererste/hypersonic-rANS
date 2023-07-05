#include "block_rANS32x32_16w.h"

#include "hist.h"
#include "simd_platform.h"

#include <string.h>
#include <math.h>

constexpr size_t StateCount = 32; // Needs to be a power of two.
constexpr bool EncodeNoBranch = false;
constexpr size_t SafeHistBitMax = 0;
constexpr size_t MinBlockSizeBits = 15;
constexpr size_t MinBlockSize = 1 << MinBlockSizeBits;

template <size_t TotalSymbolCountBits>
struct HistReplaceMul
{
  constexpr static size_t GetValue();
};

template <> struct HistReplaceMul<15> { constexpr static size_t GetValue() { return 822; } };
template <> struct HistReplaceMul<14> { constexpr static size_t GetValue() { return 2087; } };
template <> struct HistReplaceMul<13> { constexpr static size_t GetValue() { return 3120; } };
template <> struct HistReplaceMul<12> { constexpr static size_t GetValue() { return 5600; } };
template <> struct HistReplaceMul<11> { constexpr static size_t GetValue() { return 7730; } };
template <> struct HistReplaceMul<10> { constexpr static size_t GetValue() { return 4000; } };

size_t block_rANS32x32_16w_capacity(const size_t inputSize)
{
  const size_t baseSize = 2 * sizeof(uint64_t) + 256 * sizeof(uint16_t) + inputSize + StateCount * sizeof(uint32_t);
  const size_t blockCount = (inputSize + MinBlockSize) / MinBlockSize + 1;
  const size_t perBlockExtraSize = sizeof(uint64_t) + 256 * sizeof(uint16_t);

  return baseSize + blockCount * perBlockExtraSize; // inputIndex hope this covers all of our bases.
}

//////////////////////////////////////////////////////////////////////////

static const uint8_t _Rans32x32_idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
static_assert(sizeof(_Rans32x32_idx2idx) == StateCount);

//////////////////////////////////////////////////////////////////////////

struct _rans_encode_state_t
{
  uint32_t states[StateCount];
  hist_t hist;
  uint16_t *pEnd, *pStart; // both compressed.
};

enum rans32x32_encoder_type_t
{
  r32x32_et_scalar,
};

template <rans32x32_encoder_type_t type>
struct rans32x32_16w_encoder
{
  template <uint32_t TotalSymbolCountBits>
  static void encode_section(_rans_encode_state_t *pState, const uint8_t *pInData, const size_t startIndex, const size_t targetIndex);
};

template <>
struct rans32x32_16w_encoder<r32x32_et_scalar>
{
  template <uint32_t TotalSymbolCountBits>
  static void encode_section(_rans_encode_state_t *pState, const uint8_t *pInData, const size_t startIndex, const size_t targetIndex)
  {
    int64_t targetCmp = targetIndex + StateCount;

    constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

    for (int64_t i = startIndex; i >= (int64_t)targetCmp; i -= StateCount)
    {
      for (int64_t j = StateCount - 1; j >= 0; j--)
      {
        const uint8_t index = _Rans32x32_idx2idx[j];

        const uint8_t in = pInData[i - StateCount + index];
        const uint32_t symbolCount = pState->hist.symbolCount[in];
        const uint32_t max = EncodeEmitPoint * symbolCount;

        const size_t stateIndex = j;

        uint32_t state = pState->states[stateIndex];

        if constexpr (EncodeNoBranch)
        {
          const bool write = state >= max;
          *pState->pStart = (uint16_t)(state & 0xFFFF);
          *pState->pStart -= (size_t)write;
          state = write ? state >> 16 : state;
        }
        else
        {
          if (state >= max)
          {
            *pState->pStart = (uint16_t)(state & 0xFFFF);
            pState->pStart--;
            state >>= 16;
          }
        }

        pState->states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)pState->hist.cumul[in] + (state % symbolCount);
      }
    }
  }
};

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
static bool _CanExtendHist(const uint8_t *pData, const size_t nextBlockStartOffset, const size_t nextBlockSize, hist_t *pOldHist, uint32_t symCount[256])
{
  constexpr bool IsSafeHist = TotalSymbolCountBits >= SafeHistBitMax;

  memset(symCount, 0, sizeof(uint32_t) * 256);
  observe_hist(symCount, pData + nextBlockStartOffset, nextBlockSize);

  // Do we include a symbol that hasn't been included before?
  if constexpr (!IsSafeHist)
  {
    for (size_t j = 0; j < 256; j++)
      if (symCount[j] > 0 && pOldHist->symbolCount[j] == 0)
        return false;
  }

  hist_t newHist;

  if constexpr (!IsSafeHist && TotalSymbolCountBits == MinBlockSizeBits)
  {
    for (size_t j = 0; j < 256; j++)
      newHist.symbolCount[j] = (uint16_t)symCount[j];

    size_t counter = 0;

    for (size_t j = 0; j < 256; j++)
    {
      newHist.cumul[j] = (uint16_t)counter;
      counter += newHist.symbolCount[j];
    }
  }
  else
  {
    if constexpr (IsSafeHist)
    {
      for (size_t j = 0; j < 256; j++)
        symCount[j]++;

      normalize_hist(&newHist, symCount, MinBlockSize + 256, TotalSymbolCountBits);
    }
    else
    {
      normalize_hist(&newHist, symCount, MinBlockSize, TotalSymbolCountBits);
    }
  }

  constexpr size_t totalSymbolCount = (1 << TotalSymbolCountBits);
  constexpr size_t histReplacePoint = (totalSymbolCount * HistReplaceMul<TotalSymbolCountBits>::GetValue()) >> 12;

  // this comparison isn't fair or fast, but should be a good starting point hopefully.
  float costBefore = 0;
  float costAfter = 0;

  if constexpr (IsSafeHist)
  {
    for (size_t j = 0; j < 256; j++)
    {
      if (symCount[j] == 0)
        continue;

      const float before = (symCount[j] - 1) * log2f(pOldHist->symbolCount[j] / (float)totalSymbolCount);
      const float after = (symCount[j] - 1) * log2f(newHist.symbolCount[j] / (float)totalSymbolCount);

      costBefore -= before;
      costAfter -= after;
    }
  }
  else
  {
    for (size_t j = 0; j < 256; j++)
    {
      if (symCount[j] == 0)
        continue;

      const float before = symCount[j] * log2f(pOldHist->symbolCount[j] / (float)totalSymbolCount);
      const float after = symCount[j] * log2f(newHist.symbolCount[j] / (float)totalSymbolCount);

      costBefore -= before;
      costAfter -= after;
    }
  }

  const float diff = costBefore - costAfter;

  return (diff < histReplacePoint);
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits, rans32x32_encoder_type_t Impl>
size_t block_rANS32x32_16w_encode(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity)
{
  if (outCapacity < block_rANS32x32_16w_capacity(length))
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

  constexpr bool IsSafeHist = TotalSymbolCountBits >= SafeHistBitMax;

  _rans_encode_state_t encodeState;
  encodeState.pEnd = reinterpret_cast<uint16_t *>(pOutData + outCapacity - sizeof(uint16_t));
  encodeState.pStart = encodeState.pEnd;
  
  size_t inputBlockTargetIndex = (((length - 1) & ~(size_t)(StateCount - 1)) & ~(size_t)(MinBlockSize - 1));

  if (inputBlockTargetIndex > MinBlockSize)
    inputBlockTargetIndex -= MinBlockSize;

  size_t blockBackPoint = length;

  uint32_t symCount[256];
  observe_hist(symCount, pInData + inputBlockTargetIndex, blockBackPoint - inputBlockTargetIndex);

  if constexpr (IsSafeHist)
    for (size_t j = 0; j < 256; j++)
      if (symCount[j] == 0)
        symCount[j] = 1;

  normalize_hist(&encodeState.hist, symCount, blockBackPoint - inputBlockTargetIndex, TotalSymbolCountBits);

  while (inputBlockTargetIndex > 0)
  {
    if (_CanExtendHist<TotalSymbolCountBits>(pInData, inputBlockTargetIndex - MinBlockSize, MinBlockSize, &encodeState.hist, symCount))
      inputBlockTargetIndex -= MinBlockSize;
    else
      break;
  }

  // Performance of this could be improved by keeping the current counts around. (or simply using the original hist, if that was only good for one block)
  observe_hist(symCount, pInData + inputBlockTargetIndex, blockBackPoint - inputBlockTargetIndex);
  normalize_hist(&encodeState.hist, symCount, blockBackPoint - inputBlockTargetIndex, TotalSymbolCountBits);
  blockBackPoint = length;

  // Init States.
  for (size_t i = 0; i < StateCount; i++)
    encodeState.states[i] = DecodeConsumePoint16;

  int64_t inputIndex = length - 1;
  inputIndex &= ~(size_t)(StateCount - 1);
  inputIndex += StateCount;

  for (int64_t j = StateCount - 1; j >= 0; j--)
  {
    const uint8_t index = _Rans32x32_idx2idx[j];

    if (inputIndex - (int64_t)StateCount + (int64_t)index < (int64_t)length)
    {
      const uint8_t in = pInData[inputIndex - StateCount + index];
      const uint32_t symbolCount = encodeState.hist.symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = encodeState.states[stateIndex];

      if (state >= max)
      {
        *encodeState.pStart = (uint16_t)(state & 0xFFFF);
        encodeState.pStart--;
        state >>= 16;
      }

      encodeState.states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)encodeState.hist.cumul[in] + (state % symbolCount);
    }
  }

  inputIndex -= StateCount;

  while (true)
  {
    rans32x32_16w_encoder<Impl>::template encode_section<TotalSymbolCountBits>(&encodeState, pInData, inputIndex, inputBlockTargetIndex);
    inputIndex = inputBlockTargetIndex;

    // Write hist.
    {
      const uint64_t blockSize = blockBackPoint - inputBlockTargetIndex;

      encodeState.pStart++;
      encodeState.pStart -= 256;
      memcpy(encodeState.pStart, encodeState.hist.symbolCount, sizeof(encodeState.hist.symbolCount));

      encodeState.pStart -= sizeof(uint64_t) / sizeof(uint16_t);
      memcpy(encodeState.pStart, &blockSize, sizeof(blockSize));

      encodeState.pStart--;
    }

    if (inputIndex == 0)
      break;

    // Determine new histogram.
    {
      inputBlockTargetIndex -= MinBlockSize;

      observe_hist(symCount, pInData + inputBlockTargetIndex, MinBlockSize);

      if constexpr (IsSafeHist)
        for (size_t j = 0; j < 256; j++)
          if (symCount[j] == 0)
            symCount[j] = 1;

      normalize_hist(&encodeState.hist, symCount, MinBlockSize, TotalSymbolCountBits);

      while (inputBlockTargetIndex > 0)
      {
        if (_CanExtendHist<TotalSymbolCountBits>(pInData, inputBlockTargetIndex - MinBlockSize, MinBlockSize, &encodeState.hist, symCount))
          inputBlockTargetIndex -= MinBlockSize;
        else
          break;
      }

      // Performance of this could be improved by keeping the current counts around. (or simply using the original hist, if that was only good for one block)
      observe_hist(symCount, pInData + inputBlockTargetIndex, blockBackPoint - inputBlockTargetIndex);
      normalize_hist(&encodeState.hist, symCount, blockBackPoint - inputBlockTargetIndex, TotalSymbolCountBits);
      blockBackPoint = inputIndex;
    }
  }

  uint8_t *pWrite = pOutData;
  size_t outIndex = 0;

  *reinterpret_cast<uint64_t *>(pWrite + outIndex) = (uint64_t)length;
  outIndex += sizeof(uint64_t);

  // compressed expected length.
  outIndex += sizeof(uint64_t);

  for (size_t j = 0; j < StateCount; j++)
  {
    *reinterpret_cast<uint32_t *>(pWrite + outIndex) = encodeState.states[j];
    outIndex += sizeof(uint32_t);
  }

  const size_t size = (encodeState.pEnd - encodeState.pStart) * sizeof(uint16_t);

  memmove(pWrite + outIndex, encodeState.pStart + 1, size);
  outIndex += size;

  *reinterpret_cast<uint64_t *>(pOutData + sizeof(uint64_t)) = outIndex; // write total output length.

  return outIndex;
}

//////////////////////////////////////////////////////////////////////////

size_t block_rANS32x32_16w_encode_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode<15, r32x32_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode<14, r32x32_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode<13, r32x32_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode<12, r32x32_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode<11, r32x32_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode<10, r32x32_et_scalar>(pInData, length, pOutData, outCapacity); }
