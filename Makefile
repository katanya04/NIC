# Nombre del binario
TARGET = networking

# Directorios
BIN_DIR = bin
SRC_DIR = src
INC_DIR = include

# Compilador y flags
CC = gcc
CFLAGS = -Wall -Wextra -O2 -pthread -I$(INC_DIR)
LDFLAGS = -pthread

# Archivos fuente y objeto
SOURCES = $(wildcard $(SRC_DIR)/*.c)
OBJECTS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%.o, $(SOURCES))

# Regla por defecto
all: $(BIN_DIR) $(BIN_DIR)/$(TARGET)

# Crear directorio bin si no existe
$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Enlazar el ejecutable
$(BIN_DIR)/$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

# Compilar archivos fuente a objetos
$(BIN_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Limpiar archivos compilados
clean:
	rm -rf $(BIN_DIR)

# Recompilar todo
rebuild: clean all

.PHONY: all clean rebuild