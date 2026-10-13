CC = gcc
CPPFLAGS = -D_DEFAULT_SOURCE -D_POSIX_C_SOURCE=200809L -Iinclude
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -MMD -MP
LDFLAGS =
TARGET = myls
SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:.c=.o)
DEPS = $(OBJECTS:.o=.d)

.PHONY: all clean test
all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	python3 tests/test_myls.py ./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS) $(DEPS)

-include $(DEPS)
