#include <stdint.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "simd_platform.h"

constexpr uint32_t TotalSymbolCountBits = 5;
constexpr uint32_t TotalSymbolCount = ((uint32_t)1 << TotalSymbolCountBits);

struct hist_t
{
  uint32_t symbolCount[256];
  uint32_t cumul[256];
  uint32_t total;
};

struct hist_dec_t : hist_t
{
  uint8_t cumulInv[TotalSymbolCount];
};

void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size)
{
  memset(pHist, 0, sizeof(hist_t));

  for (size_t i = 0; i < size; i++)
    pHist->symbolCount[pData[i]]++;

  uint32_t counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = counter;
    counter += pHist->symbolCount[i];
  }

  pHist->total = counter;

  const uint32_t div = counter / TotalSymbolCount;

  uint32_t capped[256];
  uint32_t cappedSum = 0;

  if (div)
  {
    const uint32_t add = div / 2;

    for (size_t i = 0; i < 256; i++)
    {
      capped[i] = ((pHist->symbolCount[i] + add) / div) | (pHist->symbolCount[i]);
      cappedSum += capped[i];
    }
  }
  else
  {
    const uint32_t mul = TotalSymbolCount / counter;

    for (size_t i = 0; i < 256; i++)
    {
      capped[i] = pHist->symbolCount[i] * mul;
      cappedSum += capped[i];
    }
  }

  if (cappedSum != TotalSymbolCount)
  {
    float realProb[256];

    for (size_t i = 0; i < 256; i++)
      realProb[i] = pHist->symbolCount[i] / (float)size;

    (void)realProb;

    while (cappedSum > TotalSymbolCount) // Start stealing.
    {
      size_t target = 2;

      while (true)
      {
        size_t found = TotalSymbolCount;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] > target && capped[i] < found)
            found = capped[i];

        if (found == TotalSymbolCount)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]--;
            cappedSum--;

            if (cappedSum == TotalSymbolCount)
              goto hist_ready;
          }
        }

        target = found + 1;
      }
    }
    
    while (cappedSum < TotalSymbolCount) // Start a charity.
    {
      size_t target = TotalSymbolCount;

      while (true)
      {
        size_t found = 1;

        for (size_t i = 0; i < 256; i++)
          if (capped[i] < target && capped[i] > found)
            found = capped[i];

        if (found == 1)
          break;

        for (size_t i = 0; i < 256; i++)
        {
          if (capped[i] == found)
          {
            capped[i]++;
            cappedSum++;

            if (cappedSum == TotalSymbolCount)
              goto hist_ready;
          }
        }

        target = found - 1;
      }
    }
  }

hist_ready:

  counter = 0;

  for (size_t i = 0; i < 256; i++)
  {
    pHist->cumul[i] = counter;
    pHist->symbolCount[i] = capped[i];
    counter += capped[i];
  }

  pHist->total = counter;
}

void make_dec_hist(hist_dec_t *pHistDec, const hist_t *pHist)
{
  memcpy(pHistDec, pHist, sizeof(hist_t));

  uint8_t sym = 0;

  for (size_t i = 0; i < TotalSymbolCount; i++)
  {
    while (sym != 0xFF && (!pHist->symbolCount[sym] || pHist->cumul[sym + 1] <= i))
      sym++;

    pHistDec->cumulInv[i] = sym;
  }
}

uint32_t encode_symbol(const uint8_t symbol, hist_t *pHist, const uint32_t state)
{
  const uint32_t symbolCount = pHist->symbolCount[symbol];

  return (state / symbolCount) * TotalSymbolCount + pHist->cumul[symbol] + (state % symbolCount);
}

uint8_t decode_symbol(uint32_t *pState, hist_dec_t *pHist)
{
  const uint32_t slot = *pState % TotalSymbolCount;
  const uint8_t symbol = pHist->cumulInv[slot];
  const uint32_t previousState = (*pState / TotalSymbolCount) * pHist->symbolCount[symbol] + slot - pHist->cumul[symbol];

  *pState = previousState;

  return symbol;
}

int32_t main(const int64_t argc, char **pArgv)
{
  if (argc == 0)
  {
    puts("Invalid Parameter.");
    return 1;
  }

  size_t fileSize = 0;
  uint8_t *pUncompressedData = nullptr;
  uint8_t *pDecompressedData = nullptr;

  // Read File.
  {
    FILE *pFile = fopen(pArgv[1], "rb");

    if (!pFile)
    {
      puts("Failed to read file.");
      return 1;
    }

    fseek(pFile, 0, SEEK_END);
    fileSize = ftell(pFile);

    if (fileSize <= 0)
    {
      puts("Invalid File size / failed to read file.");
      fclose(pFile);
      return 1;
    }

    fseek(pFile, 0, SEEK_SET);

    pUncompressedData = (uint8_t *)malloc(fileSize);
    pDecompressedData = (uint8_t *)malloc(fileSize);

    if (pUncompressedData == nullptr || pDecompressedData == nullptr)
    {
      puts("Memory allocation failure.");
      fclose(pFile);
      return 1;
    }

    if (fileSize != fread(pUncompressedData, 1, (size_t)fileSize, pFile))
    {
      puts("Failed to read file.");
      fclose(pFile);
      return 1;
    }

    fclose(pFile);
  }

  hist_t hist;
  make_hist(&hist, pUncompressedData, fileSize);

  if (hist.total != TotalSymbolCount)
  {
    puts("Ahhhh! Not `TotalSymbolCount`!");
    return 1;
  }

  puts("Hist:");

  for (size_t i = 0; i < 256; i++)
    if (hist.symbolCount[i])
      printf("0x%02" PRIX8 "(%c): %" PRIu32 " (%" PRIu32 ")\n", (uint8_t)i, (char)i, hist.symbolCount[i], hist.cumul[i]);

  puts("");

  uint32_t state = 0;

  for (int64_t i = fileSize - 1; i >= 0; i--)
  {
    printf("%02" PRIX8 " (%c) ", pUncompressedData[i], (char)pUncompressedData[i]);

    state = encode_symbol(pUncompressedData[i], &hist, state);

    printf("=> %" PRIu32 "\n", state);
  }

  unsigned long bits;
  _BitScanReverse(&bits, state);

  printf("\n%4.2f bytes (%" PRIu32 " bits) from %" PRIu64 " bytes.\n", bits / 8.0, bits, fileSize);

  puts("");

  hist_dec_t histDec;
  make_dec_hist(&histDec, &hist);

  puts("DecHist:");

  for (size_t i = 0; i < TotalSymbolCount;)
  {
    const uint8_t sym = histDec.cumulInv[i];

    size_t j = i + 1;

    for (; j < TotalSymbolCount; j++)
      if (histDec.cumulInv[j] != sym)
        break;

    printf("%02" PRIX8 "(%c): %" PRIu64 " ~ %" PRIu64 "\n", sym, (char)sym, i, j - 1);

    i = j;
  }

  puts("");

  for (size_t i = 0; i < fileSize; i++)
  {
    printf("%" PRIu32 " ", state);

    pDecompressedData[i] = decode_symbol(&state, &histDec);

    printf("=> %02" PRIX8 " (%c)\n", pDecompressedData[i], (char)pDecompressedData[i]);
  }

  return 0;
}
