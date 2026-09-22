# ------------------------------------------------------------------
# Nstrike — fichier de build (make, make test, make run, make clean).
# Auteur : Martial Zinsou
# ------------------------------------------------------------------
CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Wno-unused-parameter -Isrc
LDFLAGS ?=

SRC := src/common.cpp src/sha256.cpp src/keccak.cpp src/json.cpp src/crypto.cpp \
       src/state.cpp src/tx.cpp src/vm.cpp src/chain.cpp src/rpc.cpp src/main.cpp

OBJ := $(SRC:%.cpp=build/%.o)
LIBOBJ := $(filter-out build/src/main.o,$(OBJ))

BIN := build/nstrike
TESTBIN := build/nstrike_test

.PHONY: all clean test run

all: $(BIN)

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(TESTBIN): build/tests/test_main.o $(LIBOBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

build/tests/test_main.o: tests/test_main.cpp
	@mkdir -p build/tests
	$(CXX) $(CXXFLAGS) -c -o $@ $<

test: $(TESTBIN)
	./$(TESTBIN) --all

run: $(BIN)
	./$(BIN) --help

clean:
	rm -rf build