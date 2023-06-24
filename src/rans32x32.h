#ifndef rans32x32_h__
#define rans32x32_h__

#include "hist.h"

size_t rANS32x32_capacity(const size_t inputSize);

size_t rANS32x32_encode_basic_15(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA2_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB2_15(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

size_t rANS32x32_encode_basic_14(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA2_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB2_14(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

size_t rANS32x32_encode_basic_13(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA2_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB2_13(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

size_t rANS32x32_encode_basic_12(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA2_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB2_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varC_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varC2_12(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

size_t rANS32x32_encode_basic_11(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA2_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB2_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varC_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varC2_11(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

size_t rANS32x32_encode_basic_10(const uint8_t *pInData, const size_t length, uint8_t *pOutData, const size_t outCapacity, const hist_t *pHist);
size_t rANS32x32_decode_basic_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varA2_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varB2_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varC_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);
size_t rANS32x32_decode_avx2_varC2_10(const uint8_t *pInData, const size_t inLength, uint8_t *pOutData, const size_t outCapacity);

#endif // rans32x32_h__
