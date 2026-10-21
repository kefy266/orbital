---
name: docker
description: "Dockerfile, docker-compose ya da konteyner yapılandırması istendiğinde: küçük, güvenli ve önbellek dostu yapılandırma yaz"
---

# Docker

- Resmi ve sürümü sabitlenmiş, küçük temel imaj seç (ör. `python:3.12-slim`).
- Çok aşamalı derleme kullan; derleme araçları son imaja girmesin.
- Önbellek için önce bağımlılık dosyalarını kopyala, sonra kaynak kodu.
- Root olmayan kullanıcıyla çalıştır; gereksiz paket kurma.
- `.dockerignore` ekle (node_modules, .git, .env).
- compose'da sırları ortam dosyasından al, sağlık kontrolü (healthcheck) ekle.
