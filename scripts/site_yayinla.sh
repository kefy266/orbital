#!/usr/bin/env bash
# Siteyi (docs/) orbital.oedge.xyz'ye yükler: index.html, assets/, store/, install.sh.
# Güncelleme dosyaları (/var/www/orbital/guncelleme) ve /api, /v1 vekilleri dokunulmadan kalır.
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/build_site.py
SUNUCU="${ORBITAL_SITE_SUNUCU:-root@213.142.159.20}"
rsync -rlt --delete --chmod=D755,F644 docs/assets docs/store "$SUNUCU:/var/www/orbital/"
rsync -lt --chmod=F644 docs/index.html docs/install.sh "$SUNUCU:/var/www/orbital/"
echo "Yüklendi: https://orbital.oedge.xyz/"
