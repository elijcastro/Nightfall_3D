workspace "ProyectoOpenGL"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "App"

project "App"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    targetdir ("bin/%{cfg.buildcfg}")
    objdir ("bin-int/%{cfg.buildcfg}")

files {
    "src/main.cpp",
    "src/glad.c",
    "src/cube.h",
    "src/shader.vert",
    "src/*.c",
    "src/shader.frag",
}

    includedirs {
        "include",
        "include/glad",
        "include/GLFW",
        "IluminacionBasica/src"
    }

    libdirs {
        "libs"
    }

    links {
        "glfw3",
        "opengl32"
    }

    filter "configurations:Debug"
        symbols "On"

    filter "configurations:Release"
        optimize "On"

        