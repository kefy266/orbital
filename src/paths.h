#pragma once

#include <QJsonObject>
#include <QString>

// Orbital's on-disk layout:
//   ~/.config/orbital/settings.json
//   ~/.config/orbital/skills/<name>/SKILL.md
//   ~/.config/orbital/plugins/<id>/plugin.json
namespace Paths {
QString configDir();
QString skillsDir();
QString pluginsDir();
QString settingsFile();
QString dataDir(); // ~/.local/share/orbital
QString noFolderDir(); // /orbital/no-folder or ~/.local/share/orbital/no-folder

// Copies bundled example skills/plugins from resources on first launch.
void installBundledContent();

QJsonObject loadSettings();
void saveSettings(const QJsonObject &settings);
}
