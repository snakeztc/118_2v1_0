#include "global.h"
#include "channel.h"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <sys/stat.h>

using namespace std;

const char base[] = "./senderRoot";

bool threeWayHandShake(Channel &ch, unsigned short& windowSize, string& fileName)
{
    // buffer and header
    char buf[MAX_PAYLOAD_SIZE];
    memset(buf,0,MAX_PAYLOAD_SIZE);
    struct header h;
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
    ch.Csend(buf,strlen(buf),&h);
    
    // #3 TWH: wait for reqeust from receiver
    memset(buf,0,MAX_PAYLOAD_SIZE);
    ch.Crecv(buf, MAX_PAYLOAD_SIZE, &h);
    
    string temp(buf);
    fileName = temp;
    
    return true;
}

bool prepareForData(string fileName, vector<char*>& dataSequence)
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
        inFile.read(buf, MAX_PAYLOAD_SIZE);
        dataSequence.push_back(buf);
    }
    //append the remaining bytes to dataSequence
    if ((signed int)(numberOfPacket * MAX_PAYLOAD_SIZE) < fileSize)
    {
        int offset = fileSize - numberOfPacket * MAX_PAYLOAD_SIZE;
        char *buf = new char[offset];
        inFile.read(buf, offset);
        dataSequence.push_back(buf);
    }
    inFile.close();
    cerr << "Data Sequence generated with " << dataSequence.size() << " Packets" << endl;
    return true;
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
    Channel ch;
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
    if (!prepareForData(fileName, data))
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
    for (int i = 0; i < numberOfPacket; i++)
    {
        h.type = DAT;
        h.checksum = 0xbbbb;
        h.windowSize = windowSize;
        h.sequence = i;
        ch.Csend(data[i], MAX_PAYLOAD_SIZE, &h);
    }

    // file transmission complete: send FIN
    h.type = FIN;
    h.checksum = 0xdddd;
    h.windowSize = windowSize;
    h.sequence = -1;
    ch.Csend(NULL, 0, &h);

    return 0;
}
