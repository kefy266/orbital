---
name: hata-ayiklama
description: "Bir hata, çökme, beklenmeyen çıktı ya da başarısız test incelenirken: yeniden üret, kök nedeni bul, en küçük düzeltmeyi yap"
---

# Sistematik hata ayıklama

1. Hatayı yeniden üret: tam komutu, girdiyi ve beklenen/gerçek çıktıyı yaz.
2. Hata mesajı ve yığın izinden başla; ilgili kodu oku, tahmin yürütme.
3. Hipotez kur ve en ucuz deneyle sına (log, küçük betik, tek test). Bir seferde tek değişken değiştir.
4. Kök nedeni bulunca en küçük düzeltmeyi yap; belirtiyi bastırma.
5. Hatayı yakalayan bir test ekle, sonra tüm testleri çalıştır.
6. Özette: neden oldu, ne değişti, nasıl doğrulandı.
