CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

FTXUI_DIR ?= $(HOME)/.local
FTXUI     := -I$(FTXUI_DIR)/include -L$(FTXUI_DIR)/lib -lftxui-component -lftxui-dom -lftxui-screen

.PHONY: all clean

all: lob

lob: src/main.cpp src/book.cpp src/orders.cpp src/common.h
	$(CXX) $(CXXFLAGS) $(FTXUI) -o $@ $^ -lpthread

clean:
	rm -f lob
