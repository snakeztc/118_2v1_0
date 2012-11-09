#include <iostream>
#include <string>
#include "global.h"
#include "channel.h"
using namespace std;

int main(int argc, char* argv[])
{
    unsigned short portNumber = 30118;

    const string usage(
    "usage: receiver [sender hostname] [sender portnumber] [filename]");

    if(argc != 4)
    {
        cerr << usage << endl;
        return 1;
    }

    string senderHostname(argv[1]);
    unsigned short senderPortNumber=getPortNumber(argv[2]);
    string filename(argv[3]);
    
    cerr << "----------------------------------" << endl;
    cerr << "Sender hostname: " << senderHostname << endl;
    cerr << "Port:            " << senderPortNumber << endl;
    cerr << "Requesting file: " << filename << endl;
    cerr << "----------------------------------" << endl; 
    
    // setup communication channel
    Channel ch;
    while (ch.bindPort(portNumber,NORMAL)==false)
    {
        portNumber++;
        if (portNumber == 0)
        {
            cerr << "No usable port!" << endl;
            exit(1);
        }
    }

    // setup server address
    ch.setupAddress(argv[1],senderPortNumber);

    // make request header
    struct header h;
    h.sequence = -1;
    h.type = GET;
    h.checksum = 0xdead;

    // receiver sends file request to the sender
    ch.Csend(filename.c_str(),filename.length()+1,&h);

    char buf[MAX_PAYLOAD_SIZE];
    memset(buf,0,MAX_PAYLOAD_SIZE);
    
    // FIXME: we will need MAX_PAYLOAD_SIZE instead of MAX_PAYLOAD_SIZE-1
    // when reading a real file
    ch.Crecv(buf,MAX_PAYLOAD_SIZE-1,&h);
 
    cerr << "Reply from sender: " << string(buf) << endl;

    // echo response
    echoHeader(&h);
 
    // TODO:
    // get ACK: establish data transfer
    // receive data until getting a FIN: send FINACK and shut down

    return 0;
}
