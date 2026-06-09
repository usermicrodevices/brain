#include "GlobalStop.hpp"

GlobalStop::GlobalStop(int listenPort) : listenPort_(listenPort) {}

GlobalStop::~GlobalStop() {
    stop();
}

void GlobalStop::start() {
    listenerThread_ = std::thread(&GlobalStop::listenerLoop, this);
}

void GlobalStop::stop() {
    globalStop_ = true;
    if (serverFd_ >= 0) {
        shutdown(serverFd_, SHUT_RDWR);
        close(serverFd_);
        serverFd_ = -1;
    }
    if (listenerThread_.joinable()) listenerThread_.join();
}

bool GlobalStop::shouldStop() const {
    return globalStop_.load();
}

void GlobalStop::requestStop() {
    globalStop_ = true;
    std::ofstream out(".global_stop");
    out << "stopped\n";
    out.close();
}

void GlobalStop::registerWorker() {
    activeWorkers_++;
}

void GlobalStop::workerFinished() {
    int count = activeWorkers_--;
    if (count <= 1 && globalStop_) {
        removeFile();
    }
}

bool GlobalStop::allWorkersStopped() const {
    return activeWorkers_.load() <= 0;
}

bool GlobalStop::fileExists() {
    struct stat st;
    return stat(".global_stop", &st) == 0;
}

void GlobalStop::removeFile() {
    std::remove(".global_stop");
}

void GlobalStop::listenerLoop() {
    for (int attempt = 0; attempt < 5 && !globalStop_; ++attempt) {
        serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (serverFd_ < 0) {
            std::cerr << "[GlobalStop] Failed to create socket\n";
            return;
        }

        int opt = 1;
        setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        setsockopt(serverFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(listenPort_);

        if (bind(serverFd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "[GlobalStop] Failed to bind on port " << listenPort_
                      << " (attempt " << attempt + 1 << "/5)\n";
            close(serverFd_);
            serverFd_ = -1;
            sleep(1);
            continue;
        }

        if (listen(serverFd_, 5) < 0) {
            std::cerr << "[GlobalStop] Failed to listen\n";
            close(serverFd_);
            serverFd_ = -1;
            return;
        }

        break;
    }

    if (serverFd_ < 0) {
        std::cerr << "[GlobalStop] Could not bind after 5 attempts, TCP listener disabled\n";
        return;
    }

    std::cout << "[GlobalStop] Listening on port " << listenPort_ << "\n";

    while (!globalStop_) {
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        int clientFd = accept(serverFd_, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientFd < 0) continue;

        char buffer[256] = {0};
        int bytesRead = read(clientFd, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            std::string cmd(buffer, bytesRead);
            while (!cmd.empty() && (cmd.back() == '\n' || cmd.back() == '\r' || cmd.back() == ' '))
                cmd.pop_back();

            if (cmd == "STOP") {
                requestStop();
                const char* resp = "OK\n";
                write(clientFd, resp, strlen(resp));
            } else if (cmd == "STATUS") {
                std::string status = "{\"stopped\":" + std::string(globalStop_ ? "true" : "false") +
                                    ",\"active_workers\":" + std::to_string(activeWorkers_.load()) + "}\n";
                write(clientFd, status.c_str(), status.size());
            } else if (cmd == "PING") {
                const char* resp = "PONG\n";
                write(clientFd, resp, strlen(resp));
            }
        }
        close(clientFd);
    }
}
