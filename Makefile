.phony default:
default: ACS

ACS: ACS.c
	gcc -pthread ACS.c -o ACS

.PHONY clean:
clean:
	-rm -rf *.o *.exe