#pragma once

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
    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("sh"))
        return {QStringLiteral("bash"), {path}};
    if (suffix == QLatin1String("ps1"))
        return {QStringLiteral("powershell.exe"),
                {QStringLiteral("-NoProfile"), QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
                 QStringLiteral("-File"), path}};
    if (suffix == QLatin1String("py"))
        return {QStringLiteral("python"), {path}};
#endif
    return {path, {}};
}

}
