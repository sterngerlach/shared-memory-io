
/* shm_comm.h */

#ifndef SHM_COMM_H
#define SHM_COMM_H

#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * ShmCommState enum definitions
 */

enum ShmCommState
{
    Init       = 0,
    Published  = 1,
    Subscribed = 2,
    GotResult  = 3,
};

template <typename DataType, typename ResultType>
class DataPublisher;

template <typename DataType, typename ResultType>
class DataSubscriber;

/*
 * SharedData struct definitions
 */

template <typename DataType, typename ResultType>
struct SharedData
{
public:
    friend class DataPublisher<DataType, ResultType>;
    friend class DataSubscriber<DataType, ResultType>;
    
private:
    SharedData() { }
    ~SharedData() { }

    SharedData(const SharedData& other);
    SharedData(SharedData&& other);
    SharedData& operator=(const SharedData& other);
    SharedData& operator=(SharedData&& other);

private:
    ShmCommState    mState;
    bool            mPublisherActive;
    bool            mSubscriberActive;
    DataType        mData;
    ResultType      mResult;
    pthread_mutex_t mMutex;
    pthread_cond_t  mCondPublisherReady;
    pthread_cond_t  mCondSubscriberReady;
    pthread_cond_t  mCondSubscribed;
    pthread_cond_t  mCondGotResult;
    pthread_cond_t  mCondSubscriberDone;
};

/*
 * DataPublisher class definitions
 */

template <typename DataType, typename ResultType>
class DataPublisher
{
public:
    typedef SharedData<DataType, ResultType>  SharedType;
    typedef SharedData<DataType, ResultType>* SharedPtrType;

public:
    DataPublisher();
    ~DataPublisher();

    bool Initialize(const char* sharedMemoryName);
    void Destroy();
    void Publish(DataType& sharedData);
    void WaitForResult();
    void Stop();

    inline ResultType& GetResult() const { return this->mpShared->mResult; }

private:
    DataPublisher(const DataPublisher& other);
    DataPublisher(DataPublisher&& other);
    DataPublisher& operator=(const DataPublisher& other);
    DataPublisher& operator=(DataPublisher&& other);

private:
    SharedPtrType mpShared;
    const char*   mShmName;
    int           mShmFd;
};

/*
 * DataPublisher class methods
 */

template <typename DataType, typename ResultType>
DataPublisher<DataType, ResultType>::DataPublisher() :
    mpShared(NULL),
    mShmName(NULL),
    mShmFd(-1)
{
}

template <typename DataType, typename ResultType>
DataPublisher<DataType, ResultType>::~DataPublisher()
{
    this->Destroy();
}

template <typename DataType, typename ResultType>
bool DataPublisher<DataType, ResultType>::Initialize(
    const char* sharedMemoryName)
{
    if (!sharedMemoryName) {
        std::cerr << "Invalid shared memory name" << std::endl;
        return false;
    }
    
    this->mShmName = sharedMemoryName;

    /* Create Posix shared memory object */
    this->mShmFd = shm_open(this->mShmName,
                            O_RDWR | O_CREAT,
                            S_IRUSR | S_IWUSR);
    
    if (this->mShmFd == -1) {
        std::cerr << "Error: shm_open() failed" << std::endl;
        return false;
    }

    /* Set the size of shared memory object */
    if (ftruncate(this->mShmFd, sizeof(SharedType)) == -1) {
        std::cerr << "Error: ftruncate() failed" << std::endl;
        return false;
    }

    /* Map shared memory object to memory */
    void* pShared = mmap(NULL,
                         sizeof(SharedType),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         this->mShmFd,
                         0);
    
    if (pShared == MAP_FAILED) {
        std::cerr << "Error: mmap() failed" << std::endl;
        return false;
    }

    /* Set the pointer to shared memory */
    this->mpShared = reinterpret_cast<SharedPtrType>(pShared);

    /* Initialize pthread mutex object */
    pthread_mutexattr_t mutexAttr;
    pthread_mutexattr_init(&mutexAttr);
    pthread_mutexattr_setpshared(&mutexAttr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&this->mpShared->mMutex, &mutexAttr);

    /* Initialize pthread condition variable object */
    pthread_condattr_t condAttr;
    pthread_condattr_init(&condAttr);
    pthread_condattr_setpshared(&condAttr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&this->mpShared->mCondPublisherReady, &condAttr);
    pthread_cond_init(&this->mpShared->mCondSubscriberReady, &condAttr);
    pthread_cond_init(&this->mpShared->mCondSubscribed, &condAttr);
    pthread_cond_init(&this->mpShared->mCondGotResult, &condAttr);
    pthread_cond_init(&this->mpShared->mCondSubscriberDone, &condAttr);

    /* Initialize other members */
    this->mpShared->mState = ShmCommState::Init;
    this->mpShared->mPublisherActive = true;
    this->mpShared->mSubscriberActive = true;

    return true;
}

template <typename DataType, typename ResultType>
void DataPublisher<DataType, ResultType>::Destroy()
{
    if (this->mpShared != NULL) {
        /* Destroy condition variables */
        pthread_cond_destroy(&this->mpShared->mCondSubscriberDone);
        pthread_cond_destroy(&this->mpShared->mCondGotResult);
        pthread_cond_destroy(&this->mpShared->mCondSubscribed);
        pthread_cond_destroy(&this->mpShared->mCondSubscriberReady);
        pthread_cond_destroy(&this->mpShared->mCondPublisherReady);

        /* Destroy mutex */
        pthread_mutex_destroy(&this->mpShared->mMutex);

        /* Unmap shared memory */
        munmap(this->mpShared, sizeof(SharedType));
    }

    /* Close Posix shared memory object */
    if (this->mShmFd != -1)
        close(this->mShmFd);

    /* Unlink Posix shared memory object */
    if (this->mShmName != NULL)
        shm_unlink(this->mShmName);

    this->mpShared = NULL;
    this->mShmName = NULL;
    this->mShmFd = -1;
}

template <typename DataType, typename ResultType>
void DataPublisher<DataType, ResultType>::Publish(DataType& sharedData)
{
    pthread_mutex_lock(&this->mpShared->mMutex);

    /* Wait for subscriber to get ready */
    while (this->mpShared->mState != ShmCommState::Init)
        pthread_cond_wait(&this->mpShared->mCondSubscriberReady,
                          &this->mpShared->mMutex);

    /* Pass data object to subscriber */
    this->mpShared->mData = sharedData;
    
    /* Update the current state */
    this->mpShared->mState = ShmCommState::Published;

    /* Notify subscriber that publisher is ready */
    pthread_cond_signal(&this->mpShared->mCondPublisherReady);
    
    pthread_mutex_unlock(&this->mpShared->mMutex);
}

template <typename DataType, typename ResultType>
void DataPublisher<DataType, ResultType>::WaitForResult()
{
    pthread_mutex_lock(&this->mpShared->mMutex);

    /* Wait for subscriber to process shared data */
    while (this->mpShared->mState != ShmCommState::Subscribed)
        pthread_cond_wait(&this->mpShared->mCondSubscribed,
                          &this->mpShared->mMutex);

    /* Result returned by subscriber is stored in this->mpShared->mResult */
    /* Update the current state */
    this->mpShared->mState = ShmCommState::GotResult;

    /* Notify subscriber that publisher received result */
    pthread_cond_signal(&this->mpShared->mCondGotResult);

    pthread_mutex_unlock(&this->mpShared->mMutex);
}

template <typename DataType, typename ResultType>
void DataPublisher<DataType, ResultType>::Stop()
{
    /* Stop publisher */
    pthread_mutex_lock(&this->mpShared->mMutex);
    
    /* Publisher is now inactive */
    this->mpShared->mPublisherActive = false;

    pthread_cond_signal(&this->mpShared->mCondPublisherReady);

    /* Wait for subscriber to stop */
    while (this->mpShared->mSubscriberActive)
        pthread_cond_wait(&this->mpShared->mCondSubscriberDone,
                          &this->mpShared->mMutex);

    pthread_mutex_unlock(&this->mpShared->mMutex);
}

/*
 * DataSubscriber class definitions
 */
template <typename DataType, typename ResultType>
class DataSubscriber
{
public:
    typedef SharedData<DataType, ResultType>  SharedType;
    typedef SharedData<DataType, ResultType>* SharedPtrType;

public:
    DataSubscriber();
    ~DataSubscriber();

    bool Initialize(const char* sharedMemoryName);
    void Destroy();
    bool Subscribe();
    void SendResult(ResultType& resultData);

    inline DataType& GetData() const { return this->mpShared->mData; }

private:
    DataSubscriber(const DataSubscriber& other);
    DataSubscriber(DataSubscriber&& other);
    DataSubscriber& operator=(const DataSubscriber& other);
    DataSubscriber& operator=(DataSubscriber&& other);

private:
    SharedPtrType mpShared;
    const char*   mShmName;
    int           mShmFd;
};

/*
 * DataSubscriber class methods
 */

template <typename DataType, typename ResultType>
DataSubscriber<DataType, ResultType>::DataSubscriber() :
    mpShared(NULL),
    mShmName(NULL),
    mShmFd(-1)
{
}

template <typename DataType, typename ResultType>
DataSubscriber<DataType, ResultType>::~DataSubscriber()
{
    this->Destroy();
}

template <typename DataType, typename ResultType>
bool DataSubscriber<DataType, ResultType>::Initialize(
    const char* sharedMemoryName)
{
    if (!sharedMemoryName) {
        std::cerr << "Invalid shared memory name" << std::endl;
        return false;
    }

    this->mShmName = sharedMemoryName;

    /* Open Posix shared memory object */
    this->mShmFd = shm_open(this->mShmName,
                            O_RDWR,
                            S_IRUSR | S_IWUSR);

    if (this->mShmFd == -1) {
        std::cerr << "Error: shm_open() failed" << std::endl;
        return false;
    }

    /* Set the size of shared memory object */
    if (ftruncate(this->mShmFd, sizeof(SharedType)) == -1) {
        std::cerr << "Error: ftruncate() failed" << std::endl;
        return false;
    }

    /* Map shared memory object to memory */
    void* pShared = mmap(NULL,
                         sizeof(SharedType),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED,
                         this->mShmFd,
                         0);

    if (pShared == MAP_FAILED) {
        std::cerr << "Error: mmap() failed" << std::endl;
        return false;
    }

    /* Set the pointer to shared memory */
    this->mpShared = reinterpret_cast<SharedPtrType>(pShared);

    return true;
}

template <typename DataType, typename ResultType>
void DataSubscriber<DataType, ResultType>::Destroy()
{
    /* Unmap shared memory */
    if (this->mpShared != NULL)
        munmap(this->mpShared, sizeof(SharedType));

    /* Close Posix shared memory object */
    if (this->mShmFd != -1)
        close(this->mShmFd);

    /* Unlink Posix shared memory object */
    if (this->mShmName != NULL)
        shm_unlink(this->mShmName);

    this->mpShared = NULL;
    this->mShmName = NULL;
    this->mShmFd = -1;
}

template <typename DataType, typename ResultType>
bool DataSubscriber<DataType, ResultType>::Subscribe()
{
    pthread_mutex_lock(&this->mpShared->mMutex);

    /* Wait for publisher to get ready */
    while (this->mpShared->mState != ShmCommState::Published &&
           this->mpShared->mPublisherActive)
        pthread_cond_wait(&this->mpShared->mCondPublisherReady,
                          &this->mpShared->mMutex);

    if (this->mpShared->mState == ShmCommState::Published) {
        /* Data object from publisher is stored in this->mpShared->mData */
        pthread_mutex_unlock(&this->mpShared->mMutex);
        return true;
    }

    /* Exit if publisher is not active anymore */
    this->mpShared->mSubscriberActive = false;

    /* Notify publisher that subscriber is now inactive */
    pthread_cond_signal(&this->mpShared->mCondSubscriberDone);

    pthread_mutex_unlock(&this->mpShared->mMutex);

    return false;
}

template <typename DataType, typename ResultType>
void DataSubscriber<DataType, ResultType>::SendResult(ResultType& resultData)
{
    pthread_mutex_lock(&this->mpShared->mMutex);

    if (this->mpShared->mState != ShmCommState::Published) {
        pthread_mutex_unlock(&this->mpShared->mMutex);
        return;
    }
        
    /* Pass result to publisher */
    this->mpShared->mResult = resultData;

    /* Update the current state */
    this->mpShared->mState = ShmCommState::Subscribed;

    /* Notify publisher that subscriber received data object */
    pthread_cond_signal(&this->mpShared->mCondSubscribed);
    
    /* Wait for publisher to check result */
    while (this->mpShared->mState != ShmCommState::GotResult)
        pthread_cond_wait(&this->mpShared->mCondGotResult,
                          &this->mpShared->mMutex);
    
    /* Update the current state */
    this->mpShared->mState = ShmCommState::Init;

    /* Notify publisher that subscriber is ready */
    pthread_cond_signal(&this->mpShared->mCondSubscriberReady);

    pthread_mutex_unlock(&this->mpShared->mMutex);
}

#endif /* SHM_COMM_H */

