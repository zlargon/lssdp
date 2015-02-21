CFLAGS = -g -Wall -I../

ifdef OSX
CFLAGS += -D__OSX__
endif

OBJS = ../lssdp.o

all: test interface_monitor

test: $(OBJS) test.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

interface_monitor: $(OBJS) interface_monitor.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

clean:
	rm -rf *.o *.exe
