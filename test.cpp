#include <iostream>
#include "global.h"
#include <vector>
#include <fstream>
#include <sys/stat.h>
using namespace std;

int main()
{
    cout << "Size of header:" <<sizeof(struct header) << endl;    
    vector<char*> ok;
    ifstream inputfile;
    inputfile.open("./senderRoot/ok");
    struct stat file_stat;
    stat("./senderRoot/ok", &file_stat);
    cerr << "Filesize" << file_stat.st_size << endl;


    char *buf = new char[10];
    char *buf2 = new char[10];
    inputfile.read(buf, 9);
    inputfile.read(buf2, 9);
    buf[9] = '\0';
    buf2[9] = '\0';
    
    cerr << buf << endl;
    cerr << buf2 << endl;

    return 0;

}
