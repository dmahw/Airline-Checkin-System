all: ACS

ASC: ACS.c
	gcc -pthread -o ACS ACS.c 

