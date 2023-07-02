#include "simd_platform.h"

#include <string.h>

//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define cpuid __cpuid
#else
#include <cpuid.h>

static void cpuid(int info[4], int infoType)
{
  __cpuid_count(infoType, 0, info[0], info[1], info[2], info[3]);
}

// This appears to be defined in newer versions of clang & gcc
//uint64_t _xgetbv(unsigned int index)
//{
//  uint32_t eax, edx;
//  __asm__ __volatile__("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
//  return ((uint64_t)edx << 32) | eax;
//}

#ifndef _XCR_XFEATURE_ENABLED_MASK
#define _XCR_XFEATURE_ENABLED_MASK  0
#endif
#endif

bool _CpuFeaturesDetected = false;
bool sseSupported = false;
bool sse2Supported = false;
bool sse3Supported = false;
bool ssse3Supported = false;
bool sse41Supported = false;
bool sse42Supported = false;
bool avxSupported = false;
bool avx2Supported = false;
bool fma3Supported = false;
bool avx512FSupported = false;
bool avx512PFSupported = false;
bool avx512ERSupported = false;
bool avx512CDSupported = false;
bool avx512BWSupported = false;
bool avx512DQSupported = false;
bool avx512VLSupported = false;
bool avx512IFMASupported = false;
bool avx512VBMISupported = false;
bool avx512VNNISupported = false;
bool avx512VBMI2Supported = false;
bool avx512POPCNTDQSupported = false;
bool avx512BITALGSupported = false;
bool avx5124VNNIWSupported = false;
bool avx5124FMAPSSupported = false;
bool aesNiSupported = false;

vendor_t _CpuVendor = cpu_vendor_Unknown;
char _CpuName[0x80] = { 0 };
uint8_t _CpuFamily = 0;
uint8_t _CpuModel = 0;
uint8_t _CpuStepping = 0;
uint8_t _CpuExtFamily = 0;
uint8_t _CpuExtModel = 0;

void _DetectCPUFeatures()
{
  if (_CpuFeaturesDetected)
    return;

  int32_t info[4];
  cpuid(info, 0);
  const uint32_t idCount = info[0];

  const uint8_t amdVendorString[] = { 0x41, 0x75, 0x74, 0x68, 0x63, 0x41, 0x4D, 0x44, 0x65, 0x6E, 0x74, 0x69 }; // AuthenticAMD
  const uint8_t intelVendorString[] = { 0x47, 0x65, 0x6E, 0x75, 0x6E, 0x74, 0x65, 0x6C, 0x69, 0x6E, 0x65, 0x49 }; // GenuineIntel

  if (memcmp(info + 1, amdVendorString, sizeof(amdVendorString)) == 0)
    _CpuVendor = cpu_vendor_AMD;
  else if (memcmp(info + 1, intelVendorString, sizeof(intelVendorString)) == 0)
    _CpuVendor = cpu_vendor_Intel;

  if (idCount >= 0x1)
  {
    int32_t cpuInfo[4];
    cpuid(cpuInfo, 1);

    _CpuStepping = cpuInfo[0] & 0b1111;
    _CpuModel = (cpuInfo[0] >> 4) & 0b1111;
    _CpuFamily = (cpuInfo[0] >> 8) & 0b1111;
    _CpuExtModel = (cpuInfo[0] >> 16) & 0b1111;
    _CpuExtFamily = (cpuInfo[0] >> 20) & 0b1111111;

    if (_CpuFamily == 0x6 || _CpuFamily == 0xF)
      _CpuModel |= _CpuExtModel << 4;

    if (_CpuFamily == 0xF)
      _CpuFamily += _CpuExtFamily;

    const bool osUsesXSAVE_XRSTORE = (cpuInfo[2] & (1 << 27)) != 0;
    const bool cpuAVXSuport = (cpuInfo[2] & (1 << 28)) != 0;

    if (osUsesXSAVE_XRSTORE && cpuAVXSuport)
    {
      uint64_t xcrFeatureMask = _xgetbv(_XCR_XFEATURE_ENABLED_MASK);
      avxSupported = (xcrFeatureMask & 0x6) != 0;
    }

    sseSupported = (cpuInfo[3] & (1 << 25)) != 0;
    sse2Supported = (cpuInfo[3] & (1 << 26)) != 0;
    sse3Supported = (cpuInfo[2] & (1 << 0)) != 0;
    ssse3Supported = (cpuInfo[2] & (1 << 9)) != 0;
    sse41Supported = (cpuInfo[2] & (1 << 19)) != 0;
    sse42Supported = (cpuInfo[2] & (1 << 20)) != 0;
    fma3Supported = (cpuInfo[2] & (1 << 12)) != 0;
    aesNiSupported = (cpuInfo[2] & (1 << 25)) != 0;
  }

  if (idCount >= 0x7)
  {
    int32_t cpuInfo[4];
    cpuid(cpuInfo, 7);

    avx2Supported = (cpuInfo[1] & (1 << 5)) != 0;
    avx512FSupported = (cpuInfo[1] & 1 << 16) != 0;
    avx512PFSupported = (cpuInfo[1] & 1 << 26) != 0;
    avx512ERSupported = (cpuInfo[1] & 1 << 27) != 0;
    avx512CDSupported = (cpuInfo[1] & 1 << 28) != 0;
    avx512BWSupported = (cpuInfo[1] & 1 << 30) != 0;
    avx512DQSupported = (cpuInfo[1] & 1 << 17) != 0;
    avx512VLSupported = (cpuInfo[1] & 1 << 31) != 0;
    avx512IFMASupported = (cpuInfo[1] & 1 << 21) != 0;
    avx512VBMISupported = (cpuInfo[2] & 1 << 1) != 0;
    avx512VNNISupported = (cpuInfo[2] & 1 << 11) != 0;
    avx512VBMI2Supported = (cpuInfo[2] & 1 << 6) != 0;
    avx512POPCNTDQSupported = (cpuInfo[2] & 1 << 14) != 0;
    avx512BITALGSupported = (cpuInfo[2] & 1 << 12) != 0;
    avx5124VNNIWSupported = (cpuInfo[3] & 1 << 2) != 0;
    avx5124FMAPSSupported = (cpuInfo[3] & 1 << 3) != 0;
  }

  cpuid(info, 0x80000000);
  const uint32_t extLength = info[0];

  // Parse CPU Name.
  for (uint32_t i = 0x80000002; i <= extLength && i <= 0x80000005; i++)
  {
    cpuid(info, i);

    const int32_t index = (i & 0x7) - 2;

    if (index >= 0 && index <= 4)
      memcpy(_CpuName + sizeof(info) * index, info, sizeof(info));
  }

  // Trim Spaces at the end.
  {
    size_t end = 1;

    for (; end < sizeof(_CpuName); end++)
      if (_CpuName[end] == '\0')
        break;

    end--;

    for (; end > 1; end--)
    {
      if (_CpuName[end] == ' ')
        _CpuName[end] = '\0';
      else
        break;
    }
  }

  _CpuFeaturesDetected = true;
}
