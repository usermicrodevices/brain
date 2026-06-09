#pragma once

#include <atomic>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <cstring>

class GlobalStop {
public:
    GlobalStop(int listenPort = 9999);
    ~GlobalStop();

    void start();
    void stop();

    bool shouldStop() const;
    void requestStop();
    void registerWorker();
    void workerFinished();
    bool allWorkersStopped() const;

    static bool fileExists();
    static void removeFile();

private:
    std::atomic<bool> globalStop_{false};
    std::atomic<int> activeWorkers_{0};
    std::thread listenerThread_;
    int listenPort_;
    int serverFd_{-1};

    void listenerLoop();
};
