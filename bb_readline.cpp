#include "bb_readline.h"
#include "bb_util.h"

#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <algorithm>
#include <limits.h>
#include <cstdlib>
#include <iostream>
#include <sys/stat.h>
#include <sys/ioctl.h>

static int termCols() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return (int)ws.ws_col;
    return 80;
}

static int visibleLenAnsi(const std::string& s) {
    int n = 0;

    for (size_t i = 0; i < s.size();) {
        unsigned char c = (unsigned char)s[i];

        if (c == 0x1B && i + 1 < s.size() && s[i + 1] == '[') {
            i += 2;

            while (i < s.size()) {
                unsigned char x = (unsigned char)s[i++];
                if (x >= 0x40 && x <= 0x7E) break;
            }

            continue;
        }

        n++;
        i++;
    }

    return n;
}

static std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;

    for (char ch : s) {
        if (ch == '\n') {
            out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(ch);
        }
    }

    out.push_back(cur);

    return out;
}

static void redraw(const std::string& prompt, const std::string& buf, size_t cur) {
    static int lastTotalLines = 0;
    const int cols = termCols();

    auto plines = splitLines(prompt);

    const int promptLines = (int)plines.size();
    const int promptLastVis = visibleLenAnsi(plines.empty() ? "" : plines.back());
    const int inputTotal = promptLastVis + (int)buf.size();
    const int inputLines = (inputTotal / cols) + 1;

    int totalLinesNow = (promptLines - 1) + inputLines;

    if (totalLinesNow < 1) totalLinesNow = 1;

    const int cursorPos = promptLastVis + (int)cur;
    const int curLine = cursorPos / cols;
    const int curCol  = cursorPos % cols;

    std::cout << "\r";

    if (lastTotalLines > 1) std::cout << "\033[" << (lastTotalLines - 1) << "A";

    for (int i = 0; i < lastTotalLines; i++) {
        std::cout << "\033[2K";
        if (i + 1 < lastTotalLines) std::cout << "\n";
    }

    if (lastTotalLines > 1) std::cout << "\033[" << (lastTotalLines - 1) << "A";

    std::cout << "\r" << prompt << buf;

    const int endPos  = promptLastVis + (int)buf.size();
    const int endLine = endPos / cols;
    const int endCol  = endPos % cols;
    const int up = endLine - curLine;

    if (up > 0) std::cout << "\033[" << up << "A";

    std::cout << "\r";

    if (curCol > 0) std::cout << "\033[" << curCol << "C";

    std::cout << std::flush;

    lastTotalLines = totalLinesNow;
}


static std::string expandTilde(const std::string& p) {
    if (p == "~") {
        const char* h = getenv("HOME");
        return (h && *h) ? std::string(h) : p;
    }

    if (p.size() >= 2 && p[0] == '~' && p[1] == '/') {
        const char* h = getenv("HOME");
        if (h && *h) return std::string(h) + p.substr(1);
    }

    return p;
}

static bool isDirPath(const std::string& p) {
    struct stat st{};
    if (stat(p.c_str(), &st) != 0) return false;
    return S_ISDIR(st.st_mode);
}


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


static void printMatches(const std::vector<std::string>& m,
                         const std::string& prompt,
                         const std::string& buf,
                         size_t cur)
{
    std::cout << "\n";

    for (size_t i = 0; i < m.size(); i++) {
        std::cout << m[i];
        if (i + 1 < m.size()) std::cout << "  ";
    }

    std::cout << "\n";
    redraw(prompt, buf, cur);
}

static int selectFromList(const std::vector<std::string>& items, int maxMenu = 8) {
    if (items.empty()) return -1;

    int total = (int)items.size();
    int view = total > maxMenu ? maxMenu : total;

    bool more = total > view;

    int lines = view + (more ? 1 : 0);
    int sel = 0;
    int top = 0;

    auto up = [&](int n) { if (n > 0) std::cout << "\033[" << n << "A"; };
    auto clearLine = [&]() { std::cout << "\r\033[2K"; };

    auto clearMenu = [&]() {
        up(lines);

        for (int i = 0; i < lines; i++) {
            clearLine();
            if (i + 1 < lines) std::cout << "\n";
        }

        up(lines - 1);
        std::cout << std::flush;
    };

    auto drawMenu = [&]() {
        up(lines);

        for (int i = 0; i < view; i++) {
            int idx = top + i;

            clearLine();

            if (idx == sel) std::cout << "\033[7m";

            std::cout << items[idx];

            if (idx == sel) std::cout << "\033[0m";

            std::cout << "\n";
        }

        if (more) {
            clearLine();
            int hidden = total - view;
            std::cout << "... (+" << hidden << ")\n";
        }

        up(lines - 1);
        std::cout << std::flush;
    };

    std::cout << "\n";

    for (int i = 0; i < lines; i++) {
        clearLine();
        if (i + 1 < lines) std::cout << "\n";
    }

    up(lines - 1);

    std::cout << std::flush;

    drawMenu();

    while (true) {
        unsigned char c = 0;

        if (!readByte(c)) continue;

        if (c == '\n' || c == '\r') {
            clearMenu();
            return sel;
        }

        if (c == 3) {
            clearMenu();
            return -1;
        }

        if (c == 27) {
            unsigned char a = 0, b = 0;

            if (!readByte(a) || !readByte(b)) {
                clearMenu();
                return -1;
            }

            if (a == '[') {
                if (b == 'A') {
                    if (sel > 0) sel--;
                    if (sel < top) top = sel;
                    if (sel >= top + view) top = sel - (view - 1);

                    drawMenu();

                    continue;
                }

                if (b == 'B') {
                    if (sel < total - 1) sel++;
                    if (sel < top) top = sel;
                    if (sel >= top + view) top = sel - (view - 1);

                    drawMenu();

                    continue;
                }
            }

            clearMenu();
            return -1;
        }
    }
}


std::string readLineNice(const std::string& prompt, const CommandMap& commands, std::vector<std::string>& history) {
    TermRaw tr;

    std::string buf;
    std::string histSaved;

    size_t cur = 0;

    long histPos = (long)history.size();
    bool tabPending = false;

    redraw(prompt, buf, cur);

    while (true) {
        unsigned char c = 0;

        if (!readByte(c)) continue;

        if (c != 9) tabPending = false;

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

            if (firstToken &&
                token.find('/') == std::string::npos &&
                !startsWith(token, ".") &&
                !startsWith(token, "~"))
            {
                matches = completeCommands(token, commands);
            } else {
                matches = completeFiles(token);
            }

            if (matches.empty()) {
                std::cout << "\a" << std::flush;
                tabPending = false;
                continue;
            }

            if (matches.size() > 1) {
                int idx = selectFromList(matches);

                if (idx >= 0) {
                    std::string m = matches[idx];
                    std::string add = m.substr(token.size());

                    buf.insert(cur, add);
                    cur += add.size();

                    std::string real = expandTilde(m);

                    if (isDirPath(real) && cur > 0 && buf[cur - 1] != '/') {
                        buf.insert(cur, "/");
                        cur++;
                    }

                    redraw(prompt, buf, cur);
                } else {
                    redraw(prompt, buf, cur);
                }

                continue;
            }

            std::string cp = commonPrefix(matches);

            if (cp.size() > token.size()) {
                std::string add = cp.substr(token.size());

                buf.insert(cur, add);
                cur += add.size();

                std::string real = expandTilde(cp);

                if (isDirPath(real) && cur > 0 && buf[cur - 1] != '/') {
                    buf.insert(cur, "/");
                    cur++;
                }

                redraw(prompt, buf, cur);

                tabPending = false;

                continue;
            }

            if (tabPending) {
                printMatches(matches, prompt, buf, cur);
                tabPending = false;
            } else {
                std::cout << "\a" << std::flush;
                tabPending = true;
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

                if (b == '1' || b == '4' || b == '7' || b == '8') {
                    unsigned char t = 0;

                    if (!readByte(t)) continue;

                    if (t == '~') {
                        if (b == '1' || b == '7') cur = 0;
                        else cur = buf.size();

                        redraw(prompt, buf, cur);
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

                continue;
            }

            if (a == 'O') {
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

                redraw(prompt, buf, cur);

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
