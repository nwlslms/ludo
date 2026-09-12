CC      := gcc
CFLAGS  := -Wall -Wextra -O2
SRC     := ludo.c
BUILD   := build
TARGET  := $(BUILD)/ludo

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)
GUI_SRC    := ludo_gui.c
GUI_TARGET := $(BUILD)/ludo-gui

.PHONY: all run run-gui ludo-gui clean

all: $(TARGET) $(GUI_TARGET)

ludo-gui: $(GUI_TARGET)

$(TARGET): $(SRC) | $(BUILD)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

$(GUI_TARGET): $(GUI_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $(GUI_TARGET) $(GUI_SRC) $(SDL_LIBS)

$(BUILD):
	mkdir -p $(BUILD)

run: all
	./$(TARGET)

run-gui: all
	./$(GUI_TARGET)

clean:
	rm -rf $(BUILD)