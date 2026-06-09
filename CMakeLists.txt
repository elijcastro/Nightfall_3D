cmake_minimum_required(VERSION 3.10)

if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

project(Nightfall_3D LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB SRC_FILES
    "src/*.cpp"
    "src/*.c"
)

add_executable(Nightfall_3D ${SRC_FILES})

target_include_directories(Nightfall_3D PRIVATE include)

find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

target_link_libraries(Nightfall_3D PRIVATE glfw OpenGL::GL glm::glm assimp::assimp)

foreach(RUNTIME_DIR assets sonidos textures)
    if(EXISTS "${CMAKE_SOURCE_DIR}/${RUNTIME_DIR}")
        add_custom_command(TARGET Nightfall_3D POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/${RUNTIME_DIR}"
                "$<TARGET_FILE_DIR:Nightfall_3D>/${RUNTIME_DIR}"
        )
    endif()
endforeach()

file(GLOB SHADER_FILES
    "src/*.frag"
    "src/*.vert"
)

if(SHADER_FILES)
    add_custom_command(TARGET Nightfall_3D POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:Nightfall_3D>/src"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            ${SHADER_FILES}
            "$<TARGET_FILE_DIR:Nightfall_3D>/src"
    )
endif()