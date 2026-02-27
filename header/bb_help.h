#pragma once
#include <string>

enum class PkgMgr;

void basicHelp(PkgMgr pm);
void helpFor(const std::string& command, PkgMgr pm);
