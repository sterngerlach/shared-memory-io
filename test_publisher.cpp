
/* test_publisher.cpp */

#include <cstdlib>
#include <iostream>

#include "shm_comm.h"

int main(int argc, char** argv)
{
    const char* sharedMemoryName = "/test";
    DataPublisher<int, double> dataPub;

    if (!dataPub.Initialize(sharedMemoryName)) {
        std::cerr << "Publisher: initialization failed"
                  << std::endl;
        return EXIT_FAILURE;
    }

    std::cerr << "Publisher: started" << std::endl;

    for (int i = 0; i < 5; ++i) {
        int publishedData = i + 1;
        dataPub.Publish(publishedData);
        std::cerr << "Publisher: data published: "
                  << publishedData << std::endl;

        dataPub.WaitForResult();
        sleep(1);

        std::cerr << "Publisher: result received: "
                  << dataPub.GetResult() << std::endl;
        sleep(1);
    }

    sleep(2);

    dataPub.Stop();
    std::cerr << "Publisher: stopped" << std::endl;

    dataPub.Destroy();

    return EXIT_SUCCESS;
}

