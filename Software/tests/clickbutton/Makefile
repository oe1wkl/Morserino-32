# Host-side tests for ClickButton (ClickButton.cpp/.h).
# No device, no PlatformIO — just a C++11 compiler.
#
#   make        build the test binary
#   make run    build and run (exits non-zero on failure -> CI-friendly)
#   make clean

CXX      ?= c++
# ARDUINO >= 100 makes ClickButton.h include <Arduino.h> (our stub) rather
# than the pre-1.0 <WProgram.h>.
CXXFLAGS ?= -std=c++11 -Wall -Wextra -O0 -g -DARDUINO=200
BIN       = run_tests
FW        = ../../src/Version\ 6\ and\ newer
SRC       = test_clickbutton.cpp $(FW)/ClickButton.cpp

$(BIN): test_clickbutton.cpp Arduino.h arduino_pin_shim.h $(FW)/ClickButton.cpp $(FW)/ClickButton.h
	$(CXX) $(CXXFLAGS) -I. $(SRC) -o $(BIN)

.PHONY: run clean
run: $(BIN)
	./$(BIN)

clean:
	rm -f $(BIN)
