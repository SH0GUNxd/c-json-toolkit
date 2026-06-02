CC      = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -Isrc

SRC = src/lexer.c src/unescape.c src/parser.c src/json.c \
      src/stringify.c src/json_schema.c \
      src/json_pointer.c src/json_patch.c
OBJ     = $(SRC:.c=.o)

# Detect Windows (Git Bash / MinGW)
ifeq ($(OS),Windows_NT)
    EXE = .exe
else
    EXE =
endif

.PHONY: all check valgrind bench jsonlint fuzz fuzz-standalone clean

# Library
all: libjson.a

libjson.a: $(OBJ)
	ar rcs $@ $^

# Tests
TEST_SRC = $(wildcard tests/*.c)

# Tests
check: $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) $(SRC) $(TEST_SRC) -o test_runner$(EXE) -lm
	./test_runner$(EXE)

valgrind: $(SRC) $(TEST_SRC)
	$(CC) $(CFLAGS) -g -O0 $(SRC) $(TEST_SRC) -o test_runner_dbg$(EXE) -lm
	valgrind --leak-check=full --error-exitcode=1 ./test_runner_dbg$(EXE)

# Tools
jsonlint: $(SRC) tools/jsonlint.c
	$(CC) $(CFLAGS) $(SRC) tools/jsonlint.c -o jsonlint$(EXE) -lm

bench: $(SRC) tools/bench.c
	$(CC) $(CFLAGS) -O2 $(SRC) tools/bench.c -o bench$(EXE) -lm
	./bench$(EXE)

# Fuzz
fuzz: $(SRC) tools/fuzz.c
	clang -std=c99 -fsanitize=fuzzer,address \
	      -Isrc \
	      $(SRC) tools/fuzz.c -o fuzz_libfuzzer$(EXE) -lm
	@echo "Run: ./fuzz_libfuzzer$(EXE) corpus/ -max_len=4096"

fuzz-standalone: $(SRC) tools/fuzz.c
	$(CC) $(CFLAGS) -DFUZZ_STANDALONE $(SRC) tools/fuzz.c \
	      -o fuzz_standalone$(EXE) -lm
	@echo "Run: echo '{\"x\":1}' | ./fuzz_standalone$(EXE)"

fuzz-afl: $(SRC) tools/fuzz.c
	AFL_USE_ASAN=1 afl-clang-fast -std=c99 \
	      -Isrc \
	      $(SRC) tools/fuzz.c -o fuzz_afl$(EXE) -lm
	@echo "Run: afl-fuzz -i corpus/ -o findings/ -- ./fuzz_afl$(EXE)"

# Clean
clean:
	rm -f $(OBJ) libjson.a test_runner$(EXE) test_runner_dbg$(EXE) \
	      bench$(EXE) jsonlint$(EXE) fuzz_libfuzzer$(EXE) \
	      fuzz_standalone$(EXE) fuzz_afl$(EXE)
