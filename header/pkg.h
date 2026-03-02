#pragma once
#include <string>
#include <vector>

enum class PkgMgr {
    APT,
    DNF,
    YUM,
    PACMAN,
    ZYPPER,
    APK,
    XBPS,
    PKG,
    UNKNOWN
};

PkgMgr detectPkgMgr();
std::string mgrName(PkgMgr m);

void pkgUpdate(PkgMgr m);
void pkgUpgrade(PkgMgr m);
void pkgAutoremove(PkgMgr m);
void pkgInstall(PkgMgr m, const std::vector<std::string>& pkgs);
void pkgUninstall(PkgMgr m, const std::vector<std::string>& pkgs);
