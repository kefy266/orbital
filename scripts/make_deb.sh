#!/bin/sh
# Builds a .deb from the linuxdeploy AppDir (Qt bundled), installed under /opt/orbital.
# Usage: scripts/make_deb.sh <AppDir> <version> <output.deb>
set -eu

appdir=$1
version=$2
out=$3
root=$(mktemp -d)
trap 'rm -rf "$root"' EXIT

mkdir -p "$root/DEBIAN" "$root/opt/orbital" "$root/usr/bin" \
         "$root/usr/share/applications" "$root/usr/share/icons/hicolor/scalable/apps"
cp -a "$appdir/usr" "$root/opt/orbital/"
rm -rf "$root/opt/orbital/usr/share/applications" "$root/opt/orbital/usr/share/icons"
cp -a "$appdir/apprun-hooks" "$root/opt/orbital/" 2>/dev/null || true
cp "$(dirname "$0")/../resources/orbital.svg" "$root/usr/share/icons/hicolor/scalable/apps/orbital.svg"

# Launcher: same environment the AppImage sets up, then the bundled binary.
cat > "$root/usr/bin/orbital" << 'EOF'
#!/bin/sh
export ORBITAL_PACKAGE=deb
for hook in /opt/orbital/apprun-hooks/*.sh; do
    [ -f "$hook" ] && . "$hook"
done
exec /opt/orbital/usr/bin/orbital "$@"
EOF
chmod 755 "$root/usr/bin/orbital"

cat > "$root/usr/share/applications/orbital.desktop" << 'EOF'
[Desktop Entry]
Type=Application
Name=Orbital
Comment=AI coding assistant
Exec=/usr/bin/orbital
Icon=orbital
Categories=Development;
Terminal=false
StartupWMClass=orbital
EOF

# mktemp and the extracted AppImage leave owner-only directories; packages need 755/644.
chmod -R u+rwX,go+rX,go-w "$root"
chmod 755 "$root"

size=$(du -sk "$root" | cut -f1)
cat > "$root/DEBIAN/control" << EOF
Package: orbital
Version: $version
Section: devel
Priority: optional
Architecture: amd64
Installed-Size: $size
Depends: libc6 (>= 2.35), libgl1, libegl1, libfontconfig1, libfreetype6, libx11-6, libx11-xcb1, libxcb1, libxkbcommon0, libglib2.0-0
Recommends: xdg-desktop-portal, git
Maintainer: kefy266 <kefy266@users.noreply.github.com>
Homepage: https://kefy266.github.io/orbital
Description: Sade ve güçlü masaüstü AI kod asistanı
 Orbital projenin içinde çalışır: dosyaları okur, düzenler, komut çalıştırır.
 Skill'ler ve eklentilerle genişler; ücretsiz bulut modelleriyle hazır gelir.
EOF

cat > "$root/DEBIAN/postinst" << 'EOF'
#!/bin/sh
set -e
command -v update-desktop-database >/dev/null 2>&1 && update-desktop-database -q /usr/share/applications || true
command -v gtk-update-icon-cache >/dev/null 2>&1 && gtk-update-icon-cache -q /usr/share/icons/hicolor || true
EOF
chmod 755 "$root/DEBIAN/postinst"

dpkg-deb --root-owner-group -Zxz --build "$root" "$out"
