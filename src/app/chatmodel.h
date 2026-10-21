#pragma once

#include <QAbstractListModel>
#include <QJsonArray>
#include <QJsonObject>
#include <QSet>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

// The transcript as a flat list for a QML ListView. Item kinds:
//   user       — text, skills
//   text       — assistant markdown split into segments [{type:"text"|"code", text, lang}]
//   reasoning  — text
//   tool       — toolName, toolTitle, toolSubtitle, toolStatus, toolDetails
//   error      — text
class ChatModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)

public:
    enum Role {
        KindRole = Qt::UserRole + 1,
        TextRole,
        SegmentsRole,
        SkillsRole,
        ToolNameRole,
        ToolTitleRole,
        ToolSubtitleRole,
        ToolStatusRole,
        ToolDetailsRole,
        ToolIconRole,
        LastInTurnRole,
    };

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void clear();
    void addUser(const QString &text, const QStringList &skills);
    void appendText(const QString &key, const QString &delta);
    void setText(const QString &key, const QString &text);
    void appendReasoning(const QString &key, const QString &delta);
    void toolUpdate(const QString &id, const QString &name, const QJsonObject &input, const QString &status,
                    const QString &output);
    void addError(const QString &text);
    void addHistory(const QJsonArray &items);

    // Plain text of an assistant turn ending at row (for "copy").
    Q_INVOKABLE QString turnText(int row) const;
    Q_INVOKABLE QString lastUserPrompt() const;
    Q_INVOKABLE QString exportMarkdown(const QString &sessionTitle, const QString &projectName) const;

    static QVariantList splitMarkdown(const QString &markdown);

Q_SIGNALS:
    void countChanged();

private:
    struct Item
    {
        QString kind;
        QString key;
        QString text;
        QVariantList segments;
        QStringList skills;
        QString toolName;
        QString toolSubtitle;
        QString toolStatus;
        QString toolInput;
        QString toolOutput;
    };

    int indexOfKey(const QString &key) const;
    int append(const Item &item);
    void flushDirty();

    QList<Item> m_items;
    QSet<QString> m_dirty;
    QTimer m_throttle;
};

// Chat history for the sidebar, grouped by recency (ListView sections).
class SessionModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Role { IdRole = Qt::UserRole + 1, TitleRole, SectionRole, TimeRole, DirectoryRole, ProjectNameRole };

    using QAbstractListModel::QAbstractListModel;

    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : m_rows.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    struct Row
    {
        QString id;
        QString title;
        qint64 updated = 0;
        QString directory;
    };
    void setRows(const QList<Row> &rows);
    QString titleOf(const QString &id) const;
    QString directoryOf(const QString &id) const;
    QList<Row> allRows() const { return m_rows; }

private:
    QList<Row> m_rows;
};
