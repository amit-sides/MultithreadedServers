
CC=gcc
LD=gcc
CFLAGS=-g
LFLAGS=
SOURCES=poll_chat.c
CLIENT_SOURCES=client.c
OBJECTS=$(SOURCES:.c=.o)
CLIENT_OBJECTS=$(CLIENT_SOURCES:.c=.o)

TARGET=poll
CLIENT=client

.PHONY: all clean rebuild

all: $(TARGET) $(CLIENT)

rebuild: clean all

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(LD) $(LFLAGS) $? -o $@

$(CLIENT): $(CLIENT_OBJECTS)
	$(LD) $(LFLAGS) $? -o $@

clean:
	rm -rf $(TARGET) $(OBJECTS) $(CLIENT) $(CLIENT_OBJECTS)