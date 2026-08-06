CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
TARGET := eval

OBJS := tokenize.o eval.o main.o

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

tokenize.o: tokenize.cpp tokenize.hpp
eval.o: eval.cpp eval.hpp tokenize.hpp
main.o: main.cpp eval.hpp tokenize.hpp

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) $(OBJS)