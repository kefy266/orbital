---
name: commit-mesaji
description: "Commit mesajı yazılması istendiğinde: değişikliklerden Conventional Commits biçiminde mesaj üret"
---

# Commit mesajı

1. `git diff --staged` çalıştır; boşsa `git diff` kullan.
2. Biçim: `<tür>(<kapsam>): <özet>` — türler: feat, fix, refactor, docs, test, chore, perf, build, ci.
3. Özet 72 karakteri geçmesin, emir kipinde olsun, nokta ile bitmesin.
4. Gerekirse boş satırdan sonra *neden* yapıldığını anlatan kısa bir gövde ekle.
5. Commit'i kullanıcı açıkça istemedikçe kendin atma; sadece mesajı öner.
