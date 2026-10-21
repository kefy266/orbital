---
name: refactor
description: "Kodun okunabilirliğini ya da yapısını iyileştirme (refactor) istendiğinde: davranışı koruyarak küçük ve test edilen adımlarla ilerle"
---

# Güvenli yeniden düzenleme

- Başlamadan testlerin geçtiğini doğrula; test yoksa önce davranışı sabitleyen birkaç test yaz.
- Küçük adımlar at: yeniden adlandır, fonksiyon çıkar, tekrarı kaldır — her adımdan sonra derle ve test et.
- Genel API'yi ve davranışı değiştirme; değişmesi gerekiyorsa kullanıcıya sor.
- Biçimlendirme değişikliklerini mantık değişiklikleriyle karıştırma.
- Sonunda neyin neden değiştiğini kısa bir liste halinde özetle.
