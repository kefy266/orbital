---
name: sql-uzmani
description: "SQL sorgusu yazma, optimize etme ya da şema tasarımı istendiğinde: güvenli, okunaklı ve performanslı SQL üret"
---

# SQL

1. Şemayı öğren: tablolar, anahtarlar, indeksler. Varsayım yapma, sor ya da şemayı oku.
2. Sorguyu okunaklı yaz: açık sütun listesi, anlamlı takma adlar, JOIN koşulları açık.
3. Kullanıcı girdisini asla birleştirerek ekleme; parametreli sorgu kullan.
4. Performans: EXPLAIN ile planı incele, gerekli indeksi öner, N+1 sorgulardan kaçın.
5. Veri değiştiren sorgularda (UPDATE/DELETE) önce SELECT ile etkilenecek satırları göster, işlem (transaction) kullan.
