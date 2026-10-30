---
name: figma-mcp
description: "Figma tasarım dosyalarını, URL'lerini veya bileşenlerini inceleme, tasarım token'larını okuma ve piksel-hassas koda dönüştürme istendiğinde: Figma MCP araçlarını kullan"
---

# Figma MCP Tasarım & Entegrasyon

Bu skill, Figma tasarımlarını MCP (Model Context Protocol) araçları üzerinden okuyup projeye uygun, temiz ve piksel-hassas koda dönüştürmek için kullanılır.

1. **Figma Bağlantılarını Anlama**:
   - Kullanıcı bir Figma URL'si (örneğin `https://www.figma.com/design/...` veya `https://www.figma.com/file/...`) veya `node-id` paylaştığında, Figma MCP araçlarını kullanarak dosya ve bileşen yapısını sorgula.
   - Tasarım ağacındaki ana çerçeveleri (frames), bileşenleri (components), grupları ve katmanları hiyerarşik olarak analiz et.

2. **Tasarım Token'larını ve Stilleri Çıkarma**:
   - **Renk Paleti**: Arka plan, metin, vurgu ve kenarlık renklerinin HEX/RGBA ve değişken (token) karşılıklarını belirle.
   - **Tipografi**: Yazı tipi ailesi, boyutu, ağırlığı (font-weight), satır yüksekliği (line-height) ve harf aralığını doğrula.
   - **Düzen ve Boşluklar (Spacing)**: Padding, margin, gap ve AutoLayout yönelimlerini (Row, Column, Wrap) tam olarak ölç.
   - **Efektler**: Border radius, gölge (box-shadow) ve opaklık değerlerini birebir aktar.

3. **Piksel-Hassas Kod Üretimi**:
   - Projenin kullandığı teknolojiye göre (QML, React, Vue, HTML/CSS, Tailwind CSS vb.) Figma tasarımını modern, temiz ve duyarlı (responsive) bileşenlere dönüştür.
   - Görsel hiyerarşiyi ve hizalamayı koru; sabit piksel değerleri yerine gerektiğinde esnek (flex/grid/layout) yapıları tercih et.

4. **MCP Bağlantı Durumu**:
   - Eğer Figma MCP sunucusu bağlıysa doğrudan sağlanan araçları (`get_file`, `get_node`, `get_comments` vb.) çağır.
   - Eğer yetkilendirme (OAuth veya Personal Access Token) gerekiyorsa, kullanıcıyı anlaşılır şekilde bilgilendir ve gerekli adımları göster.
