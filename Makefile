CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2 -g
LIBS = -lm

# Source and build directories
SRC_DIR = src
BUILD_DIR = build

# Target executable name
TARGET = $(BUILD_DIR)/main

# Source files (all .c files in src directory)
SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Object files in build directory, mirroring src directory
OBJECTS = $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SOURCES))

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory if it doesn't exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Link the executable
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile source files to object files in build directory
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean build artifacts
clean:
	rm -rf $(BUILD_DIR)

# Rebuild everything
rebuild: clean all

# Phony targets
.PHONY: all clean rebuild