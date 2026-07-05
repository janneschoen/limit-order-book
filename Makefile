CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

FTXUI_DIR ?= $(HOME)/.local
FTXUI_INC := -I$(FTXUI_DIR)/include
FTXUI_LIB := -L$(FTXUI_DIR)/lib -lftxui-component -lftxui-dom -lftxui-screen

SRCS := src/main.cpp src/book.cpp src/orders.cpp

.PHONY: all clean

all: lob

lob: $(SRCS) src/common.h
	$(CXX) $(CXXFLAGS) $(FTXUI_INC) -o $@ $(SRCS) $(FTXUI_LIB) -lpthread

clean:
	rm -f lob
