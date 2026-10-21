---
name: kod-inceleme
description: "Kod incelemesi (code review) istendiğinde: hataları, riskleri ve eksik testleri önem sırasına göre raporla"
---

# Kod inceleme

1. Kapsamı belirle: `git diff` (commit edilmemiş) veya kullanıcının verdiği dosyalar.
2. Her değişikliği oku; çevresindeki kodu da okuyarak bağlamı anla.
3. Şunları ara, en ciddiden başla:
   - Mantık hataları, sınır durumları (boş girdi, null, taşma, off-by-one)
   - Kaynak sızıntıları, yarış durumları, hata yönetimi eksikleri
   - Güvenlik: enjeksiyon, doğrulanmamış girdi, sızan sırlar
   - Eksik veya yetersiz testler
4. Her bulgu için: `dosya:satır`, sorun, somut senaryo, önerilen düzeltme.
5. Emin olmadığın bulguları "olası" diye işaretle; stil yorumlarını en sona bırak.
