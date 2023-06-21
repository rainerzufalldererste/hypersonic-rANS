#ifndef rans32x1_h__
#define rans32x1_h__

#include "hist.h"

size_t rANS32x1_capacity(const size_t inputSize);

size_t rANS32x1_encode(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_enc_t *pHist);
size_t rANS32x1_decode(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec2_t *pHist);

size_t rANS32x1_encode_basic(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x1_decode_basic(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outLength, const hist_dec_t *pHist);

#endif // rans32x1_h__
