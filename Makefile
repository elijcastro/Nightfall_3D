CXX = g++
CXXFLAGS = -Iinclude -Wall -Wextra -std=c++17

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S), Linux)
	LDFLAGS = -Lsonidos/libs -lglfw -lGL -lX11 -lXrandr -lXi -ldl -lpthread
else
	LDFLAGS = -Lsonidos/libs -lglfw3 -lopengl32 -lgdi32
endif

SRC_CPP = $(wildcard src/*.cpp)
SRC_C   = $(wildcard src/*.c)
SRC     = $(SRC_CPP) $(SRC_C)

TARGET = app.exe

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(SRC) $(CXXFLAGS) $(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

run: all
	./$(TARGET)
