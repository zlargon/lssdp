CFLAGS = -g -Wall -I../
ifdef OSX
CFLAGS += -D__MAC__ -D__IOS__
endif

OBJS = ../lssdp.o

all: daemon network_interface packet_listener

network_interface: $(OBJS) network_interface.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

daemon: $(OBJS) daemon.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

packet_listener: $(OBJS) packet_listener.o
	$(CC) $(CFLAGS) -o $@.exe $@.o $(OBJS)

clean:
	rm -rf *.o *.exe
