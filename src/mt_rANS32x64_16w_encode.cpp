#include "mt_rANS32x64_16w.h"

#include "hist.h"
#include "simd_platform.h"
#include "block_codec64.h"

#include <string.h>
#include <math.h>

constexpr size_t SafeHistBitMax = 0;

constexpr size_t MinMinBlockSizeBits = 15;
constexpr size_t MinMinBlockSize = (size_t)1 << MinMinBlockSizeBits;

template <size_t TotalSymbolCountBits>
struct HistReplaceMul
{
  constexpr static size_t GetValue();
};

template <> struct HistReplaceMul<15> { constexpr static size_t GetValue() { return 50; } };
template <> struct HistReplaceMul<14> { constexpr static size_t GetValue() { return 500; } };
template <> struct HistReplaceMul<13> { constexpr static size_t GetValue() { return 500; } };
template <> struct HistReplaceMul<12> { constexpr static size_t GetValue() { return 500; } };
template <> struct HistReplaceMul<11> { constexpr static size_t GetValue() { return 500; } };
template <> struct HistReplaceMul<10> { constexpr static size_t GetValue() { return 500; } };

template <size_t TotalSymbolCountBits>
struct MinBlockSizeBits
{
  constexpr static size_t GetValue();
};

template <> struct MinBlockSizeBits<15> { constexpr static size_t GetValue() { return 16; } };
template <> struct MinBlockSizeBits<14> { constexpr static size_t GetValue() { return 16; } };
template <> struct MinBlockSizeBits<13> { constexpr static size_t GetValue() { return 16; } };
template <> struct MinBlockSizeBits<12> { constexpr static size_t GetValue() { return 16; } };
template <> struct MinBlockSizeBits<11> { constexpr static size_t GetValue() { return 16; } };
template <> struct MinBlockSizeBits<10> { constexpr static size_t GetValue() { return 16; } };

template <uint32_t TotalSymbolCountBits>
constexpr size_t MinBlockSize()
{
  return (size_t)1 << MinBlockSizeBits<TotalSymbolCountBits>::GetValue();
}

constexpr size_t MaxBlockSizeBits = 25;
constexpr size_t MaxBlockSize = (size_t)1 << MaxBlockSizeBits;

size_t mt_rANS32x64_16w_capacity(const size_t inputSize)
{
  const size_t baseSize = 2 * sizeof(uint64_t) + 256 * sizeof(uint16_t) + inputSize + StateCount * sizeof(uint32_t);
  const size_t blockCount = (inputSize + MinMinBlockSize) / MinMinBlockSize + 1;
  const size_t perBlockExtraSize = sizeof(uint64_t) * 2 + 256 * sizeof(uint16_t) + StateCount * sizeof(uint32_t);

  return baseSize + blockCount * perBlockExtraSize; // inputIndex hope this covers all of our bases.
}

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

  if constexpr (TotalSymbolCountBits == MinBlockSize<TotalSymbolCountBits>())
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
    normalize_hist(&newHist, symCount, MinBlockSize<TotalSymbolCountBits>(), TotalSymbolCountBits);
  }

  constexpr size_t totalSymbolCount = (1 << TotalSymbolCountBits);
  constexpr size_t histReplacePoint = (totalSymbolCount * HistReplaceMul<TotalSymbolCountBits>::GetValue()) >> 12;

  // this comparison isn't fair or fast, but should be a good starting point hopefully.
  float costBefore = 0;
  float costAfter = (float)(sizeof(uint16_t) * 256 + StateCount * sizeof(uint32_t) + sizeof(uint64_t) * 2) * 0.5f; // let's assume that block will be able to share it's histogram with someone else.

  if constexpr (IsSafeHist)
  {
    for (size_t j = 0; j < 256; j++)
    {
      if (symCount[j] == 0)
        continue;

      const float before = (symCount[j] - 1) * log2f(pOldHist->symbolCount[j] / (float)totalSymbolCount);
      const float after = symCount[j] * log2f(newHist.symbolCount[j] / (float)totalSymbolCount);

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

template <uint32_t TotalSymbolCountBits, rans32x64_encoder_type_t Impl>
size_t mt_rANS32x64_16w_encode(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity)
{
  if (outCapacity < mt_rANS32x64_16w_capacity(length))
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

  constexpr bool IsSafeHist = TotalSymbolCountBits >= SafeHistBitMax;
  constexpr size_t MinBlockSizeX = MinBlockSize<TotalSymbolCountBits>();

  _rans_encode_state64_t encodeState;
  encodeState.pEnd = reinterpret_cast<uint16_t *>(pOutData + outCapacity - sizeof(uint16_t));
  encodeState.pStart = encodeState.pEnd;
  
  size_t inputBlockTargetIndex = (((length - 1) & ~(size_t)(StateCount - 1)) & ~(size_t)(MinBlockSizeX - 1));

  if (inputBlockTargetIndex > MinBlockSizeX)
    inputBlockTargetIndex -= MinBlockSizeX;

  uint16_t *pBlockEnd = encodeState.pEnd;
  size_t blockBackPoint = length;

  uint32_t symCount[256];
  observe_hist(symCount, pInData + inputBlockTargetIndex, blockBackPoint - inputBlockTargetIndex);

  size_t extraCount = 0;

  if constexpr (IsSafeHist)
  {
    for (size_t j = 0; j < 256; j++)
    {
      if (symCount[j] == 0)
      {
        symCount[j] = 1;
        extraCount++;
      }
    }
  }

  normalize_hist(&encodeState.hist, symCount, blockBackPoint - inputBlockTargetIndex + extraCount, TotalSymbolCountBits);

  while (inputBlockTargetIndex > 0 && blockBackPoint - inputBlockTargetIndex < MaxBlockSize)
  {
    if (_CanExtendHist<TotalSymbolCountBits>(pInData, inputBlockTargetIndex - MinBlockSizeX, MinBlockSizeX, &encodeState.hist, symCount))
      inputBlockTargetIndex -= MinBlockSizeX;
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
    const uint8_t index = _Rans32x64_idx2idx[j];

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
    rans32x64_16w_encoder<Impl>::template encode_section<TotalSymbolCountBits>(&encodeState, pInData, inputIndex, inputBlockTargetIndex);
    inputIndex = inputBlockTargetIndex;

    // Write hist & states.
    {
      const uint64_t blockSize = blockBackPoint - inputBlockTargetIndex;

      encodeState.pStart++;
      encodeState.pStart -= 256;
      memcpy(encodeState.pStart, encodeState.hist.symbolCount, sizeof(encodeState.hist.symbolCount));

      encodeState.pStart -= sizeof(uint32_t) / sizeof(uint16_t) * StateCount;
      memcpy(encodeState.pStart, encodeState.states, sizeof(uint32_t) * StateCount);

      const uint64_t writeHeadOffset = pBlockEnd - (encodeState.pStart + 1);

      encodeState.pStart -= sizeof(uint64_t) / sizeof(uint16_t);
      memcpy(encodeState.pStart, &writeHeadOffset, sizeof(writeHeadOffset));

      encodeState.pStart -= sizeof(uint64_t) / sizeof(uint16_t);
      memcpy(encodeState.pStart, &blockSize, sizeof(blockSize));

      pBlockEnd = encodeState.pStart;
      encodeState.pStart--;
    }

    if (inputIndex == 0)
      break;

    // Determine new histogram.
    {
      inputBlockTargetIndex -= MinBlockSizeX;

      observe_hist(symCount, pInData + inputBlockTargetIndex, MinBlockSizeX);

      if constexpr (IsSafeHist)
        for (size_t j = 0; j < 256; j++)
          if (symCount[j] == 0)
            symCount[j] = 1;

      normalize_hist(&encodeState.hist, symCount, MinBlockSizeX, TotalSymbolCountBits);

      while (inputBlockTargetIndex > 0 && blockBackPoint - inputBlockTargetIndex < MaxBlockSize)
      {
        if (_CanExtendHist<TotalSymbolCountBits>(pInData, inputBlockTargetIndex - MinBlockSizeX, MinBlockSizeX, &encodeState.hist, symCount))
          inputBlockTargetIndex -= MinBlockSizeX;
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

  const size_t size = (encodeState.pEnd - encodeState.pStart) * sizeof(uint16_t);

  memmove(pWrite + outIndex, encodeState.pStart + 1, size);
  outIndex += size;

  *reinterpret_cast<uint64_t *>(pOutData + sizeof(uint64_t)) = outIndex; // write total output length.

  return outIndex;
}

//////////////////////////////////////////////////////////////////////////

size_t mt_rANS32x64_16w_encode_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return mt_rANS32x64_16w_encode<15, r32x64_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t mt_rANS32x64_16w_encode_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return mt_rANS32x64_16w_encode<14, r32x64_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t mt_rANS32x64_16w_encode_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return mt_rANS32x64_16w_encode<13, r32x64_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t mt_rANS32x64_16w_encode_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return mt_rANS32x64_16w_encode<12, r32x64_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t mt_rANS32x64_16w_encode_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return mt_rANS32x64_16w_encode<11, r32x64_et_scalar>(pInData, length, pOutData, outCapacity); }
size_t mt_rANS32x64_16w_encode_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return mt_rANS32x64_16w_encode<10, r32x64_et_scalar>(pInData, length, pOutData, outCapacity); }
