# -----------------------------------------
# Configuración del compilador y flags
# -----------------------------------------
CXX = g++
CXXFLAGS = -Iinclude -Wall -Wextra -std=c++17

# -----------------------------------------
# Librerías
# -----------------------------------------
LDFLAGS = -Lsonidos/libs -lglfw3 -lopengl32 -lgdi32

# -----------------------------------------
# Archivos fuente
# -----------------------------------------
SRC_CPP = $(wildcard src/*.cpp)
SRC_C   = $(wildcard src/*.c)
SRC     = $(SRC_CPP) $(SRC_C)

# -----------------------------------------
# Ejecutable
# -----------------------------------------
TARGET = app.exe

# -----------------------------------------
# Regla principal
# -----------------------------------------
all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET)

# -----------------------------------------
# Limpiar archivos generados
# -----------------------------------------
clean:
	del /Q $(TARGET)

# -----------------------------------------
# Ejecutar el programa
# -----------------------------------------
run: all
	./$(TARGET)
