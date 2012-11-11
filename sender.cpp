#include "global.h"
#include "channel.h"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sys/stat.h>

using namespace std;

const char base[] = "./senderRoot";
const int INTIAL_RTO = 1000000;

bool threeWayHandShake(Channel &ch, unsigned short& windowSize, string& fileName)
{
    // buffer and header
    char buf[MAX_PAYLOAD_SIZE];
    memset(buf,0,MAX_PAYLOAD_SIZE);
    struct header h;
    struct header hr;
    // #1 TWH: use Crecvfrom to read incoming request; setup client IP and port
    // wait for three-way hand shake
    if(ch.Crecvfrom(buf,MAX_PAYLOAD_SIZE-1,&h)<0) {
        perror("Crecvfrom(buf)");
        return false;
    }
    
    cerr << "Got request: " << string(buf) << endl;
    
    // echo header
    echoHeader(&h);
    
    // fast fail if the message is not a SYN
    if (h.type != SYN) {
        cerr << "Sender: Non-SYN from receiver" << endl;
        return false;
    }
    
    windowSize = h.windowSize;
    if (windowSize < 1) {
        cerr << "Sender: window size too small" << endl;
        return false;
    }
    
    // #2 TWH: respond
    h.sequence = 0;
    h.type = SYN;
    h.checksum = 0xbeef;
    h.windowSize = windowSize;
    ch.Csend(NULL, 0, &h);
    
    // #3 TWH: wait for file reqeust from receiver
    memset(buf,0,MAX_PAYLOAD_SIZE);
    while (ch.CrecvTimeout(buf, MAX_PAYLOAD_SIZE, &hr, INTIAL_RTO) < 0) {
        memset(buf,0,MAX_PAYLOAD_SIZE);
        ch.Csend(NULL,0,&h);
        cerr << "Sender: TWH time out" << endl;
    }
    cerr << "Sender: Get File Name" << endl;
    string temp(buf);
    fileName = temp;
    
    
    return true;
}

bool prepareForData(string fileName, vector<char*>& dataSequence, int& lastpacketSize)
{
    //create URL
    ifstream inFile;
    string url = "./";
    url += fileName;
    
    // eliminate trailing '/'
    if (url.at(url.length() - 1) == '/') {
        url = url.substr(0, url.length() - 1);
    }
    //check the exisitance of file
    inFile.open(url.c_str(), ifstream::binary);
    if (!inFile) {
        cerr << "Sender: Fail to open file: " << url << endl;
        return false;
    }
    
    //divide file into chunks with size MAX_PAY_LOAD
    struct stat file_stat;
    int n = stat(url.c_str(), &file_stat);
    if (n < 0) {
        cerr << "Sender: file_stat fail";
        return false;
    }
    int fileSize = file_stat.st_size;
    int numberOfPacket = fileSize / MAX_PAYLOAD_SIZE;
    
    cerr << "Sender: file size is: " << fileSize << endl;

    for (int i = 0; i < numberOfPacket; i++)
    {
        char *buf = new char[MAX_PAYLOAD_SIZE];
        memset(buf, 0, MAX_PAYLOAD_SIZE);
        inFile.read(buf, MAX_PAYLOAD_SIZE);
        dataSequence.push_back(buf);
    }
    //append the remaining bytes to dataSequence
    if ((signed int)(numberOfPacket * MAX_PAYLOAD_SIZE) < fileSize)
    {
        int offset = fileSize - numberOfPacket * MAX_PAYLOAD_SIZE;
        lastpacketSize = offset;
        char *buf = new char[offset];
        inFile.read(buf, offset);
        dataSequence.push_back(buf);
    }
    inFile.close();
    cerr << "Data has " << dataSequence.size() << " Packets" << endl;
    return true;
}

void resendPackets(Channel& ch, vector<char*>& data, int start, int end, int numberOfPacket, int offset, int window)
{
    if (end > numberOfPacket - 1) {
        cerr << "Sender: " << end << " > " << numberOfPacket - 1 << endl;
        end = numberOfPacket - 1;
    }
    if (start < 0) {
        cerr << "Sender: " << start << " < " << "0" << endl;
        start = 0;
    }
    cerr <<"*********************" << endl;
    cerr <<"Resend packet from " << start << " to " << end << endl;
    for (int i = start; i <= end; i++)
    {
        struct header h;
        h.type = DAT;
        h.sequence = i;
        h.windowSize = window;
        if (end == numberOfPacket - 1) {
            h.checksum = CheckSum(data[i], offset);
            ch.Csend(data[i], offset, &h);
        } else {
            h.checksum = CheckSum(data[i], MAX_PAYLOAD_SIZE);
            ch.Csend(data[i], MAX_PAYLOAD_SIZE, &h);
        }
        cerr << "Sender: resend packet " << i << endl;
    }
    cerr << "*********************" << endl;
}
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
    
    //change working directory to sender root
    if (chdir(base) < 0) {
        cerr << "Failed to change to base directory" << endl;
        exit(1);
    }

    // Setup communication channel and bind port
    // do simulation here
    Channel ch (0.1, 0.1);
    ch.bindPort(portNumber,FAILFAST);

    // flow control: windowSize is determined by receiver
    unsigned short windowSize = 0;
    string fileName = "";
    
    if (!threeWayHandShake(ch, windowSize, fileName)) {
        cerr << "Sender: Three way hand shake failed." << endl;
        exit (1);
    }

    // check if the file exist, if no return a BAD_REQUEST message
    // else return a vector of data with MAX_PAY_LOAD SIZE for each
    vector <char*> data;
    struct header h;
    int lastpacketSize = MAX_PAYLOAD_SIZE;
    if (!prepareForData(fileName, data, lastpacketSize))
    {
        cerr << "Sender: Bad request of File" <<endl;
        h.sequence = -1;
        h.type = BAD;
        h.checksum = 0xbead;
        h.windowSize = windowSize;
        ch.Csend(NULL, 0, &h);
        exit(1);
    }
    
    //send all packets of data in GBN fashion
    //establish reliable data transfer
    int numberOfPacket = data.size();
    int ackedPtr = 0;
    int lastAcked = -1;
    int dupAckCounter = 0;
    
    for (int i = 0; lastAcked < numberOfPacket - 1;)
    {
        while ((ackedPtr + windowSize < i || (i == numberOfPacket && lastAcked < numberOfPacket - 1) )|| (numberOfPacket < windowSize && lastAcked < numberOfPacket - 1 && i == numberOfPacket)) {
            char buf[MAX_PAYLOAD_SIZE];
            memset(buf, 0, MAX_PAYLOAD_SIZE);
            int result = -1;
            while (result < 0)
            {
                result = ch.CrecvTimeout(buf, MAX_PAYLOAD_SIZE, &h, INTIAL_RTO);
             
                if (result == -2) continue;
                if (result == -3) {
                    memset(buf, 0, MAX_PAYLOAD_SIZE);
                    // start retransimission
                    cerr << "Sender: Data time out." << endl;
                    resendPackets(ch, data, ackedPtr, i - 1, numberOfPacket, lastpacketSize, windowSize);
                }
            }
            if (h.type == ACK) {
                // record the sequence number of ACK
                // trigger retrainsimission when receive the 3rd dup ack
                if (lastAcked == h.sequence ) {
                    dupAckCounter++;
                    if (dupAckCounter == 3) {
                        cerr << "Sender: receive the 3rd dupACK " << lastAcked << endl;
                        // start retransimission
                        resendPackets(ch, data, ackedPtr, i - 1 , numberOfPacket, lastpacketSize, windowSize);
                    }
                }
                
                //increment ackedPTR when receive expeceted ACK
                if (h.sequence == ackedPtr + 1) {
                    cerr << "Sender: get ACK " << h.sequence << endl;
                    lastAcked = ackedPtr;
                    dupAckCounter = 0;
                    ackedPtr++;
                } else if (h.sequence == i) {//leap window forward when get outboudn ACK
                    cerr << "Sender: get ACK " << h.sequence << endl;
                    ackedPtr = h.sequence;
                    lastAcked = h.sequence - 1;
                    dupAckCounter = 0;
                }
            }
        } 
        if (i < numberOfPacket) {
            h.type = DAT;
            h.windowSize = windowSize;
            h.sequence = i;
            
            if (i == numberOfPacket - 1) {
                h.checksum = CheckSum(data[i],lastpacketSize);
                ch.Csend(data[i], lastpacketSize, &h);
            } else {
                h.checksum = CheckSum(data[i],MAX_PAYLOAD_SIZE);
                ch.Csend(data[i], MAX_PAYLOAD_SIZE, &h);
            }
            cerr << "Sender: send packet " << i << endl;
            i++;
        }
    }

    // file transmission complete: send FIN
    cerr << "Sender: send FIN" << endl;
    h.type = FIN;
    h.checksum = 0xdddd;
    h.windowSize = windowSize;
    h.sequence = -1;
    ch.Csend(NULL, 0, &h);

    //if resendCounter becomes 3. Assume file transimission completed
    int resendCounter = 0;
    //wait for FIN from receive
    do {
        cerr << "Sender: Wait for FIN" << endl;
        char buf[MAX_PAYLOAD_SIZE];
        memset(buf, 0, MAX_PAYLOAD_SIZE);
        struct header hr;
        int result = ch.CrecvTimeout(buf, MAX_PAYLOAD_SIZE, &hr, INTIAL_RTO);
        if (result == -2) continue;
        else if (result == -3) {
            cerr << "Sender: resend FIN" << endl;
            ch.Csend(NULL, 0, &h);
            resendCounter++;
            if (resendCounter == 3) {
                cerr <<"Sender: FIN wait Time out" << endl;
                break;
            }
        } else if (hr.type == FIN) {
            cerr << "Sender: Get FIN" << endl;
            break;
        }
    } while(true);
    return 0;
}
