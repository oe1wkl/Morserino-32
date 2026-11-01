# Host-side test of the firmware's cw/play text path: what the M32 sounds for a given
# `PUT cw/play/<text>` (prosigns, case, the trainer's groups). No device, no PlatformIO.
#
#   make run    extract the code from m32_v6.ino, build, run (non-zero exit on failure)
#   make clean

CXX      ?= c++
CXXFLAGS ?= -std=c++14 -Wall -Wno-unused-function -Wno-dangling-else -O0 -g
FW        = ../../src/Version\ 6\ and\ newer

run_tests: test_cw_play_text.cpp string_shim.h extracted.inc
	$(CXX) $(CXXFLAGS) test_cw_play_text.cpp -o run_tests

extracted.inc: extract.py $(FW)/m32_v6.ino $(FW)/MorsePreferences.h
	python3 extract.py

.PHONY: run clean
run: run_tests
	./run_tests

clean:
	rm -f run_tests extracted.inc
