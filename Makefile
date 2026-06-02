CC      = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -Isrc -Iinclude

SRC = src/lexer.c src/unescape.c src/parser.c src/json.c \
      src/stringify.c src/json_schema.c \
      src/json_pointer.c src/json_patch.c
OBJ     = $(SRC:.c=.o)

.PHONY: all check valgrind bench jsonlint fuzz fuzz-standalone clean

# Library
all: libjson.a

libjson.a: $(OBJ)
	ar rcs $@ $^

# Tests
check: $(SRC) tests/test_json.c
	$(CC) $(CFLAGS) $(SRC) tests/test_json.c -o test_runner -lm
	./test_runner

valgrind: $(SRC) tests/test_json.c
	$(CC) $(CFLAGS) -g -O0 $(SRC) tests/test_json.c -o test_runner_dbg -lm
	valgrind --leak-check=full --error-exitcode=1 ./test_runner_dbg

# Tools
jsonlint: $(SRC) tools/jsonlint.c
	$(CC) $(CFLAGS) $(SRC) tools/jsonlint.c -o jsonlint -lm

bench: $(SRC) tools/bench.c
	$(CC) $(CFLAGS) -O2 $(SRC) tools/bench.c -o bench -lm
	./bench

# Fuzz
# libFuzzer (clang only)
fuzz: $(SRC) tools/fuzz.c
	clang -std=c99 -fsanitize=fuzzer,address \
	      -Isrc -Iinclude \
	      $(SRC) tools/fuzz.c -o fuzz_libfuzzer -lm
	@echo "Run: ./fuzz_libfuzzer corpus/ -max_len=4096"

# Standalone fuzz (reads stdin, no fuzzer engine needed)
fuzz-standalone: $(SRC) tools/fuzz.c
	$(CC) $(CFLAGS) -DFUZZ_STANDALONE $(SRC) tools/fuzz.c \
	      -o fuzz_standalone -lm
	@echo "Run: echo '{\"x\":1}' | ./fuzz_standalone"

# AFL++ (requires afl-clang-fast in PATH)
fuzz-afl: $(SRC) tools/fuzz.c
	AFL_USE_ASAN=1 afl-clang-fast -std=c99 \
	      -Isrc -Iinclude \
	      $(SRC) tools/fuzz.c -o fuzz_afl -lm
	@echo "Run: afl-fuzz -i corpus/ -o findings/ -- ./fuzz_afl"

# Clean
clean:
	rm -f $(OBJ) libjson.a test_runner test_runner_dbg \
	      bench jsonlint fuzz_libfuzzer fuzz_standalone fuzz_afl
