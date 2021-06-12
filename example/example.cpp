#include "shared_memory.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <string>

void SendStr()
{
    SharedMemoryManager sharedMemWriter("shared_memory", "/tmp/example.lock");

    std::string str = "hello";
    sharedMemWriter.Write(str.c_str(), str.length());
    printf("write:%s\n", str.c_str());

    str = "world";
    sharedMemWriter.Write(str.c_str(), str.length());
    printf("write:%s\n", str.c_str());
}

void ReceiveStr()
{
    SharedMemoryManager sharedMemReader("shared_memory", "/tmp/example.lock");
    SharedMemoryData sdata;
    for (int i = 0; i < 2; ++i)
    {
        if (sharedMemReader.Read(sdata))
            printf("read:%s id:%d\n", sdata.m_data, sdata.m_id);
    }
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        printf("%s [server|client|help]\n", argv[0]);
        return -1;
    }

    int opt;
    static const struct option opts[] = {
        {"server", no_argument, NULL, 's'},
        {"client", no_argument, NULL, 'c'},
        {"help", no_argument, NULL, 'h'},
    };

    while ((opt = getopt_long_only(argc, argv, "sch", opts, NULL)) != -1)
    {
        switch (opt)
        {
            case 's':
                SendStr();
                break;
            case 'c':
                ReceiveStr();
                break;
            case 'h':
                printf("%s [server|client|help]\n", argv[0]);
                break;
            default:
                printf("%s [server|client|help]\n", argv[0]);
                break;
        }
    }

    return 0;
}
