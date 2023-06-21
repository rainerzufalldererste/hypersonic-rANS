ProjectName = "hsrans"
project(ProjectName)

  --Settings
  kind "ConsoleApp"
  language "C++"
  staticruntime "On"
  cppdialect "C++20"

  filter { "system:windows" }
    buildoptions { '/Gm-' }
    buildoptions { '/MP' }

    ignoredefaultlibraries { "msvcrt" }
    exceptionhandling "On" -- nobody wants this, but msvc is to incompetent otherwise because codegen workaround require workarounds for which this is the workaround.
  filter { "system:linux" }
    buildoptions { "-mxsave" }
    exceptionhandling "Off"
  filter { }
  
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
  buildoptions { "-O3" }

filter { "system:windows", "configurations:Release", "action:vs2012" }
	buildoptions { "/d2Zi+" }

filter { "system:windows", "configurations:Release", "action:vs2013" }
	buildoptions { "/Zo" }

filter { "system:windows", "configurations:Release" }
	flags { "NoIncrementalLink" }

editandcontinue "Off"
