all:
	$(MAKE) -C test OSX=$(OSX)

clean:
	rm -rf *.o
	$(MAKE) -C test clean
