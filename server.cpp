
/* server.cpp */

#include <cstdlib>
#include <cstring>
#include <iostream>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <pthread.h>

#include "data.h"

int main(int argc, char** argv)
{
    /* 共有メモリの名前 */
    const char* shared_memory_name = "/test";

    /* 共有メモリオブジェクトを作成 */
    int fd = shm_open(shared_memory_name,
                      O_RDWR | O_CREAT,
                      S_IRUSR | S_IWUSR);

    if (fd == -1) {
        std::cerr << "Error: shm_open() failed" << std::endl;
        return EXIT_FAILURE;
    }

    /* 必要なサイズを確保 */
    if (ftruncate(fd, sizeof(shared_data)) == -1) {
        std::cerr << "Error: ftruncate() failed" << std::endl;
        return EXIT_FAILURE;
    }

    /* ミューテックスのための共有メモリを作成 */
    void* shared = mmap(nullptr,
                        sizeof(shared_data),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED,
                        fd,
                        0);
    
    if (shared == MAP_FAILED) {
        std::cerr << "Error: mmap() failed" << std::endl;
        return EXIT_FAILURE;
    }

    shared_data* shared_data_ptr = reinterpret_cast<shared_data*>(shared);

    /* ミューテックスの初期化 */
    pthread_mutexattr_t mutexattr;
    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_data_ptr->mutex, &mutexattr);

    /* 条件変数の初期化 */
    pthread_condattr_t condattr;
    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&shared_data_ptr->server_ready, &condattr);
    pthread_cond_init(&shared_data_ptr->client_ready, &condattr);
    pthread_cond_init(&shared_data_ptr->client_processed, &condattr);
    pthread_cond_init(&shared_data_ptr->server_received, &condattr);
    pthread_cond_init(&shared_data_ptr->client_done, &condattr);

    /* フラグの初期化 */
    shared_data_ptr->flag = State::Init;
    shared_data_ptr->server_running = 1;
    shared_data_ptr->client_running = 1;
    shared_data_ptr->data = 0;

    /* 生産者の動作 */
    for (int i = 0; i < 5; ++i) {
        pthread_mutex_lock(&shared_data_ptr->mutex);

        /* クライアントの準備完了を待機 */
        while (shared_data_ptr->flag != State::Init)
            pthread_cond_wait(&shared_data_ptr->client_ready,
                              &shared_data_ptr->mutex);

        /* クライアントに渡すデータを生成 */
        shared_data_ptr->flag = State::ServerSent;
        shared_data_ptr->data = (i + 1) * 13;
        std::cerr << "Producer: sent: "
                  << shared_data_ptr->data << std::endl;

        /* クライアントにデータを渡す */
        pthread_cond_signal(&shared_data_ptr->server_ready);
        pthread_mutex_unlock(&shared_data_ptr->mutex);

        pthread_mutex_lock(&shared_data_ptr->mutex);
        
        /* クライアントのデータ処理待ち */
        while (shared_data_ptr->flag != State::ClientProcessed)
            pthread_cond_wait(&shared_data_ptr->client_processed,
                              &shared_data_ptr->mutex);
        
        shared_data_ptr->flag = State::ServerReceived;
        std::cerr << "Producer: received: "
                  << shared_data_ptr->data << std::endl;
        
        pthread_cond_signal(&shared_data_ptr->server_received);
        pthread_mutex_unlock(&shared_data_ptr->mutex);
    }

    /* 待機 */
    sleep(2);

    /* クライアントに終了を通知 */
    pthread_mutex_lock(&shared_data_ptr->mutex);
    shared_data_ptr->server_running = 0;
    pthread_mutex_unlock(&shared_data_ptr->mutex);
    
    /* クライアントに終了を確実に伝える */
    pthread_cond_signal(&shared_data_ptr->server_ready);

    /* クライアントの終了を待機 */
    pthread_mutex_lock(&shared_data_ptr->mutex);

    /* クライアントの終了を待機 */
    while (shared_data_ptr->client_running)
        pthread_cond_wait(&shared_data_ptr->client_done,
                          &shared_data_ptr->mutex);
    
    pthread_mutex_unlock(&shared_data_ptr->mutex);

    /* 条件変数の破棄 */
    if (pthread_cond_destroy(&shared_data_ptr->client_done) != 0) {
        std::cerr << "Error: pthread_cond_destroy() failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (pthread_cond_destroy(&shared_data_ptr->server_received) != 0) {
        std::cerr << "Error: pthread_cond_destroy() failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (pthread_cond_destroy(&shared_data_ptr->client_processed) != 0) {
        std::cerr << "Error: pthread_cond_destroy() failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (pthread_cond_destroy(&shared_data_ptr->client_ready) != 0) {
        std::cerr << "Error: pthread_cond_destroy() failed" << std::endl;
        return EXIT_FAILURE;
    }

    if (pthread_cond_destroy(&shared_data_ptr->server_ready) != 0) {
        std::cerr << "Error: pthread_cond_destroy() failed" << std::endl;
        return EXIT_FAILURE;
    }

    /* ミューテックスの破棄 */
    if (pthread_mutex_destroy(&shared_data_ptr->mutex) != 0) {
        std::cerr << "Error: pthread_mutex_destroy() failed" << std::endl;
        return EXIT_FAILURE;
    }

    /* 共有メモリのアンマップ */
    if (munmap(shared, sizeof(shared_data)) == -1) {
        std::cerr << "Error: munmap() failed" << std::endl;
        return EXIT_FAILURE;
    }

    shared = nullptr;
    shared_data_ptr = nullptr;

    /* 共有メモリオブジェクトの破棄 */
    if (close(fd) == -1) {
        std::cerr << "Error: close() failed" << std::endl;
        return EXIT_FAILURE;
    }

    /* 共有メモリオブジェクトの破棄 */
    int ret = shm_unlink(shared_memory_name);

    if (ret == -1 && errno != ENOENT) {
        std::cerr << "Error: shm_unlink() failed" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
