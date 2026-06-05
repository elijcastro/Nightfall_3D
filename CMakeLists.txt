cmake_minimum_required(VERSION 3.10)
project(Nightfall_3D)

set(CMAKE_CXX_STANDARD 17)

# Integración con vcpkg
if(DEFINED ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Archivos fuente
file(GLOB SRC_FILES
    "src/*.cpp"
    "src/*.c"
)

add_executable(Nightfall_3D ${SRC_FILES})

# Incluir headers
target_include_directories(Nightfall_3D PRIVATE include)

# Buscar dependencias
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(assimp CONFIG REQUIRED)

# Enlazar librerías
target_link_libraries(Nightfall_3D PRIVATE glfw OpenGL::GL glm::glm assimp::assimp)


# Copiar assets al directorio de build
add_custom_command(TARGET Nightfall_3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:Nightfall_3D>/assets"
)

# Copiar sonidos
add_custom_command(TARGET Nightfall_3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/sonidos"
        "$<TARGET_FILE_DIR:Nightfall_3D>/sonidos"
)
