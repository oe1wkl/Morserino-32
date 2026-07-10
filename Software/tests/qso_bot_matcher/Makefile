# Host-side unit tests for the QSO Bot matcher (MorseQsoBotMatch.h).
# No device, no PlatformIO — just a C++11 compiler.
#
#   make        build the test binary
#   make run    build and run (exits non-zero on failure -> CI-friendly)
#   make clean

CXX      ?= c++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O0 -g
BIN       = run_tests
SRC       = test_qso_bot_matcher.cpp

$(BIN): $(SRC) arduino_string_shim.h ../../src/Version\ 6\ and\ newer/MorseQsoBotMatch.h
	$(CXX) $(CXXFLAGS) $(SRC) -o $(BIN)

.PHONY: run clean
run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
