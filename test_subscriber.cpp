
/* test_subscriber.cpp */

#include <cstdlib>
#include <iostream>

#include "shm_comm.h"

int main(int argc, char** argv)
{
    const char* sharedMemoryName = "/test";
    DataSubscriber<int, double> dataSub;

    if (!dataSub.Initialize(sharedMemoryName)) {
        std::cerr << "Subscriber: initialization failed"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cerr << "Subscriber: started" << std::endl;

    while (true) {
        bool dataPublished = dataSub.Subscribe();
        sleep(1);
        
        if (!dataPublished) {
            std::cerr << "Subscriber: exiting" << std::endl;
            break;
        }

        int receivedData = dataSub.GetData();
        std::cerr << "Subscriber: data received: "
                  << receivedData << std::endl;

        sleep(3);

        double resultData = static_cast<double>(receivedData) * 3.14;
        dataSub.SendResult(resultData);
        std::cerr << "Subscriber: result sent: "
                  << resultData << std::endl;
    }

    dataSub.Destroy();

    return EXIT_SUCCESS;
}

