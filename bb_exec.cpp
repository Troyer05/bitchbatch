#include "bb_exec.h"
#include "bb_util.h"
#include "bb_aliases.h"

#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <cctype>
#include <vector>
#include <signal.h>

static int g_lastStatus = 0;

int lastCmdStatus() {
    return g_lastStatus;
}

static std::string trim(const std::string& s) {
    size_t a = 0;

    while (a < s.size() && std::isspace((unsigned char)s[a])) a++;

    size_t b = s.size();

    while (b > a && std::isspace((unsigned char)s[b - 1])) b--;

    return s.substr(a, b - a);
}

static std::vector<std::string> splitArgs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;

    bool inQuote = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        if (c == '"') {
            inQuote = !inQuote;
            continue;
        }

        if (!inQuote && std::isspace((unsigned char)c)) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }

            continue;
        }

        cur.push_back(c);
    }

    if (!cur.empty()) out.push_back(cur);

    return out;
}

struct ChainCmd {
    std::string cmd;
    bool onlyOnSuccess;
};

static std::vector<ChainCmd> splitChain(const std::string& line) {
    std::vector<ChainCmd> out;
    std::string cur;
    
    bool inQuote = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        if (c == '"') {
            inQuote = !inQuote;
            cur.push_back(c);

            continue;
        }

        if (!inQuote) {
            if (c == ';') {
                out.push_back({trim(cur), false});
                cur.clear();

                continue;
            }

            if (c == '&' && i + 1 < line.size() && line[i + 1] == '&') {
                out.push_back({trim(cur), true});
                cur.clear();

                i++;

                continue;
            }
        }

        cur.push_back(c);
    }

    out.push_back({trim(cur), false});

    std::vector<ChainCmd> clean;
    clean.reserve(out.size());

    for (auto& x : out) {
        if (!x.cmd.empty()) clean.push_back(x);
    }

    return clean;
}

static std::vector<std::string> splitPipes(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;

    bool inQuote = false;

    for (size_t i = 0; i < line.size(); i++) {
        char c = line[i];

        if (c == '"') {
            inQuote = !inQuote;
            cur.push_back(c);
            continue;
        }

        if (!inQuote && c == '|') {
            out.push_back(trim(cur));
            cur.clear();
            continue;
        }

        cur.push_back(c);
    }

    out.push_back(trim(cur));

    std::vector<std::string> clean;

    for (auto& x : out) if (!x.empty()) clean.push_back(x);

    return clean;
}

static int cmdPipeArgs(const std::vector<std::vector<std::string>>& parts) {
    if (parts.empty()) return 0;

    std::vector<pid_t> pids;
    pids.reserve(parts.size());

    int prevRead = -1;

    for (size_t i = 0; i < parts.size(); i++) {
        int fd[2] = {-1, -1};

        bool last = (i + 1 == parts.size());

        if (!last) {
            if (pipe(fd) != 0) {
                std::cerr << "pipe failed: " << std::strerror(errno) << "\n";
                if (prevRead != -1) close(prevRead);
                return -1;
            }
        }

        pid_t pid = fork();

        if (pid == 0) {
            signal(SIGINT, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);

            if (prevRead != -1) {
                dup2(prevRead, STDIN_FILENO);
            }

            if (!last) {
                dup2(fd[1], STDOUT_FILENO);
            }

            if (prevRead != -1) close(prevRead);

            if (!last) {
                close(fd[0]);
                close(fd[1]);
            }

            std::vector<char*> argv;
            argv.reserve(parts[i].size() + 1);

            for (auto& t : parts[i]) argv.push_back(const_cast<char*>(t.c_str()));

            argv.push_back(nullptr);

            execvp(argv[0], argv.data());
            
            std::cerr << "exec failed: " << std::strerror(errno) << "\n";

            _exit(127);
        }

        if (pid < 0) {
            std::cerr << "fork failed: " << std::strerror(errno) << "\n";

            if (prevRead != -1) close(prevRead);

            if (!last) {
                close(fd[0]);
                close(fd[1]);
            }

            return -1;
        }

        pids.push_back(pid);

        if (prevRead != -1) close(prevRead);

        if (!last) {
            close(fd[1]);
            prevRead = fd[0];
        } else {
            prevRead = -1;
        }
    }

    int rc = 0;

    for (auto pid : pids) {
        int status = 0;

        for (;;) {
            if (waitpid(pid, &status, 0) < 0) {
                if (errno == EINTR) continue;
                std::cerr << "waitpid failed: " << std::strerror(errno) << "\n";
                return -1;
            }

            break;
        }

        if (WIFEXITED(status)) rc = WEXITSTATUS(status);
        else rc = -1;
    }

    return rc;
}

static int cmdPipe(const std::string& line) {
    auto segs = splitPipes(line);

    if (segs.empty()) return 0;

    (void)getAliases();

    std::vector<std::vector<std::string>> parts;
    parts.reserve(segs.size());

    for (auto& s : segs) {
        auto p = splitArgs(s);
        if (p.empty()) return -1;

        p = getAliases().expand_first_token(p);

        if (p.empty()) return -1;

        parts.push_back(std::move(p));
    }

    return cmdPipeArgs(parts);
}

int cmdArgs(const std::vector<std::string>& parts) {
    if (parts.empty()) {
        g_lastStatus = 0;
        return 0;
    }

    std::vector<char*> argv;
    argv.reserve(parts.size() + 1);

    for (auto& s : parts) argv.push_back(const_cast<char*>(s.c_str()));

    argv.push_back(nullptr);

    pid_t pid = fork();

    if (pid == 0) {
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);

        execvp(argv[0], argv.data());

        std::cerr << "exec failed: " << std::strerror(errno) << "\n";
        _exit(127);
    }

    if (pid < 0) {
        std::cerr << "fork failed: " << std::strerror(errno) << "\n";
        g_lastStatus = -1;
        return g_lastStatus;
    }

    int status = 0;

    for (;;) {
        if (waitpid(pid, &status, 0) < 0) {
            if (errno == EINTR) continue;
            std::cerr << "waitpid failed: " << std::strerror(errno) << "\n";
            g_lastStatus = -1;
            return g_lastStatus;
        }

        break;
    }

    if (WIFEXITED(status)) {
        g_lastStatus = WEXITSTATUS(status);
        return g_lastStatus;
    }

    g_lastStatus = -1;
    return g_lastStatus;
}

static int cmdOne(const std::string& line) {
    if (line.find('|') != std::string::npos) return cmdPipe(line);

    (void)getAliases();

    auto parts = splitArgs(line);

    if (parts.empty()) return 0;

    parts = getAliases().expand_first_token(parts);

    if (parts.empty()) return 0;

    return cmdArgs(parts);
}

static int cmdChain(const std::string& line) {
    auto chain = splitChain(line);
    int lastStatus = 0;

    for (auto& c : chain) {
        if (c.onlyOnSuccess && lastStatus != 0) break;
        lastStatus = cmdOne(c.cmd);
    }

    return lastStatus;
}

int cmd(const std::string& line) {
    if (line.find(';') != std::string::npos || line.find("&&") != std::string::npos) {
        g_lastStatus = cmdChain(line);
        return g_lastStatus;
    }

    g_lastStatus = cmdOne(trim(line));
    return g_lastStatus;
}