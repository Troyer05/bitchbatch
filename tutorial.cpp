#include "tutorial.h"
#include "bb_env.h"
#include "framework.h"     
#include "bb_exec.h"       
#include "bb_readline.h"   

#include <iostream>
#include <vector>
#include <string>
#include <functional>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

using std::cout;
using std::string;

namespace fs = std::filesystem;

static string trim(string s) {
    auto isws = [](unsigned char c){ return std::isspace(c); };

    while (!s.empty() && isws((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isws((unsigned char)s.back())) s.pop_back();

    return s;
}

static string lower(string s) {
    for (auto &ch : s) ch = (char)std::tolower((unsigned char)ch);
    return s;
}

static string upper(string s) {
    for (auto &ch : s) ch = (char)std::toupper((unsigned char)ch);
    return s;
}

static bool startsWith(const string& s, const string& p) {
    return s.size() >= p.size() && std::equal(p.begin(), p.end(), s.begin());
}

static std::vector<string> splitWs(const string& s) {
    std::istringstream iss(s);
    std::vector<string> out;

    string t;

    while (iss >> t) out.push_back(t);

    return out;
}

static string getHomeDir() {
    const char* h = std::getenv("HOME");
    if (h && *h) return string(h);
    return "/tmp";
}

static void safeMkdirs(const fs::path& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

static bool writeFile(const fs::path& p, const string& content) {
    std::ofstream f(p);

    if (!f) return false;

    f << content;

    return true;
}

static void hr() {
    cout << Color::GRAY << "------------------------------------------------------------" << Color::RESET << "\n";
}

static void printHeader(const string& t) {
    cout << Color::CYAN << "\n== " << t << " ==" << Color::RESET << "\n\n";
}

static void printLines(const std::vector<string>& ls) {
    for (auto& l : ls) cout << l << "\n";
    cout << "\n";
}

static void showTutorHelp() {
    hr();

    cout << Color::YELLOW
         << "Tutor commands:\n"
         << "  :help        show this\n"
         << "  :menu        back to chapter menu\n"
         << "  :next        next step\n"
         << "  :back        previous step\n"
         << "  :repeat      repeat step\n"
         << "  :skip        skip current step\n"
         << "  :hint        show hint (practice)\n"
         << "  :solution    show solution (practice)\n"
         << "  :where       show sandbox folder\n"
         << "  :progress    show saved progress\n"
         << "  :reset       reset progress (this tutorial)\n"
         << "  :pause       toggle pause-after-exec\n"
         << "  :exit        quit tutorial\n"
         << Color::RESET << "\n";

    hr();

    cout << "\n";
}

static string readTutorLine(const string& prompt) {
    cout << Color::GREEN << prompt << Color::RESET;

    std::string s;
    std::getline(std::cin, s);
    
    return trim(s);
}

static void printCmdLine(const string& s) {
    cout << Color::GRAY << "$ " << s << Color::RESET << "\n";
}

static void pauseIf(bool enabled) {
    if (!enabled) return;
    
    cout << Color::GRAY << "Press ENTER to continue..." << Color::RESET;
    
    std::string _;
    std::getline(std::cin, _);
    
    cout << "\n";
}

enum class StepType { TEXT, DEMO, PRACTICE, QUIZ };
enum class RunMode { SHELL, SAFE_NOEXEC };

struct QuizOption {
    string key;
    string text;

    bool correct = false;
};

struct Step {
    StepType type;
    string title;

    std::vector<string> lines;

    string demoCmd;
    string practiceGoal;
    string hint;
    string solution;

    std::function<bool(const string&)> validate;

    string quizQuestion;

    std::vector<QuizOption> quizOptions;

    string quizExplain;

    RunMode runMode = RunMode::SHELL;
};

struct Chapter {
    string id;
    string title;
    string desc;

    std::vector<Step> steps;
};





static Step Text(string title, std::vector<string> lines, RunMode mode = RunMode::SHELL) {
    Step s{};

    s.type = StepType::TEXT;
    s.title = std::move(title);
    s.lines = std::move(lines);
    s.runMode = mode;

    return s;
}

static Step Demo(string title, std::vector<string> lines, string cmd, RunMode mode = RunMode::SHELL) {
    Step s{};

    s.type = StepType::DEMO;
    s.title = std::move(title);
    s.lines = std::move(lines);
    s.demoCmd = std::move(cmd);
    s.runMode = mode;

    return s;
}

static Step Practice(string title, std::vector<string> lines, string goal,
                     string hint, string solution,
                     std::function<bool(const string&)> validate = {},
                     RunMode mode = RunMode::SHELL) {
    Step s{};

    s.type = StepType::PRACTICE;
    s.title = std::move(title);
    s.lines = std::move(lines);
    s.practiceGoal = std::move(goal);
    s.hint = std::move(hint);
    s.solution = std::move(solution);
    s.validate = std::move(validate);
    s.runMode = mode;

    return s;
}

static Step Quiz(string title, std::vector<string> lines,
                 string question, std::vector<QuizOption> opts, string explain) {
    Step s{};

    s.type = StepType::QUIZ;
    s.title = std::move(title);
    s.lines = std::move(lines);
    s.quizQuestion = std::move(question);
    s.quizOptions = std::move(opts);
    s.quizExplain = std::move(explain);

    return s;
}

static fs::path progressFilePath() {
    return fs::path(getHomeDir()) / ".bitchbatch" / "learn_linux_terminal.progress";
}

static void ensureProgressDir() {
    safeMkdirs(fs::path(getHomeDir()) / ".bitchbatch");
}

static void saveProgress(const string& chapterId, int stepIndex) {
    ensureProgressDir();

    std::ofstream f(progressFilePath());

    if (!f) return;

    f << chapterId << "\n" << stepIndex << "\n";
}

static bool loadProgress(string& chapterId, int& stepIndex) {
    std::ifstream f(progressFilePath());

    if (!f) return false;

    std::getline(f, chapterId);
    string line;
    std::getline(f, line);

    if (chapterId.empty()) return false;

    try { stepIndex = std::stoi(line); } catch (...) { stepIndex = 0; }

    return true;
}

static void resetProgress() {
    std::error_code ec;
    fs::remove(progressFilePath(), ec);
}

static fs::path sandboxPath() {
    return fs::path(getHomeDir()) / ".bitchbatch" / "learn_sandbox";
}

static void seedSandbox() {
    auto root = sandboxPath();

    safeMkdirs(root);
    safeMkdirs(root / "docs");
    safeMkdirs(root / "projects" / "demo");

    writeFile(root / "hello.txt",
              "Hello from BitchBatch Learn Mode!\n"
              "This file is safe to play with.\n");

    writeFile(root / "docs" / "notes.txt",
              "TODO:\n"
              "- learn ls/cd/pwd\n"
              "- learn grep/find\n"
              "- learn pipes and redirects\n");

    writeFile(root / "projects" / "demo" / "app.log",
              "2026-02-25 INFO  server started\n"
              "2026-02-25 WARN  disk at 82%\n"
              "2026-02-25 ERROR failed to connect to db\n"
              "2026-02-25 INFO  retry ok\n");

    writeFile(root / "projects" / "demo" / "users.csv",
              "id,name,role\n"
              "1,markus,admin\n"
              "2,alex,user\n"
              "3,sam,user\n");

    writeFile(root / ".secret",
              "shh. hidden files start with a dot.\n");
}





static bool isBannedCommand(const string& raw, string& reason) {
    string s = trim(raw);

    if (s.empty()) { reason = "empty"; return true; }
    if (s.find(":(){") != string::npos) { reason = "fork bomb"; return true; }

    auto toks = splitWs(s);

    if (toks.empty()) { reason = "empty"; return true; }

    string c = lower(toks[0]);

    static const std::vector<string> hard = {
        "dd", "mkfs", "wipefs", "shutdown", "reboot", "poweroff", "halt",
        "systemctl", "service", "init", "telinit",
        "kill", "killall", "pkill",
        "iptables", "nft", "ufw",
        "mount", "umount",
        "chmod", "chown", "useradd", "usermod", "passwd",
        "sudo"
    };

    if (std::find(hard.begin(), hard.end(), c) != hard.end()) {
        reason = "blocked command in learn mode";
        return true;
    }

    if (c == "rm") { reason = "rm is blocked in learn mode"; return true; }

    if (s.find("/etc/") != string::npos || s.find("/bin/") != string::npos || s.find("/usr/") != string::npos) {
        if (c == "cat" || c == "less" || c == "head" || c == "tail" || c == "grep") return false;
        reason = "writing into system paths is blocked";
        return true;
    }

    
    if (s.find(" >") != string::npos || startsWith(s, ">") || s.find(">>") != string::npos) {
        reason = "redirects are blocked in exec mode (we still teach them)";
        return true;
    }

    return false;
}

static string wrapInSandbox(const string& userCmd) {
    return "cd \"" + sandboxPath().string() + "\" && " + userCmd;
}

static void runShellInSandbox(const string& userCmd) {
    std::vector<string> args = { "/bin/sh", "-c", wrapInSandbox(userCmd) };
    cmdArgs(args);
}

static std::function<bool(const string&)> vExact(const string& exact) {
    return [exact](const string& s){ return trim(s) == exact; };
}

static std::function<bool(const string&)> vAnyOf(std::vector<std::function<bool(const string&)>> vs) {
    return [vs](const string& s){
        for (auto& v : vs) if (v && v(s)) return true;
        return false;
    };
}

static Chapter makeChapterBasics() {
    Chapter ch;

    ch.id = "basics";
    ch.title = "Basics: Orientierung + Hilfe";
    ch.desc = "pwd / ls / cd / man / --help";

    ch.steps.push_back(Text(
        "0) Willkommen (Sandbox!)",
        {
            "Du bist im Learn Mode. Alles was wir ausfuehren, passiert in einer Sandbox.",
            "Du kannst jederzeit :where tippen um den Pfad zu sehen.",
            "",
            "In Learn Mode sind gefaehrliche Befehle blockiert (rm, sudo, systemctl, ...).",
            "Damit du nichts zerschiesst."
        },
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Demo(
        "1) Wo bin ich? (pwd)",
        {
            "pwd = print working directory.",
            "Nutzen: du bist nie lost."
        },
        "pwd"
    ));

    ch.steps.push_back(Practice(
        "2) Uebung: pwd",
        { "Aufgabe: fuehre 'pwd' aus." },
        "Gib 'pwd' ein.",
        "Ein Wort: pwd",
        "pwd",
        vExact("pwd")
    ));

    ch.steps.push_back(Text(
        "3) ls: Dateien anzeigen",
        {
            "ls zeigt Dateien/Ordner.",
            "",
            "Haeufige Flags:",
            "  ls -l    Details",
            "  ls -a    auch hidden (.)",
            "  ls -la   beides"
        }
    ));

    ch.steps.push_back(Practice(
        "4) Uebung: ls -la",
        { "Aufgabe: zeig alles inkl. hidden und Details." },
        "ls -l = details, ls -a = hidden",
        "Kombiniere -l und -a",
        "ls -la",
        vAnyOf({vExact("ls -la"), vExact("ls -al")})
    ));

    ch.steps.push_back(Text(
        "5) cd: Verzeichnis wechseln",
        {
            "cd wechselt den Ordner.",
            "Wichtig:",
            "  cd ..   (eins hoch)",
            "  cd -    (zurueck)"
        }
    ));

    ch.steps.push_back(Practice(
        "6) Uebung: cd docs",
        { "Aufgabe: wechsle in den Ordner docs." },
        "cd docs",
        "Wechsle in docs: cd docs",
        "cd docs",
        vExact("cd docs")
    ));

    ch.steps.push_back(Practice(
        "7) Uebung: pwd check",
        { "Aufgabe: pruefe mit pwd, ob du in docs bist." },
        "pwd",
        "pwd",
        "pwd",
        vExact("pwd")
    ));

    ch.steps.push_back(Text(
        "8) Hilfe finden: man + --help",
        {
            "Wenn du einen Befehl nicht kennst:",
            "  man <cmd>",
            "  <cmd> --help"
        }
    ));

    ch.steps.push_back(Quiz(
        "9) Mini-Quiz",
        { "Kurz check ob's sitzt:" },
        "Was zeigt dir deinen aktuellen Ordner?",
        { {"A","ls",false}, {"B","pwd",true}, {"C","cd",false} },
        "pwd = print working directory."
    ));

    return ch;
}

static std::vector<Chapter> buildChapters() {
    return { makeChapterBasics() };
}

static int menuPick(const std::vector<Chapter>& chapters) {
    CLS();

    cout << Color::GREEN << "BitchBatch Learn Mode: Linux Terminal" << Color::RESET << "\n";
    cout << Color::GRAY << "Sandbox: " << sandboxPath().string() << Color::RESET << "\n\n";
    cout << "Chapters:\n\n";

    for (size_t i=0;i<chapters.size();i++) {
        cout << Color::CYAN << "  [" << (i+1) << "] " << chapters[i].title << Color::RESET << "\n";
        cout << "      " << Color::GRAY << chapters[i].desc << Color::RESET << "\n";
    }

    cout << "\n";
    cout << Color::YELLOW << "Commands:" << Color::RESET << "\n";
    cout << "  r  resume saved progress\n";
    cout << "  1.." << chapters.size() << "  start chapter\n";
    cout << "  x  exit\n\n";

    string in = readTutorLine("bb(learn-menu)# ");

    if (in == "x" || in == ":exit") return -1;
    if (in == "r") return -2;

    try {
        int n = std::stoi(in);
        if (n >= 1 && n <= (int)chapters.size()) return n-1;
    } catch (...) {}

    return -3;
}

static void showProgress() {
    string cid; int idx=0;
    if (!loadProgress(cid, idx)) { cout << "No saved progress.\n\n"; return; }
    cout << "Saved progress: chapter='" << cid << "' step=" << idx << "\n\n";
}

static int chapterIndexById(const std::vector<Chapter>& chapters, const string& id) {
    for (size_t i=0;i<chapters.size();i++) if (chapters[i].id == id) return (int)i;
    return -1;
}

static void runStepDemo(const Step& st, bool pauseAfterExec) {
    printCmdLine(st.demoCmd);

    if (st.runMode == RunMode::SAFE_NOEXEC) {
        cout << Color::YELLOW << "(demo not executed in SAFE_NOEXEC)" << Color::RESET << "\n\n";
        return;
    }

    string reason;

    if (isBannedCommand(st.demoCmd, reason)) {
        cout << Color::YELLOW << "(blocked in learn mode: " << reason << ")" << Color::RESET << "\n\n";
        return;
    }

    runShellInSandbox(st.demoCmd);

    cout << "\n";

    pauseIf(pauseAfterExec);
}

static bool runPracticeAttempt(const Step& st, const string& in, bool pauseAfterExec) {
    string reason;

    if (isBannedCommand(in, reason)) {
        cout << Color::RED << "Blocked in learn mode: " << reason << Color::RESET << "\n\n";
        return false;
    }

    printCmdLine(in);
    runShellInSandbox(in);

    cout << "\n";

    pauseIf(pauseAfterExec);

    if (!st.validate) return true;

    return st.validate(in);
}

static void runChapter(const std::vector<Chapter>& chapters, int chIndex, int startStep) {
    if (chIndex < 0 || chIndex >= (int)chapters.size()) return;

    const Chapter& ch = chapters[chIndex];
    int i = startStep;
    bool pauseAfterExec = true;

    while (true) {
        if (i < 0) i = 0;

        if (i >= (int)ch.steps.size()) {
            cout << Color::GREEN << "\nChapter complete: " << ch.title << "\n" << Color::RESET;

            saveProgress(ch.id, (int)ch.steps.size());

            cout << "\nType :menu to pick another chapter or :exit to leave.\n\n";

            while (true) {
                string in = readTutorLine("bb(learn)# ");

                if (in == ":menu") return;
                if (in == ":exit") return;
                if (in == ":help") { showTutorHelp(); continue; }
                if (in == ":pause") { pauseAfterExec = !pauseAfterExec; cout << "pauseAfterExec=" << (pauseAfterExec?"on":"off") << "\n\n"; continue; }

                cout << "Use :menu or :exit (or :help).\n\n";
            }
        }

        const Step& st = ch.steps[i];

        CLS();

        cout << Color::GREEN << "Learn: " << ch.title << Color::RESET << "\n";
        cout << Color::GRAY << "Sandbox: " << sandboxPath().string() << Color::RESET << "\n";
        cout << Color::GRAY << "Progress: " << ch.id << " step " << (i+1) << "/" << ch.steps.size() << Color::RESET << "\n";
        cout << Color::GRAY << "pauseAfterExec: " << (pauseAfterExec ? "on" : "off") << Color::RESET << "\n";

        hr();
        printHeader(st.title);

        if (!st.lines.empty()) printLines(st.lines);

        if (st.type == StepType::DEMO) {
            runStepDemo(st, pauseAfterExec);
            cout << "(:next) weiter | (:back) zurueck | (:menu) menu | (:exit)\n\n";
        } else if (st.type == StepType::TEXT) {
            cout << "(:next) weiter | (:back) zurueck | (:menu) menu | (:exit)\n\n";
        } else if (st.type == StepType::PRACTICE) {
            cout << Color::CYAN << "Practice goal: " << st.practiceGoal << Color::RESET << "\n";
            cout << "(:hint) (:solution) (:skip) (:menu) (:exit)\n\n";
        } else if (st.type == StepType::QUIZ) {
            cout << Color::CYAN << st.quizQuestion << Color::RESET << "\n\n";
            for (auto& o : st.quizOptions) cout << "  " << o.key << ") " << o.text << "\n";
            cout << "\nAntwort (A/B/C... oder :skip):\n\n";
        }

        while (true) {
            saveProgress(ch.id, i);

            string in = readTutorLine("bb(learn)# ");

            if (in.empty()) continue;
            if (in == ":help") { showTutorHelp(); continue; }
            if (in == ":exit") { cout << "\nExit learn mode.\n\n"; return; }
            if (in == ":menu") return;
            if (in == ":where") { cout << "Sandbox: " << sandboxPath().string() << "\n\n"; continue; }
            if (in == ":progress") { showProgress(); continue; }
            if (in == ":reset") { resetProgress(); cout << "Progress reset.\n\n"; continue; }
            if (in == ":pause") { pauseAfterExec = !pauseAfterExec; cout << "pauseAfterExec=" << (pauseAfterExec?"on":"off") << "\n\n"; continue; }
            if (in == ":next") { i++; break; }
            if (in == ":back") { i--; break; }
            if (in == ":repeat") { break; }
            if (in == ":skip") { i++; break; }

            if (st.type == StepType::PRACTICE) {
                if (in == ":hint") {
                    if (!st.hint.empty()) cout << Color::YELLOW << "Hint: " << st.hint << Color::RESET << "\n\n";
                    else cout << "(no hint)\n\n";
                    continue;
                }

                if (in == ":solution") {
                    if (!st.solution.empty()) cout << Color::YELLOW << "Solution:\n" << st.solution << Color::RESET << "\n\n";
                    else cout << "(no solution)\n\n";
                    continue;
                }

                bool ok = runPracticeAttempt(st, in, pauseAfterExec);

                if (st.validate && ok) { cout << Color::GREEN << "Nice. Step passed." << Color::RESET << "\n\n"; i++; break; }
                if (!st.validate) { cout << Color::GREEN << "OK. When you're ready: :next" << Color::RESET << "\n\n"; continue; }

                cout << Color::RED << "Not quite. Try again or use :hint / :solution" << Color::RESET << "\n\n";

                continue;
            }

            if (st.type == StepType::QUIZ) {
                if (in == ":skip") { i++; break; }

                string ans = upper(trim(in));
                bool correct = false;

                for (auto& o : st.quizOptions) if (upper(o.key) == ans && o.correct) correct = true;
                cout << (correct ? Color::GREEN : Color::RED) << (correct ? "Correct." : "Not correct.") << Color::RESET << "\n";

                if (!st.quizExplain.empty()) cout << Color::GRAY << st.quizExplain << Color::RESET << "\n\n";
                cout << "Continue with :next\n\n";

                continue;
            }

            cout << "Use :next / :back / :menu / :help\n\n";
        }
    }
}

void learnLinuxTerminal() {
    seedSandbox();

    auto chapters = buildChapters();

    while (true) {
        int pick = menuPick(chapters);

        if (pick == -1) { cout << "\nExit learn mode.\n\n"; return; }
        if (pick == -3) { cout << "\nInvalid selection.\n"; continue; }

        if (pick == -2) {
            string cid; int idx=0;

            if (!loadProgress(cid, idx)) { cout << "\nNo progress saved.\n"; continue; }
            int chIndex = chapterIndexById(chapters, cid);
            if (chIndex < 0) { cout << "\nSaved chapter not found. Resetting progress.\n"; resetProgress(); continue; }

            runChapter(chapters, chIndex, idx);

            continue;
        }

        runChapter(chapters, pick, 0);
    }
}
