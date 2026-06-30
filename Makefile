CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -g

.PHONY: all clean

all: lob

lob: main.cpp orders.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f lob
