#include "createacc.h"
#include "ui_createacc.h"
#include <QPixmap>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QCryptographicHash>
#include <QMessageBox>


CreateAcc::CreateAcc(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::CreateAcc)
{
    ui->setupUi(this);

    QPixmap pix("logoQt.png");
    ui->logo->setPixmap(pix);
    ui->logo->setScaledContents(true);
    ui->backgroundFrame->lower();
    ui->backgroundFrame->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "stop:0 #6ec6ff,"   // lighter top blue
        "stop:1 #42a5f5);"  // slightly darker bottom blue
        );
    ui->backgroundFrame1->lower();
    ui->backgroundFrame1->setStyleSheet(
        "background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "stop:0 #6ec6ff,"   // lighter top blue
        "stop:1 #42a5f5);"  // slightly darker bottom blue
        );
}

CreateAcc::~CreateAcc()
{
    delete ui;
}


void CreateAcc::on_cancel_clicked()
{
    reject();
}



void CreateAcc::on_signup_clicked()
{
    QString firstName = ui->firstNameEdit->text();
    QString lastName = ui->lastNameEdit->text();
    QString role = ui->role_option->currentText();
    QString email = ui->emailEdit->text();
    QString password = ui->passwordEdit->text();

    if (firstName.isEmpty() || email.isEmpty() || password.isEmpty() || lastName.isEmpty()) {
        QMessageBox::warning(this, "Error", "All fields are required");
        return;
    }
    // store local user data in the app folder; do NOT store plaintext passwords
    QFile file("users.txt");

    //check my user's data files exist
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        QMessageBox::critical(this, "Error", "Cannot open users file");
        return;
    }

    // writing data in specific format:
    QTextStream out(&file);
    // Hash the password before storing
    QByteArray hash = QCryptographicHash::hash(password.toUtf8(), QCryptographicHash::Sha256).toHex();
    out << firstName << ";"
        << lastName  << ";"
        << role      << ";"
        << email     << ";"
        << hash.constData() << "\n";

    // free file
    file.close();

    QMessageBox::information(this, "Success","Account created successfully!");

    accept(); // close dialog
}

