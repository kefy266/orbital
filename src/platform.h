#pragma once

#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>

// Small cross-platform helpers for running commands (Linux, macOS, Windows).
namespace Platform {

struct Command
{
    QString program;
    QStringList arguments;
};

// A shell command line: bash on Unix, PowerShell on Windows.
inline Command shell(const QString &commandLine)
{
#ifdef Q_OS_WIN
    return {QStringLiteral("powershell.exe"),
            {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"), QStringLiteral("-Command"), commandLine}};
#else
    return {QStringLiteral("/bin/bash"), {QStringLiteral("-c"), commandLine}};
#endif
}

// An executable shipped by a plugin (tool or hook). Shell scripts need an
// interpreter on Windows: use bash (Git for Windows) from PATH; .ps1 files run in PowerShell.
inline Command script(const QString &path)
{
#ifdef Q_OS_WIN
    const QFileInfo fi(path);
    const QString suffix = fi.suffix().toLower();
    if (suffix == QLatin1String("sh")) {
        // If a .ps1 with the same base name exists, prefer PowerShell on Windows
        const QString ps1 = fi.dir().filePath(fi.completeBaseName() + QStringLiteral(".ps1"));
        if (QFile::exists(ps1)) {
            return {QStringLiteral("powershell.exe"),
                    {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                     QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                     QStringLiteral("-File"), QDir::toNativeSeparators(ps1)}};
        }
        // If it's check.sh, run regex validation directly via PowerShell without requiring Git Bash
        if (fi.fileName() == QLatin1String("check.sh")) {
            return {QStringLiteral("powershell.exe"),
                    {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                     QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                     QStringLiteral("-Command"),
                     QStringLiteral("$in = [Console]::In.ReadToEnd(); if ($in -match '(sk-[A-Za-z0-9_-]{20,}|sk-ant-[A-Za-z0-9_-]{20,}|AKIA[0-9A-Z]{16}|ghp_[A-Za-z0-9]{36}|-----BEGIN [A-Z ]*PRIVATE KEY-----)') { [Console]::Error.WriteLine('Mesajda bir API anahtarı veya özel anahtar var gibi görünüyor; güvenlik için gönderilmedi.'); exit 1 } else { exit 0 }")}};
        }
        // Try common Git Bash locations before falling back to plain 'bash'
        const QString home = QDir::homePath();
        const QStringList gitBashPaths = {
            QStringLiteral("bash"),
            QStringLiteral("C:\\Program Files\\Git\\bin\\bash.exe"),
            QStringLiteral("C:\\Program Files (x86)\\Git\\bin\\bash.exe"),
            home + QStringLiteral("/AppData/Local/Programs/Git/bin/bash.exe")
        };
        for (const QString &p : gitBashPaths) {
            if (p != QLatin1String("bash") && QFile::exists(p))
                return {p, {path}};
        }
        return {QStringLiteral("bash"), {path}};
    }
    if (suffix == QLatin1String("ps1"))
        return {QStringLiteral("powershell.exe"),
                {QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
                 QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                 QStringLiteral("-File"), QDir::toNativeSeparators(path)}};
    if (suffix == QLatin1String("py"))
        return {QStringLiteral("python"), {path}};
#endif
    return {path, {}};
}

}
