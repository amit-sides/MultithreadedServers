
CC=gcc
LD=gcc
CFLAGS=
LFLAGS=
SOURCES=echo.c
OBJECTS=$(SOURCES:.c=.o)

TARGET=echo

.PHONY: all clean rebuild

all: $(TARGET)

rebuild: clean all

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(LD) $(LFLAGS) $? -o $@

clean:
	rm -rf $(TARGET) $(OBJECTS)