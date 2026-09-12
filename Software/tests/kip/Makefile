# Host build of the M32KIP protocol core and its tests. No device, no Arduino toolchain.
#
#   make            build and run everything
#   make vectors    print the wire test vectors as hex (consumed by check_vectors.py)
#
# The firmware source directory has spaces in its name, so prerequisites use the escaped form and
# recipes the quoted one. Keep both in step if the path ever changes.

SRC     = ../../src/Version 6 and newer
SRC_ESC = ../../src/Version\ 6\ and\ newer
CXX ?= c++
CXXFLAGS = -std=c++11 -O1 -Wall -Wextra -Wno-unused-parameter -g -fsanitize=address,undefined -I"$(SRC)"

BIN = test_core

all: run

$(BIN): test_core.cpp $(SRC_ESC)/M32Kip.cpp $(SRC_ESC)/M32Kip.h
	$(CXX) $(CXXFLAGS) -o $@ test_core.cpp "$(SRC)/M32Kip.cpp"

run: $(BIN)
	./$(BIN)

vectors: $(BIN)
	./$(BIN) --vectors

clean:
	rm -rf $(BIN) $(BIN).dSYM

.PHONY: all run vectors clean
