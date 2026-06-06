#include "mainwindow.h"
#include "connection.h"
#include <QMainWindow>
#include <QApplication>
#include <QFile>
#include <QFontDatabase>
#include <QDebug>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QMessageBox>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    //loading the fonts

    int fontId = QFontDatabase::addApplicationFont(":/fonts/Hey Comic.otf");
    int fontId3 = QFontDatabase::addApplicationFont(":/fonts/Bion-BoldCond.otf");
    int fontId4 = QFontDatabase::addApplicationFont("C:/Users/SKYMIL_INFORMATIQUE/OneDrive - ESPRIT/Documents/Qt project/Sign_up/Hawata/fonts/Poppins-SemiBold.ttf");
    int fontId5 = QFontDatabase::addApplicationFont("C:/Users/SKYMIL_INFORMATIQUE/OneDrive - ESPRIT/Documents/Qt project/Sign_up/Hawata/fonts/TANTWINKLE.ttf");
    int fontId6 = QFontDatabase::addApplicationFont("C:/Users/SKYMIL_INFORMATIQUE/OneDrive - ESPRIT/Documents/Qt project/Sign_up/Hawata/fonts/Mileast.otf");
    int fontId2 = QFontDatabase::addApplicationFont(":/fonts/Behove.ttf");
    if (fontId == -1) {
        qDebug() << "Font failed to load";
    } else {
        QString family = QFontDatabase::applicationFontFamilies(fontId).at(0);
        qDebug() << "Loaded font:" << family;
    }
    if (fontId2 == -1) {
        qDebug() << "Font failed to load";
    } else {
        QString family = QFontDatabase::applicationFontFamilies(fontId2).at(0);
        qDebug() << "Loaded font:" << family;
    }
    if (fontId3 == -1) {
        qDebug() << "Font failed to load";
    } else {
        QString family = QFontDatabase::applicationFontFamilies(fontId3).at(0);
        qDebug() << "Loaded font:" << family;
    }
    if (fontId4 == -1) {
        qDebug() << "Font failed to load";
    } else {
        QString family = QFontDatabase::applicationFontFamilies(fontId4).at(0);
        qDebug() << "Loaded font:" << family;
    }
    if (fontId5 == -1) {
        qDebug() << "Font failed to load";
    } else {
        QString family = QFontDatabase::applicationFontFamilies(fontId5).at(0);
        qDebug() << "Loaded font:" << family;
    }
    if (fontId6 == -1) {
        qDebug() << "Font failed to load";
    } else {
        QString family = QFontDatabase::applicationFontFamilies(fontId6).at(0);
        qDebug() << "Loaded font:" << family;
    }
    //loadstylesheet
    QFile f(":/marine_theme.qss");
    if (f.open(QIODevice::ReadOnly)) {
        qDebug() << "STYLESHEET LOADED";
        qApp->setStyleSheet(f.readAll());
    } else {
        qDebug() << "FAILED TO LOAD STYLESHEET";
    }

    Connection conn;
    if (conn.createconnect()) {
        QMessageBox::information(nullptr, "Database Connection", "Database connection succeeded!");
    } else {
        QMessageBox::warning(nullptr,
                             "Database Connection",
                             QString("Database connection failed, but the app will still open.\n\n%1\n\n"
                                     "Check your Oracle client/ODBC driver setup before using database features.")
                                 .arg(conn.lastError()));
    }

    MainWindow w;
    if (!w.isAutoLoginTriggered()) {
        w.show();
    }

    return app.exec();
}




