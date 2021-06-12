# sharedmemory
c++ IPC shared memory，auto release lock

linux多进程通信，使用的共享内存，flock文件锁来保证创建共享内存时的原子性, 而且flock在进程退出时会自动释放锁，可防止进程异常退出时的死锁问题。
