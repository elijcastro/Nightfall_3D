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
    "IluminacionBasica/src/main.cpp",
    "IluminacionBasica/src/glad.c",
    "src/cube.h",
    "IluminacionBasica/src/shader.vert",
    "IluminacionBasica/src/*.c"
    "IluminacionBasica/src/shader.frag"
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

        