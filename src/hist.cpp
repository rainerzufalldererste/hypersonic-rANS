#include "hist.h"

#include <string.h>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////

void observe_hist(uint32_t hist[256], const uint8_t *pData, const size_t size)
{
  memset(hist, 0, sizeof(uint32_t) * 256);

  for (size_t i = 0; i < size; i++)
    hist[pData[i]]++;
}

void normalize_hist(hist_t *pHist, const uint32_t hist[256], const size_t dataBytes, const size_t totalSymbolCountBits)
{
  const uint32_t totalSymbolCount = ((uint32_t)1 << totalSymbolCountBits);

  size_t counter = dataBytes;
  uint16_t capped[256];
  size_t cappedSum = 0;

  constexpr bool FloatingPointHistLimit = true;

  if constexpr (FloatingPointHistLimit)
  {
    constexpr bool NewHistModeling = false; // This is far more consistent performance wise, but ends up being slower on average.

    if constexpr (NewHistModeling)
    {
      const float mulStage1 = (float)totalSymbolCount / counter;

      uint32_t nonOneCount = 0;
      uint32_t onlyOneCount = 0;

      for (size_t i = 0; i < 256; i++)
      {
        if (hist[i] * mulStage1 >= 1.5)
          nonOneCount += hist[i];
        else
          onlyOneCount++;
      }

      const float maxCombined = nonOneCount + onlyOneCount * totalSymbolCount / (float)(counter - onlyOneCount);
      const float mulStage2 = (float)totalSymbolCount / maxCombined;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)(hist[i] * mulStage2 + 0.5f);

        if (capped[i] == 0 && hist[i])
          capped[i] = 1;

        cappedSum += capped[i];
      }
    }
    else
    {
      const float mul = (float)totalSymbolCount / counter;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)(hist[i] * mul + 0.5f);

        if (capped[i] == 0 && hist[i])
          capped[i] = 1;

        cappedSum += capped[i];
      }
    }
  }
  else
  {
    const uint32_t div = (uint32_t)(counter / (size_t)totalSymbolCount);

    if (div)
    {
      const uint32_t add = div / 2;

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)((hist[i] + add) / div);

        if (capped[i] == 0 && hist[i])
          capped[i] = 1;

        cappedSum += capped[i];
      }
    }
    else
    {
      const uint32_t mul = (uint32_t)((size_t)totalSymbolCount / counter);

      for (size_t i = 0; i < 256; i++)
      {
        capped[i] = (uint16_t)(hist[i] * mul);
        cappedSum += capped[i];
      }
    }
  }

  if (cappedSum != totalSymbolCount)
  {
    uint8_t sortedIdx[256];

    for (size_t i = 0; i < 256; i++)
      sortedIdx[i] = (uint8_t)i;

    struct _internal
    {
      static void heapify(uint8_t *pIdx, const uint16_t *pVal, const int64_t n, const int64_t i)
      {
        const int64_t left = 2 * i + 1;
        const int64_t right = 2 * i + 2;
        int64_t largest = i;

        if (left < n && pVal[pIdx[left]] > pVal[pIdx[largest]])
          largest = left;

        if (right < n && pVal[pIdx[right]] > pVal[pIdx[largest]])
          largest = right;

        if (largest != i)
        {
          std::swap(pIdx[i], pIdx[largest]);
          heapify(pIdx, pVal, n, largest);
        }
      }

      static void heapSort(uint8_t *pIdx, const uint16_t *pVal, const size_t length)
      {
        for (int64_t i = (int64_t)length / 2 - 1; i >= 0; i--)
          heapify(pIdx, pVal, length, i);

        for (int64_t i = length - 1; i >= 0; i--)
        {
          std::swap(pIdx[0], pIdx[i]);
          heapify(pIdx, pVal, i, 0);
        }
      }
    };

    _internal::heapSort(sortedIdx, capped, 256);
    size_t minTwo = 0;

    for (size_t i = 0; i < 256; i++)
    {
      if (capped[sortedIdx[i]] >= 2)
      {
        minTwo = i;
        break;
      }
    }

    while (cappedSum > totalSymbolCount) // Start stealing.
    {
      for (size_t i = minTwo; i < 256; i++)
      {
        capped[sortedIdx[i]]--;
        cappedSum--;

        if (cappedSum == totalSymbolCount)
          goto hist_ready;
      }

      // Re-Adjust `minTwo`.
      for (size_t i = minTwo; i < 256; i++)
      {
        if (capped[sortedIdx[i]] >= 2)
        {
          minTwo = i;
          break;
        }
      }
    }

    while (cappedSum < totalSymbolCount) // Start a charity.
    {
      for (int64_t i = 255; i >= (int64_t)minTwo; i--)
      {
        capped[sortedIdx[i]]++;
        cappedSum++;

        if (cappedSum == totalSymbolCount)
          goto hist_ready;
      }

      // Re-Adjust `minTwo`.
      for (size_t i = minTwo; i < 256; i++)
      {
        if (capped[sortedIdx[i]] >= 2)
        {
          minTwo = i;
          break;
        }
      }
    }
  }

hist_ready:
  counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    pHist->symbolCount[i] = capped[i];
    counter += capped[i];
  }

#if defined(_DEBUG) && defined(_MSC_VER)
  if (counter != totalSymbolCount)
    __debugbreak();
#endif
}

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size, const size_t totalSymbolCountBits)
{
  uint32_t hist[256];
  observe_hist(hist, pData, size);
  normalize_hist(pHist, hist, size, totalSymbolCountBits);
}

void make_enc_hist(hist_enc_t *pHistEnc, const hist_t *pHist)
{
  for (size_t i = 0; i < 256; i++)
    pHistEnc->symbols[i] = pHist->cumul[i] << 16 | pHist->symbolCount[i];
}

template <uint32_t TotalSymbolCountBits>
void make_dec_hist(hist_dec_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  memcpy(pHistDec, pHist, sizeof(hist_t));

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

template <uint32_t TotalSymbolCountBits>
void make_dec2_hist(hist_dec2_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  for (size_t i = 0; i < 256; i++)
  {
    pHistDec->symbols[i].cumul = pHist->cumul[i];
    pHistDec->symbols[i].freq = pHist->symbolCount[i];
  }

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

template <uint32_t TotalSymbolCountBits>
void make_dec3_hist(hist_dec3_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
    pHistDec->symbolFomCumul[i].cumul = pHist->cumul[sym];
    pHistDec->symbolFomCumul[i].freq = pHist->symbolCount[sym];
  }
}

template <uint32_t TotalSymbolCountBits>
void make_dec_pack_hist(hist_dec_pack_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist)
{
  static_assert(TotalSymbolCountBits <= 12);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->symbol[i] = sym | ((uint32_t)pHist->cumul[sym] << 8) | ((uint32_t)pHist->symbolCount[sym] << 20);
  }
}

bool inplace_complete_hist(hist_t *pHist, const size_t totalSymbolCountBits)
{
  uint32_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    counter += pHist->symbolCount[i];
  }

#if defined(_DEBUG) && defined(_MSC_VER)
  if (counter != ((uint32_t)1 << totalSymbolCountBits))
    __debugbreak();
#endif

  return (counter == (uint32_t)(1 << totalSymbolCountBits));
}

template <uint32_t TotalSymbolCountBits>
bool inplace_make_hist_dec(hist_dec_t<TotalSymbolCountBits> *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint16_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = (uint16_t)counter;
    counter += pHist->symbolCount[i];
  }

  if (counter != TotalSymbolCount)
    return false;

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHist->cumulInv[i] = sym;
  }

  return true;
}

template <uint32_t TotalSymbolCountBits>
bool inplace_make_hist_dec2(hist_dec2_t<TotalSymbolCountBits> *pHist)
{
  static_assert(TotalSymbolCountBits < 16);
  constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

  uint16_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->symbols[i].cumul = (uint16_t)counter;
    counter += pHist->symbols[i].freq;
  }

  if (counter != TotalSymbolCount)
    return false;

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbols[sym].freq || pHist->symbols[sym + 1].cumul <= i))
      sym++;

    pHist->cumulInv[i] = sym;
  }

  return true;
}

template void make_dec_hist(hist_dec_t<15> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<14> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<13> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<12> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<11> *pHistDec, const hist_t *pHist);
template void make_dec_hist(hist_dec_t<10> *pHistDec, const hist_t *pHist);

template void make_dec2_hist(hist_dec2_t<15> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<14> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<13> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<12> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<11> *pHistDec, const hist_t *pHist);
template void make_dec2_hist(hist_dec2_t<10> *pHistDec, const hist_t *pHist);

template void make_dec3_hist(hist_dec3_t<15> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<14> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<13> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<12> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<11> *pHistDec, const hist_t *pHist);
template void make_dec3_hist(hist_dec3_t<10> *pHistDec, const hist_t *pHist);

template bool inplace_make_hist_dec(hist_dec_t<15> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<14> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<13> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<12> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<11> *pHist);
template bool inplace_make_hist_dec(hist_dec_t<10> *pHist);

template bool inplace_make_hist_dec2(hist_dec2_t<15> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<14> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<13> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<12> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<11> *pHist);
template bool inplace_make_hist_dec2(hist_dec2_t<10> *pHist);

template void make_dec_pack_hist(hist_dec_pack_t<12> *pHistDec, const hist_t *pHist);
template void make_dec_pack_hist(hist_dec_pack_t<11> *pHistDec, const hist_t *pHist);
template void make_dec_pack_hist(hist_dec_pack_t<10> *pHistDec, const hist_t *pHist);
