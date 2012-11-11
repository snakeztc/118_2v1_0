#include <iostream>
#include <string>
#include "global.h"
#include "channel.h"
#include <vector>
#include "fstream"

using namespace std;

const unsigned short MAX_WINDOW_SIZE = 4;
const char base[] = "./receiverRoot";
const int INTIAL_RTO = 10000000;

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
    h1.checksum = CheckSum(filename.c_str(),filename.length()+1);
    h1.windowSize = MAX_WINDOW_SIZE;

    struct header hr;
    // #1 TWH: receiver sends file request to the sender
    ch.Csend(NULL, 0, &h);
    

    // #2 TWH: wait for syn from sender
    char buf[MAX_PAYLOAD_SIZE];
    while (true)
    {
        memset(buf,0,MAX_PAYLOAD_SIZE);
        int result = ch.CrecvTimeout(buf, MAX_PAYLOAD_SIZE, &hr, INTIAL_RTO);
        if(result == -2) continue;
        else if(result == -3) {
            cerr << "Receiver: resend SYN" << endl;
            ch.Csend(NULL, 0, &h);
        } else {
            break;
        }
    }
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

void resendACK(Channel &ch, int expected)
{
    struct header h;
    h.type = ACK;
    h.sequence = expected;
    h.checksum = 0xaaaa;
    h.windowSize = MAX_WINDOW_SIZE;
    ch.Csend(NULL,0,&h);
    cerr << "Resend ACK " << expected << endl;
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
    Channel ch(0.1,0.1);
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
    vector <unsigned int> datasize;
    int expectedPacket = 0;
    while (true) {
        struct header h;
        char *buf = new char[MAX_PAYLOAD_SIZE];
        memset(buf,0,MAX_PAYLOAD_SIZE);
        unsigned int size = 0;
        int result = 0;
        do {
            result = ch.CrecvTimeout(buf,MAX_PAYLOAD_SIZE,&h, INTIAL_RTO);
            //if time out resend ACK
            if (result < 0) {
                resendACK(ch, expectedPacket);
            }
        } while(result < 0);
        size = (unsigned int) result;
     
        if (h.type == DAT) {
           if (h.sequence == expectedPacket)
           {
               //save data
               data.push_back(buf);
               datasize.push_back(size);
               expectedPacket++;
               cerr << "Receiver: get packet " << h.sequence << endl;          
           }
           //send back ACK
           h.sequence = expectedPacket;
           h.type = ACK;
           h.windowSize = MAX_WINDOW_SIZE;
           h.checksum = 0xaaaa;
           ch.Csend(NULL, 0, &h);
        } else if (h.type == BAD) {
            cerr << "Receiver: Bad Request" << endl;
            break;
        } //receive FIN Pacet, save file to root and quit
        else if (h.type == FIN) {
            cerr << "Receiver: file transmission complete" << endl;
            //send a FIN BACK to confirm
            h.sequence = -1;
            h.type = FIN;
            h.windowSize = MAX_WINDOW_SIZE;
            h.checksum = 0xaaaa;
            cerr << "Receiver: reply FIN" << endl;
            ch.Csend(NULL, 0, &h);
            //FIXME if this packet is lost
            //push data to app layer
            ofstream outfile (filename.c_str());
            for (unsigned int i = 0; i < data.size(); i++)
            {
                outfile.write(data[i], datasize[i]);
            }
            break;
        } else {
            cerr << "Receiver: receive SYN" << endl;
            //resend GET message
            struct header hg;
            hg.sequence = -1;
            hg.type = GET;
            hg.checksum = CheckSum(filename.c_str(), filename.length()+1);
            hg.windowSize = MAX_WINDOW_SIZE;
            ch.Csend(filename.c_str(), filename.length()+1, &hg);
        }
     
        //echoHeader(&h);
    }

    return 0;
}
