#!/usr/bin/env bash
set -euo pipefail

need_cmd() { command -v "$1" >/dev/null 2>&1; }

install_gpp() {
  if need_cmd apt-get; then
    sudo apt-get update
    sudo apt-get install -y g++
    sudo apt-get install -y figlet
    sudo apt-get install -y lolcat
    return
  fi
  if need_cmd dnf; then
    sudo dnf -y install figlet
    sudo dnf -y install g++
    sudo dnf -y install lolcat
    return
  fi
  if need_cmd pacman; then
    sudo pacman -S --noconfirm figlet
    sudo pacman -S --noconfirm g++
    sudo pacman -S --noconfirm lolcat
    return
  fi
  if need_cmd zypper; then
    sudo zypper --non-interactive install -y figlet
    sudo zypper --non-interactive install -y g++
    sudo zypper --non-interactive install -y lolcat
    return
  fi
  if need_cmd apk; then
    sudo apk add figlet
    sudo apk add g++
    sudo apk add lolcat
    return
  fi

  echo "Kein unterstützter Paketmanager gefunden. Bitte g++, lolcat und figlet manuell installieren."
  exit 1
}

# deps
if ! need_cmd g++; then
  install_gpp
fi

if ! need_cmd figlet; then
  install_gpp
fi

if ! need_cmd lolcat; then
  install_gpp
fi

# build
g++ *.cpp -Iheader -O2 -o biba

# install
sudo install -m 0755 biba /usr/local/bin/biba
sudo install -m 0755 biba /sbin/biba

clear
echo "Bitch Batch installiert - starte mit: biba"