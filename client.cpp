
/* client.cpp */

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

    /* 共有メモリオブジェクトの作成 */
    int fd = shm_open(shared_memory_name,
                      O_RDWR,
                      S_IRUSR | S_IWUSR);
    
    /* 先にサーバを起動しないと失敗する */
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

    /* 共有データを取得 */
    shared_data* shared_data_ptr = reinterpret_cast<shared_data*>(shared);

    /* 消費者の動作 */
    while (true) {
        pthread_mutex_lock(&shared_data_ptr->mutex);

        /* データがサーバから届くのを待機 */
        while (shared_data_ptr->flag != State::ServerSent &&
               shared_data_ptr->server_running)
            pthread_cond_wait(&shared_data_ptr->server_ready,
                              &shared_data_ptr->mutex);
        
        /* データを処理 */
        if (shared_data_ptr->flag == State::ServerSent) {
            shared_data_ptr->flag = State::ClientProcessed;
            std::cerr << "Consumer: received: "
                      << shared_data_ptr->data << std::endl;

            /* データの処理 */
            shared_data_ptr->data += 1;
            std::cerr << "Consumer: data processed: "
                      << shared_data_ptr->data << std::endl;

            /* データの処理完了をサーバに通知 */
            pthread_cond_signal(&shared_data_ptr->client_processed);
            pthread_mutex_unlock(&shared_data_ptr->mutex);
        } else {
            /* 終了が通知されたらループを抜ける */
            std::cerr << "Consumer: exiting" << std::endl;
            shared_data_ptr->client_running = 0;
            pthread_cond_signal(&shared_data_ptr->client_done);
            pthread_mutex_unlock(&shared_data_ptr->mutex);
            break;
        }

        pthread_mutex_lock(&shared_data_ptr->mutex);

        /* サーバのデータ確認待ち */
        while (shared_data_ptr->flag != State::ServerReceived)
            pthread_cond_wait(&shared_data_ptr->server_received,
                              &shared_data_ptr->mutex);

        shared_data_ptr->flag = State::Init;

        /* クライアントの準備完了をサーバに通知 */
        pthread_cond_signal(&shared_data_ptr->client_ready);
        pthread_mutex_unlock(&shared_data_ptr->mutex);
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
