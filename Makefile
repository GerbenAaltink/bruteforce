all: bruteforce run


bruteforce: bruteforce.c rlib.h
	gcc bruteforce.c -o bruteforce

run:
	./bruteforce

clean:		
	rm -f bruteforce
