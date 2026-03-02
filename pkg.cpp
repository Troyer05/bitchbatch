#include "pkg.h"
#include "bb_exec.h"
#include <unistd.h>
#include <cstdlib>
#include <iostream>

static bool binExists(const std::string& bin) {
    std::string s = "command -v " + bin + " >/dev/null 2>&1";
    return (std::system(s.c_str()) == 0);
}

PkgMgr detectPkgMgr() {
    if (binExists("apt-get") || binExists("apt")) return PkgMgr::APT;
    if (binExists("dnf")) return PkgMgr::DNF;
    if (binExists("yum")) return PkgMgr::YUM;
    if (binExists("pacman")) return PkgMgr::PACMAN;
    if (binExists("zypper")) return PkgMgr::ZYPPER;
    if (binExists("apk")) return PkgMgr::APK;
    if (binExists("xbps-install")) return PkgMgr::XBPS;
    if (binExists("pkg")) return PkgMgr::PKG;

    return PkgMgr::UNKNOWN;
}

std::string mgrName(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT: return "apt";
        case PkgMgr::DNF: return "dnf";
        case PkgMgr::YUM: return "yum";
        case PkgMgr::PACMAN: return "pacman";
        case PkgMgr::ZYPPER: return "zypper";
        case PkgMgr::APK: return "apk";
        case PkgMgr::XBPS: return "xbps";
        case PkgMgr::PKG: return "pkg";

        default: return "unknown";
    }
}

void pkgUpdate(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT:    cmd("sudo apt update"); break;
        case PkgMgr::DNF:    cmd("sudo dnf -y makecache"); break;
        case PkgMgr::YUM:    cmd("sudo yum -y makecache"); break;
        case PkgMgr::PACMAN: cmd("sudo pacman -Sy --noconfirm"); break;
        case PkgMgr::ZYPPER: cmd("sudo zypper --non-interactive refresh"); break;
        case PkgMgr::APK:    cmd("sudo apk update"); break;
        case PkgMgr::XBPS:   cmd("sudo xbps-install -Suy"); break;
        case PkgMgr::PKG:    cmd("sudo pkg update -f"); break;

        default: std::cout << "No supported package manager found.\n"; break;
    }
}

void pkgUpgrade(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT:    cmd("sudo apt upgrade -y"); break;
        case PkgMgr::DNF:    cmd("sudo dnf -y upgrade"); break;
        case PkgMgr::YUM:    cmd("sudo yum -y update"); break;
        case PkgMgr::PACMAN: cmd("sudo pacman -Syu --noconfirm"); break;
        case PkgMgr::ZYPPER: cmd("sudo zypper --non-interactive update"); break;
        case PkgMgr::APK:    cmd("sudo apk upgrade"); break;
        case PkgMgr::XBPS:   cmd("sudo xbps-install -Suy"); break;
        case PkgMgr::PKG:    cmd("sudo pkg upgrade -y"); break;

        default: std::cout << "No supported package manager found.\n"; break;
    }
}

void pkgAutoremove(PkgMgr m) {
    switch (m) {
        case PkgMgr::APT:    cmd("sudo apt autoremove -y"); break;
        case PkgMgr::DNF:    cmd("sudo dnf -y autoremove"); break;
        case PkgMgr::YUM:    cmd("sudo yum -y autoremove"); break;
        case PkgMgr::PACMAN:
            cmd("bash -lc 'sudo pacman -Qtdq >/dev/null 2>&1 && sudo pacman -Rns --noconfirm $(pacman -Qtdq) || true'");
            break;
        case PkgMgr::ZYPPER: cmd("sudo zypper --non-interactive packages --orphaned"); break;
        case PkgMgr::APK:    break;
        case PkgMgr::XBPS:   cmd("sudo xbps-remove -o"); break;
        case PkgMgr::PKG:    cmd("sudo pkg autoremove -y"); break;

        default: break;
    }
}

void pkgInstall(PkgMgr m, const std::vector<std::string>& pkgs) {
    if (pkgs.empty()) return;

    auto join = [&](const std::string& base) {
        std::string s = base;
        for (auto& p : pkgs) s += " " + p;
        cmd(s);
    };

    switch (m) {
        case PkgMgr::APT:    join("sudo apt install -y"); break;
        case PkgMgr::DNF:    join("sudo dnf -y install"); break;
        case PkgMgr::YUM:    join("sudo yum -y install"); break;
        case PkgMgr::PACMAN: join("sudo pacman -S --noconfirm"); break;
        case PkgMgr::ZYPPER: join("sudo zypper --non-interactive install"); break;
        case PkgMgr::APK:    join("sudo apk add"); break;
        case PkgMgr::XBPS:   join("sudo xbps-install -y"); break;
        case PkgMgr::PKG:    join("sudo pkg install -y"); break;

        default: std::cout << "No supported package manager found.\n"; break;
    }
}

void pkgUninstall(PkgMgr m, const std::vector<std::string>& pkgs) {
    if (pkgs.empty()) return;

    auto join = [&](const std::string& base) {
        std::string s = base;
        for (auto& p : pkgs) s += " " + p;
        cmd(s);
    };

    switch (m) {
        case PkgMgr::APT:
            join("sudo apt purge -y");
            cmd("sudo apt autoremove -y");

            break;

        case PkgMgr::DNF:
            join("sudo dnf -y remove");
            cmd("sudo dnf -y autoremove");

            break;

        case PkgMgr::YUM:
            join("sudo yum -y remove");
            break;

        case PkgMgr::PACMAN:
            join("sudo pacman -Rns --noconfirm");
            break;

        case PkgMgr::ZYPPER:
            join("sudo zypper --non-interactive remove");
            break;

        case PkgMgr::APK:
            join("sudo apk del");
            break;

        case PkgMgr::XBPS:
            join("sudo xbps-remove -Ry");
            break;

        case PkgMgr::PKG:
            join("sudo pkg delete -y");
            cmd("sudo pkg autoremove -y");

            break;

        default:
            std::cout << "No supported package manager found.\n";
            break;
    }
}
