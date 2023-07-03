#include "block_rANS32x32_16w.h"

#include "hist.h"
#include "simd_platform.h"

#include <string.h>
#include <math.h>

#include <stdio.h>
#include <inttypes.h>

constexpr size_t StateCount = 32; // Needs to be a power of two.
constexpr bool EncodeNoBranch = false;
//constexpr bool DecodeNoBranch = false;
constexpr size_t SafeHistBitMax = 0;
constexpr size_t MinBlockSize = 1 << 15;

template <size_t TotalSymbolCountBits>
struct HistReplaceMul
{
  constexpr static size_t GetValue();
};

template <> struct HistReplaceMul<15> { constexpr static size_t GetValue() { return 200; } };
template <> struct HistReplaceMul<14> { constexpr static size_t GetValue() { return 200; } };
template <> struct HistReplaceMul<13> { constexpr static size_t GetValue() { return 200; } };
template <> struct HistReplaceMul<12> { constexpr static size_t GetValue() { return 200; } };
template <> struct HistReplaceMul<11> { constexpr static size_t GetValue() { return 200; } };
template <> struct HistReplaceMul<10> { constexpr static size_t GetValue() { return 200; } };

size_t block_rANS32x32_16w_capacity(const size_t inputSize)
{
  const size_t baseSize = 2 * sizeof(uint64_t) + 256 * sizeof(uint16_t) + inputSize + StateCount * sizeof(uint32_t);
  const size_t blockCount = (inputSize + MinBlockSize) / MinBlockSize + 1;
  const size_t perBlockExtraSize = sizeof(uint64_t) + 256 * sizeof(uint16_t);

  return baseSize + blockCount * perBlockExtraSize; // i hope this covers all of our bases.
}

//////////////////////////////////////////////////////////////////////////

template <uint32_t TotalSymbolCountBits>
size_t block_rANS32x32_16w_encode_scalar(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity)
{
  if (outCapacity < block_rANS32x32_16w_capacity(length))
    return 0;

  static_assert(TotalSymbolCountBits < 16);
  constexpr size_t EncodeEmitPoint = ((DecodeConsumePoint16 >> TotalSymbolCountBits) << 16);

  constexpr bool IsSafeHist = TotalSymbolCountBits >= SafeHistBitMax;

  uint32_t states[StateCount];
  uint16_t *pEnd = reinterpret_cast<uint16_t *>(pOutData + outCapacity - sizeof(uint16_t));
  uint16_t *pStart = pEnd;
  uint16_t *pBlockBack = pStart;
  size_t blockLowI = (((length - 1) & ~(size_t)(StateCount - 1)) & ~(size_t)(MinBlockSize - 1));
  size_t blockLowCmp = blockLowI + StateCount;

  size_t histCount = 1;
  size_t histPotentialCount = 1;
  size_t histDiff = 0;
  size_t histPotentialDiff = 0;
  size_t histRejectedDiff = 0;

  if (blockLowI > MinBlockSize)
    blockLowI -= MinBlockSize;

  uint32_t symCount[256];
  observe_hist(symCount, pInData + blockLowI, length - blockLowI);

  if constexpr (IsSafeHist)
    for (size_t j = 0; j < 256; j++)
      if (symCount[j] == 0)
        symCount[j] = 1;

  hist_t hist;
  normalize_hist(&hist, symCount, length - blockLowI, TotalSymbolCountBits);

  // Init States.
  for (size_t i = 0; i < StateCount; i++)
    states[i] = DecodeConsumePoint16;

  const uint8_t idx2idx[] = { 0x00, 0x01, 0x02, 0x03, 0x10, 0x11, 0x12, 0x13, 0x04, 0x05, 0x06, 0x07, 0x14, 0x15, 0x16, 0x17, 0x08, 0x09, 0x0A, 0x0B, 0x18, 0x19, 0x1A, 0x1B, 0x0C, 0x0D, 0x0E, 0x0F, 0x1C, 0x1D, 0x1E, 0x1F };
  static_assert(sizeof(idx2idx) == StateCount);

  int64_t i = length - 1;
  i &= ~(size_t)(StateCount - 1);
  i += StateCount;

  for (int64_t j = StateCount - 1; j >= 0; j--)
  {
    const uint8_t index = idx2idx[j];

    if (i - (int64_t)StateCount + (int64_t)index < (int64_t)length)
    {
      const uint8_t in = pInData[i - StateCount + index];
      const uint32_t symbolCount = hist.symbolCount[in];
      const uint32_t max = EncodeEmitPoint * symbolCount;

      const size_t stateIndex = j;

      uint32_t state = states[stateIndex];

      if constexpr (EncodeNoBranch)
      {
        const bool write = state >= max;
        *pStart = (uint16_t)(state & 0xFFFF);
        *pStart -= (size_t)write;
        state = write ? state >> 16 : state;
      }
      else
      {
        if (state >= max)
        {
          *pStart = (uint16_t)(state & 0xFFFF);
          pStart--;
          state >>= 16;
        }
      }

      states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)hist.cumul[in] + (state % symbolCount);
    }
  }

  i -= StateCount;

  while (true)
  {
    for (; i >= (int64_t)blockLowCmp; i -= StateCount)
    {
      for (int64_t j = StateCount - 1; j >= 0; j--)
      {
        const uint8_t index = idx2idx[j];

        const uint8_t in = pInData[i - StateCount + index];
        const uint32_t symbolCount = hist.symbolCount[in];
        const uint32_t max = EncodeEmitPoint * symbolCount;

        const size_t stateIndex = j;

        uint32_t state = states[stateIndex];

        if constexpr (EncodeNoBranch)
        {
          const bool write = state >= max;
          *pStart = (uint16_t)(state & 0xFFFF);
          *pStart -= (size_t)write;
          state = write ? state >> 16 : state;
        }
        else
        {
          if (state >= max)
          {
            *pStart = (uint16_t)(state & 0xFFFF);
            pStart--;
            state >>= 16;
          }
        }

        states[stateIndex] = ((state / symbolCount) << TotalSymbolCountBits) + (uint32_t)hist.cumul[in] + (state % symbolCount);
      }
    }

    if (i == 0)
      break;

    // Potentially replace histogram.
    {
      blockLowI = i - MinBlockSize;
      blockLowCmp = blockLowI + StateCount;

      memset(symCount, 0, sizeof(symCount));
      observe_hist(symCount, pInData + blockLowI, MinBlockSize);

      bool mustReplaceHist = false;

      if constexpr (!IsSafeHist)
      {
        for (size_t j = 0; j < 256; j++)
        {
          if (symCount[j] > 0 && hist.symbolCount[j] == 0)
          {
            mustReplaceHist = true;
            normalize_hist(&hist, symCount, MinBlockSize, TotalSymbolCountBits);
            break;
          }
        }
      }

      if (!mustReplaceHist)
      {
        if constexpr (IsSafeHist)
          for (size_t j = 0; j < 256; j++)
            if (symCount[j] == 0)
              symCount[j] = 1;

        hist_t newHist;

        if constexpr (!IsSafeHist && (1 << TotalSymbolCountBits) == MinBlockSize)
        {
          for (size_t j = 0; j < 256; j++)
            newHist.symbolCount[j] = (uint16_t)symCount[j];
        }
        else
        {
          normalize_hist(&newHist, symCount, MinBlockSize, TotalSymbolCountBits);
        }

        size_t accumAbsDiff = 0;

        for (size_t j = 0; j < 256; j++)
          accumAbsDiff += (size_t)llabs(hist.symbolCount[j] - newHist.symbolCount[j]);

        histPotentialCount++;
        histPotentialDiff += accumAbsDiff;

        constexpr size_t histReplacePoint = ((1 << TotalSymbolCountBits) * HistReplaceMul<TotalSymbolCountBits>::GetValue()) >> 10;

        if (accumAbsDiff >= histReplacePoint)
        {
          histDiff += accumAbsDiff;
          mustReplaceHist = true;
          hist = newHist;
        }
        else
        {
          histRejectedDiff += accumAbsDiff;
        }
      }

      if (mustReplaceHist)
      {
        const uint64_t blockSize = pBlockBack - pStart;

        pStart++;
        pStart -= 256;
        memcpy(pStart, hist.symbolCount, sizeof(hist.symbolCount));
        pStart -= sizeof(uint64_t);
        memcpy(pStart, &blockSize, sizeof(blockSize));

        pStart--;
        pBlockBack = pStart;

        histCount++;
      }
    }
  }

  uint8_t *pWrite = pOutData;
  size_t outIndex = 0;

  *reinterpret_cast<uint64_t *>(pWrite + outIndex) = (uint64_t)length;
  outIndex += sizeof(uint64_t);

  // compressed expected length.
  outIndex += sizeof(uint64_t);

  for (size_t j = 0; j < 256; j++)
  {
    *reinterpret_cast<uint16_t *>(pWrite + outIndex) = hist.symbolCount[j];
    outIndex += sizeof(uint16_t);
  }

  for (size_t j = 0; j < StateCount; j++)
  {
    *reinterpret_cast<uint32_t *>(pWrite + outIndex) = states[j];
    outIndex += sizeof(uint32_t);
  }

  const size_t size = (pEnd - pStart) * sizeof(uint16_t);

  memmove(pWrite + outIndex, pStart + 1, size);
  outIndex += size;

  *reinterpret_cast<uint64_t *>(pOutData + sizeof(uint64_t)) = outIndex; // write total output length.

  printf("\t>>>>> %" PRIu64 " / %" PRIu64 " histograms used. approx block size: %6.3f KiB. avg diff selected: %5.3fk, total: %5.3fk, rejected: %5.3fk\n", histCount, histPotentialCount, (length / 1024.0) / histCount, (histDiff / 1024.0) / histCount, (histPotentialDiff / 1024.0) / histPotentialCount, (histRejectedDiff / 1024.0) / (histPotentialCount - histCount));

  return outIndex;
}



//////////////////////////////////////////////////////////////////////////

size_t block_rANS32x32_16w_encode_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode_scalar<15>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode_scalar<14>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode_scalar<13>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode_scalar<12>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode_scalar<11>(pInData, length, pOutData, outCapacity); }
size_t block_rANS32x32_16w_encode_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity) { return block_rANS32x32_16w_encode_scalar<10>(pInData, length, pOutData, outCapacity); }

size_t block_rANS32x32_16w_decode_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { (void)pInData; (void)inLength; (void)pOutData; (void)outCapacity; return 0; }
size_t block_rANS32x32_16w_decode_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { (void)pInData; (void)inLength; (void)pOutData; (void)outCapacity; return 0; }
size_t block_rANS32x32_16w_decode_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { (void)pInData; (void)inLength; (void)pOutData; (void)outCapacity; return 0; }
size_t block_rANS32x32_16w_decode_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { (void)pInData; (void)inLength; (void)pOutData; (void)outCapacity; return 0; }
size_t block_rANS32x32_16w_decode_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { (void)pInData; (void)inLength; (void)pOutData; (void)outCapacity; return 0; }
size_t block_rANS32x32_16w_decode_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity) { (void)pInData; (void)inLength; (void)pOutData; (void)outCapacity; return 0; }
