# ◉ Orbital

[![Build](https://github.com/kefy266/orbital/actions/workflows/build.yml/badge.svg)](https://github.com/kefy266/orbital/actions/workflows/build.yml) · **Site:** https://kefy266.github.io/orbital

**Skill ve eklenti destekli, sade bir masaüstü AI kod asistanı.** Qt Quick (QML) ile yazıldı, sistemin açık/koyu temasını izler. İsteğe bağlı EdgeAccount girişiyle API anahtarı gerektirmeyen ORBITAL Cloud modelleriyle çalışır; istersen kendi OpenAI, Anthropic, Gemini… anahtarlarını bağlayabilirsin.

## Özellikler

- **ORBITAL Cloud (EdgeAccount):** Ayarlar → Genel → *EdgeAccount ile giriş yap*. GPT-OSS 120B/20B ve Qwen 3.8 modelleri API anahtarı olmadan, ücretsiz (günlük adil kullanım sınırıyla); istekler orbital.oedge.xyz üzerinden geçer.
- **Süreli model kilitleri:** Kota/limit hatası alan model 🔒 ile kilitlenir ve sağlayıcının bildirdiği süre (yoksa 5 dk–6 sa) dolunca kendiliğinden açılır; `/kilit-ac` ile hemen açılabilir.
- **Kendi sağlayıcıların:** Ayarlar → Sağlayıcılar'dan OpenAI, Anthropic, Google Gemini, OpenRouter, DeepSeek, Groq, Mistral, xAI ya da herhangi bir OpenAI uyumlu adres. Anahtar bağlanmadan önce test edilir.
- **Gerçek bir kod ajanı:** dosya okur, yazar, düzenler, komut çalıştırır, arama yapar. Riskli adımlardan önce izin ister; araç başına izin ayarlanabilir.
- **Skill'ler:** modelin belirli işleri nasıl yapacağını anlatan `SKILL.md` paketleri. Model gerektiğinde kendisi yükler, ya da `/skill-adı` ve `+` menüsüyle mesaja iliştirirsin.
- **Eklentiler:** tek bir `plugin.json` ile slash komutları, MCP araç sunucuları, betik araçları, paketlenmiş skill'ler, sistem talimatları, eylemler ve prompt hook'ları.
- **Mağaza:** skill ve eklentileri uygulamanın içinden tek tıkla yükle/kaldır. Katalog bu depodaki `store/` klasöründedir.
- **Arayüz:** gruplu sohbet geçmişi, akan markdown, KDE renklendirmeli kod blokları, açılır araç kartları, Build/Plan modları.
- **Otomatik güncelleme:** Linux (AppImage), Windows ve macOS'ta yeni sürümler uygulama içinden (Ayarlar → Genel) tek tıkla denetlenir ve güncellenir. Sürüm bilgisi ve paketler `https://orbital.oedge.xyz/guncelleme` adresinden gelir.

## Kurulum

### macOS (Homebrew veya Tek Satır)

Homebrew ile tek komutla kurabilirsiniz (Gatekeeper sorunu olmadan doğrudan `/Applications` klasörüne eklenir):

```bash
brew install --cask kefy266/orbital/orbital
```

Veya terminalden tek satırla kurup açın:

```bash
curl -fsSL https://raw.githubusercontent.com/kefy266/orbital/main/install.sh | bash
```

> [!TIP]
> DMG'yi doğrudan indirip açtığınızda macOS *"Orbital hasarlı olduğu için açılamıyor"* uyarısı verirse Terminal'e şu komutu yazarak karantina bayrağını temizleyebilirsiniz:
> ```bash
> xattr -cr /Applications/Orbital.app
> ```

### Hazır paketler

Hazır paketler [Sürümler](https://github.com/kefy266/orbital/releases/latest) sayfasında da mevcuttur. Windows ve macOS paketleri Qt'yi içinde taşır:

| Sistem | Paket | Not |
|---|---|---|
| Windows 10/11 (x64) | [`Orbital-windows-x64.zip`](https://github.com/kefy266/orbital/releases/latest/download/Orbital-windows-x64.zip) | Zip'i aç, `Orbital.exe`'yi çalıştır. SmartScreen uyarısında **Ek bilgi → Yine de çalıştır**. Dahili otomatik güncelleme. |
| macOS 12+ (Apple Silicon) | [`Orbital-macos-arm64.dmg`](https://github.com/kefy266/orbital/releases/latest/download/Orbital-macos-arm64.dmg) | Homebrew (`--cask`), `install.sh` veya DMG ile kurulum. Dahili otomatik güncelleme. |
| Linux (x86_64) | [`Orbital-x86_64.AppImage`](https://github.com/kefy266/orbital/releases/latest/download/Orbital-x86_64.AppImage) · [`.tar.gz`](https://github.com/kefy266/orbital/releases/latest/download/orbital-linux-x86_64.tar.gz) | AppImage kurulum gerektirmez (`chmod +x` verip çalıştır). Dahili otomatik güncelleme. |

İlk açılışta Orbital bulut modellerini hazırlamak için motorunu bir kez indirir (yaklaşık 60–180 MB).

### Kaynaktan derleme

**Linux** (Arch/CachyOS paket adları):

```bash
sudo pacman -S qt6-base qt6-declarative qt6-svg syntax-highlighting base-devel
```

```bash
qmake6 orbital.pro -o build.mk && make -f build.mk -j$(nproc) && ./build/orbital
```

**macOS** ([Qt 6.10+](https://www.qt.io/download-qt-installer) ya da `brew install qt`):

```bash
qmake CONFIG+=release orbital.pro -o build.mk && make -f build.mk -j8 && open build/Orbital.app
```

**Windows** (Qt 6.10+ MSVC 2022 ve Visual Studio; "x64 Native Tools" komut isteminde):

```bat
qmake CONFIG+=release orbital.pro -o build.mk && nmake /f build.mk && build\Orbital.exe
```

Kod renklendirmesi KDE'nin KSyntaxHighlighting kütüphanesini kullanır; bulunmadığı sistemlerde kod blokları renksiz gösterilir. Windows'ta `.sh` uzantılı eklenti betikleri için Git for Windows'un `bash`'i gerekir; eklentiler `.ps1` ya da `.py` betikleri de kullanabilir.

Her commit'te [GitHub Actions](https://github.com/kefy266/orbital/actions) üç platformda derler ve test eder; `v` ile başlayan her etiket bu üç paketle otomatik bir sürüm oluşturur.

## Mağazaya katkı

1. `store/skills/<id>/SKILL.md` ya da `store/plugins/<id>/plugin.json` (ve eklentinin dosyaları) ekle.
2. `store/index.json` içine öğeyi `id`, `type`, `name`, `description`, `version`, `tags` ve `files` alanlarıyla yaz.
3. `python scripts/build_site.py` ile siteyi güncelle ve PR aç.

## Mimari

```
src/
  engine.h            ortak motor arayüzü
  nativeengine.*      Orbital Core: model API'lerine doğrudan bağlanan ajan döngüsü (araçlar, skill, eklenti, MCP)
  edgeaccount.*       EdgeAccount girişi (OIDC + PKCE, 127.0.0.1 dönüşü) ve ORBITAL Cloud oturumu
  llm.*               OpenAI uyumlu ve Anthropic akışlı istemci
  tools.* mcpclient.* dosya/kabuk/arama araçları, MCP istemcisi
  skillmanager.* pluginmanager.*
  app/                QML köprüsü (AppController, sohbet/oturum modelleri, ikon sağlayıcı)
resources/qml/        arayüz (Main, Sidebar, ChatView, Composer, SettingsDialog…)
resources/skills|plugins/  ilk açılışta kurulan örnekler
```

ORBITAL Cloud, EdgeAccount ile giriş yapan kullanıcıların isteklerini `orbital.oedge.xyz` üzerinden model sağlayıcısına iletir; sağlayıcı anahtarları yalnızca sunucuda durur, kullanıcı başına adil kullanım kotası orada uygulanır.

## Dosyalar

| Yol | İçerik |
|---|---|
| `~/.config/orbital/config.json` | Sağlayıcılar ve API anahtarları, izinler |
| `~/.config/orbital/skills/<ad>/SKILL.md` | Skill'ler |
| `~/.config/orbital/plugins/<id>/plugin.json` | Eklentiler |
| `~/.local/share/orbital/` | Sohbetler ve motor dosyaları |

API anahtarların yalnızca bu dosyada, senin bilgisayarında durur; depoya girmez.

## Skill biçimi

```markdown
---
name: kod-inceleme
description: "Kod incelemesi istendiğinde: hataları önem sırasına göre raporla"
---

# Talimatlar
...
```

## Eklenti biçimi

```json
{
  "id": "benim-eklentim",
  "name": "Benim Eklentim",
  "version": "1.0.0",
  "skills": "skills",
  "commands": { "ozetle": { "description": "Dosyayı özetle", "template": "Summarize $1. $ARGUMENTS" } },
  "tools": [{ "name": "sistem_bilgisi", "description": "…", "parameters": { "type": "object", "properties": {} }, "run": "tools/sysinfo.sh" }],
  "mcp": { "fetch": { "type": "local", "command": ["uvx", "mcp-server-fetch"] } },
  "instructions": ["kurallar.md"],
  "actions": [{ "label": "Git durumu", "run": "git status --short" }],
  "hooks": { "beforePrompt": "hooks/check.sh" }
}
```

Örnek eklentiler ilk açılışta kurulur: Git Araçları, Sistem Araçları, Gizli Anahtar Koruması ve Web Fetch (MCP, varsayılan kapalı).

## Kısayollar

| Tuş | İşlev |
|---|---|
| Enter / Shift+Enter | Gönder / yeni satır |
| Ctrl+N | Yeni sohbet |
| Ctrl+B | Kenar çubuğu |
| Ctrl+L | Mesaj kutusu |
| Ctrl+, | Ayarlar |
| Esc | Yanıtı durdur |
