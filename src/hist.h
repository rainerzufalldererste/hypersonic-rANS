#ifndef hist_h__
#define hist_h__

#include "rans.h"

struct hist_t
{
  uint16_t symbolCount[256];
  uint16_t cumul[256];
};

typedef uint32_t enc_sym_t;

struct hist_enc_t
{
  enc_sym_t symbols[256];
};

template <uint32_t TotalSymbolCountBits>
struct hist_dec_t : hist_t
{
  uint8_t cumulInv[1 << TotalSymbolCountBits];
};

struct dec_sym_t
{
  uint16_t freq, cumul;
};

static_assert(sizeof(dec_sym_t) == sizeof(uint32_t));

template <uint32_t TotalSymbolCountBits>
struct hist_dec2_t
{
  dec_sym_t symbols[256];
  uint8_t cumulInv[1 << TotalSymbolCountBits];
};

template <uint32_t TotalSymbolCountBits>
struct hist_dec3_t
{
  dec_sym_t symbolFomCumul[1 << TotalSymbolCountBits];
  uint8_t cumulInv[1 << TotalSymbolCountBits];
};

template <uint32_t TotalSymbolCountBits> // only for `TotalSymbolCountBits` <= 12
struct hist_dec_pack_t
{
  uint32_t symbol[1 << TotalSymbolCountBits];
};

//////////////////////////////////////////////////////////////////////////

// `totalSymbolCountBits` should be <= 15
void make_hist(hist_t *pHist, const uint8_t *pData, const size_t size, const size_t totalSymbolCountBits);

void make_enc_hist(hist_enc_t *pHistEnc, const hist_t *pHist);

template <uint32_t TotalSymbolCountBits>
void make_dec_hist(hist_dec_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist);

template <uint32_t TotalSymbolCountBits>
void make_dec2_hist(hist_dec2_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist);

template <uint32_t TotalSymbolCountBits>
void make_dec3_hist(hist_dec3_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist);

template <uint32_t TotalSymbolCountBits>
void make_dec_pack_hist(hist_dec_pack_t<TotalSymbolCountBits> *pHistDec, const hist_t *pHist);

bool inplace_complete_hist(hist_t *pHist, const size_t totalSymbolCountBits);

template <uint32_t TotalSymbolCountBits>
bool inplace_make_hist_dec(hist_dec_t<TotalSymbolCountBits> *pHist);

template <uint32_t TotalSymbolCountBits>
bool inplace_make_hist_dec2(hist_dec2_t<TotalSymbolCountBits> *pHist);

#endif // hist_h__
