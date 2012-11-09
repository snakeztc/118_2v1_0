#ifndef CHANNEL_H
#define CHANNEL_H

#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h> 
#include <netdb.h> 
#include <cstring>
#include <unistd.h>
#include <string>

enum FailMode
{
    FAILFAST,
    NORMAL,
};

typedef float PacketLossRate;
typedef float PacketCorruptRate;

// This is the communication channel that sender and receiver are using; 
// the channel has a fixed packet loss and corrupt rate.
 
class Channel {
public:
 
    // Initialize the channel packet loss rate and packet corrupt
    // rate; if the values l and c are not appropriate (less than 0.0 or
    // greater than MAXRATE), fail the program. Also open a socket.
    Channel(PacketLossRate l, PacketCorruptRate c);

    // Initialize a perfect channel: pl = pc = 0. Also open a socket.
    Channel();

    // Close the socket if we are done
    ~Channel();

    // Bind to a given port number; fail the program if can't do and use
    // FAILFAST mode
    bool bindPort(unsigned short portNumber, FailMode fm);

    // Setup receiver address
    void setupAddress(const char* ip, unsigned short portNumber);

    // Try to send the message of length 'length' to the receiver; must call
    // this function after setupAddress();
    // hptr points to the header we are trying to send.
    // To send only a header, pass NULL as message.
    // This function fails if hptr is NULL; returns -1 if can't send
    // the message
    ssize_t Csend(const char* message, size_t length, struct header *hptr);

    // Try to receive 'length' bytes from the port and put them into buffer;
    // must call this function after bindPort(). Drop packet and return -2 if
    // the packet is corrupted.
    // After a successful call, hptr contains the header fields
    // in the message. This function fails if buffer is NULL, but hptr is
    // allowed to be NULL.
    ssize_t Crecv(char* buffer, size_t length, struct header *hptr);
    
    // Try to receive 'length' bytes from the port and put them into buf;
    // use Crecvfrom if we don't yet know the receiver's IP and port number. 
    // This function will get these information and call setupAddress().
    // Drop packet and return -2 if the packet is corrupted, and don't setup
    // return address. This function fails if buffer is NULL, but hptr is
    // allowed to be NULL.
    ssize_t Crecvfrom(char* buffer, size_t length, struct header *hptr);

private:
    // Try to open a UDP socket; fail the program on failure
    void openSocket();

    PacketLossRate pl;
    PacketCorruptRate pc;
    int socketfd;

    // state variables
    bool addressValid;
    bool portBinded;

    // return address and self address
    socklen_t address_len;
    struct sockaddr_in address,selfaddress;

    int sequence;
};

#endif
