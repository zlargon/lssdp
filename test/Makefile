CFLAGS = -g -Wall -I../

ifdef OSX
CFLAGS += -D__OSX__
endif

OBJS = ../lssdp.o

all: daemon

daemon: $(OBJS) daemon.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

clean:
	rm -rf *.o *.exe
