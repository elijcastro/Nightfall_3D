@echo off
echo ============================================
echo   Nightfall_3D - Rebuild completo
echo ============================================

cd /d "%~dp0"

echo.
echo [1] Eliminando carpeta build...
rmdir /s /q build 2>nul
mkdir build

echo.
echo [2] Eliminando vcpkg corrupto...
rmdir /s /q vcpkg 2>nul

echo.
echo [3] Clonando vcpkg limpio...
git clone https://github.com/microsoft/vcpkg.git

echo.
echo [4] Ejecutando bootstrap...
cd vcpkg
bootstrap-vcpkg.bat
cd ..

echo.
echo [5] Generando proyecto con CMake correcto...
"C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" ^
  -S . ^
  -B build ^
  -G "Visual Studio 18 2026" ^
  -A x64 ^
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake

echo.
echo [6] Compilando...
cmake --build build --config Debug

echo.
echo [7] Ejecutando Nightfall_3D...
cd build/Debug
Nightfall_3D.exe

echo.
echo ============================================
echo   PROCESO COMPLETO
echo ============================================
pause
