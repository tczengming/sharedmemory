// Last Update:2021-06-12 11:40:41
/**
 * @file shared_memory.c
 * @brief 
 * @author tczengming@163.com www.benewtech.cn
 * @version 0.1.00
 * @date 2021-06-09
 */

#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/file.h>

#include <iostream>

#include "shared_memory.h"

ReadWriteFileLock::ReadWriteFileLock(const std::string &name)
{
    m_lock = open(name.c_str(), O_WRONLY|O_CREAT, 0664);
    if (m_lock < 0)
        printf("%s %d failed:%s\n", __func__, __LINE__, strerror(errno));
}

ReadWriteFileLock::~ReadWriteFileLock()
{
    if (m_lock != -1)
    {
        close(m_lock);
    }
}

void ReadWriteFileLock::Lock(LockType type)
{
    if (m_lock == -1)
        return;
    while (!TryLock(type)) {
        usleep(50);
    }
}

bool ReadWriteFileLock::TryLock(LockType type)
{
    if (m_lock == -1)
        return false;

    int operation;
    if (READ == type)
        operation = LOCK_SH | LOCK_NB;
    else
        operation = LOCK_EX | LOCK_NB;
    return (flock(m_lock, operation) == 0);
}

void ReadWriteFileLock::Unlock()
{
    if (m_lock == -1)
        return;
    flock(m_lock, LOCK_UN);
}

void SharedMemoryCell::Init()
{
    m_data.m_id = 0;
    m_data.m_validLen = 0;

    memset(m_data.m_data, 0, sizeof(m_data.m_data));

    pthread_mutexattr_t ma;
    pthread_mutexattr_init(&ma);
    pthread_mutexattr_setpshared(&ma, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&ma, PTHREAD_MUTEX_ROBUST);
    pthread_mutex_init(&m_mutex, &ma);
}

SharedMemoryCell::SharedMemoryCell()
{
    Init();
}

SharedMemoryQueue *SharedMemoryQueue::Get(const char *name)
{
    int size = sizeof(SharedMemoryQueue);
    SharedMemoryQueue *c = nullptr;

    int fd = shm_open(name, O_RDWR, 0777);
    if (fd < 0)
    {
        fd = shm_open(name, O_CREAT|O_RDWR, 0777);
        if (fd < 0) {
            printf("%s %d failed:%s\n", __func__, __LINE__, strerror(errno));
            return NULL;
        }

        int r = ftruncate(fd, size);
        if (r < 0) {
            printf("%s %d failed:%s\n", __func__, __LINE__, strerror(errno));
            return NULL;
        }

        c = (SharedMemoryQueue *)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, SEEK_SET);
        if (c == NULL) {
            printf("create shm.ctrl: mmap failed\n");
            return NULL;
        }

        c->write_idx_ = 0;
        c->farthest_read_idx_ = 0;
        c->read_idx = 0;

        for (int i = 0; i < sizeof(m_cells)/sizeof(m_cells[0]); ++i)
        {
            c->m_cells[i].Init();
        }
    }
    else
    {
        c = (SharedMemoryQueue *)mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, SEEK_SET);
        if (c == NULL) {
            printf("create shm.ctrl: mmap failed\n");
            return NULL;
        }
    }

    return c;
}

void SharedMemoryQueue::Put()
{
    for (int i = 0; i < sizeof(m_cells)/sizeof(m_cells[0]); ++i)
    {
        pthread_mutex_unlock(&(m_cells[i].m_mutex));
    }
    munmap(this, sizeof(SharedMemoryQueue));
}

SharedMemoryManager::SharedMemoryManager(const char *memName, const char *lockName)
    :m_lock(lockName),
    m_queue(nullptr),
    m_id(0)
{
    m_lock.Lock(ReadWriteFileLock::WRITE);
    m_queue = SharedMemoryQueue::Get(memName);
    m_lock.Unlock();
}

SharedMemoryManager::~SharedMemoryManager()
{
    m_lock.Lock(ReadWriteFileLock::WRITE);
    if (m_queue)
        m_queue->Put();
    m_lock.Unlock();
}

bool SharedMemoryManager::IsValid()
{
    return m_queue != nullptr;
}

bool SharedMemoryManager::TryRead(SharedMemoryData &data)
{
    SharedMemoryCell* m_ptr;
    {
        // make sure index operation is locked.
        // Index operation is light-weighted.
        // Using a lock doesn't hurt performance much.
        //lock_guard lock(q_shared_->operation_lock_);
        m_lock.Lock(ReadWriteFileLock::WRITE);

        if (m_queue->read_idx >= m_queue->write_idx_) {
            m_lock.Unlock();
            return false;
        }

        // Jump to lastest message.
        // It is the flexibility of shared memory. In TCP, you can't do this.
        if (m_queue->read_idx < m_queue->farthest_read_idx_) {
            const int32_t last_read_idx = m_queue->read_idx;
            m_queue->read_idx = m_queue->farthest_read_idx_.load();
            // It doesn't hold when int overflow
            assert(last_read_idx < m_queue->read_idx);
        }

        m_ptr = &m_queue->m_cells[m_queue->read_idx % QUEUE_LEN];

        // Using a semaphore to track how many process is reading the current message.
        // "signal" a reader is here
        m_ptr->m_sem.Increase();

        // Check if someone is writing to this cell
        // This only happen if the queue warp around.
        int r = pthread_mutex_trylock(&m_ptr->m_mutex);
        if (0 != r) {
            if (r == EOWNERDEAD) {
                pthread_mutex_consistent(&m_ptr->m_mutex);
                printf("mutex_lock: mark consistent\n");
                m_ptr->m_sem.SetZero();
            } else {
                m_ptr->m_sem.Decrease();
            }
            std::cout << "try_read|someone is writing" << std::endl;
            m_lock.Unlock();
            return false;
        } else {
            // Unlock it since I lock the cell in if statement.
            //m_ptr->writer_lock_.unlock();
            pthread_mutex_unlock(&m_ptr->m_mutex);
        }

        ++m_queue->read_idx;
        m_lock.Unlock();
    }

    data = m_ptr->m_data;

    m_ptr->m_sem.Decrease();

    return true;
}

bool SharedMemoryManager::Read(SharedMemoryData &data)
{
    if (!IsValid())
    {
        printf("%s %d failed\n", __func__, __LINE__);
        return false;
    }

    return TryRead(data);
}

bool SharedMemoryManager::TryWrite(const char *data, int len)
{
    SharedMemoryCell & m = m_queue->m_cells[m_queue->write_idx_ % QUEUE_LEN];

    // If some processes is reading, do nothing
    if (!m.m_sem.IsZero()) {
        return false;
    }

    {
        // The write operation is protected by a lock *in the message*.
        int r = pthread_mutex_lock(&m.m_mutex);
        if (r == EOWNERDEAD) {
            pthread_mutex_consistent(&m.m_mutex);
            printf("mutex_lock: mark consistent\n");
        }

        m.m_data.m_id = ++m_id;
        m.m_data.m_validLen = len;
        strncpy(m.m_data.m_data, data, len);
        if (len < sizeof(m.m_data.m_data))
            m.m_data.m_data[len] = '\0';

        {
            // Index operation is protected by lock for *write* and *read*.
            // Because index operation is light,
            // using a lock doesn't hurt the performance much.
            
            //lock_guard lock(operation_lock_);
            m_lock.Lock(ReadWriteFileLock::WRITE);
            ++m_queue->write_idx_;

            // Queue warp around.
            if (m_queue->write_idx_ - m_queue->farthest_read_idx_ > QUEUE_LEN) {
                int32_t last_farest_read_idx = m_queue->farthest_read_idx_;
                m_queue->farthest_read_idx_++;

                assert(m_queue->farthest_read_idx_ = m_queue->write_idx_ - QUEUE_LEN);
                // doesn't hold when int overflow
                assert(m_queue->farthest_read_idx_ > last_farest_read_idx);
            }

            m_lock.Unlock();
        }

        pthread_mutex_unlock(&m.m_mutex);
    }

    return true;
}

bool SharedMemoryManager::Write(const std::string &data)
{
    return Write(data.c_str(), data.size());
}

bool SharedMemoryManager::Write(const char *data, int len)
{
    if (!IsValid() || len > sizeof(m_queue->m_cells[0].m_data))
    {
        printf("%s %d %d %d %dfalied\n", __func__, __LINE__, IsValid(), len, sizeof(m_queue->m_cells[0].m_data));
        return false;
    }

    while (!TryWrite(data, len)) {
        // Unlikely. Happends when the queue warp around.
        std::cout << "write|spinning" << std::endl;
        usleep(1000);
    }

    return true;
}
