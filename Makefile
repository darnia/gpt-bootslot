TARGET = gpt-bootslot
LIBS = -lfdisk
PREFIX = /usr

.PHONY: default all clean

default: $(TARGET)
all: default

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))
HEADERS = $(wildcard *.h)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

.PRECIOUS: $(TARGET) $(OBJECTS)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) $(LDFLAGS) -o $@

install:
	-install -d $(DESTDIR)$(PREFIX)/sbin
	-install -m 0755 $(TARGET) $(DESTDIR)$(PREFIX)/sbin/

clean:
	-rm -f *.o
	-rm -f $(TARGET)
