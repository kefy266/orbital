---
name: guvenlik-incelemesi
description: "Güvenlik incelemesi, açık taraması ya da güvenli kodlama istendiğinde: yaygın açık sınıflarını sistematik olarak kontrol et"
---

# Güvenlik incelemesi

Kontrol listesi (en ciddiden başla, her bulgu için dosya:satır ve somut saldırı senaryosu ver):
- Enjeksiyon: SQL, kabuk komutu, yol (path traversal), şablon, deserialization.
- Kimlik doğrulama/yetkilendirme: eksik kontrol, IDOR, oturum yönetimi.
- Sırlar: koda gömülü anahtarlar, loglara düşen parolalar, .env dosyaları.
- Girdi doğrulama ve çıktı kaçışlama (XSS).
- Bağımlılıklar: bilinen açıklı sürümler.
- Kriptografi: zayıf algoritma, sabit IV/tuz, kendi yazılmış kripto.
Emin olmadığın bulguları "olası" diye işaretle; düzeltme önerisini kısa tut.
