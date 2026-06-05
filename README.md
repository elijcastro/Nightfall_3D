Nightfall_3D — Proyecto en C++/OpenGL usando Visual Studio + vcpkg.

Requisitos
Instalar Visual Studio 2022/2025/2026

Workload: Desktop development with C++

Instalar vcpkg

Instalar dependencias (triplet x64-windows):
vcpkg install glfw3 glm assimp stb --triplet x64-windows

Compilación
Clonar el repositorio
git clone <repo>

Entrar al proyecto
cd Nightfall_3D

Crear carpeta de build
mkdir build
cd build

Generar proyecto con CMake: 
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=../vcpkg/scripts/buildsystems/vcpkg.cmake
"Para lo anteior deben tener instalado Visual studio 2022"
Compilar
cmake --build . --config Debug

El ejecutable queda en:
build/Debug/Nightfall_3D.exe

Ejecución
cd build/Debug
Nightfall_3D.exe

Notas
Los assets y shaders ya están incluidos en el repo.

CMake los copia automáticamente al directorio del ejecutable.

Si aparece un error de “archivo no encontrado”, verificar que existan las carpetas:
build/Debug/src
build/Debug/assets
