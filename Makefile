.PHONY all:
all: ACS

ASC: ACS.c
	gcc -pthread ACS.c -o ACS

