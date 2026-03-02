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
    safeMkdirs(root / "projects" / "demo" / "src");
    safeMkdirs(root / "projects" / "demo" / "conf");

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

    writeFile(root / "projects" / "demo" / "README.md",
              "# demo\n"
              "This is a sandbox project folder for learn mode.\n");

    writeFile(root / "projects" / "demo" / "conf" / "app.conf",
              "PORT=8080\n"
              "MODE=dev\n"
              "FEATURE_X=on\n");

    writeFile(root / "projects" / "demo" / "src" / "main.c",
              "#include <stdio.h>\n"
              "int main(){ printf(\"hi\\n\"); return 0; }\n");

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

    return false;
}

static bool hasRedirect(const string& raw) {
    for (char c : raw) if (c == '>') return true;
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

static std::function<bool(const string&)> vStarts(string p) {
    p = trim(p);
    return [p](const string& s){ return startsWith(trim(s), p); };
}

static std::function<bool(const string&)> vContains(string needle) {
    needle = lower(trim(needle));
    return [needle](const string& s){ return lower(s).find(needle) != string::npos; };
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

static Chapter makeChapterFiles() {
    Chapter ch;

    ch.id = "files";
    ch.title = "Files: erstellen, kopieren, bewegen";
    ch.desc = "touch / mkdir / cp / mv / wildcards";

    ch.steps.push_back(Text(
        "0) Setup",
        {
            "Wir bleiben in der Sandbox.",
            "Ziel: du lernst Dateien/Ordner zu erstellen und sauber zu kopieren/verschieben.",
            "",
            "Wichtig: rm ist im Learn Mode geblockt. Wir loeschen nichts."
        },
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Demo(
        "1) touch: Datei anlegen",
        {
            "touch erstellt eine leere Datei (oder updated die Zeit).",
            "Beispiel: touch demo.txt"
        },
        "touch demo.txt"
    ));

    ch.steps.push_back(Practice(
        "2) Uebung: touch",
        { "Erstelle eine Datei namens 'note.txt'." },
        "note.txt existiert danach.",
        "touch <dateiname>",
        "touch note.txt",
        vExact("touch note.txt")
    ));

    ch.steps.push_back(Demo(
        "3) mkdir: Ordner anlegen",
        {
            "mkdir erstellt Ordner.",
            "Mit -p kannst du verschachtelt erstellen: mkdir -p a/b/c"
        },
        "mkdir play"
    ));

    ch.steps.push_back(Practice(
        "4) Uebung: mkdir -p",
        { "Erstelle den Pfad 'play/a/b'." },
        "Nutze mkdir -p damit es in einem go geht.",
        "mkdir -p play/a/b",
        "mkdir -p play/a/b",
        vExact("mkdir -p play/a/b")
    ));

    ch.steps.push_back(Text(
        "5) Wildcards (globbing)",
        {
            "Wildcards helfen dir mehrere Dateien auf einmal zu matchen.",
            "  *   alles",
            "  ?.txt  genau 1 Zeichen + .txt",
            "  app.*  alles was mit app. startet",
            "",
            "Beispiel: ls *.txt"
        }
    ));

    ch.steps.push_back(Practice(
        "6) Uebung: ls *.txt",
        { "Liste alle .txt Dateien im aktuellen Ordner." },
        "ls *.txt",
        "Wildcard: *.txt",
        "ls *.txt",
        vExact("ls *.txt")
    ));

    ch.steps.push_back(Text(
        "7) cp / mv",
        {
            "cp kopiert Dateien/Ordner.",
            "mv bewegt oder benennt um.",
            "",
            "Beispiele:",
            "  cp note.txt play/",
            "  mv demo.txt demo_old.txt"
        }
    ));

    ch.steps.push_back(Practice(
        "8) Uebung: cp",
        { "Kopiere note.txt in den Ordner play." },
        "cp <file> <ziel>/",
        "Ziel ist ein Ordner: play/",
        "cp note.txt play/",
        vAnyOf({vExact("cp note.txt play/"), vExact("cp note.txt play")})
    ));

    ch.steps.push_back(Practice(
        "9) Uebung: mv (umbenennen)",
        { "Benenne demo.txt um zu demo_old.txt." },
        "mv <alt> <neu>",
        "mv demo.txt demo_old.txt",
        "mv demo.txt demo_old.txt",
        vExact("mv demo.txt demo_old.txt")
    ));

    ch.steps.push_back(Quiz(
        "10) Mini-Quiz",
        { "Kurz check:" },
        "Welcher Befehl benennt eine Datei um?",
        { {"A","cp",false}, {"B","mv",true}, {"C","touch",false} },
        "mv bewegt oder benennt um."
    ));

    return ch;
}

static Chapter makeChapterViewText() {
    Chapter ch;

    ch.id = "text";
    ch.title = "Text: anschauen und filtern";
    ch.desc = "cat / less / head / tail";

    ch.steps.push_back(Text(
        "0) Warum Text-Tools?",
        {
            "Linux ist voll mit Logs, Configs und Textfiles.",
            "Wenn du Text lesen kannst wie ein Pro, bist du 10x schneller.",
            "",
            "Wir nutzen dafuer app.log und users.csv in projects/demo."
        }
    ));

    ch.steps.push_back(Practice(
        "1) Uebung: cd projects/demo",
        { "Wechsle in projects/demo." },
        "cd projects/demo",
        "cd projects/demo",
        "cd projects/demo",
        vExact("cd projects/demo")
    ));

    ch.steps.push_back(Demo(
        "2) cat: alles ausgeben",
        {
            "cat zeigt den kompletten Inhalt.",
            "Gut fuer kleine Files."
        },
        "cat users.csv"
    ));

    ch.steps.push_back(Demo(
        "3) head / tail",
        {
            "head zeigt den Anfang, tail das Ende.",
            "Mit -n kannst du Zeilen angeben.",
            "Beispiel: tail -n 2 app.log"
        },
        "tail -n 2 app.log"
    ));

    ch.steps.push_back(Practice(
        "4) Uebung: head -n 1 users.csv",
        { "Zeig nur die Header-Zeile aus users.csv." },
        "head -n 1 users.csv",
        "Der Header ist die erste Zeile.",
        "head -n 1 users.csv",
        vExact("head -n 1 users.csv")
    ));

    ch.steps.push_back(Text(
        "5) less",
        {
            "less ist wie ein Viewer.",
            "Navigation:",
            "  q    quit",
            "  /x   suchen nach x",
            "  n    next match",
            "  g/G  top/bottom"
        },
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Practice(
        "6) Uebung: less app.log",
        { "Starte less auf app.log und beende mit q." },
        "less app.log",
        "Gib 'less app.log' ein, dann in less drueck q.",
        "less app.log",
        vExact("less app.log"),
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Quiz(
        "7) Mini-Quiz",
        { "Noch ein Check:" },
        "Mit welchem Befehl siehst du die letzten Zeilen einer Datei?",
        { {"A","tail",true}, {"B","head",false}, {"C","pwd",false} },
        "tail zeigt das Ende."
    ));

    return ch;
}

static Chapter makeChapterSearch() {
    Chapter ch;

    ch.id = "search";
    ch.title = "Search: grep + find";
    ch.desc = "grep / find / einfache Patterns";

    ch.steps.push_back(Text(
        "0) Der Plan",
        {
            "Du willst Dinge finden: Text in Files, oder Files im System.",
            "In der Sandbox ist alles klein, aber das Prinzip ist identisch.",
            "",
            "Wir nutzen projects/demo/app.log und docs/notes.txt"
        }
    ));

    ch.steps.push_back(Practice(
        "1) Uebung: zur Sandbox root",
        { "Geh zurueck in den Sandbox Root (nicht /)." },
        "cd .. bis du wieder oben bist.",
        "Du warst in projects/demo. Zwei mal cd .. bringt dich nach oben.",
        "cd ../..",
        vAnyOf({vExact("cd ../.."), vExact("cd ../../"), vExact("cd ..")})
    ));

    ch.steps.push_back(Demo(
        "2) grep: Text suchen",
        {
            "grep <pattern> <file>",
            "Beispiel: grep ERROR projects/demo/app.log"
        },
        "grep ERROR projects/demo/app.log"
    ));

    ch.steps.push_back(Practice(
        "3) Uebung: grep WARN",
        { "Suche nach WARN in projects/demo/app.log." },
        "grep WARN projects/demo/app.log",
        "Grep Syntax: grep <PATTERN> <FILE>",
        "grep WARN projects/demo/app.log",
        vExact("grep WARN projects/demo/app.log")
    ));

    ch.steps.push_back(Text(
        "4) grep Flags",
        {
            "Nutzbare Flags:",
            "  -n   Zeilennummern",
            "  -i   ignore case",
            "  -r   rekursiv (ganzer Ordner)",
            "",
            "Beispiel: grep -in error projects/demo/app.log"
        }
    ));

    ch.steps.push_back(Practice(
        "5) Uebung: grep -n",
        { "Zeig dir ERROR mit Zeilennummern." },
        "grep -n ERROR projects/demo/app.log",
        "Flag -n",
        "grep -n ERROR projects/demo/app.log",
        vExact("grep -n ERROR projects/demo/app.log")
    ));

    ch.steps.push_back(Demo(
        "6) find: Dateien finden",
        {
            "find <path> -name <pattern>",
            "Beispiel: find . -name '*.txt'"
        },
        "find . -name '*.txt'"
    ));

    ch.steps.push_back(Practice(
        "7) Uebung: find users.csv",
        { "Finde users.csv irgendwo in der Sandbox." },
        "find . -name users.csv",
        "Name ohne Wildcard geht auch.",
        "find . -name users.csv",
        vExact("find . -name users.csv")
    ));

    ch.steps.push_back(Quiz(
        "8) Mini-Quiz",
        { "Fix noch:" },
        "Was macht grep?",
        { {"A","findet Text in Dateien",true}, {"B","zeigt deinen Ordner",false}, {"C","wechselt Ordner",false} },
        "grep sucht Text-Pattern in Dateien."
    ));

    return ch;
}

static Chapter makeChapterPipesAndRedirects() {
    Chapter ch;

    ch.id = "pipes";
    ch.title = "Pipes + Redirects";
    ch.desc = "| / > / >> / 2> (theory + safe practice)";

    ch.steps.push_back(Text(
        "0) Idee: Output zu Input",
        {
            "Eine Pipe '|' nimmt stdout vom linken Befehl und gibt ihn als stdin rechts rein.",
            "Das ist das Linux-Superpower Ding.",
            "",
            "Redirects schreiben Output in Dateien:",
            "  >   overwrite",
            "  >>  append",
            "  2>  stderr"
        },
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Demo(
        "1) Pipe Demo: grep + wc",
        {
            "Wir filtern app.log und zaehlen Treffer.",
            "wc -l zaehlt Zeilen."
        },
        "cat projects/demo/app.log | grep INFO | wc -l"
    ));

    ch.steps.push_back(Practice(
        "2) Uebung: grep ERROR | wc -l",
        { "Zaehle wie viele ERROR Zeilen in app.log sind." },
        "cat ... | grep ... | wc -l",
        "Pattern ERROR",
        "cat projects/demo/app.log | grep ERROR | wc -l",
        vContains("| grep")
    ));

    ch.steps.push_back(Text(
        "3) Redirects (safe)",
        {
            "In Learn Mode fuehren wir Redirects nicht aus, aber du lernst die Syntax.",
            "Beispiele:",
            "  echo hello > out.txt",
            "  echo again >> out.txt",
            "",
            "Warum? Weil '>' schnell mal was ueberschreibt und das wollen wir safe halten."
        },
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Practice(
        "4) Uebung: echo hello > out.txt",
        { "Gib den Redirect-Befehl exakt ein (wird NICHT ausgefuehrt)." },
        "Syntax merken.",
        "echo hello > out.txt",
        "echo hello > out.txt",
        vExact("echo hello > out.txt"),
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Practice(
        "5) Uebung: append",
        { "Gib einen Append-Redirect ein (wird NICHT ausgefuehrt)." },
        "Syntax: >>",
        "echo again >> out.txt",
        "echo again >> out.txt",
        vExact("echo again >> out.txt"),
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Quiz(
        "6) Mini-Quiz",
        { "Letzter Check:" },
        "Was macht '>>'?",
        { {"A","schreibt ans Ende (append)",true}, {"B","ueberschreibt Datei",false}, {"C","pipe",false} },
        "'>>' append. '>' overwrite."
    ));

    return ch;
}

static Chapter makeChapterShellDaily() {
    Chapter ch;

    ch.id = "daily";
    ch.title = "Daily Shell: history, env";
    ch.desc = "echo / env / history";

    ch.steps.push_back(Text(
        "0) Bash Basics",
        {
            "Ein paar Sachen, die du jeden Tag brauchst:",
            "  echo text",
            "  echo $HOME",
            "  env | grep PATH",
            "",
            "Diese Dinge sind nicht scary, aber super nuetzlich."
        }
    ));

    ch.steps.push_back(Demo(
        "1) echo",
        { "echo gibt Text aus." },
        "echo hello"
    ));

    ch.steps.push_back(Demo(
        "2) Variablen",
        {
            "Mit $ greifst du auf ENV vars zu.",
            "Beispiel: echo $HOME"
        },
        "echo $HOME"
    ));

    ch.steps.push_back(Demo(
        "3) env",
        {
            "env zeigt Environment Variablen.",
            "Mit grep kannst du filtern."
        },
        "env | grep PATH"
    ));

    ch.steps.push_back(Practice(
        "4) Uebung: echo $USER",
        { "Gib deinen User aus." },
        "echo $USER",
        "$USER",
        "echo $USER",
        vExact("echo $USER")
    ));

    ch.steps.push_back(Text(
        "5) history",
        {
            "history zeigt dir deine letzten Befehle.",
            "Beispiele:",
            "  history",
            "  history | tail",
            "",
            "Pro Tipp: mit STRG+R kannst du suchen (reverse search)."
        },
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Practice(
        "6) Uebung: history",
        { "Tippe 'history' (nur Syntax check, nicht executed)." },
        "history",
        "history",
        "history",
        vExact("history"),
        RunMode::SAFE_NOEXEC
    ));

    ch.steps.push_back(Quiz(
        "7) Mini-Quiz",
        { "Ok:" },
        "Was gibt 'echo $HOME' aus?",
        { {"A","Home-Ordner Pfad",true}, {"B","CPU Temperatur",false}, {"C","den Kernel",false} },
        "$HOME ist dein Home directory."
    ));

    return ch;
}

static std::vector<Chapter> buildChapters() {
    return { makeChapterBasics(), makeChapterFiles(), makeChapterViewText(), makeChapterSearch(), makeChapterPipesAndRedirects(), makeChapterShellDaily() };
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
    if (st.runMode == RunMode::SAFE_NOEXEC) {
        printCmdLine(in);
        cout << Color::YELLOW << "(not executed in SAFE_NOEXEC)" << Color::RESET << "\n\n";
        pauseIf(pauseAfterExec);
        
        if (!st.validate) return true;
        
        return st.validate(in);
    }

    string reason;

    if (isBannedCommand(in, reason)) {
        cout << Color::RED << "Blocked in learn mode: " << reason << Color::RESET << "\n\n";
        return false;
    }

    if (hasRedirect(in)) {
        cout << Color::YELLOW << "Blocked in learn mode: redirects are not executed here" << Color::RESET << "\n\n";
        if (!st.validate) return true;
        return st.validate(in);
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
