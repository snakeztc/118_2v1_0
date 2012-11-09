CC=g++
CFLAGS=-Wall -Wextra -Werror
all: receiver sender
receiver: receiver.o channel.o global.o
	$(CC) receiver.o channel.o global.o -o receiver
receiver.o: receiver.cpp global.h
	$(CC) receiver.cpp $(CFLAGS) -c -o receiver.o

sender: sender.o channel.o
	$(CC) sender.o channel.o global.o -o sender
sender.o: sender.cpp global.h
	$(CC) sender.cpp $(CFLAGS) -c -o sender.o

channel.o: channel.cpp channel.h global.h
	$(CC) channel.cpp $(CFLAGS) -c -o channel.o 
global.o: global.cpp global.h
	$(CC) global.cpp $(CFLAGS) -c -o global.o

clean:
	rm -rf *.o *~ receiver sender test
test: global.o
	$(CC) test.cpp global.o -o test
