#ifndef CHATBOTDIALOG_H
#define CHATBOTDIALOG_H

#include <QDialog>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>

namespace Ui {
class chatbotdialog;
}

class ChatbotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatbotDialog(QWidget *parent = nullptr, int currentUserId = -1, const QString &currentUserRole = QString());
    ~ChatbotDialog();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private slots:
    void onSendClicked();
    void onChip1Clicked();
    void onChip2Clicked();
    void onChip3Clicked();

private:
    Ui::chatbotdialog *ui;

    // UI helpers
    void addMessage(const QString &text, bool isUser);
    void scrollToBottom();

    // Core query dispatcher
    QString processQuery(const QString &input);
    void callOllama(const QString &input);
    // DB query helpers
    QString queryStock(const QString &input);
    QString queryDocks(const QString &input);
    QString queryBoats(const QString &input);
    QString queryUsers(const QString &input);
    QString queryCompanies(const QString &input);
    QString querySummary();
    QString queryListTables();
    QString dumpTable(const QString &tableName, int limit = 10);
    QString actualTable(const QString &baseName) const;

    // Drag to move
    bool   m_dragging   = false;
    QPoint m_dragOffset;
    int m_currentUserId = -1;
    QString m_currentUserRole;
};

#endif // CHATBOTDIALOG_H
