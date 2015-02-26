CFLAGS = -g -Wall -I../

OBJS = ../lssdp.o

all: daemon network_interface

network_interface: $(OBJS) network_interface.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

daemon: $(OBJS) daemon.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

clean:
	rm -rf *.o *.exe
