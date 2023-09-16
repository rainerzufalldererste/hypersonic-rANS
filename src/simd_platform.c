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

#ifndef _MSC_VER
__attribute__((target("xsave")))
#endif
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

const char *_GetCPUArchitectureName()
{
  _DetectCPUFeatures();

  if (_CpuVendor == cpu_vendor_AMD)
  {
    switch (_CpuFamily)
    {
    default: return "AMD (Unknown Architecture)";
    case cpu_family_amd_bobcat: return "AMD Bobcat";
    case cpu_family_amd_bulldozer_piledriver_steamroller_excavator: return "AMD Bulldozer/Piledriver/Steamroller/Excavator";
    case cpu_family_amd_jaguar_puma: return "AMD Jaguar/Puma";
    case cpu_family_amd_zen_zenplus_zen2: return "AMD Zen/Zen+/zen2";
    case cpu_family_amd_zen3_zen4: return "AMD Zen3/Zen4";
    case cpu_family_amd_zen5: return "AMD Zen5";
    }
  }
  else if (_CpuVendor == cpu_vendor_Intel)
  {
    switch (_CpuModel)
    {
    default: return "Intel (Unknown Architecture)";
    case cpu_model_intel_raptorlake_s: return "Intel Raptor Lake S";
    case cpu_model_intel_raptorlake_p: return "Intel Raptor Lake P";
    case cpu_model_intel_alderlake_s: return "Intel Alder Lake S";
    case cpu_model_intel_alderlake_p: return "Intel Alder Lake P";
    case cpu_model_intel_rocketlake_s: return "Intel Rocket Lake";
    case cpu_model_intel_tigerlake_h: return "Intel Tiger Lake H";
    case cpu_model_intel_tigerlake_u: return "Intel Tiger Lake U";
    case cpu_model_intel_icelake_client_u_y: return "Intel Ice Lake (Client)";
    case cpu_model_intel_cometlake_s_h: return "Intel Comet Lake S,H";
    case cpu_model_intel_cometlake_u_amberlake_y_whiskeylake_u_coffeelake_u_kabylake_y_u: return "Intel Comet Lake U / Amber Lake / whiskey Lake / Coffee Lake U / Kaby Lake Y,U";
    case cpu_model_intel_cannonlake_u: return "Intel Cannon Lake";
    case cpu_model_intel_coffeelake_s_h_e_kabylake_dt_h_s_x: return "Intel Coffee Lake S,H,E / Kaby Lake DT,H,S,X";
    case cpu_model_intel_skylake_client_dt_h_s: return "Intel Skylake (Client) DT,H,S";
    case cpu_model_intel_skylake_client_y_u: return "Intel Skylake (Client) Y,U";
    case cpu_model_intel_broadwell_client_c_w_h: return "Intel Broadwell (Client) C,W,H";
    case cpu_model_intel_broadwell_client_u_y_s: return "Intel Broadwell (Client) U,Y,S";
    case cpu_model_intel_haswell_client_gt3e: return "Intel Haswell (Client) GT3E";
    case cpu_model_intel_haswell_client_ult: return "Intel Haswell (Client) ULT";
    case cpu_model_intel_haswell_client_s: return "Intel Haswell (Client) S";
    case cpu_model_intel_ivybridge_client_m_h_gladden: return "Intel Ivy Bridge (Client)";
    case cpu_model_intel_sandybridge_client_m_h_celeron: return "Intel Sandy Bridge (Client)";
    case cpu_model_intel_sapphirerapids: return "Intel Sapphire Rapids";
    case cpu_model_intel_icelake_server_de: return "Intel Ice Lake (Server) DE";
    case cpu_model_intel_icelake_server_sp: return "Intel Ice Lake (Server) SP";
    case cpu_model_intel_cooperlake_cascadelake_sp_x_w_skylake_server_sp_x_de_w: return "Intel Cooper Lake / Cascade Lake / Skylake (Server)";
    case cpu_model_intel_broadwell_server_e_ep_ex: return "Intel Broadwell (Server) E,EP,EX";
    case cpu_model_intel_broadwell_server_de_hewittlake: return "Intel Broadwell (Server) DE,Hewitt Lake";
    case cpu_model_intel_haswell_server_e_ep_ex: return "Intel Haswell (Server)";
    case cpu_model_intel_ivybridge_server_e_en_ep_ex: return "Intel Ivy Bridge (Server)";
    case cpu_model_intel_sandybridge_server_e_en_ep: return "Intel Sandy Bridge (Server)";
    case cpu_model_intel_tremont_jasperlake: return "Intel Tremont Jasper Lake";
    case cpu_model_intel_tremont_elkhartlake: return "Intel Tremont Elkhart Lake";
    case cpu_model_intel_tremont_lakefield: return "Intel Tremont Lakefield";
    case cpu_model_intel_goldmontplus_geminilake: return "Intel Goldmont Plus";
    case cpu_model_intel_goldmont_denverton: return "Intel Goldmont Denverton";
    case cpu_model_intel_goldmont_apollolake: return "Intel Goldmont Apollo Lake";
    case cpu_model_intel_airmont_cherrytrail_braswell: return "Intel Airmont Cherrytrail,Braswell";
    case cpu_model_intel_silvermont_sofia: return "Intel Silvermont SoFIA";
    case cpu_model_intel_silvermont_anniedale: return "Intel Silvermont Anniedale";
    case cpu_model_intel_silvermont_avoton_rangeley: return "Intel Silvermont Avoton, Rangeley";
    case cpu_model_intel_silvermont_tangier: return "Intel Silvermont Tangier";
    case cpu_model_intel_silvermont_baytrail: return "Intel Silvermont Baytrail";
    case cpu_model_intel_knightsmill: return "Intel Knights Mill";
    case cpu_model_intel_knightslanding: return "Intel Knights Landing";
    }
  }
  else
  {
    return "Unknown";
  }
}
