all:
	$(MAKE) -C test

clean:
	rm -rf *.o
	$(MAKE) -C test clean
