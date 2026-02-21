#!/usr/bin/env bash
set -euo pipefail

need_cmd() { command -v "$1" >/dev/null 2>&1; }

install_gpp() {
  if need_cmd apt-get; then
    sudo apt-get update
    sudo apt-get install -y g++
    return
  fi
  if need_cmd dnf; then
    sudo dnf -y install gcc-c++
    return
  fi
  if need_cmd pacman; then
    sudo pacman -S --noconfirm gcc
    return
  fi
  if need_cmd zypper; then
    sudo zypper --non-interactive install -y gcc-c++
    return
  fi
  if need_cmd apk; then
    sudo apk add g++
    return
  fi

  echo "Kein unterstützter Paketmanager gefunden. Bitte g++ manuell installieren."
  exit 1
}

# deps
if ! need_cmd g++; then
  install_gpp
fi

# build
g++ *.cpp -Iheader -O2 -o biba

# install
sudo install -m 0755 biba /usr/local/bin/biba

clear
echo "Bitch Batch installiert - starte mit: biba"