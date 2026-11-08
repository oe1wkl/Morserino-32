# The firmware's getRandomCall() on the host, writing the fixture the M32 Trainer's Node test
# compares its JS port against (Software/tests/trainer/callsign.test.js). No device, no PlatformIO.
#
#   make run     extract the code from m32_v6.ino, build, write ../trainer/fixtures/random_calls.json
#   make clean

CXX      ?= c++
CXXFLAGS ?= -std=c++14 -Wall -Wno-unused-function -O0
FW        = ../../src/Version\ 6\ and\ newer

gen_fixture: gen_fixture.cpp extracted.inc $(FW)/callsign_prefixes.h
	$(CXX) $(CXXFLAGS) gen_fixture.cpp -o gen_fixture

extracted.inc: extract.py $(FW)/m32_v6.ino
	python3 extract.py

.PHONY: run clean
run: gen_fixture
	./gen_fixture

clean:
	rm -rf gen_fixture gen_fixture.dSYM extracted.inc
