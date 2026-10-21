---
name: test-yazimi
description: "Test yazılması ya da test kapsamının artırılması istendiğinde: projenin test düzenine uygun birim testleri yaz"
---

# Test yazımı

1. Projenin test çatısını ve mevcut test dosyalarının yerini/adlandırmasını bul; aynı düzeni izle.
2. Test edilecek birimin davranışını oku: girdiler, çıktılar, yan etkiler, hata yolları.
3. Her davranış için bir test yaz: olağan durum, sınır değerler (boş, sıfır, en büyük), hatalı girdi.
4. Testler birbirinden bağımsız olsun; zamana, ağa ve rastgeleliğe bağlıysa sahte (mock/fake) kullan.
5. Test adları neyi doğruladığını söylesin (`bos_listede_sifir_dondurur`).
6. Testleri çalıştır; başarısız olanı düzeltirken üretim kodunu değil, gerçek bir hata bulduysan onu raporla.
