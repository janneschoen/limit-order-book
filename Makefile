CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

FTXUI_DIR ?= $(HOME)/.local
FTXUI_INC := -I$(FTXUI_DIR)/include
FTXUI_LIB := -L$(FTXUI_DIR)/lib -lftxui-component -lftxui-dom -lftxui-screen

.PHONY: all clean

all: lob

lob: src/main.cpp src/book.cpp src/orders.cpp src/common.h
	$(CXX) $(CXXFLAGS) $(FTXUI_INC) -o $@ $^ $(FTXUI_LIB) -lpthread

clean:
	rm -f lob
