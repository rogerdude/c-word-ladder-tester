CC = gcc
CFLAGS = -pedantic -Wall -std=gnu99 -I/local/courses/csse2310/include
LFLAGS = -L/local/courses/csse2310/lib -lcsse2310a3

testuqwordladder: testUQWordLadder.o
	$(CC) $(CFLAGS) $(LFLAGS) -o $@ $<

testUQWordLadder.o: testUQWordLadder.c
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f testuqwordladder testUQWordLadder.o

