#include "connection.h"
#include <QDebug>


Connection::Connection() = default;

bool Connection::createconnect()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QODBC");

    db.setDatabaseName("HWT");
    db.setUserName("qtuser");
    db.setPassword("mypassword123");

    if (db.open()) {
        qDebug() << "Connected to database!";
        return true;
    }

    m_lastError = db.lastError().text();
    qDebug() << "Error:" << m_lastError;
    return false;
}

QString Connection::lastError() const
{
    return m_lastError;
}