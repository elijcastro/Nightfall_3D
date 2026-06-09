# Nightfall_3D

Motor 3D en C++ con OpenGL, CMake y vcpkg

Nightfall_3D es un motor gráfico educativo y modular escrito en C++17, diseñado para aprender y experimentar con:

- OpenGL moderno (3.3+)
- Modelos GLTF/OBJ
- Iluminación dinámica
- Shaders personalizados
- Audio (SoLoud)
- Carga de texturas, geometría y escenas

El proyecto está configurado para que cualquier colaborador pueda compilarlo sin configuraciones manuales, usando únicamente:

- Visual Studio 2022/2026
- CMake integrado
- vcpkg local incluido en el repositorio

## Requisitos

- Visual Studio 2022 o 2026 (Community es suficiente)
- Windows 10/11
- Git instalado

No se requiere instalar vcpkg globalmente (el proyecto usa su propio vcpkg local)

## Dependencias (instaladas automáticamente)

El archivo vcpkg.json gestiona todas las dependencias:

- glfw3
- glm
- assimp
- stb

vcpkg se ejecuta automáticamente cuando Visual Studio configura el proyecto.

## Cómo compilar (método recomendado)

Clonar el repositorio:

```
git clone https://github.com/tuusuario/Nightfall_3D.git (github.com in Bing)
```

Abrir el proyecto en Visual Studio:

```
File → Open → Folder → Nightfall_3D/
```

Visual Studio detectará automáticamente:

- CMakeLists.txt
- vcpkg local
- toolchain
- dependencias

Compilar:

```
Build → Build All
```

Ejecutar:

```
Debug → Start Debugging
```

o ejecutar directamente:

```
build/Debug/Nightfall_3D.exe
```

## Estructura del proyecto

```
Nightfall_3D/
│
├─ src/                (Código fuente C++)
├─ include/            (Headers)
├─ assets/             (Modelos, texturas, shaders)
├─ sonidos/            (Archivos de audio)
│
├─ vcpkg/              (vcpkg local incluido en el repo)
├─ vcpkg.json          (Dependencias del proyecto)
├─ CMakeLists.txt      (Configuración principal)
└─ build/              (Generado automáticamente)
```

## Copia automática de recursos

El proyecto copia automáticamente:

- assets/
- sonidos/
- src/ (para shaders)

al directorio donde se genera el ejecutable.

Esto garantiza que los shaders y recursos siempre estén disponibles al ejecutar el motor.

## Contribuir

Crear un branch:

```
git checkout -b feature/nueva-funcionalidad
```

Hacer cambios y commitear:

```
git commit -m "Agrega nueva funcionalidad"
```

Subir el branch:

```
git push origin feature/nueva-funcionalidad
```

Crear un Pull Request en GitHub.

## Reportar errores

Abrir un Issue en GitHub con:

- Descripción del problema
- Pasos para reproducirlo
- Capturas de pantalla o logs
- Versión de Windows y Visual Studio

## Licencia

MIT License (o la que prefieras)

## Autores

*Nombres pendientes*

Desarrollador del motor Nightfall_3D

CMake los copia automáticamente al directorio del ejecutable.

Si aparece un error de “archivo no encontrado”, verificar que existan las carpetas:

```
build/Debug/src
build/Debug/assets
```
