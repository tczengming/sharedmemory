// Last Update:2021-06-12 10:19:41
/**
 * @file shared_memory.h
 * @brief 
 * @author tczengming@163.com www.benewtech.cn
 * @version 0.1.00
 * @date 2021-06-09
 */

#ifndef __SHARED_MEMORY_H__
#define __SHARED_MEMORY_H__

#include <pthread.h>
#include <string>
#include <atomic>

#define QUEUE_LEN 2

struct Semaphore {
    inline void Increase()
    {
        count_++;
    }

    inline void Decrease()
    {
        count_--;
    }
    
    inline bool IsZero()
    {
        int32_t expect = 0;
        return count_.compare_exchange_strong(expect, 0);
    }

    inline void SetZero()
    {
        count_ = 0;
    }

    std::atomic<int32_t> count_ = {0};
};

struct SharedMemoryData {
    char m_data[2592 * 1936 * 3 / 2 + 12];
    int m_id;
    int m_validLen;
};

struct SharedMemoryCell {
    void Init();
    SharedMemoryCell();

    SharedMemoryData m_data;
    pthread_mutex_t m_mutex;
    Semaphore m_sem;
};

class SharedMemoryQueue {
public:
    static SharedMemoryQueue *Get(const char *name);
    void Put();

private:
    SharedMemoryQueue()=default;

public:
    std::atomic<int32_t> write_idx_{ 0 };
    std::atomic<int32_t> farthest_read_idx_{ 0 };

    // The read idx for this reader.
    int32_t read_idx{ 0 };

    SharedMemoryCell m_cells[QUEUE_LEN];
};

class ReadWriteFileLock {
public:
    enum LockType
    {
        READ,
        WRITE,
    };

    ReadWriteFileLock(const std::string &name);
    ~ReadWriteFileLock();

    void Lock(LockType type);
    bool TryLock(LockType type);
    void Unlock();

private:
    int m_lock;
};

class SharedMemoryManager
{
public:
    SharedMemoryManager(const char *memName, const char *lockName);
    ~SharedMemoryManager();

    bool IsValid();
    bool Read(SharedMemoryData &data);
    bool TryRead(SharedMemoryData &data);

    bool Write(const std::string &data);
    bool Write(const char *data, int len);
    bool TryWrite(const char *data, int len);
    
private:
    ReadWriteFileLock m_lock;
    SharedMemoryQueue *m_queue;
    int m_id;
};

#endif  /*__SHARED_MEMORY_H__*/
