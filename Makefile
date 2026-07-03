CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

FTXUI_DIR := /home/js/.cache/yay/ftxui/FTXUI-7.0.0
FTXUI_INC := -I$(FTXUI_DIR)/include
FTXUI_LIB := -L$(FTXUI_DIR)/build -lftxui-component -lftxui-dom -lftxui-screen

.PHONY: all clean

all: lob lob-tui

lob: main.cpp orders.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

lob-tui: tui.cpp orders.cpp
	$(CXX) $(CXXFLAGS) $(FTXUI_INC) -o $@ $^ $(FTXUI_LIB) -lpthread

clean:
	rm -f lob lob-tui
