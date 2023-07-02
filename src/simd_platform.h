#ifndef simd_platform_h__
#define simd_platform_h__

#include <stdbool.h>
#include <stdint.h>

#ifdef _MSC_VER
#include <intrin.h>
#define __builtin_popcount __popcnt
#else
#include <x86intrin.h>
#endif

//////////////////////////////////////////////////////////////////////////

#ifdef __cplusplus
extern "C"
{
#endif

  extern bool _CpuFeaturesDetected;
  extern bool sseSupported;
  extern bool sse2Supported;
  extern bool sse3Supported;
  extern bool ssse3Supported;
  extern bool sse41Supported;
  extern bool sse42Supported;
  extern bool avxSupported;
  extern bool avx2Supported;
  extern bool fma3Supported;
  extern bool avx512FSupported;
  extern bool avx512PFSupported;
  extern bool avx512ERSupported;
  extern bool avx512CDSupported;
  extern bool avx512BWSupported;
  extern bool avx512DQSupported;
  extern bool avx512VLSupported;
  extern bool avx512IFMASupported;
  extern bool avx512VBMISupported;
  extern bool avx512VNNISupported;
  extern bool avx512VBMI2Supported;
  extern bool avx512POPCNTDQSupported;
  extern bool avx512BITALGSupported;
  extern bool avx5124VNNIWSupported;
  extern bool avx5124FMAPSSupported;
  extern bool aesNiSupported;

  typedef enum
  {
    cpu_vendor_Unknown,
    cpu_vendor_AMD,
    cpu_vendor_Intel,
  } vendor_t;

  extern vendor_t _CpuVendor;
  extern char _CpuName[0x80];
  extern uint8_t _CpuFamily;
  extern uint8_t _CpuModel;
  extern uint8_t _CpuStepping;
  extern uint8_t _CpuExtFamily;
  extern uint8_t _CpuExtModel;

  enum
  {
    // https://en.wikichip.org/wiki/intel/cpuid
    cpu_family_intel_recent = 0x6,
    cpu_ext_model_intel_raptorlake = 0xB,
    cpu_ext_model_intel_rocketlake_cometlake_s_h = 0xA,
    cpu_ext_model_intel_alderlake_coffeelake_s_h_e_kabylake_dt_h_s_x_tremont_jasperlake_elkhartlake = 0x9,
    cpu_ext_model_intel_tigerlake_cometlake_u_amberlake_whiskeylake_u_sapphirerapids_tremont_lakefield_coffeelake_u_kabylake_y_u_knights_mill = 0x8,
    cpu_ext_model_intel_icelake_client_geminilake = 0x7,
    cpu_ext_model_intel_cannonlake_icelake_server = 0x6,
    cpu_ext_model_intel_skylake_client_dt_h_s_cooperlake_cascadelake_skylake_server_broadwell_server_de_hewittlake_goldmont_silvermont_sofia_anniedale_knightslanding = 0x5,
    cpu_ext_model_intel_skylake_client_y_u_broadwell_server_e_ep_ex_broadwell_client_c_w_h_haswell_client_gt3e_ult = 0x4,
    cpu_ext_model_intel_broadwell_client_u_y_s_haswell_client_s_ivybridge_client_haswell_server_ivybridge_server = 0x3,
    cpu_ext_model_intel_sandybridge_client_westmere_client_sandybridge_server_westmere_server_nehalem_server_ex = 0x2,

    // https://en.wikichip.org/wiki/amd/cpuid
    cpu_family_amd_bobcat = 0x14,
    cpu_family_amd_bulldozer_piledriver_steamroller_excavator = 0x15,
    cpu_family_amd_jaguar_puma = 0x16,
    cpu_family_amd_zen_zenplus_zen2 = 0x17,
    cpu_family_amd_zen3_zen4 = 0x19,
    cpu_family_amd_zen5 = 0x1A, // according to leaks.
  };

  void _DetectCPUFeatures();

#ifdef __cplusplus
}
#endif

#endif // simd_platform_h__
