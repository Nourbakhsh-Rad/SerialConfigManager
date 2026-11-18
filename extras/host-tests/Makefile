# Host-side unit tests (no hardware needed): builds the library against tiny Arduino stubs.
CXX ?= g++
SRC := ../../src
CXXFLAGS := -std=c++17 -Wall -Wextra -Wno-unused-parameter -Istub -I$(SRC)

all: test32 test8266

test32:
	$(CXX) $(CXXFLAGS) -DESP32 test.cpp $(SRC)/SerialConfigManager.cpp -o $@ && ./$@

test8266:
	$(CXX) $(CXXFLAGS) -DESP8266 -c $(SRC)/SerialConfigManager.cpp -o /dev/null

clean:
	rm -f test32
