#include "bb_mcfont.h"
#include "bb_exec.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <iostream>

using std::string;
namespace fs = std::filesystem;

static string homeDir() {
    const char* h = std::getenv("HOME");
    return h ? string(h) : string("/root");
}

static bool fileExists(const string& p) {
    std::error_code ec;
    return fs::exists(p, ec);
}

static string readAll(const string& p) {
    std::ifstream f(p);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static void writeAll(const string& p, const string& s) {
    fs::create_directories(fs::path(p).parent_path());
    std::ofstream f(p, std::ios::trunc);
    f << s;
}

static string trim(string s) {
    auto isws = [](unsigned char c){ return std::isspace(c); };

    while (!s.empty() && isws((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && isws((unsigned char)s.back())) s.pop_back();

    return s;
}

static string cfgPath() {
    return homeDir() + "/.config/bitchbatch/mcfont";
}

static string cfgGet(const string& key, const string& def) {
    string p = cfgPath();
    if (!fileExists(p)) return def;

    std::istringstream in(readAll(p));
    string line;

    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) continue;

        if (line.rfind(key + "=", 0) == 0) return trim(line.substr(key.size() + 1));
    }

    return def;
}

static void cfgSet(const string& key, const string& val) {
    string p = cfgPath();

    string c = fileExists(p) ? readAll(p) : "";
    std::istringstream in(c);
    std::ostringstream out;
    string line;

    bool done = false;

    while (std::getline(in, line)) {
        string t = trim(line);

        if (!t.empty() && t.rfind(key + "=", 0) == 0) {
            out << key << "=" << val << "\n";
            done = true;
        } else {
            out << line << "\n";
        }
    }

    if (!done) out << key << "=" << val << "\n";

    writeAll(p, out.str());
}

static bool isWSL() {
    const char* w = std::getenv("WSL_INTEROP");
    if (w && *w) return true;

    if (fileExists("/proc/version")) {
        string v = readAll("/proc/version");
        for (auto& ch : v) ch = (char)std::tolower((unsigned char)ch);
        if (v.find("microsoft") != string::npos) return true;
    }

    return false;
}

static void installFontLocal() {
    string src1 = "/usr/local/share/fonts/bitchbatch/Monocraft.ttc";
    string src2 = "/usr/share/fonts/bitchbatch/Monocraft.ttc";
    string src3 = "./assets/fonts/Monocraft.ttc";

    string src;

    if (fileExists(src1)) src = src1;
    else if (fileExists(src2)) src = src2;
    else if (fileExists(src3)) src = src3;
    else return;

    string dstDir = homeDir() + "/.local/share/fonts";
    string dst = dstDir + "/Monocraft.ttc";

    cmd("mkdir -p \"" + dstDir + "\"");
    cmd("cp -f \"" + src + "\" \"" + dst + "\"");
    cmd("fc-cache -f >/dev/null 2>&1 || true");
}

static bool setXfceTerminalFont(bool on) {
    string p = homeDir() + "/.config/xfce4/terminal/terminalrc";

    if (!fileExists(p)) return false;

    string c = readAll(p);
    std::istringstream in(c);
    std::ostringstream out;
    string line;

    bool hasCustom = false;
    bool hasFont = false;

    while (std::getline(in, line)) {
        string t = trim(line);

        if (t.rfind("FontName=", 0) == 0) {
            hasFont = true;
            out << "FontName=" << (on ? "Monocraft 11" : "Monospace 11") << "\n";
            continue;
        }

        if (t.rfind("UseCustomFont=", 0) == 0) {
            hasCustom = true;
            out << "UseCustomFont=" << (on ? "TRUE" : "FALSE") << "\n";
            continue;
        }

        out << line << "\n";
    }

    if (!hasCustom) out << "UseCustomFont=" << (on ? "TRUE" : "FALSE") << "\n";
    if (!hasFont) out << "FontName=" << (on ? "Monocraft 11" : "Monospace 11") << "\n";

    writeAll(p, out.str());

    return true;
}

static string psEscape(const string& s) {
    string o;
    o.reserve(s.size() + 8);

    for (char c : s) {
        if (c == '\\') o += "\\\\";
        else if (c == '"') o += "\\\"";
        else o.push_back(c);
    }

    return o;
}

static bool setWindowsTerminalFont(bool on) {
    if (!isWSL()) return false;

    string face = on ? "Monocraft" : cfgGet("win_prev", "");
    if (!on && face.empty()) face = "";

    string script;

    script += "$pkgs = Join-Path $env:LOCALAPPDATA 'Packages';";
    script += "$cand = Get-ChildItem $pkgs -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like 'Microsoft.WindowsTerminal*' };";
    script += "$p = $null;";
    script += "foreach($c in $cand){ $s = Join-Path $c.FullName 'LocalState\\settings.json'; if(Test-Path $s){ $p=$s; break } };";
    script += "if(-not $p){ exit 2 };";
    script += "$j = Get-Content $p -Raw | ConvertFrom-Json;";
    script += "if(-not $j.profiles){ $j | Add-Member -NotePropertyName profiles -NotePropertyValue (@{}) };";
    script += "if(-not $j.profiles.defaults){ $j.profiles | Add-Member -NotePropertyName defaults -NotePropertyValue (@{}) -Force };";

    if (on) {
        script += "$cur = $j.profiles.defaults.font.face;";
        script += "if($cur){ $cur } else { '' } | Set-Variable -Name _prev;";
        script += "$j.profiles.defaults.font.face = 'Monocraft';";
        script += "$j | ConvertTo-Json -Depth 64 | Set-Content $p -Encoding utf8;";
        script += "Write-Output $_prev;";
    } else {
        script += "if('" + psEscape(face) + "' -eq ''){ if($j.profiles.defaults.font){ $j.profiles.defaults.font.PSObject.Properties.Remove('face') | Out-Null }; } else { if(-not $j.profiles.defaults.font){ $j.profiles.defaults | Add-Member -NotePropertyName font -NotePropertyValue (@{}) -Force }; $j.profiles.defaults.font.face = '" + psEscape(face) + "'; };";
        script += "$j | ConvertTo-Json -Depth 64 | Set-Content $p -Encoding utf8;";
        script += "Write-Output 'ok';";
    }

    string cmdline = "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"" + psEscape(script) + "\"";

    int rc = cmd(cmdline + " > /tmp/.bb_mcfont_ps 2>/tmp/.bb_mcfont_ps_err");
    if (rc != 0) return false;

    if (on) {
        string prev = fileExists("/tmp/.bb_mcfont_ps") ? trim(readAll("/tmp/.bb_mcfont_ps")) : "";
        cfgSet("win_prev", prev);
    }

    return true;
}

static void printNotes(bool wslDone, bool xfceDone) {
    if (wslDone) {
        std::cout << "Windows Terminal settings.json updated. Restart Windows Terminal.\n\n";
        std::cout << "Font must be installed on Windows host too (Monocraft).\n\n";
    }

    if (xfceDone) {
        std::cout << "XFCE terminal config updated. Open a new terminal window.\n\n";
    }

    if (!wslDone && !xfceDone) {
        std::cout << "No supported terminal config found.\n\n";
        std::cout << "WSL: run inside Windows Terminal.\n";
        std::cout << "VM: xfce4-terminal uses ~/.config/xfce4/terminal/terminalrc\n\n";
    }
}

static void apply(bool on, bool quiet) {
    if (on) installFontLocal();

    bool wslDone = setWindowsTerminalFont(on);
    bool xfceDone = setXfceTerminalFont(on);

    cfgSet("enabled", on ? "1" : "0");

    if (!quiet) {
        if (on) std::cout << "mcfont ON\n\n";
        else std::cout << "mcfont OFF\n\n";

        printNotes(wslDone, xfceDone);
    }
}

void mcfontApplySaved() {
    string e = cfgGet("enabled", "0");
    bool on = (e == "1" || e == "true" || e == "on");

    if (!on) return;
    apply(true, true);
}

void mcfontRun(const std::vector<std::string>& args) {
    bool on = true;

    if (args.size() >= 2) {
        if (args[1] == "-no" || args[1] == "off" || args[1] == "0") on = false;
        if (args[1] == "-on" || args[1] == "on" || args[1] == "1") on = true;
    }

    apply(on, false);
}
