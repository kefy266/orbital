#!/usr/bin/env bash
set -euo pipefail

# Orbital Universal One-Line Installer
# Supports: macOS (Apple Silicon & Intel) & Linux (x86_64)
# Usage: curl -fsSL https://raw.githubusercontent.com/kefy266/orbital/main/install.sh | bash

BOLD='\033[1m'
CYAN='\033[0;36m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
NC='\033[0m'

REPO="kefy266/orbital"
RELEASE_URL="https://github.com/${REPO}/releases/latest/download"

printf "${BOLD}${CYAN}✦ Orbital Kurulumu Başlatılıyor...${NC}\n"

OS="$(uname -s)"
ARCH="$(uname -m)"

if [ "$OS" = "Darwin" ]; then
  printf "${CYAN}→ İşletim Sistemi:${NC} macOS (%s)\n" "$ARCH"

  if command -v brew >/dev/null 2>&1; then
    printf "${CYAN}→ Homebrew tespit edildi, Cask üzerinden kuruluyor...${NC}\n"
    if brew install --cask kefy266/orbital/orbital; then
      printf "${GREEN}✓ Orbital Homebrew ile kuruldu: /Applications/Orbital.app${NC}\n"
      printf "${BOLD}${CYAN}→ Orbital başlatılıyor...${NC}\n"
      open /Applications/Orbital.app 2>/dev/null || true
      printf "\n${BOLD}${GREEN}✦ Orbital hazır! İyi çalışmalar.${NC}\n"
      exit 0
    fi
    printf "${YELLOW}→ Homebrew kurulumu atlandı, doğrudan disk imajı ile devam ediliyor...${NC}\n"
  fi

  if [ "$ARCH" = "x86_64" ]; then
    printf "${YELLOW}→ Intel tabanlı Mac tespit edildi.${NC}\n"
    if command -v brew >/dev/null 2>&1; then
      printf "${CYAN}→ Homebrew ile Intel mimarisine uygun olarak kuruluyor...${NC}\n"
      brew install kefy266/orbital/orbital
      printf "${GREEN}✓ Orbital başarıyla kuruldu!${NC}\n"
      open /Applications/Orbital.app 2>/dev/null || orbital || true
      exit 0
    fi
  fi

  DMG_URL="${RELEASE_URL}/Orbital-macos-arm64.dmg"
  TEMP_DIR="$(mktemp -d)"
  TEMP_DMG="${TEMP_DIR}/Orbital.dmg"
  MOUNT_DIR="${TEMP_DIR}/mount"

  cleanup() {
    if [ -d "$MOUNT_DIR" ]; then
      hdiutil detach "$MOUNT_DIR" -quiet 2>/dev/null || true
    fi
    rm -rf "$TEMP_DIR" 2>/dev/null || true
  }
  trap cleanup EXIT

  printf "${CYAN}→ Orbital paketi indiriliyor...${NC}\n"
  curl -fSL --progress-bar "$DMG_URL" -o "$TEMP_DMG"

  printf "${CYAN}→ Disk imajı bağlanıyor...${NC}\n"
  mkdir -p "$MOUNT_DIR"
  hdiutil attach "$TEMP_DMG" -mountpoint "$MOUNT_DIR" -nobrowse -quiet

  TARGET_DIR="/Applications"
  if [ ! -w "$TARGET_DIR" ]; then
    TARGET_DIR="${HOME}/Applications"
    mkdir -p "$TARGET_DIR"
  fi

  printf "${CYAN}→ %s klasörüne yükleniyor...${NC}\n" "$TARGET_DIR"
  pkill -x Orbital 2>/dev/null || true
  rm -rf "${TARGET_DIR}/Orbital.app"
  cp -R "$MOUNT_DIR/Orbital.app" "$TARGET_DIR/"

  printf "${CYAN}→ Gatekeeper karantina bayrağı temizleniyor (xattr -cr)...${NC}\n"
  xattr -cr "${TARGET_DIR}/Orbital.app" 2>/dev/null || true

  printf "${GREEN}✓ Orbital başarıyla kuruldu: %s/Orbital.app${NC}\n" "$TARGET_DIR"
  printf "${BOLD}${CYAN}→ Orbital doğrudan açılıyor...${NC}\n"
  open "${TARGET_DIR}/Orbital.app"

elif [ "$OS" = "Linux" ]; then
  printf "${CYAN}→ İşletim Sistemi:${NC} Linux (%s)\n" "$ARCH"
  TARGET_DIR="${HOME}/.local/bin"
  mkdir -p "$TARGET_DIR"
  TARGET_BIN="${TARGET_DIR}/orbital"

  printf "${CYAN}→ Orbital AppImage indiriliyor...${NC}\n"
  curl -fSL --progress-bar "${RELEASE_URL}/Orbital-x86_64.AppImage" -o "$TARGET_BIN"
  chmod +x "$TARGET_BIN"

  ICON_DIR="${HOME}/.local/share/icons/hicolor/scalable/apps"
  mkdir -p "$ICON_DIR"
  curl -fsSL "https://raw.githubusercontent.com/kefy266/orbital/main/resources/orbital.svg" -o "${ICON_DIR}/orbital.svg" || true

  DESKTOP_DIR="${HOME}/.local/share/applications"
  mkdir -p "$DESKTOP_DIR"
  cat > "${DESKTOP_DIR}/orbital.desktop" << EOF
[Desktop Entry]
Type=Application
Name=Orbital
Exec=${TARGET_BIN}
Icon=orbital
StartupWMClass=orbital
Categories=Development;Utility;
Comment=AI-powered code assistant
Terminal=false
EOF
  update-desktop-database "$DESKTOP_DIR" >/dev/null 2>&1 || true
  gtk-update-icon-cache -q "${HOME}/.local/share/icons/hicolor" >/dev/null 2>&1 || true

  printf "${GREEN}✓ Orbital başarıyla kuruldu: %s${NC}\n" "$TARGET_BIN"
  printf "${BOLD}${CYAN}→ Orbital başlatılıyor...${NC}\n"
  nohup "$TARGET_BIN" >/dev/null 2>&1 &

else
  printf "${RED}Hata: Desteklenmeyen işletim sistemi: %s${NC}\n" "$OS"
  exit 1
fi

printf "\n${BOLD}${GREEN}✦ Orbital hazır! İyi çalışmalar.${NC}\n"
