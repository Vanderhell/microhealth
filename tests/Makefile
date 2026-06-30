CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -std=c99 -Wall -Wextra -Wpedantic -Werror
LDFLAGS ?=
LDLIBS ?=

SRC = ../src/mhealth.c
TEST_SRC = test_all.c
OUT = test_all

.PHONY: all clean

all: $(OUT)
	./$(OUT)

$(OUT): $(SRC) $(TEST_SRC) ../include/mhealth.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -I../include $(SRC) $(TEST_SRC) $(LDFLAGS) $(LDLIBS) -o $(OUT)

clean:
	rm -f $(OUT) $(OUT).exe
