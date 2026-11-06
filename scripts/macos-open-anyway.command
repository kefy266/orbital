#!/bin/bash
clear
echo "========================================================"
echo "      Orbital — macOS Güvenlik İzni & Başlatıcı"
echo "========================================================"
echo ""
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
APP_TARGET="/Applications/Orbital.app"
DMG_APP="$DIR/Orbital.app"

if [ ! -d "$APP_TARGET" ] && [ -d "$DMG_APP" ]; then
    echo "1/3 Orbital /Applications klasörüne kopyalanıyor..."
    cp -R "$DMG_APP" /Applications/
fi

if [ -d "$APP_TARGET" ]; then
    echo "2/3 Apple Gatekeeper güvenlik kilidi (karantina) kaldırılıyor..."
    xattr -cr "$APP_TARGET" 2>/dev/null
    echo "3/3 Orbital başlatılıyor..."
    open "$APP_TARGET"
    echo ""
    echo "✓ Orbital başarıyla başlatıldı!"
    echo "Artık bu pencereyi kapatabilirsiniz."
else
    echo "Hata: Orbital.app bulunamadı."
fi
exit 0
