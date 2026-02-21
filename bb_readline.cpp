#include "bb_readline.h"
#include "bb_util.h"

#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <algorithm>
#include <limits.h>
#include <cstdlib>
#include <iostream>

struct TermRaw {
    termios old{};
    bool ok = false;

    TermRaw() {
        if (tcgetattr(STDIN_FILENO, &old) == 0) {
            termios raw = old;

            raw.c_lflag &= ~(ICANON | ECHO);
            raw.c_cc[VMIN]  = 1;
            raw.c_cc[VTIME] = 0;

            ok = (tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0);
        }
    }

    ~TermRaw() {
        if (ok) tcsetattr(STDIN_FILENO, TCSANOW, &old);
    }
};

static bool readByte(unsigned char& c) {
    return read(STDIN_FILENO, &c, 1) == 1;
}

static std::vector<std::string> completeCommands(const std::string& prefix, const CommandMap& commands) {
    std::vector<std::string> out;
    out.reserve(commands.size());

    for (auto& kv : commands) {
        if (startsWith(kv.first, prefix)) out.push_back(kv.first);
    }

    std::sort(out.begin(), out.end());

    return out;
}

static std::vector<std::string> completeFiles(const std::string& token) {
    std::vector<std::string> out;

    std::string dir = ".";
    std::string pref = token;

    auto slash = token.find_last_of('/');

    if (slash != std::string::npos) {
        dir = token.substr(0, slash);
        if (dir.empty()) dir = "/";
        pref = token.substr(slash + 1);
    }

    std::string realDir = dir;

    if (realDir == "~") {
        const char* h = getenv("HOME");
        if (h && *h) realDir = h;
    } else if (startsWith(realDir, "~/")) {
        const char* h = getenv("HOME");
        if (h && *h) realDir = std::string(h) + realDir.substr(1);
    }

    DIR* dp = opendir(realDir.c_str());

    if (!dp) return out;

    while (auto* de = readdir(dp)) {
        std::string name = de->d_name;

        if (name == "." || name == "..") continue;
        if (!startsWith(name, pref)) continue;

        std::string full;

        if (slash != std::string::npos) full = token.substr(0, slash + 1) + name;
        else full = name;

        out.push_back(full);
    }

    closedir(dp);
    
    std::sort(out.begin(), out.end());

    return out;
}

static void redraw(const std::string& prompt, const std::string& buf, size_t cur) {
    std::cout << "\r\033[2K" << prompt << buf;

    size_t right = buf.size() - cur;
    
    if (right > 0) std::cout << "\033[" << right << "D";

    std::cout << std::flush;
}

static void printMatches(const std::vector<std::string>& m, const std::string& prompt, const std::string& buf, size_t cur) {
    std::cout << "\n";

    for (size_t i = 0; i < m.size(); i++) {
        std::cout << m[i];
        if (i + 1 < m.size()) std::cout << "  ";
    }

    std::cout << "\n";
    
    redraw(prompt, buf, cur);
}

std::string readLineNice(const std::string& prompt, const CommandMap& commands, std::vector<std::string>& history) {
    TermRaw tr;

    std::string buf;
    size_t cur = 0;

    long histPos = (long)history.size();
    std::string histSaved;

    std::cout << prompt << std::flush;

    while (true) {
        unsigned char c = 0;

        if (!readByte(c)) continue;

        if (c == '\n' || c == '\r') {
            std::cout << "\n";
            return buf;
        }

        if (c == 127 || c == 8) {
            if (cur > 0) {
                buf.erase(buf.begin() + (long)cur - 1);
                cur--;
                redraw(prompt, buf, cur);
            } else {
                std::cout << "\a" << std::flush;
            }

            continue;
        }

        if (c == 9) {
            std::string left = buf.substr(0, cur);
            std::string token;

            size_t pos = left.find_last_of(' ');

            if (pos == std::string::npos) token = left;
            else token = left.substr(pos + 1);

            std::vector<std::string> matches;

            bool firstToken = (pos == std::string::npos);

            if (firstToken && token.find('/') == std::string::npos && !startsWith(token, ".") && !startsWith(token, "~")) {
                matches = completeCommands(token, commands);
            } else {
                matches = completeFiles(token);
            }

            if (matches.empty()) {
                std::cout << "\a" << std::flush;
                continue;
            }

            if (matches.size() == 1) {
                std::string add = matches[0].substr(token.size());

                buf.insert(cur, add);
                cur += add.size();

                redraw(prompt, buf, cur);

                continue;
            }

            std::string cp = commonPrefix(matches);
            
            if (cp.size() > token.size()) {
                std::string add = cp.substr(token.size());

                buf.insert(cur, add);
                cur += add.size();

                redraw(prompt, buf, cur);
            } else {
                printMatches(matches, prompt, buf, cur);
            }

            continue;
        }

        if (c == 27) {
            unsigned char a = 0, b = 0;

            if (!readByte(a)) continue;
            if (!readByte(b)) continue;

            if (a == '[') {
                 if (b == '3') {
                    unsigned char t = 0;
                    
                    if (!readByte(t)) continue;

                    if (t == '~') {
                        if (cur < buf.size()) {
                            buf.erase(buf.begin() + (long)cur);
                            redraw(prompt, buf, cur);
                        } else {
                            std::cout << "\a" << std::flush;
                        }
                    }

                    continue;
                }

                if (b == 'A') {
                    if (!history.empty() && histPos > 0) {
                        if (histPos == (long)history.size()) histSaved = buf;

                        histPos--;
                        buf = history[(size_t)histPos];
                        cur = buf.size();

                        redraw(prompt, buf, cur);
                    } else {
                        std::cout << "\a" << std::flush;
                    }

                    continue;
                }

                if (b == 'B') {
                    if (histPos < (long)history.size()) {
                        histPos++;

                        if (histPos == (long)history.size()) buf = histSaved;
                        else buf = history[(size_t)histPos];

                        cur = buf.size();

                        redraw(prompt, buf, cur);
                    } else {
                        std::cout << "\a" << std::flush;
                    }

                    continue;
                }

                if (b == 'C') {
                    if (cur < buf.size()) {
                        cur++;
                        redraw(prompt, buf, cur);
                    } else {
                        std::cout << "\a" << std::flush;
                    }

                    continue;
                }

                if (b == 'D') {
                    if (cur > 0) {
                        cur--;
                        redraw(prompt, buf, cur);
                    } else {
                        std::cout << "\a" << std::flush;
                    }

                    continue;
                }

                if (b == 'H') {
                    cur = 0;
                    redraw(prompt, buf, cur);
                    continue;
                }

                if (b == 'F') {
                    cur = buf.size();
                    redraw(prompt, buf, cur);
                    continue;
                }
            }

            continue;
        }

        if (c >= 1 && c <= 26) {
            if (c == 1) {
                cur = 0;
                redraw(prompt, buf, cur);
                continue;
            }

            if (c == 5) {
                cur = buf.size();
                redraw(prompt, buf, cur);
                continue;
            }

            if (c == 11) {
                if (cur < buf.size()) {
                    buf.erase(cur);
                    redraw(prompt, buf, cur);
                }

                continue;
            }

            if (c == 21) {
                if (cur > 0) {
                    buf.erase(0, cur);
                    cur = 0;
                    redraw(prompt, buf, cur);
                }

                continue;
            }

            if (c == 23) {
                if (cur == 0) {
                    std::cout << "\a" << std::flush;
                    continue;
                }

                size_t i = cur;

                while (i > 0 && buf[i - 1] == ' ') i--;
                while (i > 0 && buf[i - 1] != ' ') i--;

                buf.erase(i, cur - i);
                cur = i;

                redraw(prompt, buf, cur);

                continue;
            }

            if (c == 12) {
                std::cout << "\033[2J\033[H" << std::flush;
                redraw(prompt, buf, cur);
                continue;
            }

            if (c == 3) {
                std::cout << "\n";

                buf.clear();
                cur = 0;
                histPos = (long)history.size();
                histSaved.clear();

                std::cout << prompt << std::flush;

                continue;
            }

            if (c == 4) {
                if (buf.empty()) {
                    std::cout << "\n";
                    exit(0);
                } else {
                    std::cout << "\a" << std::flush;
                }

                continue;
            }

            continue;
        }

        if (c >= 32 && c <= 126) {
            buf.insert(buf.begin() + (long)cur, (char)c);
            cur++;

            redraw(prompt, buf, cur);

            continue;
        }
    }
}
