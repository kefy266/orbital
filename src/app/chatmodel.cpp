#include "chatmodel.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QRegularExpression>
#include "../paths.h"

namespace {

// Browser tools arrive as "<server>_browser_<action>" from the web browser plugin.
QString browserAction(const QString &name)
{
    const int at = name.indexOf(QStringLiteral("browser_"));
    return at < 0 ? QString() : name.mid(at + 8);
}

QString toolTitle(const QString &name)
{
    const QString browser = browserAction(name);
    if (!browser.isEmpty()) {
        static const QHash<QString, QString> actions{
            {QStringLiteral("navigate"), QStringLiteral("Sayfa açıldı")},
            {QStringLiteral("navigate_back"), QStringLiteral("Geri gidildi")},
            {QStringLiteral("snapshot"), QStringLiteral("Sayfa okundu")},
            {QStringLiteral("take_screenshot"), QStringLiteral("Ekran görüntüsü alındı")},
            {QStringLiteral("click"), QStringLiteral("Tıklandı")},
            {QStringLiteral("type"), QStringLiteral("Yazıldı")},
            {QStringLiteral("fill_form"), QStringLiteral("Form dolduruldu")},
            {QStringLiteral("select_option"), QStringLiteral("Seçim yapıldı")},
            {QStringLiteral("press_key"), QStringLiteral("Tuşa basıldı")},
            {QStringLiteral("hover"), QStringLiteral("İmleç getirildi")},
            {QStringLiteral("wait_for"), QStringLiteral("Sayfa beklendi")},
            {QStringLiteral("tabs"), QStringLiteral("Sekmeler")},
            {QStringLiteral("find"), QStringLiteral("Sayfada arandı")},
            {QStringLiteral("close"), QStringLiteral("Tarayıcı kapatıldı")},
        };
        return actions.value(browser, QStringLiteral("Tarayıcı"));
    }
    static const QHash<QString, QString> titles{
        {QStringLiteral("read"), QStringLiteral("Dosya okundu")},
        {QStringLiteral("write"), QStringLiteral("Dosya yazıldı")},
        {QStringLiteral("edit"), QStringLiteral("Dosya düzenlendi")},
        {QStringLiteral("bash"), QStringLiteral("Komut çalıştırıldı")},
        {QStringLiteral("shell"), QStringLiteral("Komut çalıştırıldı")},
        {QStringLiteral("list"), QStringLiteral("Klasör listelendi")},
        {QStringLiteral("glob"), QStringLiteral("Dosyalar arandı")},
        {QStringLiteral("grep"), QStringLiteral("İçerik arandı")},
        {QStringLiteral("skill"), QStringLiteral("Skill yüklendi")},
        {QStringLiteral("webfetch"), QStringLiteral("Web sayfası okundu")},
        {QStringLiteral("todowrite"), QStringLiteral("Yapılacaklar güncellendi")},
        {QStringLiteral("task"), QStringLiteral("Alt görev")},
    };
    return titles.value(name, name);
}

QString toolIcon(const QString &name)
{
    if (!browserAction(name).isEmpty())
        return QStringLiteral("globe");
    if (name == QLatin1String("read"))
        return QStringLiteral("file");
    if (name == QLatin1String("write") || name == QLatin1String("edit") || name == QLatin1String("patch"))
        return QStringLiteral("pencil");
    if (name == QLatin1String("bash") || name == QLatin1String("shell"))
        return QStringLiteral("terminal");
    if (name == QLatin1String("list") || name == QLatin1String("glob"))
        return QStringLiteral("folder");
    if (name == QLatin1String("grep") || name == QLatin1String("websearch"))
        return QStringLiteral("search");
    if (name == QLatin1String("skill"))
        return QStringLiteral("sparkle");
    if (name == QLatin1String("webfetch"))
        return QStringLiteral("globe");
    if (name.startsWith(QLatin1String("todo")))
        return QStringLiteral("list");
    return QStringLiteral("blocks");
}

QString summarize(const QJsonObject &input)
{
    for (const char *key : {"command", "path", "filePath", "pattern", "name", "url", "query", "description"}) {
        const QString v = input.value(QLatin1String(key)).toString();
        if (!v.isEmpty())
            return v.simplified();
    }
    return input.isEmpty() ? QString() : QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Compact));
}

}

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent)
{
    // Streaming deltas arrive token by token; re-split markdown at most every 50 ms.
    m_throttle.setSingleShot(true);
    m_throttle.setInterval(50);
    connect(&m_throttle, &QTimer::timeout, this, &ChatModel::flushDirty);
}

int ChatModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_items.size());
}

QHash<int, QByteArray> ChatModel::roleNames() const
{
    return {{KindRole, "kind"},           {TextRole, "text"},
            {SegmentsRole, "segments"},   {SkillsRole, "skills"},
            {ToolNameRole, "toolName"},   {ToolTitleRole, "toolTitle"},
            {ToolSubtitleRole, "toolSubtitle"}, {ToolStatusRole, "toolStatus"},
            {ToolDetailsRole, "toolDetails"},   {ToolIconRole, "toolIcon"},
            {LastInTurnRole, "lastInTurn"}};
}

QVariant ChatModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_items.size())
        return {};
    const Item &it = m_items.at(index.row());
    switch (role) {
    case KindRole: return it.kind;
    case TextRole: return it.text;
    case SegmentsRole: return it.segments;
    case SkillsRole: return it.skills;
    case ToolNameRole: return it.toolName;
    case ToolTitleRole: return toolTitle(it.toolName);
    case ToolSubtitleRole: return it.toolSubtitle;
    case ToolStatusRole: return it.toolStatus;
    case ToolIconRole: return toolIcon(it.toolName);
    case ToolDetailsRole: {
        QString d = it.toolInput;
        if (!it.toolOutput.isEmpty())
            d += (d.isEmpty() ? QString() : QStringLiteral("\n\n")) + it.toolOutput;
        return d;
    }
    case LastInTurnRole: {
        // The last assistant text row before the next user message gets the action bar.
        if (it.kind != QLatin1String("text"))
            return false;
        for (int i = index.row() + 1; i < m_items.size(); ++i) {
            if (m_items.at(i).kind == QLatin1String("user"))
                return true;
            if (m_items.at(i).kind == QLatin1String("text"))
                return false;
        }
        return true;
    }
    default: return {};
    }
}

// Turns bare http(s):// and www. addresses into markdown autolinks so they are
// clickable; leaves inline code, existing [links](…) and <autolinks> alone.
static QString linkifyMarkdown(const QString &prose)
{
    static const QRegularExpression url(
        QStringLiteral("(?<![\\w/<(\\[\"'=])((?:https?://|www\\.)[^\\s<>()\\[\\]`\"']*[^\\s<>()\\[\\]`\"'.,;:!?*_])"),
        QRegularExpression::CaseInsensitiveOption);
    QString out;
    const QStringList parts = prose.split(QLatin1Char('`')); // odd parts are inline code
    for (int i = 0; i < parts.size(); ++i) {
        if (i > 0)
            out += QLatin1Char('`');
        if (i % 2 == 1) {
            out += parts.at(i);
            continue;
        }
        QString part = parts.at(i);
        QString result;
        qsizetype last = 0;
        auto it = url.globalMatch(part);
        while (it.hasNext()) {
            const auto m = it.next();
            // Skip URLs that already sit inside a markdown link target "](…)".
            if (m.capturedStart() >= 2 && part.mid(m.capturedStart() - 2, 2) == QLatin1String("]("))
                continue;
            result += part.mid(last, m.capturedStart() - last);
            const QString target = m.captured(1).startsWith(QStringLiteral("www."), Qt::CaseInsensitive)
                                       ? QStringLiteral("https://") + m.captured(1) : m.captured(1);
            result += QStringLiteral("[%1](%2)").arg(m.captured(1), target);
            last = m.capturedEnd();
        }
        result += part.mid(last);
        out += result;
    }
    return out;
}

QVariantList ChatModel::splitMarkdown(const QString &markdown)
{
    // Separate fenced code blocks (``` or ~~~) from prose; an unterminated
    // fence while streaming still renders as a code block.
    QVariantList out;
    QString prose;
    QString code;
    QString lang;
    QString fence;
    const auto flushProse = [&] {
        if (!prose.trimmed().isEmpty())
            out << QVariantMap{{QStringLiteral("type"), QStringLiteral("text")},
                               {QStringLiteral("text"), linkifyMarkdown(prose.trimmed())}};
        prose.clear();
    };
    static const QRegularExpression open(QStringLiteral("^\\s{0,3}(```+|~~~+)\\s*([^`\\s]*)"));
    for (const QString &line : markdown.split(QLatin1Char('\n'))) {
        if (fence.isEmpty()) {
            const auto m = open.match(line);
            if (m.hasMatch()) {
                flushProse();
                fence = m.captured(1);
                lang = m.captured(2);
                code.clear();
            } else {
                prose += line + QLatin1Char('\n');
            }
        } else if (line.trimmed().startsWith(fence) && line.trimmed().remove(fence.at(0)).isEmpty()) {
            if (code.endsWith(QLatin1Char('\n')))
                code.chop(1);
            out << QVariantMap{{QStringLiteral("type"), QStringLiteral("code")}, {QStringLiteral("text"), code},
                               {QStringLiteral("lang"), lang}};
            fence.clear();
        } else {
            code += line + QLatin1Char('\n');
        }
    }
    if (!fence.isEmpty()) {
        if (code.endsWith(QLatin1Char('\n')))
            code.chop(1);
        out << QVariantMap{{QStringLiteral("type"), QStringLiteral("code")}, {QStringLiteral("text"), code},
                           {QStringLiteral("lang"), lang}};
    }
    flushProse();
    return out;
}

int ChatModel::indexOfKey(const QString &key) const
{
    for (int i = int(m_items.size()) - 1; i >= 0; --i)
        if (m_items.at(i).key == key)
            return i;
    return -1;
}

int ChatModel::append(const Item &item)
{
    const int row = int(m_items.size());
    beginInsertRows({}, row, row);
    m_items << item;
    endInsertRows();
    if (row > 0) // previous row may have lost "lastInTurn"
        Q_EMIT dataChanged(index(row - 1), index(row - 1), {LastInTurnRole});
    Q_EMIT countChanged();
    return row;
}

void ChatModel::clear()
{
    beginResetModel();
    m_items.clear();
    m_dirty.clear();
    endResetModel();
    Q_EMIT countChanged();
}

void ChatModel::addUser(const QString &text, const QStringList &skills)
{
    Item it;
    it.kind = QStringLiteral("user");
    it.text = text;
    it.skills = skills;
    append(it);
}

void ChatModel::appendText(const QString &key, const QString &delta)
{
    int row = indexOfKey(key);
    if (row < 0) {
        Item it;
        it.kind = QStringLiteral("text");
        it.key = key;
        row = append(it);
    }
    m_items[row].text += delta;
    m_dirty.insert(key);
    if (!m_throttle.isActive())
        m_throttle.start();
}

void ChatModel::flushDirty()
{
    for (const QString &key : std::as_const(m_dirty)) {
        const int row = indexOfKey(key);
        if (row < 0)
            continue;
        m_items[row].segments = splitMarkdown(m_items.at(row).text);
        Q_EMIT dataChanged(index(row), index(row), {TextRole, SegmentsRole});
    }
    m_dirty.clear();
}

void ChatModel::setText(const QString &key, const QString &text)
{
    int row = indexOfKey(key);
    if (row < 0) {
        if (text.trimmed().isEmpty())
            return;
        Item it;
        it.kind = QStringLiteral("text");
        it.key = key;
        row = append(it);
    }
    if (text.trimmed().isEmpty()) {
        // e.g. a local model's JSON tool call that was turned into a real call
        beginRemoveRows({}, row, row);
        m_items.removeAt(row);
        endRemoveRows();
        Q_EMIT countChanged();
        return;
    }
    m_items[row].text = text;
    m_items[row].segments = splitMarkdown(text);
    m_dirty.remove(key);
    Q_EMIT dataChanged(index(row), index(row), {TextRole, SegmentsRole});
}

void ChatModel::appendReasoning(const QString &key, const QString &delta)
{
    const QString k = QStringLiteral("r:") + key;
    int row = indexOfKey(k);
    if (row < 0) {
        Item it;
        it.kind = QStringLiteral("reasoning");
        it.key = k;
        row = append(it);
    }
    m_items[row].text += delta;
    Q_EMIT dataChanged(index(row), index(row), {TextRole});
}

void ChatModel::toolUpdate(const QString &id, const QString &name, const QJsonObject &input, const QString &status,
                           const QString &output)
{
    const QString k = QStringLiteral("tool:") + id;
    int row = indexOfKey(k);
    if (row < 0) {
        Item it;
        it.kind = QStringLiteral("tool");
        it.key = k;
        it.toolName = name;
        row = append(it);
    }
    Item &it = m_items[row];
    if (!name.isEmpty())
        it.toolName = name;
    if (!input.isEmpty()) {
        it.toolSubtitle = summarize(input);
        const QString cmd = input.value(QStringLiteral("command")).toString();
        it.toolInput = cmd.isEmpty() ? QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Indented)).trimmed()
                                     : QStringLiteral("$ ") + cmd;
    }
    if (!output.isEmpty())
        it.toolOutput = output.left(20000);
    it.toolStatus = status.isEmpty() ? QStringLiteral("running") : status;
    Q_EMIT dataChanged(index(row), index(row));
}

void ChatModel::addError(const QString &text)
{
    Item it;
    it.kind = QStringLiteral("error");
    it.text = text;
    append(it);
}

void ChatModel::addHistory(const QJsonArray &items)
{
    beginResetModel();
    m_items.clear();
    int n = 0;
    for (const QJsonValue &v : items) {
        const QJsonObject item = v.toObject();
        if (item.value(QStringLiteral("role")).toString() == QLatin1String("user")) {
            Item it;
            it.kind = QStringLiteral("user");
            it.text = item.value(QStringLiteral("text")).toString();
            for (const QJsonValue &s : item.value(QStringLiteral("skills")).toArray())
                it.skills << s.toString();
            m_items << it;
            continue;
        }
        for (const QJsonValue &pv : item.value(QStringLiteral("parts")).toArray()) {
            const QJsonObject part = pv.toObject();
            const QString kind = part.value(QStringLiteral("type")).toString();
            const QString text = part.value(QStringLiteral("text")).toString();
            Item it;
            it.key = QStringLiteral("h%1").arg(n++);
            if (kind == QLatin1String("text") && !text.trimmed().isEmpty()) {
                it.kind = QStringLiteral("text");
                it.text = text;
                it.segments = splitMarkdown(text);
            } else if (kind == QLatin1String("reasoning") && !text.trimmed().isEmpty()) {
                it.kind = QStringLiteral("reasoning");
                it.text = text;
            } else if (kind == QLatin1String("tool")) {
                it.kind = QStringLiteral("tool");
                it.toolName = part.value(QStringLiteral("name")).toString();
                const QJsonObject input = part.value(QStringLiteral("input")).toObject();
                it.toolSubtitle = summarize(input);
                const QString cmd = input.value(QStringLiteral("command")).toString();
                it.toolInput = cmd.isEmpty() ? QString::fromUtf8(QJsonDocument(input).toJson(QJsonDocument::Indented)).trimmed()
                                             : QStringLiteral("$ ") + cmd;
                it.toolOutput = part.value(QStringLiteral("output")).toString().left(20000);
                it.toolStatus = part.value(QStringLiteral("status")).toString();
            } else {
                continue;
            }
            m_items << it;
        }
        const QString error = item.value(QStringLiteral("error")).toString();
        if (!error.isEmpty()) {
            Item it;
            it.kind = QStringLiteral("error");
            it.text = error;
            m_items << it;
        }
    }
    endResetModel();
    Q_EMIT countChanged();
}

QString ChatModel::turnText(int row) const
{
    QStringList parts;
    for (int i = row; i >= 0 && m_items.at(i).kind != QLatin1String("user"); --i)
        if (m_items.at(i).kind == QLatin1String("text"))
            parts.prepend(m_items.at(i).text.trimmed());
    return parts.join(QStringLiteral("\n\n"));
}

QString ChatModel::lastUserPrompt() const
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (m_items.at(i).kind == QLatin1String("user"))
            return m_items.at(i).text;
    }
    return {};
}

QString ChatModel::exportMarkdown(const QString &sessionTitle, const QString &projectName) const
{
    QString out = QStringLiteral("# %1\n\n_Proje: %2_\n\n---\n\n")
                      .arg(sessionTitle.isEmpty() ? QStringLiteral("Orbital Sohbeti") : sessionTitle,
                           projectName.isEmpty() ? QStringLiteral("Proje") : projectName);
    for (const Item &it : m_items) {
        if (it.kind == QLatin1String("user")) {
            out += QStringLiteral("### 👤 Kullanıcı\n\n%1\n\n").arg(it.text);
        } else if (it.kind == QLatin1String("text")) {
            out += QStringLiteral("### 🤖 Orbital\n\n%1\n\n").arg(it.text);
        } else if (it.kind == QLatin1String("tool")) {
            out += QStringLiteral("> ⚙️ **Araç:** `%1` (%2)\n\n").arg(it.toolName, it.toolStatus);
        } else if (it.kind == QLatin1String("error")) {
            out += QStringLiteral("> ⚠️ **Hata:** %1\n\n").arg(it.text);
        }
    }
    return out;
}

// ---------------------------------------------------------------- sessions

QHash<int, QByteArray> SessionModel::roleNames() const
{
    return {{IdRole, "sessionId"},
            {TitleRole, "title"},
            {SectionRole, "section"},
            {TimeRole, "time"},
            {DirectoryRole, "directory"},
            {ProjectNameRole, "projectName"}};
}

QVariant SessionModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row &r = m_rows.at(index.row());
    switch (role) {
    case IdRole: return r.id;
    case TitleRole:
        // The engine names untitled sessions "New session - <timestamp>".
        return r.title.isEmpty() || r.title.startsWith(QStringLiteral("New session - ")) ? QStringLiteral("Yeni sohbet") : r.title;
    case TimeRole: return QDateTime::fromMSecsSinceEpoch(r.updated).toString(QStringLiteral("dd.MM.yyyy HH:mm"));
    case DirectoryRole: return r.directory;
    case ProjectNameRole: {
        if (r.directory.isEmpty() || r.directory == Paths::noFolderDir())
            return QStringLiteral("Klasörsüz (no-folder)");
        const QString name = QDir(r.directory).dirName();
        return name.isEmpty() ? r.directory : name;
    }
    case SectionRole: {
        const QDate d = QDateTime::fromMSecsSinceEpoch(r.updated).date();
        const QDate today = QDate::currentDate();
        if (d >= today)
            return QStringLiteral("Bugün");
        if (d == today.addDays(-1))
            return QStringLiteral("Dün");
        if (d > today.addDays(-7))
            return QStringLiteral("Önceki 7 gün");
        if (d > today.addDays(-30))
            return QStringLiteral("Önceki 30 gün");
        return QStringLiteral("Daha eski");
    }
    default: return {};
    }
}

void SessionModel::setRows(const QList<Row> &rows)
{
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

QString SessionModel::titleOf(const QString &id) const
{
    for (const Row &r : m_rows)
        if (r.id == id)
            return r.title;
    return {};
}

QString SessionModel::directoryOf(const QString &id) const
{
    for (const Row &r : m_rows)
        if (r.id == id)
            return r.directory;
    return {};
}
