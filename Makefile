.phony all:
all: ACS

ASC: ACS.c
	gcc -pthread ACS.c -o ACS

.PHONY clean:
clean:
	-rm -rf *.o *.exe

