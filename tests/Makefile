CXX      := g++
CXXFLAGS := -std=c++20 -Wall -Wextra -I. -pthread
TARGET   := suitespot_tests
SOURCES  := test_main.cpp $(wildcard test_*.cpp)
OBJECTS  := $(SOURCES:.cpp=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp catch.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)
