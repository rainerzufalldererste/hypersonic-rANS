#ifndef simd_platform_h__
#define simd_platform_h__

#include <stdbool.h>
#include <stdint.h>

#if defined(_MSC_VER) && !defined(__llvm__)
#include <intrin.h>
#define __builtin_popcount __popcnt
#else
#include <x86intrin.h>
#endif

//////////////////////////////////////////////////////////////////////////

#ifdef _MSC_VER
#define _ALIGN(bytes) __declspec(align(bytes))
#else
#define _ALIGN(bytes) __attribute__((aligned(bytes)))
#endif

#if defined(_MSC_VER) || defined(__clang__)
#define _VECTORCALL __vectorcall
#else
#define _VECTORCALL
#endif

#ifdef _MSC_VER
#define _INLINE __forceinline
#else
#define _INLINE __attribute__((always_inline))
#endif

//////////////////////////////////////////////////////////////////////////

// Intel Downfall Mitigation: Simulated Gather to work around performance pitfalls...

#ifdef __cplusplus
#ifndef _MSC_VER
__attribute__((target("avx")))
#endif
inline __m256i _INLINE _mm256_i32gather_epi32_sim(const int32_t *pB, const __m256i idx, const size_t)
{
#if defined(__GNUC__) && !defined(__llvm__)
  volatile
#endif 
    _ALIGN(32) int32_t i[8];

  _mm256_store_si256((__m256i *)i, idx);

  return _mm256_set_epi32(pB[i[7]], pB[i[6]], pB[i[5]], pB[i[4]], pB[i[3]], pB[i[2]], pB[i[1]], pB[i[0]]);
}

#ifndef _MSC_VER
__attribute__((target("avx512f")))
#endif
inline __m512i _INLINE _mm512_i32gather_epi32_sim(const __m512i idx, const void *pV, const size_t)
{
#if defined(__GNUC__) && !defined(__llvm__)
  volatile
#endif 
    _ALIGN(64) int32_t i[16];

  _mm512_store_si512((__m512i *)i, idx);

  const int32_t *pB = reinterpret_cast<const int32_t *>(pV);

  return _mm512_set_epi32(pB[i[15]], pB[i[14]], pB[i[13]], pB[i[12]], pB[i[11]], pB[i[10]], pB[i[9]], pB[i[8]], pB[i[7]], pB[i[6]], pB[i[5]], pB[i[4]], pB[i[3]], pB[i[2]], pB[i[1]], pB[i[0]]);
}
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
    cpu_model_intel_raptorlake_s = 0xB7,
    cpu_model_intel_raptorlake_p = 0xBA,
    cpu_model_intel_alderlake_s = 0x97,
    cpu_model_intel_alderlake_p = 0x9A,
    cpu_model_intel_rocketlake_s = 0xA7,
    cpu_model_intel_tigerlake_h = 0x8D,
    cpu_model_intel_tigerlake_u = 0x8C,
    cpu_model_intel_icelake_client_u_y = 0x7E,
    cpu_model_intel_cometlake_s_h = 0xA5,
    cpu_model_intel_cometlake_u_amberlake_y_whiskeylake_u_coffeelake_u_kabylake_y_u = 0x8E,
    cpu_model_intel_cannonlake_u = 0x66,
    cpu_model_intel_coffeelake_s_h_e_kabylake_dt_h_s_x = 0x9E,
    cpu_model_intel_skylake_client_dt_h_s = 0x5E,
    cpu_model_intel_skylake_client_y_u = 0x4E,
    cpu_model_intel_broadwell_client_c_w_h = 0x47,
    cpu_model_intel_broadwell_client_u_y_s = 0x3D,
    cpu_model_intel_haswell_client_gt3e = 0x46,
    cpu_model_intel_haswell_client_ult = 0x45,
    cpu_model_intel_haswell_client_s = 0x3C,
    cpu_model_intel_ivybridge_client_m_h_gladden = 0x3A,
    cpu_model_intel_sandybridge_client_m_h_celeron = 0x2A,
    cpu_model_intel_sapphirerapids = 0x8F,
    cpu_model_intel_icelake_server_de = 0x6C,
    cpu_model_intel_icelake_server_sp = 0x6A,
    cpu_model_intel_cooperlake_cascadelake_sp_x_w_skylake_server_sp_x_de_w = 0x55,
    cpu_model_intel_broadwell_server_e_ep_ex = 0x4F,
    cpu_model_intel_broadwell_server_de_hewittlake = 0x56,
    cpu_model_intel_haswell_server_e_ep_ex = 0x3F,
    cpu_model_intel_ivybridge_server_e_en_ep_ex = 0x3E,
    cpu_model_intel_sandybridge_server_e_en_ep = 0x2D,
    cpu_model_intel_tremont_jasperlake = 0x9C,
    cpu_model_intel_tremont_elkhartlake = 0x96,
    cpu_model_intel_tremont_lakefield = 0x8A,
    cpu_model_intel_goldmontplus_geminilake = 0x7A,
    cpu_model_intel_goldmont_denverton = 0x5F,
    cpu_model_intel_goldmont_apollolake = 0x5C,
    cpu_model_intel_airmont_cherrytrail_braswell = 0x4C,
    cpu_model_intel_silvermont_sofia = 0x5D,
    cpu_model_intel_silvermont_anniedale = 0x5A,
    cpu_model_intel_silvermont_avoton_rangeley = 0x4D,
    cpu_model_intel_silvermont_tangier = 0x4A,
    cpu_model_intel_silvermont_baytrail = 0x37,
    cpu_model_intel_knightsmill = 0x85,
    cpu_model_intel_knightslanding = 0x57,

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
  const char *_GetCPUArchitectureName();

#ifdef __cplusplus
}
#endif

#endif // simd_platform_h__
