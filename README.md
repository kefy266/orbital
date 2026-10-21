# ◉ Orbital

**Skill ve eklenti destekli, sade bir masaüstü AI kod asistanı.** Qt Quick (QML) ile yazıldı, sistemin açık/koyu temasını izler. Kurulumdan hemen sonra API anahtarı gerektirmeyen ücretsiz bulut modelleriyle çalışır; istersen kendi OpenAI, Anthropic, Gemini… anahtarlarını bağlayabilirsin.

## Özellikler

- **Hazır bulut modelleri (ORBITAL Cloud):** DeepSeek, GLM, MiMo, Nemotron, MiniMax gibi ücretsiz modeller, anahtarsız.
- **Kendi sağlayıcıların:** Ayarlar → Sağlayıcılar'dan OpenAI, Anthropic, Google Gemini, OpenRouter, DeepSeek, Groq, Mistral, xAI ya da herhangi bir OpenAI uyumlu adres. Anahtar bağlanmadan önce test edilir.
- **Gerçek bir kod ajanı:** dosya okur, yazar, düzenler, komut çalıştırır, arama yapar. Riskli adımlardan önce izin ister; araç başına izin ayarlanabilir.
- **Skill'ler:** modelin belirli işleri nasıl yapacağını anlatan `SKILL.md` paketleri. Model gerektiğinde kendisi yükler, ya da `/skill-adı` ve `+` menüsüyle mesaja iliştirirsin.
- **Eklentiler:** tek bir `plugin.json` ile slash komutları, MCP araç sunucuları, betik araçları, paketlenmiş skill'ler, sistem talimatları, eylemler ve prompt hook'ları.
- **Arayüz:** gruplu sohbet geçmişi, akan markdown, KDE renklendirmeli kod blokları, açılır araç kartları, Build/Plan modları.

## Derleme

Gerekenler (Arch/CachyOS paket adları): `qt6-base qt6-declarative qt6-svg syntax-highlighting` ve bir C++17 derleyicisi.

```bash
qmake6 orbital.pro -o build.mk && make -f build.mk -j$(nproc)
```

```bash
./build/orbital
```

## Mimari

```
src/
  engine.h            ortak motor arayüzü
  nativeengine.*      Orbital Core: model API'lerine doğrudan bağlanan ajan döngüsü
  opencodeengine.*    ORBITAL Cloud: opencode altyapısının yalıtılmış, Orbital'e ait kopyası
  llm.*               OpenAI uyumlu ve Anthropic akışlı istemci
  tools.* mcpclient.* dosya/kabuk/arama araçları, MCP istemcisi
  skillmanager.* pluginmanager.*
  app/                QML köprüsü (AppController, sohbet/oturum modelleri, ikon sağlayıcı)
resources/qml/        arayüz (Main, Sidebar, ChatView, Composer, SettingsDialog…)
resources/skills|plugins/  ilk açılışta kurulan örnekler
```

ORBITAL Cloud motoru, [opencode](https://github.com/anomalyco/opencode) sunucusunu Orbital'in kendi klasörüne (`~/.local/share/orbital/engine`) kurar ve ayrı ayarlarla çalıştırır. Bilgisayarındaki opencode kurulumuna dokunmaz.

> [!NOTE]
> **Bağımsızlık notu:** Orbital, OpenCode ekibi tarafından geliştirilmemiştir ve OpenCode ile hiçbir şekilde bağlantılı değildir. Orbital, açık kaynak opencode projesini altyapı olarak kullanan bağımsız bir projedir.
>
> *Orbital is not built by the OpenCode team and is not affiliated with OpenCode in any way. It is an independent project that uses the open-source opencode project as part of its infrastructure.*

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
