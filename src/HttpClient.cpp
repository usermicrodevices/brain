#include "HttpClient.hpp"

using json = nlohmann::json;

HttpClient::HttpClient()
    : userAgent_("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"),
      cookiesFile_(""),
      verbose_(false),
      timeoutSec_(30),
      insecure_(false) {}

HttpClient::~HttpClient() {}

void HttpClient::setUserAgent(const std::string& ua) { userAgent_ = ua; }
void HttpClient::setCookies(const std::string& cookieFile) { cookiesFile_ = cookieFile; }
void HttpClient::setVerbose(bool verbose) { verbose_ = verbose; }
void HttpClient::setTimeout(int seconds) { timeoutSec_ = seconds; }
void HttpClient::setInsecure(bool insecure) { insecure_ = insecure; }
void HttpClient::addHeader(const std::string& header) { customHeaders_.push_back(header); }

std::string HttpClient::postJson(const std::string& url, const std::string& jsonPayload) {
    std::ostringstream cmd;
    cmd << "curl -s -X POST";

    // Add headers
    cmd << " -H \"Content-Type: application/json\"";
    for (const auto& h : customHeaders_) {
        cmd << " -H \"" << h << "\"";
    }

    // User-Agent
    if (!userAgent_.empty())
        cmd << " -H \"User-Agent: " << userAgent_ << "\"";

    // Cookies
    if (!cookiesFile_.empty())
        cmd << " -b \"" << cookiesFile_ << "\"";

    // Timeout
    cmd << " --max-time " << timeoutSec_;

    // Insecure (skip SSL verify)
    if (insecure_)
        cmd << " -k";

    // Verbose output
    if (verbose_)
        cmd << " -v";

    // Write JSON payload to a temporary file
    std::string tmpFile = "/tmp/curl_payload_" + std::to_string(rand()) + ".json";
    std::ofstream out(tmpFile);
    if (!out) {
        std::cerr << "[HttpClient] Failed to create temp file\n";
        return "";
    }
    out << jsonPayload;
    out.close();

    cmd << " --data-binary @" << tmpFile << " \"" << url << "\"";
    std::string output = runCurl(cmd.str());
    std::remove(tmpFile.c_str());

    return output;
}

std::string HttpClient::get(const std::string& url) {
    std::ostringstream cmd;
    cmd << "curl -s -X GET";

    // Add headers
    for (const auto& h : customHeaders_) {
        cmd << " -H \"" << h << "\"";
    }

    if (!userAgent_.empty())
        cmd << " -H \"User-Agent: " << userAgent_ << "\"";

    if (!cookiesFile_.empty())
        cmd << " -b \"" << cookiesFile_ << "\"";

    cmd << " --max-time " << timeoutSec_;

    if (insecure_)
        cmd << " -k";

    if (verbose_)
        cmd << " -v";

    cmd << " \"" << url << "\"";
    return runCurl(cmd.str());
}

std::string HttpClient::runCurl(const std::string& cmd) {
    std::string result;
    std::array<char, 128> buffer;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[HttpClient] popen failed\n";
        return "";
    }
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    return result;
}
