#include "global.h"
#include "channel.h"
#include <iostream>
#include <string>
using namespace std;

int main(int argc, char* argv[])
{
    const string usage("usage: sender [port number]");

    if(argc != 2) {
        cerr << usage << endl;
        exit(1);
    }

    unsigned short portNumber(getPortNumber(argv[1]));
    cerr << "----------------------------------" << endl;
    cerr << "Sender port:     " << portNumber << endl;
    cerr << "----------------------------------" << endl; 

    // Setup communication channel and bind port
    Channel ch;
    ch.bindPort(portNumber,FAILFAST);

    // buffer and header
    char buf[MAX_PAYLOAD_SIZE];
    memset(buf,0,MAX_PAYLOAD_SIZE);
    struct header h;

    // use Crecvfrom to read incoming request; setup client IP and port
    if(ch.Crecvfrom(buf,MAX_PAYLOAD_SIZE-1,&h)<0) {
        perror("Crecvfrom(buf)");
        exit(1);
    }

    cerr << "Got request: " << string(buf) << endl;
    
    // echo header
    echoHeader(&h);
    
    // respond
    h.sequence = 0;
    h.type = ACK;
    h.checksum = 0xbeef;
    ch.Csend(buf,strlen(buf),&h);

    // TODO:
    // get request: establish reliable data transfer

    // file transmission complete: send FIN

    return 0;
}
