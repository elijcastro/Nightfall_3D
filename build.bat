@echo off
echo Compilando Nightfall_3D...

"C:\msys64\mingw64\bin\g++.exe" src/*.cpp src/glad.c -Iinclude -LC:/msys64/mingw64/lib -lglfw3 -lopengl32 -lgdi32 -o app.exe

echo.
echo Compilación finalizada.
pause

