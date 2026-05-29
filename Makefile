CC      = cc
CFLAGS  = -std=c99 -pedantic -Wall -Wextra -Werror -Iinclude -Isrc

SRC     = src/lexer.c src/unescape.c src/parser.c src/json.c
OBJ     = $(SRC:.c=.o)

.PHONY: all check clean

all: libjson.a

libjson.a: $(OBJ)
	ar rcs $@ $^

check: $(SRC) tests/test_json.c
	$(CC) $(CFLAGS) $(SRC) tests/test_json.c -o test_runner -lm
	./test_runner

valgrind: $(SRC) tests/test_json.c
	$(CC) $(CFLAGS) -g -O0 $(SRC) tests/test_json.c -o test_runner_dbg -lm
	valgrind --leak-check=full --error-exitcode=1 ./test_runner_dbg

clean:
	rm -f $(OBJ) libjson.a test_runner test_runner_dbg
