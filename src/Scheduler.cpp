#include "Scheduler.hpp"

// -----------------------------------------------------------------
// Cron parser – supports numbers and steps (e.g., */2)
// -----------------------------------------------------------------
struct CronSpec {
    std::vector<int> minutes;   // 0‑59
    std::vector<int> hours;     // 0‑23
    std::vector<int> days;      // 1‑31
    std::vector<int> months;    // 1‑12
    std::vector<int> weekdays;  // 0‑6 (0 = Sunday)

    bool wildcard_minutes;
    bool wildcard_hours;
    bool wildcard_days;
    bool wildcard_months;
    bool wildcard_weekdays;
};

static std::vector<int> parseField(const std::string& field, int low, int high) {
    std::vector<int> result;
    if (field == "*") {
        return {};
    }
    if (field.find("*/") == 0) {
        int step = std::stoi(field.substr(2));
        for (int v = low; v <= high; v += step) {
            result.push_back(v);
        }
        return result;
    }
    // single number
    result.push_back(std::stoi(field));
    return result;
}

static CronSpec parseCron(const std::string& cron) {
    CronSpec spec = { {}, {}, {}, {}, {}, false, false, false, false, false };
    std::istringstream iss(cron);
    std::string minute, hour, day, month, weekday;
    if (!(iss >> minute >> hour >> day >> month >> weekday)) {
        // fallback to midnight
        minute = "0";
        hour = "0";
        day = "*";
        month = "*";
        weekday = "*";
    }

    spec.minutes = parseField(minute, 0, 59);
    spec.hours = parseField(hour, 0, 23);
    spec.days = parseField(day, 1, 31);
    spec.months = parseField(month, 1, 12);
    spec.weekdays = parseField(weekday, 0, 6);

    spec.wildcard_minutes = (minute == "*");
    spec.wildcard_hours = (hour == "*");
    spec.wildcard_days = (day == "*");
    spec.wildcard_months = (month == "*");
    spec.wildcard_weekdays = (weekday == "*");

    return spec;
}

static bool matches(const CronSpec& spec, std::tm* tm) {
    if (!spec.wildcard_minutes) {
        bool ok = false;
        for (int m : spec.minutes) if (tm->tm_min == m) { ok = true; break; }
        if (!ok) return false;
    }
    if (!spec.wildcard_hours) {
        bool ok = false;
        for (int h : spec.hours) if (tm->tm_hour == h) { ok = true; break; }
        if (!ok) return false;
    }
    if (!spec.wildcard_days) {
        bool ok = false;
        for (int d : spec.days) if (tm->tm_mday == d) { ok = true; break; }
        if (!ok) return false;
    }
    if (!spec.wildcard_months) {
        bool ok = false;
        for (int m : spec.months) if (tm->tm_mon + 1 == m) { ok = true; break; }
        if (!ok) return false;
    }
    if (!spec.wildcard_weekdays) {
        bool ok = false;
        for (int w : spec.weekdays) if (tm->tm_wday == w) { ok = true; break; }
        if (!ok) return false;
    }
    return true;
}

static std::time_t nextCronTime(const std::string& cron, std::time_t from) {
    CronSpec spec = parseCron(cron);
    std::time_t t = from + 1;
    const std::time_t max = from + 365 * 24 * 3600;
    while (t < max) {
        std::tm* tm = std::localtime(&t);
        if (matches(spec, tm)) {
            return t;
        }
        ++t;
    }
    // fallback to tomorrow midnight
    std::tm* tm = std::localtime(&from);
    tm->tm_hour = 0;
    tm->tm_min = 0;
    tm->tm_sec = 0;
    tm->tm_mday += 1;
    return std::mktime(tm);
}

// -----------------------------------------------------------------
// Parse a relative delay string like "+5m", "+1h", "+90s", "+2h30m"
// Returns seconds, or -1 if not a valid relative string.
// -----------------------------------------------------------------
static int parseRelativeDelay(const std::string& s) {
    if (s.empty() || s[0] != '+') return -1;
    std::string rest = s.substr(1);
    if (rest.empty()) return -1;

    int total_sec = 0;
    size_t pos = 0;
    while (pos < rest.size()) {
        size_t num_start = pos;
        while (pos < rest.size() && std::isdigit(rest[pos])) ++pos;
        if (num_start == pos) return -1; // no number
        int val = std::stoi(rest.substr(num_start, pos - num_start));
        if (pos >= rest.size()) return -1;
        char unit = rest[pos];
        ++pos;
        switch (unit) {
            case 's': total_sec += val; break;
            case 'm': total_sec += val * 60; break;
            case 'h': total_sec += val * 3600; break;
            default: return -1;
        }
    }
    return total_sec;
}

// -----------------------------------------------------------------
// Scheduler implementation
// -----------------------------------------------------------------
Scheduler::Scheduler(const std::string& schedule)
    : running(true), schedule(schedule) {
    start();
}

Scheduler::~Scheduler() {
    stop();
}

void Scheduler::start() {
    if (worker.joinable()) return;
    worker = std::thread(&Scheduler::run, this);
}

void Scheduler::stop() {
    running = false;
    if (worker.joinable()) worker.join();
}

void Scheduler::run() {
    // Check if schedule is a relative time
    int relative_seconds = parseRelativeDelay(schedule);
    if (relative_seconds >= 0) {
        // One‑shot: wait the given seconds, then run once and exit
        for (int i = 0; i < relative_seconds && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running) return;

        std::cout << "\n[Git] One‑time execution after " << relative_seconds << " seconds.\n";
        if (Git::bestFolderExists()) {
            std::string message = "One‑time evolution update " + std::to_string(std::time(nullptr));
            if (Git::commitAndPush("develop", message)) {
                std::cout << "[Git] Commit successful. Creating pull request...\n";
                Git::createPullRequest("main", "develop");
            } else {
                std::cerr << "[Git] Commit failed.\n";
            }
        } else {
            std::cout << "[Git] No best folder to commit.\n";
        }
        return; // stop after one execution
    }

    // Otherwise, treat as a regular cron schedule (repeating)
    while (running) {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::time_t next = nextCronTime(schedule, t);
        auto wait_seconds = next - t;

        for (std::time_t i = 0; i < wait_seconds && running; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running) break;

        std::cout << "\n[Git] Scheduled execution (cron: " << schedule << ")\n";
        if (Git::bestFolderExists()) {
            std::string message = "Scheduled evolution update " + std::to_string(std::time(nullptr));
            if (Git::commitAndPush("develop", message)) {
                std::cout << "[Git] Commit successful. Creating pull request...\n";
                Git::createPullRequest("main", "develop");
            } else {
                std::cerr << "[Git] Commit failed.\n";
            }
        } else {
            std::cout << "[Git] No best folder to commit.\n";
        }
    }
}
