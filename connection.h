#ifndef CONNECTION_H
#define CONNECTION_H
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

class Connection
{
public:
    Connection();
    bool createconnect();
    QString lastError() const;
    QStringList listTables() const;
    bool execQuery(const QString &sql, QSqlQuery &outQuery, QString &err) const;
    bool isOpen() const;

private:
    QString m_lastError;
};

#endif // CONNECTION_H
