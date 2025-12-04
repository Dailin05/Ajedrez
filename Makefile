# Compilador
CXX = g++
# Flags de compilación
CXXFLAGS = -std=c++17 -Wall -I"C:/SFML/include"
# Librerías SFML
LDFLAGS = -L"C:/SFML/lib" -lsfml-graphics -lsfml-window -lsfml-system
# Archivo ejecutable
TARGET = Ajedrez
# Archivos fuente
SRC = Ajedrez.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	del $(TARGET).exe
