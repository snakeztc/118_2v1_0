#include "channel.h"
#include "global.h"
using namespace std;

#define CHECKSUM

// Maximum allowed value for packet loss/corrupt rate
static float MAXRATE = 0.40;

Channel::Channel(PacketLossRate l, PacketCorruptRate c):socketfd(-1),addressValid(false),
portBinded(false), address_len(sizeof(address))
{
    if(l <= MAXRATE && l >= 0.0) 
        pl = l;
    else {
        perror("Invalid packet loss rate");
        exit(1);
    }
    if(c <= MAXRATE && c >= 0.0) 
        pc = c;
    else {
        perror("Invalid packet corrupt rate");
        exit(1);
    }
    memset(&address,0,sizeof(address));
    memset(&selfaddress,0,sizeof(address));
    openSocket(); 
}

Channel::Channel():pl(0),pc(0),socketfd(-1),addressValid(false),portBinded(false),
address_len(sizeof(address))
{
    memset(&address,0,sizeof(address));
    memset(&selfaddress,0,sizeof(address));
    openSocket(); 
}

Channel::~Channel() 
{
    if(socketfd != -1)
        close(socketfd);
}

void Channel::openSocket()
{
    int fd = socket(PF_INET,SOCK_DGRAM,0);
    if (fd < 0) {
        perror("Channel::openSocket");
        exit(1);
    }
    socketfd = fd;
}

bool Channel::bindPort(unsigned short portNumber, FailMode fm)
{
    if(portBinded) { 
        fprintf(stderr,"Port already binded, ignore");
        return true;
    };

    selfaddress.sin_family = AF_INET;
    selfaddress.sin_addr.s_addr = INADDR_ANY;
    selfaddress.sin_port = htons(portNumber);
    if (bind(socketfd, (struct sockaddr *) &(selfaddress),
              sizeof(address)) < 0)
    {
        if (fm==FAILFAST)
        {
            perror("Channel::bindPort Failed");
            exit(1);
        }
        else
        {
            return false;
        }
    }
    else {
        portBinded=true;
        return true;
    }
}

void Channel::setupAddress(const char* ip, unsigned short portNumber)
{
    address.sin_family = AF_INET;
    if (inet_pton(AF_INET,ip,(void*)&(address.sin_addr))!=1)
    {
        fprintf(stderr,"Invalid IP address: %s\n",ip);
        exit(1);
    }
    address.sin_port = htons(portNumber);

    addressValid = true;

    if (connect(socketfd, (struct sockaddr *) &(address),
             sizeof(address)) < 0)
    {
        perror("setupAddress");
        exit(1);
    }
}

ssize_t Channel::Csend(const char* message, size_t length, struct header *hptr)
{
    // We cannot send without a header
    if (hptr == NULL)
    {
        fprintf(stderr,"Csend: NULL hptr\n");
        exit(1);
    }
        
    if (!addressValid) {
        fprintf(stderr,"Csend: call setupAddress first\n");
        exit(1);
    }

    ssize_t offset = sizeof(struct header);

    // simulate loss: without writing, return number of bytes written
    float r = rand()*1.0/RAND_MAX;
    if (r > 0.0 && r < pl)
    {
        fprintf(stderr, "Csend: simulated packet loss\n");
        return length;
    }

    // make message
    char* hdstr=NULL;
    hdstr = new char[offset+length];
    if (hdstr == NULL)
    {
        fprintf(stderr,"Csend: can't allocate memory for header\n");
        exit(1);
    }
    
    // fill the packet with header and message
    fillHeaderStr(hdstr,hptr);
    memcpy(hdstr+offset,message,length);

    // calculate checksum
    unsigned short* ptr = &(((struct header*)hdstr)->checksum);
    *ptr = 0;
    unsigned short checksum = CheckSum(hdstr,length+offset);
    *ptr = checksum;

    // write header string
    ssize_t n = write(socketfd, hdstr, length+offset);

    delete []hdstr;
    if (n>offset)
        return n-offset;
    else if (n<0)
        return n;
    else
        return 0;
}

ssize_t Channel::Crecv(char* buffer, size_t length, struct header *hptr)
{
    // we can't recv if buffer is NULL
    if (buffer == NULL)
    {
        fprintf(stderr,"Crecv: NULL buffer\n");
        exit(1);
    }
    if (!portBinded) {
        fprintf(stderr,"Crecv: call bindPort first\n");
        exit(1);
    }

    ssize_t offset = sizeof(struct header);

    // simulate corruption
    float r = rand()*1.0/RAND_MAX;
    if (r > 0.0 && r < pc)
    {
        // does not modify buffer and hptr
        fprintf(stderr,"Crecv: packet corrupted(simulated)\n");
        return -2;
    }

    char* local=NULL;
    // MAX_PAYLOAD_SIZE defined in global.h
    local = new char[offset+MAX_PAYLOAD_SIZE];
    if (local==NULL)
    {
        fprintf(stderr,"Crecv: can't allocate memory for local buffer\n");
        exit(1);
    }
    
    // use a local buffer to read
    ssize_t n = read(socketfd, local, length+offset);
    // can't strip header
    if (n<offset)
    {
        fprintf(stderr,"Crecv: can't strip header\n");
        delete []local;
        return -1;
    }
#ifdef CHECKSUM
    // detect packet corruption
    unsigned short* ptr = &(((struct header*)local)->checksum);
    unsigned short expectedChecksum = *ptr;
    *ptr = 0;
    if(corrupted(expectedChecksum,local,n))
    {
        fprintf(stderr,"Crecv: packet corrupted\n");
        delete []local;
        return -2;
    }
#endif
    // fill buffer
    memcpy(buffer,local+offset,n-offset);
    
    // fill header struct if not NULL
    if (hptr != NULL)
    {
        fillHeader(local,hptr);
    }
    
    delete []local;
    return n-offset;
}

ssize_t Channel::Crecvfrom(char* buffer, size_t length, struct header *hptr)
{
    // we can't recv if buffer is NULL
    if (buffer == NULL)
    {
        fprintf(stderr,"Crecvfrom: NULL buffer\n");
        exit(1);
    }
    if (!portBinded) {
        fprintf(stderr,"Crecvfrom: call bindPort first\n");
        exit(1);
    }
    if (addressValid) {
        fprintf(stderr,"Crecvfrom: receiver known, use Crecv instead\n");
        exit(1);
    }

    ssize_t offset = sizeof(struct header);
    
    // simulate corruption
    float r = rand()*1.0/RAND_MAX;
    if (r > 0.0 && r < pc)
    {
        // does not modify buffer and hptr
        fprintf(stderr,"Crecvfrom: packet corrupted(simulated)\n");
        return -2;
    }

    char* local=NULL;
    // MAX_PAYLOAD_SIZE defined in global.h
    local = new char[offset+MAX_PAYLOAD_SIZE];
    if (local==NULL)
    {
        fprintf(stderr,"Crecvfrom: can't allocate memory for local buffer\n");
        exit(1);
    }
    
    // use a local buffer to read
    struct sockaddr_in client;
    ssize_t n = recvfrom(socketfd,local,length+offset,
                         0,(struct sockaddr *) &(client),
                         &address_len);
    
    // can't strip header
    if (n<offset)
    {
        fprintf(stderr,"Crecvfrom: can't strip header\n");
        delete []local;
        return -1;
    }
#ifdef CHECKSUM
    // detect packet corruption
    unsigned short* ptr = &(((struct header*)local)->checksum);
    unsigned short expectedChecksum = *ptr;
    *ptr = 0;
    if(corrupted(expectedChecksum,local,n))
    {
        fprintf(stderr,"Crecv: packet corrupted\n");
        delete []local;
        return -2;
    }
#endif
    // fill buffer
    memcpy(buffer,local+offset,n-offset);
    
    // fill header struct if not NULL
    if (hptr != NULL)
    {
        fillHeader(local,hptr);
    }
    
    delete []local;
    
    // Register return address
    if(n>0) {
        char* ip = inet_ntoa(client.sin_addr);
        unsigned short port = ntohs(client.sin_port);
        fprintf(stdout,"Client IP: %s, port: %hu\n",ip,port);
        setupAddress(ip,port);
    }
    
    return n-offset;
}

ssize_t Channel::CrecvTimeout(char* buffer, size_t length, 
                         struct header *hptr, int usec)
{
    if (usec < 0)
    {
        fprintf(stderr,"CrecvTimeout: invalid timeout value\n");
        exit(1);
    }
    if (!portBinded) {
        fprintf(stderr,"CrecvTimeout: call bindPort first\n");
        exit(1);
    }

    struct timeval timeout;    
    int sec = usec/1e6;
    usec -= sec*1e6;

    fd_set set;

    FD_ZERO(&set);
    FD_SET(socketfd,&set);
    
    timeout.tv_sec = sec;   
    timeout.tv_usec = usec;

    int n = select(socketfd+1,&set,NULL,NULL,&timeout);

    if(n != 0) 
    {
        return Crecv(buffer,length,hptr);
    }
    else
    {
	// FIXME: more information about timeout
        fprintf(stdout,"CrecvTimeout: timeout detected\n");
        return -3;
    }
}


