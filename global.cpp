#include "global.h"
using namespace std;

unsigned short getPortNumber(const char* arg)
{
    unsigned short portNumber;
    if(sscanf(arg,"%hu",&portNumber)>0)
        return portNumber;
    else {
        fprintf(stderr,"Error: invalid port number\n");
        exit(1);
    }
}

string getHeaderTypeName(PacketType t)
{
    switch (t)
    {
        case GET:
            return string("GET");
            break;
        case ACK:
            return string("ACK");
            break;
        case DAT:
            return string("DAT");
            break;
        case FIN:
            return string("FIN");
            break;
        case BAD:
            return string("BAD");
            break;
        case SYN:
            return string("SYN");
            break;
        default:
            return string("NOT_RECOGNIZED");
            break;
    }
}

void echoHeader(struct header *h)
{
    cerr<< "---- Echo HEADER ----" << endl;
    cerr<< "sequence: " << h->sequence << endl;
    cerr<< "type:     " << getHeaderTypeName(h->type) << endl;
    cerr<< "checksum: ";
    fprintf(stderr,"%x",h->checksum);
    cerr<< endl;
    cerr<< "Window size is: " << h->windowSize << endl;
    cerr<< "---- END HEADER ----" << endl;
}

void fillHeaderStr(char* hdstr, const struct header* hdptr)
{
    if (hdstr == NULL)
    {
        fprintf(stderr,"fillHeaderStr: NULL header string\n");
        exit(1);
    }
    if (hdptr == NULL)
    {
        fprintf(stderr,"fillHeaderStr: NULL header pointer\n");
        exit(1);
    }
    memcpy((void*)hdstr,(const void*)hdptr,sizeof(struct header));
}

void fillHeader(const char* hdstr, struct header* hdptr)
{
    if (hdstr == NULL)
    {
        fprintf(stderr,"fillHeader: NULL header string\n");
        exit(1);
    }
    if (hdptr == NULL)
    {
        fprintf(stderr,"fillHeader: NULL header pointer\n");
        exit(1);
    }
    memcpy((void*)hdptr,(const void*)hdstr,sizeof(struct header));
}

//caculate a 16 bit long checksum 
unsigned short CheckSum(unsigned short *buffer, int size)
{
    unsigned long cksum=0;
    while(size >1)
    {
        cksum+=*buffer++;
        size -=sizeof(unsigned short);
    }
    if(size)
        cksum += *(unsigned char*)buffer;
    
    cksum = (cksum >> 16) + (cksum & 0xffff);
    cksum += (cksum >>16);
    return (unsigned short)(~cksum);
}

// FIXME: return true if the checksum of msg is equal to expected checksum;
// false otherwise.
bool notCorrupted(unsigned short expectedChecksum, char* msg)
{
    if (msg == NULL)
    {
        fprintf(stderr,"notCorrupted: NULL msg. Expected checksum: %hu\n", expectedChecksum);
        exit(1);
    }
    
    
    return true;
}
