@echo off
echo ================================
echo Compilando Nightfall_3D en Release...
echo ================================

cd build

REM Ajusta la ruta de vcpkg a la correcta en tu PC
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/Laboratorios_OPENGL/Nightfall_3D/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build . --config Release

echo ================================
echo Ejecutando Nightfall_3D.exe (Release) desde la raíz
echo ================================

cd ..
build\Release\Nightfall_3D.exe

pause

