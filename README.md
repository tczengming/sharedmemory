# sharedmemory
c++ IPC shared memory，auto release lock

linux多进程通信，使用的共享内存，flock文件锁来保证创建共享内存时的原子性, 而且flock在进程退出时会自动释放锁，可防止进程异常退出时的死锁问题。

```c
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
```
