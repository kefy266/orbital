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

  printf "${CYAN}→ /Applications klasörüne yükleniyor...${NC}\n"
  pkill -x Orbital 2>/dev/null || true
  rm -rf /Applications/Orbital.app
  cp -R "$MOUNT_DIR/Orbital.app" /Applications/

  printf "${CYAN}→ Gatekeeper karantina bayrağı temizleniyor (xattr -cr)...${NC}\n"
  xattr -cr /Applications/Orbital.app 2>/dev/null || true

  printf "${GREEN}✓ Orbital başarıyla kuruldu: /Applications/Orbital.app${NC}\n"
  printf "${BOLD}${CYAN}→ Orbital doğrudan açılıyor...${NC}\n"
  open /Applications/Orbital.app

elif [ "$OS" = "Linux" ]; then
  printf "${CYAN}→ İşletim Sistemi:${NC} Linux (%s)\n" "$ARCH"
  TARGET_DIR="${HOME}/.local/bin"
  mkdir -p "$TARGET_DIR"
  TARGET_BIN="${TARGET_DIR}/orbital"

  printf "${CYAN}→ Orbital AppImage indiriliyor...${NC}\n"
  curl -fSL --progress-bar "${RELEASE_URL}/Orbital-x86_64.AppImage" -o "$TARGET_BIN"
  chmod +x "$TARGET_BIN"

  DESKTOP_DIR="${HOME}/.local/share/applications"
  mkdir -p "$DESKTOP_DIR"
  cat > "${DESKTOP_DIR}/orbital.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Orbital
Exec=orbital
Icon=orbital
Categories=Development;Utility;
Comment=AI-powered code assistant
Terminal=false
EOF

  printf "${GREEN}✓ Orbital başarıyla kuruldu: %s${NC}\n" "$TARGET_BIN"
  printf "${BOLD}${CYAN}→ Orbital başlatılıyor...${NC}\n"
  nohup "$TARGET_BIN" >/dev/null 2>&1 &

else
  printf "${RED}Hata: Desteklenmeyen işletim sistemi: %s${NC}\n" "$OS"
  exit 1
fi

printf "\n${BOLD}${GREEN}✦ Orbital hazır! İyi çalışmalar.${NC}\n"
