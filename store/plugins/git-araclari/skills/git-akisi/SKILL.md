---
name: git-akisi
description: "Branch açma, rebase, merge çakışması çözme gibi git işlemlerinde güvenli adımları izle"
---

# Güvenli git akışı

- İşe başlamadan `git status` ile çalışma ağacının temiz olduğunu doğrula.
- Varsayılan dalda doğrudan çalışma; `git switch -c <tür>/<kısa-ad>` ile dal aç.
- Geri alınamayan komutlardan (`reset --hard`, `push --force`, `clean -fd`) önce kullanıcıya sor.
- Çakışmada: `git diff --name-only --diff-filter=U` ile dosyaları listele, her birini oku, iki tarafın amacını koruyarak çöz, sonra derle/test et.
