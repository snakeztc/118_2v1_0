#include <iostream>
#include <string>
#include "global.h"
#include "channel.h"
#include <vector>
#include "fstream"

using namespace std;

const unsigned short MAX_WINDOW_SIZE = 4;
const char base[] = "./receiverRoot";


bool threeWayHandShake(Channel &ch, string filename)
{
    // start three-way hand shake
    // intialize the two headers
    struct header h;
    h.sequence = -1;
    h.type = SYN;
    h.checksum = 0xdead;
    h.windowSize = MAX_WINDOW_SIZE;
    
    struct header h1;
    h1.sequence = -1;
    h1.type = GET;
    h1.checksum = 0xdeed;
    h1.windowSize = MAX_WINDOW_SIZE;
    

    // #1 TWH: receiver sends file request to the sender
    ch.Csend(NULL, 0, &h);
    
    // waiting for SYN back from sender
    char buf[MAX_PAYLOAD_SIZE];
    memset(buf,0,MAX_PAYLOAD_SIZE);
    
    // #2 TWH: receive SYN from sender
    ch.Crecv(buf,MAX_PAYLOAD_SIZE-1,&h);
    
    cerr << "Reply from sender: " << string(buf) << endl;
    
    // echo response
    echoHeader(&h);
    
    // return false if the response is not SYN
    if (h.type != SYN ) {
        cerr << "Receiver: Non-SYN from sender" << endl;
        return false;
    }
    
    // #3 THW: send file rquest
    ch.Csend(filename.c_str(),filename.length()+1,&h1);
    return true;
}

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
    
    //change working directory to base 
    if (chdir(base) < 0) {
        cerr << "Failed to change to base directory" << endl;
        exit(1);
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

    // conduct three way hand shake
    // fast fail if the three way hand shake is failed
    if (!threeWayHandShake(ch, filename)) {
        cerr << "Receiver: Three way hand shake failed." << endl;
        exit (1);
    }
    
    // get ACK: establish data transfer
    // receive data until getting a FIN: send FINACK and shut down
    vector <char *> data;
    while (true) {
        struct header h;
        char *buf = new char[MAX_PAYLOAD_SIZE];
        memset(buf,0,MAX_PAYLOAD_SIZE);
        ch.Crecv(buf,MAX_PAYLOAD_SIZE,&h); 
        if (h.type == DAT) {
           data.push_back(buf);
        } else if (h.type == BAD) {
            cerr << "Receiver: Bad Request" << endl;
            break;
        } //receive FIN Pacet, save file to root and quit
        else if (h.type == FIN) {
            ofstream outfile (filename.c_str());
            for (unsigned int i = 0; i < data.size(); i++)
            {
                outfile.write(data[i], MAX_PAYLOAD_SIZE);
            }
            break;
        }
     
        echoHeader(&h);
    }

    return 0;
}
