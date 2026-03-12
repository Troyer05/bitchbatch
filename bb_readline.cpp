#include "bb_readline.h"
#include "bb_util.h"
#include "history.h"

#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#include <algorithm>
#include <limits.h>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <cwchar>
#include <clocale>

static bool readByteTimeout(unsigned char& c, int timeoutMs) {
    fd_set set;

    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);

    timeval tv{};

    tv.tv_sec  = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    int r = select(STDIN_FILENO + 1, &set, nullptr, nullptr, &tv);

    if (r > 0) return read(STDIN_FILENO, &c, 1) == 1;

    return false;
}

static int termCols() {
    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0) return (int)ws.ws_col;
    return 80;
}

static int widthUtf8Prefix(const std::string& s, size_t bytes) {
    std::mbstate_t st{};

    int w = 0;
    const char* p = s.data();

    size_t n = std::min(bytes, s.size());
    size_t i = 0;

    while (i < n) {
        wchar_t wc{};
        size_t r = std::mbrtowc(&wc, p + i, n - i, &st);

        if (r == (size_t)-2) break;

        if (r == (size_t)-1) {
            std::memset(&st, 0, sizeof(st));

            w += 1;
            i += 1;

            continue;
        }

        if (r == 0) break;

        int cw = ::wcwidth(wc);

        if (cw < 0) cw = 1;

        w += cw;
        i += r;
    }

    return w;
}

static int widthAnsi(const std::string& s) {
    int w = 0;

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

        size_t start = i;

        while (i < s.size()) {
            unsigned char x = (unsigned char)s[i];
            if (x == 0x1B && i + 1 < s.size() && s[i + 1] == '[') break;
            i++;
        }

        w += widthUtf8Prefix(s.substr(start, i - start), i - start);
    }

    return w;
}


struct VisualPos {
    int lines = 1;
    int lastCol = 0;
};

static VisualPos measureMixed(const std::string& prompt, const std::string& buf, size_t bufBytes) {
    const int cols = std::max(1, termCols());

    VisualPos out;

    auto advance = [&](int w) {
        int rem = w;

        while (rem > 0) {
            int space = cols - out.lastCol;

            if (space <= 0) {
                out.lines++;
                out.lastCol = 0;
                space = cols;
            }

            if (rem < space) {
                out.lastCol += rem;
                rem = 0;
            } else {
                rem -= space;
                out.lines++;
                out.lastCol = 0;
            }
        }
    };

    auto consume = [&](const std::string& s, bool ansiAware) {
        for (size_t i = 0; i < s.size();) {
            unsigned char c = (unsigned char)s[i];

            if (c == '\n') {
                out.lines++;
                out.lastCol = 0;
                i++;

                continue;
            }

            if (c == '\r') {
                out.lastCol = 0;
                i++;

                continue;
            }

            if (ansiAware && c == 0x1B && i + 1 < s.size() && s[i + 1] == '[') {
                i += 2;

                while (i < s.size()) {
                    unsigned char x = (unsigned char)s[i++];
                    if (x >= 0x40 && x <= 0x7E) break;
                }

                continue;
            }

            size_t start = i;

            while (i < s.size()) {
                unsigned char x = (unsigned char)s[i];
                if (x == '\n' || x == '\r') break;
                if (ansiAware && x == 0x1B && i + 1 < s.size() && s[i + 1] == '[') break;
                i++;
            }

            int w = widthUtf8Prefix(s.substr(start, i - start), i - start);

            advance(w);
        }
    };

    consume(prompt, true);
    consume(buf.substr(0, std::min(bufBytes, buf.size())), false);

    return out;
}

static void redraw(const std::string& prompt, const std::string& buf, size_t cur) {
    static int lastTotalLines = 0;

    VisualPos endPos = measureMixed(prompt, buf, buf.size());
    VisualPos curPos = measureMixed(prompt, buf, cur);

    int totalLinesNow = std::max(1, endPos.lines);

    std::cout << "\r";

    if (lastTotalLines > 1) std::cout << "\033[" << (lastTotalLines - 1) << "A";

    for (int i = 0; i < lastTotalLines; i++) {
        std::cout << "\033[2K";
        if (i + 1 < lastTotalLines) std::cout << "\n";
    }

    if (lastTotalLines > 1) std::cout << "\033[" << (lastTotalLines - 1) << "A";

    std::cout << "\r" << prompt << buf;

    int up = endPos.lines - curPos.lines;

    if (up > 0) std::cout << "\033[" << up << "A";

    std::cout << "\r";

    if (curPos.lastCol > 0) std::cout << "\033[" << curPos.lastCol << "C";

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

            if (ok) std::cout << "\033[?2004h" << std::flush;
        }
    }

    ~TermRaw() {
        if (ok) {
            std::cout << "\033[?2004l" << std::flush;
            tcsetattr(STDIN_FILENO, TCSANOW, &old);
        }
    }
};

static bool readByte(unsigned char& c) {
    return read(STDIN_FILENO, &c, 1) == 1;
}

static std::string normalizePaste(const std::string& s) {
    std::string out;
    out.reserve(s.size());

    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];

        if (c == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') continue;
            out.push_back('\n');
            continue;
        }

        out.push_back(c);
    }

    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();

    return out;
}

static bool readBracketedPaste(std::string& out) {
    std::string raw;

    while (true) {
        unsigned char c = 0;

        if (!readByte(c)) return false;

        if (c == 0x1B) {
            unsigned char a = 0;
            unsigned char b = 0;
            unsigned char d = 0;
            unsigned char e = 0;
            unsigned char f = 0;

            if (!readByteTimeout(a, 10)) {
                raw.push_back((char)c);
                continue;
            }

            if (!readByteTimeout(b, 10)) {
                raw.push_back((char)c);
                raw.push_back((char)a);

                continue;
            }

            if (a == '[' && b == '2') {
                if (!readByteTimeout(d, 10)) {
                    raw.push_back((char)c);
                    raw.push_back((char)a);
                    raw.push_back((char)b);

                    continue;
                }

                if (!readByteTimeout(e, 10)) {
                    raw.push_back((char)c);
                    raw.push_back((char)a);
                    raw.push_back((char)b);
                    raw.push_back((char)d);

                    continue;
                }

                if (!readByteTimeout(f, 10)) {
                    raw.push_back((char)c);
                    raw.push_back((char)a);
                    raw.push_back((char)b);
                    raw.push_back((char)d);
                    raw.push_back((char)e);

                    continue;
                }

                if (d == '0' && e == '1' && f == '~') break;

                raw.push_back((char)c);
                raw.push_back((char)a);
                raw.push_back((char)b);
                raw.push_back((char)d);
                raw.push_back((char)e);
                raw.push_back((char)f);

                continue;
            }

            raw.push_back((char)c);
            raw.push_back((char)a);
            raw.push_back((char)b);

            continue;
        }

        raw.push_back((char)c);
    }

    out = normalizePaste(raw);

    return true;
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
    struct Item {
        std::string shown;
        std::string name;

        bool isDir = false;
    };

    std::vector<Item> items;

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

    if (!dp) return {};

    while (auto* de = readdir(dp)) {
        std::string name = de->d_name;

        if (name == "." || name == "..") continue;
        if (!startsWith(name, pref)) continue;

        std::string shown;

        if (slash != std::string::npos) shown = token.substr(0, slash + 1) + name;
        else shown = name;

        bool isDir = false;

        if (de->d_type == DT_DIR) {
            isDir = true;
        } else {
            std::string fullReal = realDir;

            if (!fullReal.empty() && fullReal.back() != '/') fullReal += "/";

            fullReal += name;

            struct stat st{};
            
            if (stat(fullReal.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                isDir = true;
            }
        }

        items.push_back({shown, name, isDir});
    }

    closedir(dp);

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });

    items.erase(std::unique(items.begin(), items.end(),
        [](const Item& a, const Item& b){ return a.shown == b.shown; }
    ), items.end());

    std::vector<std::string> out;
    out.reserve(items.size());

    for (auto& it : items) {
        if (it.isDir) {
            if (!it.shown.empty() && it.shown.back() != '/') it.shown.push_back('/');
        }

        out.push_back(it.shown);
    }

    return out;
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

static int selectFromList(std::vector<std::string> items, int maxMenu = 8) {
    if (items.empty()) return -1;

    const int total = (int)items.size();
    const int view  = std::min(maxMenu, total);

    int sel = 0;
    int top = 0;

    const int lines = view + 1;

    auto clearLine = [&]() { std::cout << "\r\033[2K"; };

    auto clampWindow = [&]() {
        if (sel < 0) sel = 0;
        if (sel > total - 1) sel = total - 1;
        if (sel < top) top = sel;
        if (sel >= top + view) top = sel - (view - 1);

        int topMax = std::max(0, total - view);

        if (top < 0) top = 0;
        if (top > topMax) top = topMax;
    };

    auto anchorSave = [&]() { std::cout << "\033[s" << std::flush; };
    auto anchorRestore = [&]() { std::cout << "\033[u" << std::flush; };

    anchorSave();

    std::cout << "\n";

    for (int i = 0; i < lines; i++) {
        clearLine();
        if (i + 1 < lines) std::cout << "\n";
    }

    std::cout << "\033[" << (lines - 1) << "A\r" << std::flush;

    auto drawMenu = [&]() {
        clampWindow();

        for (int i = 0; i < view; i++) {
            int idx = top + i;

            clearLine();

            if (idx == sel) {
                std::cout << "\033[48;5;238m\033[38;5;15m";
                std::cout << " " << items[idx] << " ";
                std::cout << "\033[0m";
            } else {
                std::cout << " " << items[idx];
            }

            std::cout << "\n";
        }

        clearLine();

        int remainingAbove = top;
        int remainingBelow = total - (top + view);

        if (remainingAbove > 0 || remainingBelow > 0) {
            std::cout << "\033[2m... ";

            if (remainingAbove > 0) std::cout << "(+" << remainingAbove << " oben) ";
            if (remainingBelow > 0) std::cout << "(+" << remainingBelow << " unten)";

            std::cout << "\033[0m";
        }

        std::cout << std::flush;
        std::cout << "\033[" << (lines - 1) << "A\r" << std::flush;
    };

    auto clearMenu = [&]() {
        for (int i = 0; i < lines; i++) {
            clearLine();
            std::cout << "\n";
        }

        anchorRestore();

        std::cout << "\r" << std::flush;
    };

    drawMenu();

    while (true) {
        unsigned char c = 0;
        if (!readByte(c)) continue;

        if (c == '\n' || c == '\r') {
            clearMenu();
            return sel;
        }

        if (c == 'q' || c == 'Q') {
            clearMenu();
            return -1;
        }

        if (c == 3) {
            clearMenu();
            return -1;
        }

        if (c == 27) {
            unsigned char a = 0;

            if (!readByteTimeout(a, 30)) {
                clearMenu();
                return -1;
            }

            if (a == '[') {
                unsigned char b = 0;
                if (!readByteTimeout(b, 30)) {
                    clearMenu();
                    return -1;
                }

                if (b == 'A') {
                    sel--;
                    drawMenu();
                    continue;
                }

                if (b == 'B') {
                    sel++;
                    drawMenu();
                    continue;
                }

                if (b == '5' || b == '6') {
                    unsigned char t = 0;

                    if (readByteTimeout(t, 30) && t == '~') {
                        if (b == '5') sel -= view;
                        else sel += view;

                        drawMenu();

                        continue;
                    }
                }
            }

            clearMenu();

            return -1;
        }
    }
}

std::string readLineNice(const std::string& prompt, const CommandMap& commands, std::vector<std::string>& hist) {
    TermRaw tr;

    std::string buf;
    std::string histSaved;

    size_t cur = 0;

    long histPos = (long)hist.size();
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
                    
                    if (!real.empty() && real.back() == '/') real.pop_back();

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
            unsigned char a = 0;
            unsigned char b = 0;
            unsigned char d = 0;
            unsigned char e = 0;

            if (!readByteTimeout(a, 10)) continue;
            if (!readByteTimeout(b, 10)) continue;

            if (a == '[' && b == '2') {
                if (!readByteTimeout(d, 10)) continue;
                if (!readByteTimeout(e, 10)) continue;

                if (d == '0' && e == '0') {
                    unsigned char f = 0;
                    
                    if (!readByteTimeout(f, 10)) continue;

                    if (f == '~') {
                        std::string pasted;

                        if (readBracketedPaste(pasted) && !pasted.empty()) {
                            buf.insert(cur, pasted);
                            cur += pasted.size();
                            redraw(prompt, buf, cur);
                        }

                        continue;
                    }
                }
            }

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
                    if (!hist.empty() && histPos > 0) {
                        if (histPos == (long)hist.size()) histSaved = buf;

                        histPos--;
                        buf = hist[(size_t)histPos];
                        cur = buf.size();

                        redraw(prompt, buf, cur);
                    } else {
                        std::cout << "\a" << std::flush;
                    }

                    continue;
                }

                if (b == 'B') {
                    if (histPos < (long)hist.size()) {
                        histPos++;

                        if (histPos == (long)hist.size()) buf = histSaved;
                        else buf = hist[(size_t)histPos];

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
            if (c == 1) { // Ctrl+A
                cur = 0;
                redraw(prompt, buf, cur);
                continue;
            }

            if (c == 5) { // Ctrl+E
                cur = buf.size();
                redraw(prompt, buf, cur);
                continue;
            }

            if (c == 11) { // Ctrl+K
                if (cur < buf.size()) {
                    buf.erase(cur);
                    redraw(prompt, buf, cur);
                }

                continue;
            }

            if (c == 21) { // Ctrl+U
                if (cur > 0) {
                    buf.erase(0, cur);
                    cur = 0;

                    redraw(prompt, buf, cur);
                }

                continue;
            }

            if (c == 23) { // Ctrl+W
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

            if (c == 12) { // Ctrl+L
                std::cout << "\033[2J\033[H" << std::flush;
                redraw(prompt, buf, cur);
                continue;
            }

            if (c == 3) { // Ctrl+C
                std::cout << "\n";

                buf.clear();
                cur = 0;
                histPos = (long)hist.size();
                histSaved.clear();

                redraw(prompt, buf, cur);

                continue;
            }

            if (c == 4) { // Ctrl+D
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
