ProjectName = "hsrans"
project(ProjectName)

  --Settings
  kind "ConsoleApp"
  language "C++"
  staticruntime "On"

  filter { "system:windows" }
    ignoredefaultlibraries { "msvcrt" }
  filter { "system:linux" }
    buildoptions { "-mxsave" }
    linkoptions { "-pthread" }
    cppdialect "C++20"
  filter { }

  filter { "system:windows", "configurations:not *Clang" }
    buildoptions { '/std:c++20' }
    buildoptions { '/Gm-' }
    buildoptions { '/MP' }

  filter { "system:windows", "configurations:*Clang" }
    toolset("clang")
    cppdialect "C++17"
    defines { "__llvm__" }
  
  filter { "configurations:Release" }
    flags { "LinkTimeOptimization" }
  
  filter { }
  
  defines { "_CRT_SECURE_NO_WARNINGS", "SSE2" }
  
  objdir "intermediate/obj"

  files { "src/**.cpp", "src/**.c", "src/**.cc", "src/**.h", "src/**.hh", "src/**.hpp", "src/**.inl", "src/**rc" }
  files { "project.lua" }

  filter { "configurations:Debug", "system:Windows" }
    ignoredefaultlibraries { "libcmt" }
  filter { }

  filter {}
  
  targetname(ProjectName)
  targetdir "builds/bin"
  debugdir "builds/bin"
  
filter {}

warnings "Extra"
flags { "FatalWarnings" }

filter {"configurations:Release"}
  targetname "%{prj.name}"
filter {"configurations:Debug"}
  targetname "%{prj.name}D"

filter {}
flags { "NoMinimalRebuild", "NoPCH" }
rtti "Off"
floatingpoint "Fast"
exceptionhandling "Off"

filter { "configurations:Debug*" }
	defines { "_DEBUG" }
	optimize "Off"
	symbols "On"

filter { "configurations:Release" }
	defines { "NDEBUG" }
	optimize "Speed"
	flags { "NoBufferSecurityCheck", "NoIncrementalLink" }
	omitframepointer "On"
  symbols "On"

filter { "system:linux", "configurations:ReleaseClang" }
  buildoptions { "-Ofast" }

filter { "system:windows", "configurations:Release" }
	flags { "NoIncrementalLink" }

editandcontinue "Off"
