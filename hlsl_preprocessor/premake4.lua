solution "hlsl_preprocessor"
	configurations { "Debug", "Release" }
	language "C++"
	location "build"

	project "hlsl_preprocessor"
		kind "ConsoleApp"
		SRC = "../"
		files {
			SRC .. "mojoshader.*",
			SRC .. "mojoshader_common.c",
			SRC .. "mojoshader_internal.h",
			SRC .. "mojoshader_lexer.c",
			SRC .. "mojoshader_version.h",
			SRC .. "mojoshader_preprocessor.c",
			"main.cpp",
		}
		includedirs { SRC }

		buildoptions { "/TP" }
		linkoptions  { "/nodefaultlib:msvcrt.lib" }
		defines      { "_CRT_SECURE_NO_WARNINGS" }
		links { "Shlwapi" }

		configuration "Debug"
			targetdir "build/bin/debug"
			flags { "Symbols" }

		configuration "Release"
			targetdir "build/bin/release"
			flags { "Optimize" }
