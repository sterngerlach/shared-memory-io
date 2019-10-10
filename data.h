
/* data.h */

#ifndef DATA_H
#define DATA_H

#include <pthread.h>

struct shared_data {
    int                 flag;
    int                 server_running;
    int                 client_running;
    int                 data;
    pthread_mutex_t     mutex;
    pthread_cond_t      server_ready;
    pthread_cond_t      client_ready;
    pthread_cond_t      client_processed;
    pthread_cond_t      server_received;
    pthread_cond_t      client_done;
};

enum State {
    Init = 0,
    ServerSent = 1,
    ClientProcessed = 2,
    ServerReceived = 3,
};

#endif /* DATA_H */
