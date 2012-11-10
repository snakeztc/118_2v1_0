// global.h: global helper function and constants

#ifndef GLOBAL_H
#define GLOBAL_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <iostream>

// maximum payload size
const unsigned MAX_PAYLOAD_SIZE = 512;

// Turn on/off debug information printing
#define DEBUG

// Translate cstring arg to unsigned short; fail the program
// if we can't do the conversion.
unsigned short getPortNumber(const char* arg);

// FIXME: do we need more types?
enum PacketType {
    GET,                // get
    ACK,                // acknowledge
    DAT,                // data
    FIN,                // finalize
    BAD,                // bad request
    SYN,                // synchronize 
};

// Message header struct
// The size of struct should be 12 bytes, with alignment.

struct header {
    int            sequence;    // 4
    PacketType     type;        // 4
    unsigned short checksum;    // 2
    unsigned short windowSize;   // 2
};                              // 14 total

// Return type name
std::string getHeaderTypeName(PacketType t);

// Report content of header
void echoHeader(struct header *h);

// Put all the information in the header struct hdPtr into a buffer headerStr
void fillHeaderStr(char* hdstr, const struct header* hdptr);

// Convert the headerStr produced by fillHeaderStr and fill the struct hdPtr.
void fillHeader(const char* hdstr, struct header* hdptr);

// Provide a function caculate a checksum from n bits of data
unsigned short CheckSum(const void *buffer, int size);

// Return true if the packet is corrupted, i.e., checksum of msg is not 
// equal to the expected checksum; false otherwise.
bool corrupted(unsigned short expectedChecksum, char* msg, int size);

#endif
