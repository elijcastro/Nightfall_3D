@echo off
echo Compilando Nightfall_3D...

g++ src/*.cpp src/glad.c -Iinclude -LC:/msys64/ucrt64/lib -lglfw3 -lopengl32 -lgdi32 -o app.exe

echo.
echo Compilación finalizada.
pause
