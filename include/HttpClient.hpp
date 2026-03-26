#pragma once

#include <array>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <random>
#include <string>
#include <sstream>
#include <vector>
#include <unistd.h>

#include <nlohmann/json.hpp>

class HttpClient {
public:
    HttpClient();
    ~HttpClient();

    // Browser simulation options
    void setUserAgent(const std::string& ua);
    void setCookies(const std::string& cookieFile);   // Netscape/mozilla cookie file
    void setVerbose(bool verbose);
    void setTimeout(int seconds);                     // curl --max-time
    void setInsecure(bool insecure);                  // -k (skip SSL)

    // Add custom HTTP headers (e.g., "Content-Type: application/json")
    void addHeader(const std::string& header);

    // Send a POST request with JSON payload.
    // Returns the raw HTTP response body (as string).
    std::string postJson(const std::string& url, const std::string& jsonPayload);

    // Send a GET request and return the response body.
    std::string get(const std::string& url);

private:
    std::string userAgent_;
    std::string cookiesFile_;
    bool verbose_;
    int timeoutSec_;
    bool insecure_;
    std::vector<std::string> customHeaders_;

    // Helper: run curl command and capture output
    std::string runCurl(const std::string& cmd);
};
