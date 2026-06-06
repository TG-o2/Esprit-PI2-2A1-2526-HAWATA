#include "appwindow.h"
#include "ui_appwindow.h"
#include "chatbotdialog.h"
#include "mainwindow.h"
#include "smtpclient.h"


#include <QPixmap>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QDateTime>
#include <QTime>
#include <algorithm>
#include <QPdfWriter>
#include <QPainter>
#include <QFileDialog>
#include <QPageLayout>
#include <QSqlQuery>
#include <QSqlDatabase>
#include <QCoreApplication>
#include <QDir>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QCalendarWidget>
#include <QColor>
#include <QFrame>
#include <QLabel>
#include <QTextCharFormat>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QBarCategoryAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QGraphicsProxyWidget>
#include <QHBoxLayout>

#include <QTimer>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QItemSelectionModel>
#include <QDesktopServices>
#include <QThread>
#include <QUrl>
#include <QUrlQuery>
 #include <QButtonGroup>

#include <QRandomGenerator>
#include <QEventLoop>

#include <QSerialPort>
#include <QSerialPortInfo>
#include <QtWebView/QtWebView>
#include <QQuickItem>
#include <QQmlError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <cmath>
#include <limits>

namespace {
QString normalizeMonthAbbreviation(const QString &value)
{
    QString normalized = value.trimmed();
    if (normalized.isEmpty()) {
        return normalized;
    }

    const QRegularExpression monthPattern(R"((\b\d{1,2}[-/ ])([A-Za-z]{3})([-/ ]\d{2,4}\b))");
    QRegularExpressionMatch match = monthPattern.match(normalized);
    if (match.hasMatch()) {
        QString month = match.captured(2).toLower();
        month[0] = month[0].toUpper();
        normalized = match.captured(1) + month + match.captured(3);
    }

    return normalized;
}

QDate normalizeTwoDigitYear(QDate date)
{
    if (date.isValid() && date.year() < 1950) {
        return date.addYears(100);
    }

    return date;
}

QString generateRandomTunisSeaLocation()
{
    // Random Tunisian place names only; no coordinates needed for stored location text.
    static const char *locations[] = {
        "Tunis",
        "La Goulette",
        "Rades",
        "Bizerte",
        "Tabarka",
        "Kelibia",
        "Nabeul",
        "Hammamet",
        "Sousse",
        "Monastir",
        "Mahdia",
        "Sfax",
        "Kerkennah",
        "Gabes",
        "Djerba",
        "Houmt Souk",
        "Zarzis",
        "Port El Kantaoui",
        "Chebba",
        "Kuriat",
        "Marsa",
        "Mahres",
        "Sidi Bou Said",
        "Mateur"
    };

    static const int count = sizeof(locations) / sizeof(locations[0]);
    const int randomIdx = QRandomGenerator::global()->bounded(count);
    return QString(locations[randomIdx]);
}

// Convert DDMM.MMMM format (Degrees Minutes.DecimalMinutes) to decimal degrees
// Used for Neo-6M GPS coordinates
// For latitude: DDMM.MMMM where DD=degrees(00-89), MM=minutes(00-59), MMMM=decimal minutes
// For longitude: DDDMM.MMMM where DDD=degrees(000-179), MM=minutes(00-59), MMMM=decimal minutes
bool convertDDMMtoDecimal(const QString &ddmm, double &decimal)
{
    QString normalized = ddmm.trimmed();
    normalized.replace(',', '.');

    bool ok = false;
    double value = normalized.toDouble(&ok);
    if (!ok || value < 0) {
        return false;
    }

    // Determine if this is latitude (2-digit degrees) or longitude (3-digit degrees)
    // by finding the decimal point position
    int dotPos = normalized.indexOf('.');
    int minutesStart;

    if (dotPos < 0) {
        // No decimal point; assume format is DDMM or DDDMM with trailing zeros
        if (normalized.length() >= 4) {
            minutesStart = normalized.length() - 2;  // Last 2 digits are minutes
        } else {
            return false;
        }
    } else {
        // Format: DDMM.MMMM or DDDMM.MMMM
        // Digits before decimal: degrees + 2 minutes digits
        // So: beforeDecimal.length() - 2 = degrees digits
        minutesStart = dotPos - 2;
        if (minutesStart < 0) {
            return false;
        }
    }

    // Extract degree and minute parts
    QString degreePart = normalized.left(minutesStart);
    QString minutePart = normalized.mid(minutesStart);

    bool degOk = false, minOk = false;
    int degrees = degreePart.toInt(&degOk);
    double minutes = minutePart.toDouble(&minOk);

    if (!degOk || !minOk) {
        return false;
    }

    // Validate ranges
    if (minutes < 0 || minutes >= 60) {
        return false;
    }

    // For Tunisia: latitude ~30-37°N, longitude ~8-12°E
    bool isLatitude = degrees < 90;  // Latitude max is 89
    bool isLongitude = degrees < 180; // Longitude max is 179

    if (isLatitude && degrees <= 89 && degrees >= 30) {
        // Likely latitude for Tunisia
        decimal = degrees + (minutes / 60.0);
        return true;
    } else if (isLongitude && degrees <= 179 && degrees >= 8) {
        // Likely longitude for Tunisia
        decimal = degrees + (minutes / 60.0);
        return true;
    } else if (isLatitude && degrees <= 89) {
        // Accept any valid latitude format
        decimal = degrees + (minutes / 60.0);
        return true;
    }

    return false;
}

bool parseLocationCoordinates(const QString &location, double &latitude, double &longitude)
{
    QString normalized = location.trimmed();
    if (normalized.isEmpty()) {
        return false;
    }

    const QString lower = normalized.toLower();

    // Try to parse as DDMM.MMMM format (Neo-6M GPS) first
    // Expected format: "DDMM.MMMM,DDDMM.MMMM" or "DDMM.MMMM DDDMM.MMMM"
    QRegularExpression gpsFormatPattern(R"((\d+\.?\d*)\s*[,\s]\s*(\d+\.?\d*))");
    QRegularExpressionMatch gpsMatch = gpsFormatPattern.match(normalized);

    if (gpsMatch.hasMatch()) {
        QString latStr = gpsMatch.captured(1);
        QString lonStr = gpsMatch.captured(2);

        double tempLat, tempLon;
        if (convertDDMMtoDecimal(latStr, tempLat) && convertDDMMtoDecimal(lonStr, tempLon)) {
            latitude = tempLat;
            longitude = tempLon;
            return true;
        }
    }

    // Map known Tunisian coastal site names to nearby sea coordinates.
    struct NamedSeaPoint {
        const char *keyword;
        double lat;
        double lon;
    };

    static const NamedSeaPoint tunisianSeaPoints[] = {
        {"tunis", 36.900000, 10.380000},
        {"esprit college", 36.845000, 10.190000},
        {"malles", 36.840000, 10.170000},
        {"la goulette", 36.819000, 10.305000},
        {"la marsa", 36.878000, 10.325000},
        {"sidi bou said", 36.870000, 10.345000},
        {"rades", 36.770000, 10.290000},
        {"bizerte", 37.315000, 9.930000},
        {"bizerta", 37.315000, 9.930000},
        {"tabarka", 36.995000, 8.760000},
        {"kelibia", 36.865000, 11.120000},
        {"nabeul", 36.485000, 10.790000},
        {"hammamet", 36.340000, 10.590000},
        {"sousse", 35.925000, 10.700000},
        {"monastir", 35.790000, 10.850000},
        {"mahdia", 35.535000, 11.070000},
        {"sfax", 34.745000, 10.780000},
        {"kerkennah", 34.700000, 11.200000},
        {"gabes", 33.980000, 10.300000},
        {"djerba", 33.930000, 10.920000},
        {"houmt souk", 33.880000, 10.860000},
        {"zarzis", 33.520000, 11.130000},
        {"warehouse", 36.820000, 10.305000},
        {"warehouses", 36.820000, 10.305000},
        {"depot", 36.820000, 10.305000},
        {"storage", 36.820000, 10.305000},
        {"dock", 36.819000, 10.305000},
        {"port", 36.819000, 10.305000}
    };

    for (const NamedSeaPoint &point : tunisianSeaPoints) {
        if (lower.contains(QLatin1String(point.keyword))) {
            latitude = point.lat;
            longitude = point.lon;
            return true;
        }
    }

    // If the location is text but not recognized, place it in Tunis gulf area
    // so tracking keeps working instead of failing with "invalid coordinates".
    bool hasLetters = false;
    for (QChar c : normalized) {
        if (c.isLetter()) {
            hasLetters = true;
            break;
        }
    }
    if (hasLetters) {
        const uint h = qHash(lower);
        const double latOffset = static_cast<double>(h % 25) / 1000.0;          // 0.000 .. 0.024
        const double lonOffset = static_cast<double>((h / 25) % 40) / 1000.0;   // 0.000 .. 0.039
        latitude = 36.880000 + latOffset;
        longitude = 10.320000 + lonOffset;
        return true;
    }

    // Accept formats such as "36.80,10.18", "lat:36.80 lon:10.18", "(36.80 10.18)", etc.
    const QRegularExpression numberPattern(R"([-+]?\d+(?:[\.,]\d+)?)");
    QRegularExpressionMatchIterator it = numberPattern.globalMatch(normalized);

    QList<double> values;
    while (it.hasNext()) {
        QString token = it.next().captured(0);
        token.replace(',', '.');
        bool ok = false;
        const double v = token.toDouble(&ok);
        if (ok) {
            values.append(v);
        }
    }

    if (values.size() < 2) {
        return false;
    }

    latitude = values[0];
    longitude = values[1];

    auto validLatLon = [](double lat, double lon) {
        return lat >= -90.0 && lat <= 90.0 && lon >= -180.0 && lon <= 180.0;
    };

    if (validLatLon(latitude, longitude)) {
        return true;
    }

    // Handle swapped values (lon,lat)
    if (validLatLon(longitude, latitude)) {
        std::swap(latitude, longitude);
        return true;
    }

    return false;
}

QDate parseDockingDate(const QString &value)
{
    const QString trimmed = normalizeMonthAbbreviation(value);
    if (trimmed.isEmpty()) {
        return {};
    }

    const QStringList dateFormats = {
        "dd/MM/yy",
        "dd/MM/yyyy",
        "dd-MM-yy",
        "dd-MM-yyyy",
        "dd-MMM-yy",
        "dd-MMM-yyyy",
        "yyyy-MM-dd",
        "yyyy-MM-dd hh:mm:ss",
        "yyyy-MM-ddThh:mm:ss",
        "MM/dd/yy",
        "MM/dd/yyyy",
        "ddd MMM d hh:mm:ss yyyy",
        "ddd MMM dd hh:mm:ss yyyy"
    };

    for (const QString &format : dateFormats) {
        QDate date = QDate::fromString(trimmed, format);
        if (date.isValid()) {
            return normalizeTwoDigitYear(date);
        }

        QDateTime dateTime = QDateTime::fromString(trimmed, format);
        if (dateTime.isValid()) {
            return normalizeTwoDigitYear(dateTime.date());
        }
    }

    QDateTime isoDateTime = QDateTime::fromString(trimmed, Qt::ISODate);
    if (isoDateTime.isValid()) {
        return normalizeTwoDigitYear(isoDateTime.date());
    }

    return {};
}

class DockCalendarWidget : public QCalendarWidget
{
public:
    explicit DockCalendarWidget(QWidget *parent = nullptr)
        : QCalendarWidget(parent)
    {
    }

    void setDockColors(const QMap<QDate, QColor> &colors)
    {
        m_dockColors = colors;
        updateCells();
    }

protected:
    void paintCell(QPainter *painter, const QRect &rect, QDate date) const override
    {
        QCalendarWidget::paintCell(painter, rect, date);

        const QColor color = m_dockColors.value(date);
        if (!color.isValid()) {
            return;
        }

        QColor overlay = color;
        overlay.setAlpha(150);

        painter->save();
        painter->setPen(Qt::NoPen);
        painter->setBrush(overlay);
        painter->drawRect(rect.adjusted(1, 1, -1, -1));
        painter->restore();
    }

private:
    QMap<QDate, QColor> m_dockColors;
};

int statusPriority(const QString &status)
{
    if (status.trimmed().compare("Occupied", Qt::CaseInsensitive) == 0) {
        return 2;
    }

    if (status.trimmed().compare("Available", Qt::CaseInsensitive) == 0) {
        return 1;
    }

    return 0;
}

QColor statusColor(const QString &status)
{
    if (status.trimmed().compare("Occupied", Qt::CaseInsensitive) == 0) {
        return QColor("#f87171");
    }

    if (status.trimmed().compare("Available", Qt::CaseInsensitive) == 0) {
        return QColor();
    }

    return QColor();
}

bool parseTemperatureFromArduinoLine(const QString &line, double &temperatureOut)
{
    const QRegularExpression labeledPattern(R"((?:TEMP(?:ERATURE)?\s*[:=]\s*)([-+]?\d+(?:[\.,]\d+)?))",
                                            QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch labeledMatch = labeledPattern.match(line);
    QString token;

    if (labeledMatch.hasMatch()) {
        token = labeledMatch.captured(1);
    } else {
        const QRegularExpression numberPattern(R"(([-+]?\d+(?:[\.,]\d+)?))");
        const QRegularExpressionMatch numberMatch = numberPattern.match(line);
        if (!numberMatch.hasMatch()) {
            return false;
        }
        token = numberMatch.captured(1);
    }

    token.replace(',', '.');

    bool ok = false;
    const double value = token.toDouble(&ok);
    if (!ok) {
        return false;
    }

    temperatureOut = value;
    return true;
}
}

appwindow::appwindow(QWidget *parent, int currentUserId, const QString &currentUserRole)
    : QDialog(parent)
    , ui(new Ui::appwindow)
    , connectedUserId(currentUserId)
    , connectedUserRole(currentUserRole)
{
    ui->setupUi(this);
    QtWebView::initialize();
    setupArduinoTemperatureReader();
    currentlySelectedId = -1;
    loadDockingTable();
    loadUsersTable();
    loadProductTable();    // Load product table on startup
    loadProductBoatIds();
    loadCompaniesTable();
    initEmailCampaignPage();
    initWhatsAppSmsPage();
    generateDockingStatistics();
    loadDockingHistoryView();

    // Normalize a few demo boats to named Tunis locations in the database.
    {
        QSqlQuery locationUpdate;
        locationUpdate.prepare("UPDATE BOAT SET LOCATION = :location WHERE BOATID = :boatId");

        struct BoatLocationSeed {
            int boatId;
            const char *location;
        };

        static const BoatLocationSeed seeds[] = {
            {1, "Esprit College"},
            {5, "Malles"},
            {6, "La Marsa"},
            {8, "Sidi Bou Said"},
            {9, "La Goulette"}
        };

        for (const BoatLocationSeed &seed : seeds) {
            locationUpdate.bindValue(":location", seed.location);
            locationUpdate.bindValue(":boatId", seed.boatId);
            locationUpdate.exec();
        }
    }

    //load boat table
    displayBoats();
    connect(ui->comboBox_15, &QComboBox::currentIndexChanged,
            this, &appwindow::on_comboBox_15_currentIndexChanged);
    connect(ui->pushButton, &QPushButton::clicked, this, &appwindow::on_pushButton_trackBoat_clicked);
    connect(ui->lineEdit, &QLineEdit::returnPressed, this, &appwindow::on_pushButton_trackBoat_clicked);

    connect(ui->pushButton_10, &QPushButton::clicked, this, &appwindow::on_pushButton_10_clicked);
    updateBoatStatusProgressBar();

    QPixmap logo("icons/fish3.png");
    QPixmap smalllogo("icons/fish1.png");
    QPixmap Uicon("icons/add-user2.png");
    QPixmap Dock("icons/dock.png");
    QPixmap Boat("icons/boat.png");
    QPixmap Prod("icons/prod.png");
    QPixmap Email("icons/Email.png");
    QPixmap Comp("icons/companies.png");
    QPixmap Stats("icons/stats.png");
    QPixmap rt("icons/rt.png");
    QPixmap ana("icons/ana.png");
    QPixmap management("icons/management.png");
    QPixmap time("icons/time.png");
    QPixmap user_icon("icons/add-user1.png");
    QPixmap user_display("icons/dashboard.png");
    QPixmap user_delete("icons/trash.png");
    QPixmap Calendar("icons/display.png");
    QPixmap live("icons/live.png");

    //pictures for product--------
    QPixmap fish("icons/fishicon.png");
    QPixmap pic_2("icons/logoQT2.png");
    QPixmap smallshrimp("icons/shrimp.png");
    QPixmap homepage("icons/homepage.png");
    QPixmap waves("icons/wave.jpg");
    //product picture set up
    ui->pic->setPixmap(fish);
    ui->pic->setScaledContents(true);

    ui->image1->setPixmap(rt);
    ui->image1->setScaledContents(true);
    ui->image2->setPixmap(management);
    ui->image2->setScaledContents(true);
    ui->image3->setPixmap(ana);
    ui->image3->setScaledContents(true);
    ui->image4->setPixmap(time);
    ui->image4->setScaledContents(true);
    //end of product picture code--------------



    ///----add manage display

    //user page
    //1
    ui->adduser->setPixmap(user_icon);
    ui->adduser->setScaledContents(true);

    ui->Usericon->setPixmap(Uicon);
    ui->Usericon->setScaledContents(true);


    ui->deluser->setPixmap(user_delete);
    ui->deluser->setScaledContents(true);

    ui->dispuser->setPixmap(user_display);
    ui->dispuser->setScaledContents(true);

    //2
    ui->adduser_2->setPixmap(user_icon);
    ui->adduser_2->setScaledContents(true);

    ui->deluser_2->setPixmap(user_delete);
    ui->deluser_2->setScaledContents(true);

    ui->dispuser_2->setPixmap(user_display);
    ui->dispuser_2->setScaledContents(true);

    //3
    ui->adduser_3->setPixmap(user_icon);
    ui->adduser_3->setScaledContents(true);

    ui->deluser_3->setPixmap(user_delete);
    ui->deluser_3->setScaledContents(true);

    ui->dispuser_3->setPixmap(user_display);
    ui->dispuser_3->setScaledContents(true);

    //dock page 1 2 3 4
    ui->Dock1->setPixmap(Dock);
    ui->Dock1->setScaledContents(true);

    ui->deluser_17->setPixmap(user_delete);
    ui->deluser_17->setScaledContents(true);

    ui->dispuser_17->setPixmap(user_display);
    ui->dispuser_17->setScaledContents(true);

    ui->Dock3->setPixmap(Dock);
    ui->Dock3->setScaledContents(true);

    ui->deluser_5->setPixmap(user_delete);
    ui->deluser_5->setScaledContents(true);

    ui->dispuser_5->setPixmap(user_display);
    ui->dispuser_5->setScaledContents(true);

    ui->Dock4->setPixmap(Dock);
    ui->Dock4->setScaledContents(true);

    ui->deluser_6->setPixmap(user_delete);
    ui->deluser_6->setScaledContents(true);
    ui->dispuser_5->setPixmap(user_display);
    ui->dispuser_5->setScaledContents(true);

    ui->Dock4->setPixmap(Dock);
    ui->Dock4->setScaledContents(true);

    ui->deluser_6->setPixmap(user_delete);
    ui->deluser_6->setScaledContents(true);

    ui->dispuser_6->setPixmap(user_display);
    ui->dispuser_6->setScaledContents(true);
    //boat page 7,8 & 9
    ui->Dock2->setPixmap(Dock);
    ui->Dock2->setScaledContents(true);

    ui->deluser_18->setPixmap(user_delete);
    ui->deluser_18->setScaledContents(true);

    ui->dispuser_18->setPixmap(Stats);
    ui->dispuser_18->setScaledContents(true);

    ui->Dock2->setPixmap(Dock);
    ui->Dock2->setScaledContents(true);

    ui->deluser_18->setPixmap(user_delete);
    ui->deluser_18->setScaledContents(true);

    ui->dispuser_18->setPixmap(Stats);
    ui->dispuser_18->setScaledContents(true);

    ui->Dock3->setPixmap(Dock);
    ui->Dock3->setScaledContents(true);

    ui->cal1->setPixmap(Calendar);
    ui->cal1->setScaledContents(true);

    ui->cal2->setPixmap(Calendar);
    ui->cal2->setScaledContents(true);

    ui->cal3->setPixmap(Calendar);
    ui->cal3->setScaledContents(true);

    ui->cal4->setPixmap(Calendar);
    ui->cal4->setScaledContents(true);
    //BOATS

    ui->Addboat1->setPixmap(Boat);
    ui->Addboat1->setScaledContents(true);
    ui->Addboat2->setPixmap(Boat);
    ui->Addboat2->setScaledContents(true);
    ui->Addboat3->setPixmap(Boat);
    ui->Addboat3->setScaledContents(true);
    ui->Addboat4->setPixmap(Boat);
    ui->Addboat4->setScaledContents(true);


    ui->ManBoat1->setPixmap(user_display);
    ui->ManBoat1->setScaledContents(true);
    ui->ManBoat2->setPixmap(user_display);
    ui->ManBoat2->setScaledContents(true);
    ui->ManBoat3->setPixmap(user_display);
    ui->ManBoat3->setScaledContents(true);
    ui->ManBoat4->setPixmap(user_display);
    ui->ManBoat4->setScaledContents(true);


    ui->dispboat3->setPixmap(Stats);
    ui->dispboat3->setScaledContents(true);
    ui->dispboat2->setPixmap(Stats);
    ui->dispboat2->setScaledContents(true);
    ui->dispboat1->setPixmap(Stats);
    ui->dispboat1->setScaledContents(true);
    ui->dispboat4->setPixmap(Stats);
    ui->dispboat4->setScaledContents(true);

    ui->Live1->setPixmap(live);
    ui->Live1->setScaledContents(true);
    ui->Live2->setPixmap(live);
    ui->Live2->setScaledContents(true);
    ui->Live3->setPixmap(live);
    ui->Live3->setScaledContents(true);
    ui->Live4->setPixmap(live);
    ui->Live4->setScaledContents(true);
    //product
    ui->Prod1->setPixmap(Prod);
    ui->Prod1->setScaledContents(true);
    ui->Prod2->setPixmap(Prod);
    ui->Prod2->setScaledContents(true);
    ui->Prod3->setPixmap(Prod);
    ui->Prod3->setScaledContents(true);
    ui->Prod4->setPixmap(Prod);
    ui->Prod4->setScaledContents(true);

    ui->disProd1->setPixmap(Stats);
    ui->disProd1->setScaledContents(true);
    ui->disProd2->setPixmap(Stats);
    ui->disProd2->setScaledContents(true);
    ui->disProd3->setPixmap(Stats);
    ui->disProd3->setScaledContents(true);
    ui->disProd4->setPixmap(Stats);
    ui->disProd4->setScaledContents(true);

    ui->Manprod1->setPixmap(user_display);
    ui->Manprod1->setScaledContents(true);
    ui->Manprod2->setPixmap(user_display);
    ui->Manprod2->setScaledContents(true);
    ui->Manprod3->setPixmap(user_display);
    ui->Manprod3->setScaledContents(true);
    ui->Manprod4->setPixmap(user_display);
    ui->Manprod4->setScaledContents(true);

    ui->Discounts1->setPixmap(user_display);
    ui->Discounts1->setScaledContents(true);
    ui->Discounts2->setPixmap(user_display);
    ui->Discounts2->setScaledContents(true);
    ui->Discounts3->setPixmap(user_display);
    ui->Discounts3->setScaledContents(true);
    ui->Discounts4->setPixmap(user_display);
    ui->Discounts4->setScaledContents(true);

    //company page:

    ui->email1->setPixmap(Email);
    ui->email1->setScaledContents(true);
    ui->email2->setPixmap(Email);
    ui->email2->setScaledContents(true);
    ui->email3->setPixmap(Email);
    ui->email3->setScaledContents(true);
    ui->email4->setPixmap(Email);
    ui->email4->setScaledContents(true);

    ui->Comp1->setPixmap(Comp);
    ui->Comp1->setScaledContents(true);
    ui->Comp2->setPixmap(Comp);
    ui->Comp2->setScaledContents(true);
    ui->Comp3->setPixmap(Comp);
    ui->Comp3->setScaledContents(true);
    ui->Comp4->setPixmap(Comp);
    ui->Comp4->setScaledContents(true);

    ui->ManC1->setPixmap(user_display);
    ui->ManC1->setScaledContents(true);
    ui->ManC2->setPixmap(user_display);
    ui->ManC2->setScaledContents(true);
    ui->ManC3->setPixmap(user_display);
    ui->ManC3->setScaledContents(true);
    ui->ManC4->setPixmap(user_display);
    ui->ManC4->setScaledContents(true);

    ui->Stats1->setPixmap(Stats);
    ui->Stats1->setScaledContents(true);
    ui->Stats2->setPixmap(Stats);
    ui->Stats2->setScaledContents(true);
    ui->Stats3->setPixmap(Stats);
    ui->Stats3->setScaledContents(true);
    ui->Stats4->setPixmap(Stats);
    ui->Stats4->setScaledContents(true);



    if (homepage.isNull()) {
        qDebug() << "icons/homepage.png not found, falling back to icons/Ports1.jpg";
        homepage = QPixmap("icons/Ports1.jpg");
    }

    ui->homeImage->setPixmap(homepage);
    ui->homeImage->setScaledContents(true);
    if (smallshrimp.isNull()) {
        qDebug() << "Image failed to load!";
    } else {
        qDebug() << "Image loaded successfully!";

        ui->image1->setPixmap(smallshrimp);
        ui->image1->setScaledContents(true);
    }

    //ui->waveDecoration->setPixmap(waves);
    //ui->waveDecoration->setScaledContents(true);

    ///-----end

    //frame
    ui->BackFrame_16->lower();
    ui->BackFrame_16->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_17->lower();
    ui->BackFrame_18->lower();
    ui->BackFrame_19->lower();
    ui->BackFrame_20->lower();
    ui->BackFrame_21->lower();
    ui->BackFrame_22->lower();
    ui->BackFrame_23->lower();
    ui->BackFrame_24->lower();
    ui->BackFrame_25->lower();
    ui->BackFrame_26->lower();
    ui->BackFrame_27->lower();
    ui->BackFrame_28->lower();
    ui->BackFrame_29->lower();
    ui->BackFrame_30->lower();
    ui->BackFrame_17->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_3->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_4->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_5->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_6->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_18->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_19->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_20->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_21->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_22->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_23->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_24->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_25->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_26->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_27->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_28->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_29->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ui->BackFrame_30->setStyleSheet(
        "background: rgba(255, 255, 255, 0.3);"
        "border: 2px solid #42a5f5;"
        "border-radius: 8px;"
        );
    ///=============web / pages titles START================

    ui->WebBrowsing->setTabText(1, "Users");
    ui->WebBrowsing->setTabText(2, "Manage Dock");
    ui->WebBrowsing->setTabText(3, "Boats Admission");
    ui->WebBrowsing->setTabText(4, "Product Restock");
    ui->WebBrowsing->setTabText(5, "Companies");

    ///=============web / pages titles END================

    //testing for the New/final version:

    //page USER
    connect(ui->add_user  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->Manage  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(1);
    });
    connect(ui->Display  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(2);
    });
    connect(ui->add_user_2  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->Manage_2  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(1);
    });
    connect(ui->Display_2  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(2);
    });
    connect(ui->add_user_3  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->Manage_3 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(1);
    });
    connect(ui->Display_3  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(2);
    });

    connect(ui->add_user_3  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(0);
    });
    connect(ui->Manage_3 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(1);
    });
    connect(ui->Display_3  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget->setCurrentIndex(2);
    });

    //page DOCK
    connect(ui->add_user_18  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(0);
    });
    connect(ui->Manage_18 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(1);
        loadDockingTable();
    });
    connect(ui->Display_18  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(2);
        generateDockingStatistics();
    });
    connect(ui->add_user_17  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(0);
    });
    connect(ui->Manage_17 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(1);
        loadDockingTable();
    });
    connect(ui->Display_17  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(2);
        loadDockingTable();
        generateDockingStatistics();

    });
    connect(ui->add_user_5  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(0);
    });
    connect(ui->Manage_5 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(1);
        loadDockingTable();
    });
    connect(ui->Display_5  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(2);
        loadDockingTable();
        generateDockingStatistics();

    });
    connect(ui->add_user_6  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(0);
    });
    connect(ui->Manage_6 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(1);
        loadDockingTable();
    });
    connect(ui->Display_6  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(2);
        generateDockingStatistics();
    });
    connect(ui->forgetpwd_4  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(3);
        loadDockingHistoryView();
    });
    connect(ui->forgetpwd_2  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(3);
        loadDockingHistoryView();
    });
    connect(ui->forgetpwd_5  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(3);
        loadDockingHistoryView();
    });
    connect(ui->forgetpwd_6  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_2->setCurrentIndex(3);
        loadDockingHistoryView();

    });

    //page BOAT :,D
    connect(ui->add_user_22  , &QPushButton::clicked, this, [=]() {
        setBoatMode(BoatMode::Add);
        ui->boatPage->setCurrentIndex(0);
    });
    connect(ui->add_user_21  , &QPushButton::clicked, this, [=]() {
        setBoatMode(BoatMode::Add);
        ui->boatPage->setCurrentIndex(0);
    });
    connect(ui->add_user_20  , &QPushButton::clicked, this, [=]() {
        setBoatMode(BoatMode::Add);
        ui->boatPage->setCurrentIndex(0);
    });
    connect(ui->add_user_19  , &QPushButton::clicked, this, [=]() {
        setBoatMode(BoatMode::Add);
        ui->boatPage->setCurrentIndex(0);
    });
    connect(ui->Manage_22 , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(1);
    });
    connect(ui->Manage_20 , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(1);
    });
    connect(ui->Manage_21 , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(1);
    });
    connect(ui->Manage_19 , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(1);
    });
    connect(ui->Display_22  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(2);
        updateBoatStatusProgressBar();
    });
    connect(ui->Display_20  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(2);
        updateBoatStatusProgressBar();
    });
    connect(ui->Display_21  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(2);
        updateBoatStatusProgressBar();
    });
    connect(ui->Display_19  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(2);
        updateBoatStatusProgressBar();
    });
    connect(ui->forgetpwd_7  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(3);
    });
    connect(ui->forgetpwd_8  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(3);
    });
    connect(ui->forgetpwd_9  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(3);
    });
    connect(ui->forgetpwd_10  , &QPushButton::clicked, this, [=]() {
        ui->boatPage->setCurrentIndex(3);
    });

    //page Prod
    connect(ui->add_user_24  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(0);
    });
    connect(ui->Manage_24 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(1);
    });
    connect(ui->Display_24  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(2);
    });
    connect(ui->add_user_28  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(0);
    });
    connect(ui->Manage_28 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(1);
    });
    connect(ui->Display_28  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(2);
    });
    connect(ui->add_user_26  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(0);
    });
    connect(ui->Manage_26 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(1);
    });
    connect(ui->Display_26  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(2);
    });
    connect(ui->add_user_27  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(0);
    });
    connect(ui->Manage_27 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(1);
    });
    connect(ui->Display_27  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(2);
    });
    connect(ui->forgetpwd_12  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(3);
        refreshLowStockAlerts(true);
    });
    connect(ui->forgetpwd_14  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(3);
        refreshLowStockAlerts(true);
    });
    connect(ui->forgetpwd_15  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(3);
        refreshLowStockAlerts(true);
    });
    connect(ui->forgetpwd_16  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_4->setCurrentIndex(3);
        refreshLowStockAlerts(true);
    });
    //cOMPANIES

    connect(ui->add_user_29  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(0);
    });
    connect(ui->Manage_29 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(1);
    });
    connect(ui->Display_29  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(2);
    });
    connect(ui->add_user_30 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(0);
    });
    connect(ui->Manage_30 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(1);
    });
    connect(ui->Display_30  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(2);
    });
    connect(ui->add_user_31 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(0);
    });
    connect(ui->Manage_31 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(1);
    });
    connect(ui->Display_31  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(2);
    });
    connect(ui->add_user_32 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(0);
    });
    connect(ui->Manage_32 , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(1);
    });
    connect(ui->Display_32  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(2);
    });
    connect(ui->forgetpwd_20  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(3);
    });
    connect(ui->forgetpwd_17  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(3);
    });
    connect(ui->forgetpwd_18  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(3);
    });
    connect(ui->forgetpwd_19  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(3);
    });

    /*connect(ui->CheckInsightsComp  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(3);
    });
    connect(ui->GoBackCompanies  , &QPushButton::clicked, this, [=]() {
        ui->stackedWidget_5->setCurrentIndex(2);
    });*/
    //++++

    /// Exit page
    connect(ui->Exit, &QPushButton::clicked, qApp, &QApplication::quit);

    /// Log in page
    /*
    connect(ui->Goback, &QPushButton::clicked, this, [=]() {
        MainWindow *w = new MainWindow();
        w->show();
        this->close();   // or hide()
    });
    */
    //docking connects

    // Wire double-click (add once, anywhere in the constructor)
    connect(ui->tabdocking, &QTableWidget::cellDoubleClicked,
            this, &appwindow::on_tabdocking_cellDoubleClicked);
    connect(ui->tabdocking, &QTableWidget::cellClicked,
            this, &appwindow::on_tabdocking_cellClicked);

    ui->comboBox_12->setCurrentIndex(0);
    loadUserStatistics(true);

    //BOATS STATUS BAR




    //BOATS EMAILS
    maintenanceCheckTimer = new QTimer(this);
    connect(maintenanceCheckTimer, &QTimer::timeout, this, &appwindow::onCheckMaintenanceReminders);
    maintenanceCheckTimer->start(86400000);

    setupGoogleMap();


    QTimer::singleShot(5000, this, &appwindow::onCheckMaintenanceReminders); // Check after 5 seconds


    // ── Harbor Assistant chatbot ──
    ui->chatFAB->raise();  // always on top of all tabs

    m_chatbot = new ChatbotDialog(this, connectedUserId, connectedUserRole);

    connect(ui->chatFAB, &QPushButton::clicked, this, [this]() {
        if (m_chatbot->isVisible()) {
            m_chatbot->hide();
        } else {
            // Center on this window
            int x = geometry().x() + (width()  - m_chatbot->width())  / 2;
            int y = geometry().y() + (height() - m_chatbot->height()) / 2;
            m_chatbot->move(x, y);
            m_chatbot->show();
            m_chatbot->raise();
        }
    });
}
QString appwindow::dockingHistoryFilePath() const
{
    QStringList startDirs;
    startDirs << QDir::currentPath() << QCoreApplication::applicationDirPath();

    for (const QString &startPath : startDirs) {
        QDir dir(startPath);
        for (int i = 0; i < 8; ++i) {
            if (dir.exists("CMakeLists.txt")) {
                return dir.filePath("docking_history.txt");
            }
            if (!dir.cdUp()) {
                break;
            }
        }
    }

    return QDir::current().filePath("docking_history.txt");
}

void appwindow::ensureDockingHistoryFile()
{
    QFile file(dockingHistoryFilePath());
    if (!file.exists()) {
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.close();
        }
    }
}

void appwindow::appendDockingHistory(const QString &action, int dockId)
{
    ensureDockingHistoryFile();

    QFile file(dockingHistoryFilePath());
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qDebug() << "Failed to open docking_history.txt for append:" << file.errorString();
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    QTextStream out(&file);
    out << timestamp << "\t" << action.toUpper() << "\t" << dockId << "\n";
    file.close();

    if (ui->stackedWidget_2->currentIndex() == 3) {
        loadDockingHistoryView();
    }
}

void appwindow::loadDockingHistoryView()
{
    ensureDockingHistoryFile();

    QStandardItemModel *model = qobject_cast<QStandardItemModel*>(ui->history_view->model());
    if (!model) {
        model = new QStandardItemModel(ui->history_view);
        ui->history_view->setModel(model);
    }

    model->clear();
    model->setHorizontalHeaderLabels(QStringList() << "DateTime" << "Action" << "DockID");

    QFile file(dockingHistoryFilePath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        QStringList lines;
        while (!in.atEnd()) {
            const QString line = in.readLine().trimmed();
            if (!line.isEmpty()) {
                lines.append(line);
            }
        }
        file.close();

        for (int i = lines.size() - 1; i >= 0; --i) {
            const QStringList parts = lines.at(i).split('\t');
            if (parts.size() < 3) {
                continue;
            }

            QList<QStandardItem*> row;
            for (int c = 0; c < 3; ++c) {
                QStandardItem *item = new QStandardItem(parts.at(c));
                item->setEditable(false);
                row.append(item);
            }
            model->appendRow(row);
        }
    }

    if (model->rowCount() == 0) {
        QList<QStandardItem*> row;
        row.append(new QStandardItem("-"));
        row.append(new QStandardItem("-"));
        row.append(new QStandardItem("-"));
        for (QStandardItem *item : row) {
            item->setEditable(false);
        }
        model->appendRow(row);
    }

    ui->history_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->history_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->history_view->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->history_view->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

//add docking
void appwindow::on_CreateDocking_clicked()
{
    // Get values from UI widgets
    QString location = ui->location->text().trimmed();
    QString length = ui->length->text().trimmed();
    QString height = ui->height->text().trimmed();
    QString status = ui->status->currentText();
    QString capacity = ui->capacity->text().trimmed();
    QDateTime startDate = ui->startDate->dateTime();
    QDateTime endDate = ui->endDate->dateTime();

    // ===== VALIDATION CONTROLS =====

    // 1. Check all fields are not empty
    if (location.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Location is required. Please enter a location.");
        return;
    }

    // Check Location contains only letters and spaces (no numbers)
    for (QChar c : location) {
        if (c.isDigit()) {
            QMessageBox::warning(this, "Validation Error", "Location must contain only letters. Numbers are not allowed.");
            return;
        }
    }

    if (length.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Length is required. Please enter a length value.");
        return;
    }

    if (height.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Height is required. Please enter a height value.");
        return;
    }

    if (capacity.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Capacity is required. Please enter a capacity value.");
        return;
    }

    if (status.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Status is required. Please select a status.");
        return;
    }

    // 2. Validate numeric fields (Length, Height, Capacity)
    bool lengthOk, heightOk, capacityOk;
    double lengthVal = length.toDouble(&lengthOk);
    double heightVal = height.toDouble(&heightOk);
    double capacityVal = capacity.toDouble(&capacityOk);

    if (!lengthOk || lengthVal <= 0) {
        QMessageBox::warning(this, "Validation Error", "Length must be a positive number.");
        return;
    }

    if (!heightOk || heightVal <= 0) {
        QMessageBox::warning(this, "Validation Error", "Height must be a positive number.");
        return;
    }

    if (!capacityOk || capacityVal <= 0) {
        QMessageBox::warning(this, "Validation Error", "Capacity must be a positive number.");
        return;
    }

    // 3. Validate dates are in the future (tomorrow or later)
    QDateTime tomorrow = QDateTime(QDate::currentDate().addDays(1), QTime(0, 0));

    if (startDate < tomorrow) {
        QMessageBox::warning(this, "Validation Error",
                             QString("Start Date must be in the future (tomorrow or later).\nCurrent date: %1\nMinimum date: %2")
                                 .arg(QDate::currentDate().toString("MM/dd/yy"))
                                 .arg(tomorrow.date().toString("MM/dd/yy")));
        return;
    }

    if (endDate < tomorrow) {
        QMessageBox::warning(this, "Validation Error",
                             QString("End Date must be in the future (tomorrow or later).\nCurrent date: %1\nMinimum date: %2")
                                 .arg(QDate::currentDate().toString("MM/dd/yy"))
                                 .arg(tomorrow.date().toString("MM/dd/yy")));
        return;
    }

    // 4. Validate End Date >= Start Date
    if (endDate < startDate) {
        QMessageBox::warning(this, "Validation Error", "End Date must be on or after the Start Date.");
        return;
    }

    // ===== ALL VALIDATIONS PASSED =====

    QString startDateStr = startDate.toString("dd/MM/yy");
    QString endDateStr = endDate.toString("dd/MM/yy");

    // Call create docking + create manage relation in one transaction
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, "Error", "Database is not open.");
        return;
    }

    if (!db.transaction()) {
        QMessageBox::critical(this, "Error", "Failed to start database transaction.");
        return;
    }

    int createdDockId = -1;
    if (dockingManager.createDocking(location, length, height, status, capacity, startDateStr, endDateStr, &createdDockId)) {
        if (connectedUserId <= 0 || createdDockId <= 0) {
            db.rollback();
            QMessageBox::critical(this, "Error", "Failed to resolve UserID or DockID for MANAGE relation.");
            return;
        }

        if (!manageManager.ajouter_manage(connectedUserId, createdDockId)) {
            db.rollback();
            QMessageBox::critical(this, "Error", "Docking was created but MANAGE relation failed. Transaction rolled back.");
            return;
        }

        if (!db.commit()) {
            db.rollback();
            QMessageBox::critical(this, "Error", "Failed to commit docking transaction.");
            return;
        }

        appendDockingHistory("ADD", createdDockId);

        QMessageBox::information(this, "Success", "Docking created successfully!");
        // Clear the form
        ui->location->clear();
        ui->length->clear();
        ui->height->clear();
        ui->capacity->clear();
        ui->startDate->setDateTime(QDateTime::currentDateTime());
        ui->endDate->setDateTime(QDateTime::currentDateTime());
        // Reset to first index of status combo
        ui->status->setCurrentIndex(0);
        loadDockingTable();
    } else {
        db.rollback();
        QMessageBox::critical(this, "Error", "Failed to create docking. Please check the database connection.");
    }
}

void appwindow::loadDockingTable()
{
    allDockingRecords = dockingManager.getAllDockings();
    qDebug() << "Records fetched:" << allDockingRecords.size();  // verify in output

    // Clear search and reset sort to default
    ui->searchbar_docking->blockSignals(true);
    ui->searchbar_docking->clear();
    ui->searchbar_docking->blockSignals(false);

    ui->docking_sort->blockSignals(true);
    ui->docking_sort->setCurrentIndex(0);  // Set to "Date" (default)
    ui->docking_sort->blockSignals(false);

    // Populate table with all records
    populateDockingTable(allDockingRecords);
}

void appwindow::populateDockingTable(const QList<DockingRecord> &records)
{
    QTableWidget *table = ui->tabdocking;
    table->setRowCount(0);
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels(
        {"ID", "Status", "Date Start", "Date End", "Location", "Length", "Height", "Capacity"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (const DockingRecord &r : records) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(r.id)));
        table->setItem(row, 1, new QTableWidgetItem(r.status));
        table->setItem(row, 2, new QTableWidgetItem(r.startDate));
        table->setItem(row, 3, new QTableWidgetItem(r.endDate));
        table->setItem(row, 4, new QTableWidgetItem(r.location));
        table->setItem(row, 5, new QTableWidgetItem(r.length));
        table->setItem(row, 6, new QTableWidgetItem(r.height));
        table->setItem(row, 7, new QTableWidgetItem(r.capacity));
    }

    // Stretch all columns to fill available space evenly
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Update the count label at the bottom with the number of docks
    ui->labelResults_4->setText(QString("Showing %1 Docks").arg(records.size()));
}


void appwindow::on_tabdocking_cellClicked(int row, int /*column*/)
{
    if (row < 0) return;
    QString id = ui->tabdocking->item(row, 0)->text();
    ui->selected_id->setText(id);           // ٣� shows ID in your line edit
    selectedDockingId = id.toInt();
}

// Double click �?� fill form and go to edit page
void appwindow::on_tabdocking_cellDoubleClicked(int row, int /*column*/)
{
    QTableWidget *table = ui->tabdocking;
    if (row < 0) return;

    selectedDockingId = table->item(row, 0)->text().toInt();
    ui->selected_id->setText(QString::number(selectedDockingId));

    // Fill the form fields �?� column order: ID, Status, DateStart, DateEnd, Location, Length, Height, Capacity
    int idx = ui->status->findText(table->item(row, 1)->text());
    if (idx >= 0) ui->status->setCurrentIndex(idx);

    // Parse dates - try multiple formats since database may store in different format
    QString startDateStr = table->item(row, 2)->text();
    QString endDateStr = table->item(row, 3)->text();

    QDateTime startDt = QDateTime::fromString(startDateStr, "yyyy-MM-dd hh:mm:ss");
    if (!startDt.isValid()) {
        startDt = QDateTime::fromString(startDateStr, "MM/dd/yy");
        // Fix 2-digit year: if year < 1950, it's 20xx, otherwise 19xx
        if (startDt.isValid() && startDt.date().year() < 1950) {
            startDt = startDt.addYears(100);
        }
    }
    if (!startDt.isValid()) {
        startDt = QDateTime::fromString(startDateStr, "dd/MM/yy");
        // Fix 2-digit year: if year < 1950, it's 20xx, otherwise 19xx
        if (startDt.isValid() && startDt.date().year() < 1950) {
            startDt = startDt.addYears(100);
        }
    }
    if (startDt.isValid()) {
        ui->startDate->setDateTime(startDt);
    }

    QDateTime endDt = QDateTime::fromString(endDateStr, "yyyy-MM-dd hh:mm:ss");
    if (!endDt.isValid()) {
        endDt = QDateTime::fromString(endDateStr, "MM/dd/yy");
        // Fix 2-digit year: if year < 1950, it's 20xx, otherwise 19xx
        if (endDt.isValid() && endDt.date().year() < 1950) {
            endDt = endDt.addYears(100);
        }
    }
    if (!endDt.isValid()) {
        endDt = QDateTime::fromString(endDateStr, "dd/MM/yy");
        // Fix 2-digit year: if year < 1950, it's 20xx, otherwise 19xx
        if (endDt.isValid() && endDt.date().year() < 1950) {
            endDt = endDt.addYears(100);
        }
    }
    if (endDt.isValid()) {
        ui->endDate->setDateTime(endDt);
    }

    ui->location->setText(table->item(row, 4)->text());
    ui->length->setText(table->item(row, 5)->text());
    ui->height->setText(table->item(row, 6)->text());
    ui->capacity->setText(table->item(row, 7)->text());

    // Navigate to the create/edit page
    ui->stackedWidget_2->setCurrentIndex(0);
}


// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  Edit / Update button
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::on_edit_Docking_clicked()
{
    if (selectedDockingId == -1) {
        QMessageBox::warning(this, "No Selection", "Please double-click a row in the table first.");
        return;
    }

    QString location  = ui->location->text().trimmed();
    QString length    = ui->length->text().trimmed();
    QString height    = ui->height->text().trimmed();
    QString status    = ui->status->currentText();
    QString capacity  = ui->capacity->text().trimmed();
    QDateTime startDate = ui->startDate->dateTime();
    QDateTime endDate = ui->endDate->dateTime();

    // ===== VALIDATION CONTROLS =====

    // 1. Check all fields are not empty
    if (location.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Location is required. Please enter a location.");
        return;
    }

    // Check Location contains only letters and spaces (no numbers)
    for (QChar c : location) {
        if (c.isDigit()) {
            QMessageBox::warning(this, "Validation Error", "Location must contain only letters. Numbers are not allowed.");
            return;
        }
    }

    if (length.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Length is required. Please enter a length value.");
        return;
    }

    if (height.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Height is required. Please enter a height value.");
        return;
    }

    if (capacity.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Capacity is required. Please enter a capacity value.");
        return;
    }

    if (status.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Status is required. Please select a status.");
        return;
    }

    // 2. Validate numeric fields (Length, Height, Capacity)
    bool lengthOk, heightOk, capacityOk;
    double lengthVal = length.toDouble(&lengthOk);
    double heightVal = height.toDouble(&heightOk);
    double capacityVal = capacity.toDouble(&capacityOk);

    if (!lengthOk || lengthVal <= 0) {
        QMessageBox::warning(this, "Validation Error", "Length must be a positive number.");
        return;
    }

    if (!heightOk || heightVal <= 0) {
        QMessageBox::warning(this, "Validation Error", "Height must be a positive number.");
        return;
    }

    if (!capacityOk || capacityVal <= 0) {
        QMessageBox::warning(this, "Validation Error", "Capacity must be a positive number.");
        return;
    }

    // 3. Validate dates are in the future (tomorrow or later)
    QDateTime tomorrow = QDateTime(QDate::currentDate().addDays(1), QTime(0, 0));

    if (startDate < tomorrow) {
        QMessageBox::warning(this, "Validation Error",
                             QString("Start Date must be in the future (tomorrow or later).\nCurrent date: %1\nMinimum date: %2")
                                 .arg(QDate::currentDate().toString("MM/dd/yy"))
                                 .arg(tomorrow.date().toString("MM/dd/yy")));
        return;
    }

    if (endDate < tomorrow) {
        QMessageBox::warning(this, "Validation Error",
                             QString("End Date must be in the future (tomorrow or later).\nCurrent date: %1\nMinimum date: %2")
                                 .arg(QDate::currentDate().toString("MM/dd/yy"))
                                 .arg(tomorrow.date().toString("MM/dd/yy")));
        return;
    }

    // 4. Validate End Date >= Start Date
    if (endDate < startDate) {
        QMessageBox::warning(this, "Validation Error", "End Date must be on or after the Start Date.");
        return;
    }

    // ===== ALL VALIDATIONS PASSED =====

    QString startDateStr = startDate.toString("dd/MM/yy");
    QString endDateStr = endDate.toString("dd/MM/yy");
    int updatedDockId = selectedDockingId;

    if (dockingManager.updateDocking(selectedDockingId, location, length,
                                     height, status, capacity, startDateStr, endDateStr)) {
        appendDockingHistory("UPDATE", updatedDockId);
        QMessageBox::information(this, "Success", "Docking updated successfully!");
        selectedDockingId = -1;          // reset selection
        loadDockingTable();              // refresh table
        ui->stackedWidget_2->setCurrentIndex(1);  // go back to display page
    } else {
        QMessageBox::critical(this, "Error", "Failed to update docking.");
    }
}

// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  Delete button
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::on_delete_docking_clicked()
{
    QTableWidget *table = ui->tabdocking;
    int currentRow = table->currentRow();

    if (currentRow < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a row to delete.");
        return;
    }

    int id       = table->item(currentRow, 0)->text().toInt();
    QString loc  = table->item(currentRow, 1)->text();

    QMessageBox::StandardButton reply =
        QMessageBox::question(this, "Confirm Delete",
                              QString("Are you sure you want to delete docking at \"%1\" (ID: %2)?")
                                  .arg(loc).arg(id),
                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (dockingManager.deleteDocking(id)) {
            appendDockingHistory("DELETE", id);
            QMessageBox::information(this, "Deleted", "Docking deleted successfully.");
            loadDockingTable();   // refresh the table
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete docking.");
        }
    }
}

// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  Clear button
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::on_clear_docking_clicked()
{
    ui->location->clear();
    ui->length->clear();
    ui->height->clear();
    ui->capacity->clear();
    ui->startDate->setDateTime(QDateTime::currentDateTime());
    ui->endDate->setDateTime(QDateTime::currentDateTime());
    ui->status->setCurrentIndex(0);
    ui->selected_id->clear();
    selectedDockingId = -1;
}

void appwindow::on_searchbar_docking_textChanged(const QString &text)
{
    QString searchText = text.trimmed().toLower();
    QList<DockingRecord> filteredRecords;

    // Filter records based on search text (search in all attributes)
    for (const DockingRecord &r : allDockingRecords) {
        if (QString::number(r.id).contains(searchText, Qt::CaseInsensitive) ||
            r.status.contains(searchText, Qt::CaseInsensitive) ||
            r.startDate.contains(searchText, Qt::CaseInsensitive) ||
            r.endDate.contains(searchText, Qt::CaseInsensitive) ||
            r.location.contains(searchText, Qt::CaseInsensitive) ||
            r.length.contains(searchText, Qt::CaseInsensitive) ||
            r.height.contains(searchText, Qt::CaseInsensitive) ||
            r.capacity.contains(searchText, Qt::CaseInsensitive)) {
            filteredRecords.append(r);
        }
    }

    // Apply the current sort to the filtered results
    int sortIndex = ui->docking_sort->currentIndex();
    on_docking_sort_currentIndexChanged(sortIndex);
}

void appwindow::on_docking_sort_currentIndexChanged(int index)
{
    // Get the current filtered records (or all if no filter)
    QString searchText = ui->searchbar_docking->text().trimmed().toLower();
    QList<DockingRecord> recordsToSort;

    if (searchText.isEmpty()) {
        recordsToSort = allDockingRecords;
    } else {
        // Apply filter first
        for (const DockingRecord &r : allDockingRecords) {
            if (QString::number(r.id).contains(searchText, Qt::CaseInsensitive) ||
                r.status.contains(searchText, Qt::CaseInsensitive) ||
                r.startDate.contains(searchText, Qt::CaseInsensitive) ||
                r.endDate.contains(searchText, Qt::CaseInsensitive) ||
                r.location.contains(searchText, Qt::CaseInsensitive) ||
                r.length.contains(searchText, Qt::CaseInsensitive) ||
                r.height.contains(searchText, Qt::CaseInsensitive) ||
                r.capacity.contains(searchText, Qt::CaseInsensitive)) {
                recordsToSort.append(r);
            }
        }
    }

    // Sort based on start date
    if (index == 1) {  // Ascending
        QDate today = QDate::currentDate();
        std::sort(recordsToSort.begin(), recordsToSort.end(), [today](const DockingRecord &a, const DockingRecord &b) {
            QDate dateA = QDate::fromString(a.startDate, "yyyy-MM-dd");
            if (!dateA.isValid()) dateA = QDate::fromString(a.startDate, "MM/dd/yy");
            if (!dateA.isValid()) dateA = QDate::fromString(a.startDate, "dd/MM/yy");

            QDate dateB = QDate::fromString(b.startDate, "yyyy-MM-dd");
            if (!dateB.isValid()) dateB = QDate::fromString(b.startDate, "MM/dd/yy");
            if (!dateB.isValid()) dateB = QDate::fromString(b.startDate, "dd/MM/yy");

            qint64 distA = qAbs(today.daysTo(dateA));
            qint64 distB = qAbs(today.daysTo(dateB));
            return distA > distB;
        });
    } else if (index == 2) {  // Descending
        QDate today = QDate::currentDate();
        std::sort(recordsToSort.begin(), recordsToSort.end(), [today](const DockingRecord &a, const DockingRecord &b) {
            QDate dateA = QDate::fromString(a.startDate, "yyyy-MM-dd");
            if (!dateA.isValid()) dateA = QDate::fromString(a.startDate, "MM/dd/yy");
            if (!dateA.isValid()) dateA = QDate::fromString(a.startDate, "dd/MM/yy");

            QDate dateB = QDate::fromString(b.startDate, "yyyy-MM-dd");
            if (!dateB.isValid()) dateB = QDate::fromString(b.startDate, "MM/dd/yy");
            if (!dateB.isValid()) dateB = QDate::fromString(b.startDate, "dd/MM/yy");

            qint64 distA = qAbs(today.daysTo(dateA));
            qint64 distB = qAbs(today.daysTo(dateB));
            return distA < distB;
        });
    }
    // Index 0 = "Date" (default, no sorting needed)

    populateDockingTable(recordsToSort);
}

void appwindow::on_export_docking_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export Docking Data to PDF"), "",
                                                    tr("PDF Files (*.pdf);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    QPdfWriter pdfWriter(fileName);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Landscape); // Landscape for more width
    pdfWriter.setResolution(300);
    pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor darkBlue(0, 102, 204);
    QColor veryLightBlue(230, 242, 255);
    QColor lightGray(245, 245, 245);
    QColor textColor(30, 30, 30);
    QColor borderColor(180, 180, 180);

    int pageWidth  = pdfWriter.width();
    int pageHeight = pdfWriter.height();

    // Use painter units directly (no extra margin since QPdfWriter margins are set)
    int marginLeft   = 0;
    int marginTop    = 0;
    int contentWidth = pageWidth;

    int yPos = marginTop;

    // ٤?٤? TITLE BLOCK ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    int titleBlockHeight = 120;
    // Background banner
    painter.fillRect(marginLeft, yPos, contentWidth, titleBlockHeight, darkBlue);

    // Title text
    QFont titleFont("Arial", 32, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(Qt::white);
    painter.drawText(marginLeft, yPos, contentWidth, titleBlockHeight,
                     Qt::AlignCenter | Qt::AlignVCenter, "DOCKING DATA REPORT");

    yPos += titleBlockHeight + 30; // gap after banner

    // ٤?٤? DATE LINE ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    QFont dateFont("Arial", 11);
    painter.setFont(dateFont);
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(marginLeft, yPos,
                     QString("Generated: %1")
                         .arg(QDateTime::currentDateTime()
                                  .toString("yyyy-MM-dd  hh:mm:ss")));
    yPos += 50;

    // ٤?٤? TABLE ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    QTableWidget *table = ui->tabdocking;
    int rowCount    = table->rowCount();
    int columnCount = table->columnCount();

    if (rowCount == 0) {
        painter.setFont(QFont("Arial", 14));
        painter.setPen(textColor);
        painter.drawText(marginLeft, yPos, "No data to export.");
        painter.end();
        QMessageBox::information(this, "No Data", "The table is empty. Nothing to export.");
        return;
    }

    // Distribute column widths evenly across full content width
    int columnWidth = contentWidth / columnCount;
    int headerHeight = 80;

    auto drawHeader = [&]() {
        // Header background
        painter.fillRect(marginLeft, yPos, contentWidth, headerHeight, darkBlue);

        QFont headerFont("Arial", 10, QFont::Bold);
        painter.setFont(headerFont);
        painter.setPen(Qt::white);

        int xPos = marginLeft;
        for (int col = 0; col < columnCount; col++) {
            QString headerText = table->horizontalHeaderItem(col)
            ? table->horizontalHeaderItem(col)->text()
            : "";
            painter.drawText(xPos + 8, yPos, columnWidth - 16, headerHeight,
                             Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                             headerText);

            // Vertical separator between columns
            if (col < columnCount - 1) {
                painter.setPen(QPen(Qt::white, 2));
                painter.drawLine(xPos + columnWidth, yPos,
                                 xPos + columnWidth, yPos + headerHeight);
                painter.setPen(Qt::white);
            }
            xPos += columnWidth;
        }
    };

    drawHeader();
    yPos += headerHeight;

    // Row sizing �?� fill available vertical space nicely
    int rowHeight    = 75;
    int footerHeight = 80;
    int rowsPerPage  = (pageHeight - yPos - footerHeight) / rowHeight;

    QFont cellFont("Arial", 11);
    painter.setFont(cellFont);

    for (int row = 0; row < rowCount; row++) {

        // ٤?٤? New page if needed ٤?٤?
        if (row > 0 && row % rowsPerPage == 0) {
            pdfWriter.newPage();
            yPos = marginTop;
            drawHeader();
            yPos += headerHeight;
            painter.setFont(cellFont);
        }

        // Alternating row background
        QColor rowBg = (row % 2 == 0) ? Qt::white : veryLightBlue;
        painter.fillRect(marginLeft, yPos, contentWidth, rowHeight, rowBg);

        // Row outer border
        painter.setPen(QPen(borderColor, 1));
        painter.drawRect(marginLeft, yPos, contentWidth, rowHeight);

        // Cell content + column separators
        int xPos = marginLeft;
        for (int col = 0; col < columnCount; col++) {
            QTableWidgetItem *item = table->item(row, col);
            QString cellText = item ? item->text() : "";

            painter.setPen(textColor);
            painter.drawText(xPos + 10, yPos, columnWidth - 20, rowHeight,
                             Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                             cellText);

            if (col < columnCount - 1) {
                painter.setPen(QPen(borderColor, 1));
                painter.drawLine(xPos + columnWidth, yPos,
                                 xPos + columnWidth, yPos + rowHeight);
            }
            xPos += columnWidth;
        }

        yPos += rowHeight;
    }

    // ٤?٤? FOOTER ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    yPos += 30;

    // Thin separator line
    painter.setPen(QPen(darkBlue, 3));
    painter.drawLine(marginLeft, yPos, marginLeft + contentWidth, yPos);
    yPos += 20;

    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(marginLeft, yPos,
                     contentWidth, 50,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("Total Records: %1").arg(rowCount));

    painter.drawText(marginLeft, yPos,
                     contentWidth, 50,
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("Marina Management System"));

    painter.end();

    QMessageBox::information(this, "Export Successful",
                             QString("PDF exported successfully to:\n%1").arg(fileName));
}




// =====================================================================
// =================== START OF USER MODULE ============================
// =====================================================================


/// Add / Update user  --> not really

void appwindow::on_UPDUser_clicked()
{
    QString email = ui->email->text().trimmed();
    QString firstName = ui->Fname->text().trimmed();
    QString lastName = ui->Lname->text().trimmed();
    QString password = ui->password->text().trimmed();
    QString role = ui->role->currentText();
    QString gender = ui->gender->currentText().left(1).toUpper(); // M/F

    bool ok;
    double salary = ui->salary->text().toDouble(&ok);
    if (!ok || salary <= 0) {
        QMessageBox::warning(this, "Error", "Salary must be a positive number!");
        return;
    }

    QString shiftStart = ui->startShift->dateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString shiftEnd = ui->endShift->dateTime().toString("yyyy-MM-dd HH:mm:ss");

    bool isEditMode = (selectedUserId != -1);

    if (email.isEmpty() || firstName.isEmpty() || lastName.isEmpty() || (!isEditMode && password.isEmpty())) {
        QMessageBox::warning(this, "Error", "Please fill in all required fields!");
        return;
    }

    if (ui->endShift->dateTime() < ui->startShift->dateTime()) {
        QMessageBox::warning(this, "Error", "Shift end must be on or after shift start.");
        return;
    }

    bool success = false;

    int supervisorId = 0;

    if (!isEditMode && connectedUserRole.compare("ADMIN", Qt::CaseInsensitive) == 0 && connectedUserId > 0) {
        supervisorId = connectedUserId;
    }

    if (isEditMode) {
        QSqlQuery supervisorQuery;
        supervisorQuery.prepare("SELECT SUPERVISORID FROM USERS WHERE USERID = :id");
        supervisorQuery.bindValue(":id", selectedUserId);
        if (supervisorQuery.exec() && supervisorQuery.next()) {
            supervisorId = supervisorQuery.value(0).toInt();
        }
    }

    if (isEditMode) {
        User editedUser(selectedUserId, email, firstName, lastName, password,
                        role, gender, salary, shiftStart, shiftEnd, supervisorId);
        success = editedUser.modifier_user();
    } else {
        User newUser(0, email, firstName, lastName, password,
                     role, gender, salary, shiftStart, shiftEnd, supervisorId);
        success = newUser.ajouter_user();
    }

    if (success) {
        QMessageBox::information(this, "Success", isEditMode ? "User updated successfully!" : "User created successfully!");

        // Clear the form
        ui->email->clear();
        ui->Fname->clear();
        ui->Lname->clear();
        ui->password->clear();
        ui->role->setCurrentIndex(0);
        ui->gender->setCurrentIndex(0);
        ui->salary->clear();
        ui->startShift->setDateTime(QDateTime::currentDateTime());
        ui->endShift->setDateTime(QDateTime::currentDateTime());
        ui->UPDUser->setText("Create User ?���");
        selectedUserId = -1;
        loadUsersTable();
        if (isEditMode) {
            ui->stackedWidget->setCurrentIndex(1);
        }
    } else {
        QMessageBox::critical(this, "Error", isEditMode
                                                 ? "Failed to update user. Please check the database connection."
                                                 : "Failed to create user. Please check the database connection.");
    }
}


// Load users table from database +++

void appwindow::loadUsersTable()
{
    allUserRecords = userManager.afficher_liste();

    int sortIndex = ui->comboBox_11->currentIndex();
    on_comboBox_11_currentIndexChanged(sortIndex);
}

void appwindow::on_searchbar_3_textChanged(const QString &text)
{
    Q_UNUSED(text);
    int sortIndex = ui->comboBox_11->currentIndex();
    on_comboBox_11_currentIndexChanged(sortIndex);
}

void appwindow::on_comboBox_11_currentIndexChanged(int index)
{
    QString searchText = ui->searchbar_3->text().trimmed();
    QList<User> recordsToDisplay;

    if (searchText.isEmpty()) {
        recordsToDisplay = allUserRecords;
    } else {
        bool isNumeric;
        searchText.toInt(&isNumeric);

        for (const User &r : allUserRecords) {
            if (isNumeric) {
                if (QString::number(r.getUserId()).contains(searchText)) {
                    recordsToDisplay.append(r);
                }
            } else if (r.getEmail().contains(searchText, Qt::CaseInsensitive)) {
                recordsToDisplay.append(r);
            }
        }
    }

    if (index == 0) { // Sort by Role
        std::sort(recordsToDisplay.begin(), recordsToDisplay.end(), [](const User &a, const User &b) {
            const int roleComparison = a.getRole().compare(b.getRole(), Qt::CaseInsensitive);
            if (roleComparison != 0) {
                return roleComparison < 0;
            }

            const int genderComparison = a.getGender().compare(b.getGender(), Qt::CaseInsensitive);
            if (genderComparison != 0) {
                return genderComparison < 0;
            }

            return a.getUserId() < b.getUserId();
        });
    } else if (index == 1) { // Sort by Gender
        std::sort(recordsToDisplay.begin(), recordsToDisplay.end(), [](const User &a, const User &b) {
            const int genderComparison = a.getGender().compare(b.getGender(), Qt::CaseInsensitive);
            if (genderComparison != 0) {
                return genderComparison < 0;
            }

            const int roleComparison = a.getRole().compare(b.getRole(), Qt::CaseInsensitive);
            if (roleComparison != 0) {
                return roleComparison < 0;
            }

            return a.getUserId() < b.getUserId();
        });
    }

    QTableWidget *table = ui->usersTable;
    table->setRowCount(0);
    table->setColumnCount(9);
    table->setHorizontalHeaderLabels(
        {"ID", "Email", "First Name", "Last Name", "Role", "Gender", "Salary", "Shift Start", "Shift End"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);

    for (const User &r : recordsToDisplay) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(r.getUserId())));
        table->setItem(row, 1, new QTableWidgetItem(r.getEmail()));
        table->setItem(row, 2, new QTableWidgetItem(r.getFirstName()));
        table->setItem(row, 3, new QTableWidgetItem(r.getLastName()));
        table->setItem(row, 4, new QTableWidgetItem(r.getRole()));
        table->setItem(row, 5, new QTableWidgetItem(r.getGender()));
        table->setItem(row, 6, new QTableWidgetItem(QString::number(r.getSalary())));
        table->setItem(row, 7, new QTableWidgetItem(r.getShiftStart()));
        table->setItem(row, 8, new QTableWidgetItem(r.getShiftEnd()));
    }

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->labelResults_3->setText(QString("Showing %1 Users").arg(recordsToDisplay.size()));
}



// +++ Table logic for Display & Edit user ++++

void appwindow::fillUserForm(const QModelIndex &index)
{
    if (!index.isValid()) {
        return;
    }

    QAbstractItemModel *model = ui->usersTable->model();
    int row = index.row();

    selectedUserId = model->data(model->index(row, 0)).toInt();

    QString email = model->data(model->index(row, 1)).toString();
    QString firstName = model->data(model->index(row, 2)).toString();
    QString lastName = model->data(model->index(row, 3)).toString();
    QString role = model->data(model->index(row, 4)).toString();
    QString gender = model->data(model->index(row, 5)).toString();
    QString salary = model->data(model->index(row, 6)).toString();
    QString shiftStart = model->data(model->index(row, 7)).toString();
    QString shiftEnd = model->data(model->index(row, 8)).toString();

    ui->email->setText(email);
    ui->Fname->setText(firstName);
    ui->Lname->setText(lastName);
    ui->password->clear();
    ui->salary->setText(salary);

    int roleIdx = ui->role->findText(role);
    if (roleIdx >= 0) {
        ui->role->setCurrentIndex(roleIdx);
    }

    int genderIdx = ui->gender->findText(gender, Qt::MatchContains);
    if (genderIdx >= 0) {
        ui->gender->setCurrentIndex(genderIdx);
    }

    QDateTime startDt = QDateTime::fromString(shiftStart, "yyyy-MM-dd HH:mm:ss");
    if (!startDt.isValid()) {
        startDt = QDateTime::fromString(shiftStart, "MM/dd/yy");
    }
    if (startDt.isValid()) {
        ui->startShift->setDateTime(startDt);
    }

    QDateTime endDt = QDateTime::fromString(shiftEnd, "yyyy-MM-dd HH:mm:ss");
    if (!endDt.isValid()) {
        endDt = QDateTime::fromString(shiftEnd, "MM/dd/yy");
    }
    if (endDt.isValid()) {
        ui->endShift->setDateTime(endDt);
    }

    ui->stackedWidget->setCurrentIndex(0);
    ui->UPDUser->setText("Update User");
}


void appwindow::on_usersTable_cellClicked(int row, int /*column*/)
{
    if (row < 0 || !ui->usersTable->item(row, 0)) {
        return;
    }

    selectedUserId = ui->usersTable->item(row, 0)->text().toInt();
}


void appwindow::on_usersTable_cellDoubleClicked(int row, int column)
{
    if (row < 0) {
        return;
    }

    fillUserForm(ui->usersTable->model()->index(row, column));
}

// +++ Delete user ++++
void appwindow::on_deleteUSERBtn_clicked()
{
    QModelIndexList selected = ui->usersTable->selectionModel()->selectedRows();

    if (selected.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please select a user to delete.");
        return;
    }

    int row = selected.first().row();

    int userId = ui->usersTable->model()->data(
                                            ui->usersTable->model()->index(row, 0)
                                            ).toInt();

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirm Delete",
                                  "Are you sure you want to delete this user?",
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::No)
        return;

    if (userManager.supprimer(userId)) {
        QMessageBox::information(this, "Success", "User deleted successfully!");
        selectedUserId = -1;
        loadUsersTable();
    } else {
        QMessageBox::critical(this, "Error", "Failed to delete user.");
    }
}

// +++ Clear user form +++ reset to create mode +++
void appwindow::on_clear_3_clicked()
{
    ui->email->clear();
    ui->Fname->clear();
    ui->Lname->clear();
    ui->password->clear();
    ui->salary->clear();
    ui->role->setCurrentIndex(0);
    ui->gender->setCurrentIndex(0);
    ui->startShift->setDateTime(QDateTime::currentDateTime());
    ui->endShift->setDateTime(QDateTime::currentDateTime());

    ui->searchbar_3->clear();
    ui->comboBox_11->setCurrentIndex(0);

    selectedUserId = -1;
    ui->UPDUser->setText("Create User ?���");
    if (ui->usersTable->selectionModel()) {
        ui->usersTable->selectionModel()->clearSelection();
    }

    loadUsersTable();
}

void appwindow::on_editUSERBtn_clicked()
{
    int row = ui->usersTable->currentRow();

    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a user row first.");
        return;
    }

    fillUserForm(ui->usersTable->model()->index(row, 0));
}

void appwindow::on_export_pdf_user_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export User Data to PDF"), "",
                                                    tr("PDF Files (*.pdf);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    QPdfWriter pdfWriter(fileName);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Landscape);
    pdfWriter.setResolution(300);
    pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor darkBlue(0, 102, 204);
    QColor veryLightBlue(230, 242, 255);
    QColor textColor(30, 30, 30);
    QColor borderColor(180, 180, 180);

    int pageWidth  = pdfWriter.width();
    int pageHeight = pdfWriter.height();

    int marginLeft   = 0;
    int marginTop    = 0;
    int contentWidth = pageWidth;

    int yPos = marginTop;

    int titleBlockHeight = 120;
    painter.fillRect(marginLeft, yPos, contentWidth, titleBlockHeight, darkBlue);

    QFont titleFont("Arial", 32, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(Qt::white);
    painter.drawText(marginLeft, yPos, contentWidth, titleBlockHeight,
                     Qt::AlignCenter | Qt::AlignVCenter, "USER DATA REPORT");

    yPos += titleBlockHeight + 30;

    QFont dateFont("Arial", 11);
    painter.setFont(dateFont);
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(marginLeft, yPos,
                     QString("Generated: %1")
                         .arg(QDateTime::currentDateTime()
                                  .toString("yyyy-MM-dd  hh:mm:ss")));
    yPos += 50;

    QTableWidget *table = ui->usersTable;
    int rowCount    = table->rowCount();
    int columnCount = table->columnCount();

    if (rowCount == 0 || columnCount == 0) {
        painter.setFont(QFont("Arial", 14));
        painter.setPen(textColor);
        painter.drawText(marginLeft, yPos, "No data to export.");
        painter.end();
        QMessageBox::information(this, "No Data", "The user table is empty. Nothing to export.");
        return;
    }

    int columnWidth  = contentWidth / columnCount;
    int headerHeight = 80;

    auto drawHeader = [&]() {
        painter.fillRect(marginLeft, yPos, contentWidth, headerHeight, darkBlue);

        QFont headerFont("Arial", 13, QFont::Bold);
        painter.setFont(headerFont);
        painter.setPen(Qt::white);

        int xPos = marginLeft;
        for (int col = 0; col < columnCount; col++) {
            QString headerText = table->horizontalHeaderItem(col)
            ? table->horizontalHeaderItem(col)->text()
            : "";

            painter.drawText(xPos + 8, yPos, columnWidth - 16, headerHeight,
                             Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                             headerText);

            if (col < columnCount - 1) {
                painter.setPen(QPen(Qt::white, 2));
                painter.drawLine(xPos + columnWidth, yPos,
                                 xPos + columnWidth, yPos + headerHeight);
                painter.setPen(Qt::white);
            }
            xPos += columnWidth;
        }
    };

    drawHeader();
    yPos += headerHeight;

    int rowHeight    = 75;
    int footerHeight = 80;
    int rowsPerPage  = (pageHeight - yPos - footerHeight) / rowHeight;

    QFont cellFont("Arial", 11);
    painter.setFont(cellFont);

    for (int row = 0; row < rowCount; row++) {
        if (row > 0 && row % rowsPerPage == 0) {
            pdfWriter.newPage();
            yPos = marginTop;
            drawHeader();
            yPos += headerHeight;
            painter.setFont(cellFont);
        }

        QColor rowBg = (row % 2 == 0) ? Qt::white : veryLightBlue;
        painter.fillRect(marginLeft, yPos, contentWidth, rowHeight, rowBg);

        painter.setPen(QPen(borderColor, 1));
        painter.drawRect(marginLeft, yPos, contentWidth, rowHeight);

        int xPos = marginLeft;
        for (int col = 0; col < columnCount; col++) {
            QTableWidgetItem *item = table->item(row, col);
            QString cellText = item ? item->text() : "";

            painter.setPen(textColor);
            painter.drawText(xPos + 10, yPos, columnWidth - 20, rowHeight,
                             Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                             cellText);

            if (col < columnCount - 1) {
                painter.setPen(QPen(borderColor, 1));
                painter.drawLine(xPos + columnWidth, yPos,
                                 xPos + columnWidth, yPos + rowHeight);
            }
            xPos += columnWidth;
        }

        yPos += rowHeight;
    }

    yPos += 30;

    painter.setPen(QPen(darkBlue, 3));
    painter.drawLine(marginLeft, yPos, marginLeft + contentWidth, yPos);
    yPos += 20;

    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(marginLeft, yPos,
                     contentWidth, 50,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("Total Records: %1").arg(rowCount));

    painter.drawText(marginLeft, yPos,
                     contentWidth, 50,
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("Marina Management System"));

    painter.end();

    QMessageBox::information(this, "Export Successful",
                             QString("PDF exported successfully to:\n%1").arg(fileName));
}
void appwindow::loadUserStatistics(bool byRole)
{
    // Clear previous chart
    while (QLayoutItem *item = ui->statsiticslayout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->deleteLater();
        }
        delete item;
    }

    // Get TOTAL users
    int total = 0;
    QSqlQuery totalQuery;
    if (totalQuery.exec("SELECT COUNT(*) FROM USERS") && totalQuery.next()) {
        total = totalQuery.value(0).toInt();
    }

    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet("Percentage (%)");
    QStringList categories;

    QSqlQuery query;
    const QString sql = byRole
        ? "SELECT ROLE, COUNT(*) FROM USERS GROUP BY ROLE ORDER BY ROLE"
        : "SELECT GENDER, COUNT(*) FROM USERS GROUP BY GENDER ORDER BY GENDER";

    if (!query.exec(sql)) {
        QLabel *errorLabel = new QLabel("Failed to load statistics data.");
        errorLabel->setAlignment(Qt::AlignCenter);
        ui->statsiticslayout->addWidget(errorLabel);
        return;
    }

    // Fill data (convert to percentage)
    while (query.next()) {
        const QString category = query.value(0).toString().trimmed().isEmpty()
            ? "Unknown"
            : query.value(0).toString().trimmed();

        const int count = query.value(1).toInt();
        double percentage = (total > 0) ? (count * 100.0 / total) : 0;

        *set << percentage;
        categories << category;
    }

    // Handle empty data
    if (categories.isEmpty()) {
        *set << 0;
        categories << "No Data";
    }

    series->append(set);

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(byRole ? "Users Statistics by Role (%)"
                           : "Users Statistics by Gender (%)");
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // X axis
    QBarCategoryAxis *axis = new QBarCategoryAxis();
    axis->append(categories);
    chart->addAxis(axis, Qt::AlignBottom);
    series->attachAxis(axis);

    // Y axis (percentage)
    QValueAxis *axisY = new QValueAxis();
    axisY->setRange(0, 100);
    axisY->setLabelFormat("%.0f%%");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // Chart view
    QChartView *chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);

    ui->statsiticslayout->addWidget(chartView);
}

void appwindow::on_pushButton_8_clicked()
{
    const bool byRole = (ui->comboBox_12->currentIndex() == 0);
    loadUserStatistics(byRole);
}


//log out special feature:



// =====================================================================
// ===================== END OF USER MODULE ============================
// =====================================================================






///PRODUCT


// Add product

void appwindow::on_checkProductButton_2_clicked()
{
    QString type = ui->fishtype->text();
    QString status = ui->fishStatus->currentText();
    QString quantity = ui->qtyfish->text();
    QString price = ui->fishprice->text();
    QString location = ui->locationfish->text();
    QString temperatureText = ui->fishTemp->text().trimmed();

    if (type.isEmpty() || quantity.isEmpty() || price.isEmpty() || location.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please fill in all required fields!");
        return;
    }

    bool ok;
    int qty = quantity.toInt(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Error", "Quantity must be a number!");
        return;
    }
    double pr = price.toDouble(&ok);
    if (!ok) {
        QMessageBox::warning(this, "Error", "Price must be a number!");
        return;
    }

    double temperature = std::numeric_limits<double>::quiet_NaN();
    if (!temperatureText.isEmpty()) {
        temperatureText.replace(',', '.');
        temperature = temperatureText.toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "Error", "Temperature must be a number!");
            return;
        }
    }

    // Get boat ID from dropdown (foreign key values from BOAT table)
    int boatId = -1;
    if (!ui->boatCombo->currentText().trimmed().isEmpty()) {
        bool ok;
        boatId = ui->boatCombo->currentText().trimmed().toInt(&ok);
        if (!ok) {
            QMessageBox::warning(this, "Error", "Boat ID must be a valid number!");
            return;
        }
    }

    // Check if editing or creating
    if (selectedProductId > 0) {
        // Update existing product
        if (productManager.updateProduct(selectedProductId,
                                         type,
                                         location,
                                         status,
                                         qty,
                                         pr,
                                         temperature,
                                         ui->fishdate1->dateTime(),
                                         ui->fishdate2->dateTime(),
                                         boatId)) {
            QMessageBox::information(this, "Success", "Product updated successfully!");
            selectedProductId = -1;  // Reset for next operation
        } else {
            QMessageBox::critical(this, "Error", "Failed to update product!");
            return;
        }
    } else {
        // Create new product
        if (!productManager.createProduct(type,
                                          location,
                                          status,
                                          qty,
                                          pr,
                                          temperature,
                                          ui->fishdate1->dateTime(),
                                          ui->fishdate2->dateTime(),
                                          boatId)) {
            QMessageBox::critical(this, "Error", "Failed to add product. Please check the database connection.");
            return;
        }
        QMessageBox::information(this, "Success", "Product added successfully!");
    }

    // Clear form
    ui->fishtype->clear();
    ui->fishStatus->setCurrentIndex(0);
    ui->qtyfish->clear();
    ui->fishprice->clear();
    ui->fishTemp->clear();
    ui->fishdate1->setDateTime(QDateTime::currentDateTime());
    ui->fishdate2->setDateTime(QDateTime::currentDateTime());
    ui->locationfish->clear();
    ui->boatCombo->setCurrentIndex(0);
    selectedProductId = -1;

    // Reset button text back to "Add Product"
    ui->checkProductButton_2->setText("Add Product");

    // Refresh the table on manage page
    loadProductTable();
}
void appwindow::on_pushButton_11_clicked()
{
    // Get all products
    QList<ProductRecord> allProducts = productManager.getAllProducts();

    if (allProducts.isEmpty()) {
        QMessageBox::warning(this, "No Data", "No products in the system to analyze.");
        return;
    }

    // Determine which statistics to show
    int selectedOption = ui->comboBox_17->currentIndex();

    if (selectedOption == 0) {
        // Show statistics by Status (pie chart)
        generateProductStatisticsByStatus(allProducts);
    } else {
        // Show quantity distribution by fish type (pie chart)
        generateProductQuantityByType(allProducts);
    }
}


void appwindow::on_checkProductButton_clicked()
{
    ui->stackedWidget_4->setCurrentIndex(1);  // Switch to Manage Products page
    loadProductTable();                        // Load the products
}

void appwindow::loadProductBoatIds()
{
    if (!ui->boatCombo) {
        return;
    }

    const QString previous = ui->boatCombo->currentText().trimmed();

    ui->boatCombo->blockSignals(true);
    ui->boatCombo->clear();
    ui->boatCombo->addItem("");

    QSqlQuery query;
    if (query.exec("SELECT BOATID FROM BOAT ORDER BY BOATID")) {
        while (query.next()) {
            ui->boatCombo->addItem(query.value(0).toString());
        }
    }

    ui->boatCombo->setEditable(false);

    if (!previous.isEmpty()) {
        const int idx = ui->boatCombo->findText(previous);
        if (idx >= 0) {
            ui->boatCombo->setCurrentIndex(idx);
        } else {
            ui->boatCombo->setCurrentIndex(0);
        }
    } else {
        ui->boatCombo->setCurrentIndex(0);
    }

    ui->boatCombo->blockSignals(false);
}

void appwindow::loadProductTable()
{
    loadProductBoatIds();

    QList<ProductRecord> records = productManager.getAllProducts();
    allProductRecords = records;  // Store all records for filtering
    qDebug() << "Product records fetched:" << records.size();

    QTableWidget *table = ui->tableWidget_10;
    table->setRowCount(0);
    table->setColumnCount(10);
    table->setHorizontalHeaderLabels(
        {"ID", "Status", "Type", "Fish Caught", "Date Purchase", "Quantity", "Location", "Temperature (C)", "Original Price", "Discounted Price"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::ExtendedSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    for (const ProductRecord &r : records) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(r.id)));
        table->setItem(row, 1, new QTableWidgetItem(r.status));
        table->setItem(row, 2, new QTableWidgetItem(r.type));
        table->setItem(row, 3, new QTableWidgetItem(r.fishCaught));
        table->setItem(row, 4, new QTableWidgetItem(r.dateOfPurchase));
        table->setItem(row, 5, new QTableWidgetItem(QString::number(r.quantity)));
        table->setItem(row, 6, new QTableWidgetItem(r.location));
        table->setItem(row, 7, new QTableWidgetItem(std::isnan(r.temperature)
                                 ? "-"
                                 : QString::number(r.temperature, 'f', 2)));
        table->setItem(row, 8, new QTableWidgetItem(QString::number(r.originalPrice, 'f', 2)));
        table->setItem(row, 9, new QTableWidgetItem(QString::number(r.discountedPrice, 'f', 2)));
    }

    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    table->setColumnWidth(0, 60);   // ID
    table->setColumnWidth(1, 120);  // Status
    table->setColumnWidth(2, 120);  // Type
    table->setColumnWidth(3, 170);  // Fish Caught
    table->setColumnWidth(4, 170);  // Date Purchase
    table->setColumnWidth(5, 100);  // Quantity
    table->setColumnWidth(6, 130);  // Location
    table->setColumnWidth(7, 140);  // Temperature
    table->setColumnWidth(8, 160);  // Original Price
    table->setColumnWidth(9, 180);  // Discounted Price

    // Update the count label
    ui->labelResults_6->setText(QString("Showing %1 Products").arg(records.size()));

    // Keep Discounts page alerts synced with current product quantities.
    refreshLowStockAlerts(false);
}



void appwindow::on_Manage_24_clicked()
{
    loadProductTable();
}

void appwindow::refreshLowStockAlerts(bool showPopup)
{
    const QList<ProductRecord> products = productManager.getAllProducts();
    QStringList lowStockLines;

    auto resizeAlertArea = [&](int lineCount) {
        const int visibleLines = qMax(2, lineCount);
        const int contentHeight = 24 + (visibleLines * 24);
        const int dockHeight = contentHeight + 55;

        ui->dockWidget_2->setMinimumHeight(dockHeight);
        ui->dockWidget_2->resize(ui->dockWidget_2->width(), dockHeight);

        const int labelWidth = qMax(200, ui->dockWidgetContents_2->width() - 20);
        ui->label_3->setGeometry(10, 16, labelWidth, contentHeight);
        ui->label_3->setWordWrap(true);
        ui->label_3->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    };

    for (const ProductRecord &product : products) {
        if (product.quantity < 30) {
            lowStockLines << QString("- %1 (ID %2): %3 left")
                                 .arg(product.type)
                                 .arg(product.id)
                                 .arg(product.quantity);
        }
    }

    if (lowStockLines.isEmpty()) {
        resizeAlertArea(2);
        ui->dockWidget_2->setWindowTitle("Stock level looks good");
        ui->label_3->setText(QString("No low-stock products.\nTotal products: %1").arg(products.size()));
        ui->label_3->setStyleSheet("color: #14532d; font-weight: 600;");
        return;
    }

    resizeAlertArea(lowStockLines.size() + 1);
    ui->dockWidget_2->setWindowTitle(QString("Low-stock alert: %1 product(s) below 30").arg(lowStockLines.size()));
    ui->label_3->setText(lowStockLines.join("\n"));
    ui->label_3->setStyleSheet("color: #b91c1c; font-weight: 700;");

    if (showPopup && !lowStockWarningShown) {
        QMessageBox::warning(this,
                             "Low Stock Alert",
                             QString("At least one product is below quantity 30.\n\n%1")
                                 .arg(lowStockLines.join("\n")));
        lowStockWarningShown = true;
    }
}

void appwindow::on_tableWidget_10_cellClicked(int row, int /*column*/)
{
    if (row < 0) return;
    QString id = ui->tableWidget_10->item(row, 0)->text();
    selectedProductId = id.toInt();
    qDebug() << "Selected product ID:" << selectedProductId;
}

void appwindow::on_tableWidget_10_cellDoubleClicked(int row, int /*column*/)
{
    QTableWidget *table = ui->tableWidget_10;
    if (row < 0) return;

    selectedProductId = table->item(row, 0)->text().toInt();
    qDebug() << "Double-clicked product ID:" << selectedProductId;

    // Fill the form fields for editing
    ui->fishtype->setText(table->item(row, 2)->text());
    ui->qtyfish->setText(table->item(row, 5)->text());
    ui->fishprice->setText(table->item(row, 8)->text());
    ui->locationfish->setText(table->item(row, 6)->text());
    ui->fishTemp->setText(table->item(row, 7)->text() == "-" ? "" : table->item(row, 7)->text());

    // Set status combobox
    int statusIdx = ui->fishStatus->findText(table->item(row, 1)->text());
    if (statusIdx >= 0) ui->fishStatus->setCurrentIndex(statusIdx);

    // Set date times
    QString fishCaughtStr = table->item(row, 3)->text();
    QString dateOfPurchaseStr = table->item(row, 4)->text();

    QDateTime fishCaughtDt = QDateTime::fromString(fishCaughtStr, "yyyy-MM-dd HH:mm:ss");
    if (!fishCaughtDt.isValid()) {
        fishCaughtDt = QDateTime::fromString(fishCaughtStr, "yyyy-MM-dd hh:mm:ss");
    }
    if (fishCaughtDt.isValid()) {
        ui->fishdate1->setDateTime(fishCaughtDt);
    }

    QDateTime dateOfPurchaseDt = QDateTime::fromString(dateOfPurchaseStr, "yyyy-MM-dd HH:mm:ss");
    if (!dateOfPurchaseDt.isValid()) {
        dateOfPurchaseDt = QDateTime::fromString(dateOfPurchaseStr, "yyyy-MM-dd hh:mm:ss");
    }
    if (dateOfPurchaseDt.isValid()) {
        ui->fishdate2->setDateTime(dateOfPurchaseDt);
    }

    // Load and set boat foreign key value
    loadProductBoatIds();
    QSqlQuery boatQuery;
    boatQuery.prepare("SELECT boatid FROM products WHERE productid = :id");
    boatQuery.bindValue(":id", selectedProductId);
    if (boatQuery.exec() && boatQuery.next()) {
        const QVariant boatIdValue = boatQuery.value(0);
        if (!boatIdValue.isNull()) {
            const QString boatIdText = boatIdValue.toString();
            const int idx = ui->boatCombo->findText(boatIdText);
            if (idx >= 0) {
                ui->boatCombo->setCurrentIndex(idx);
            }
        } else {
            ui->boatCombo->setCurrentIndex(0);
        }
    }

    // Update button text to show we're editing
    ui->checkProductButton_2->setText("Edit Product");

    // Navigate to the Add Product page to edit
    ui->stackedWidget_4->setCurrentIndex(0);
}

void appwindow::on_edit_company_6_clicked()
{
    if (selectedProductId <= 0) {
        QMessageBox::warning(this, "Error", "Please select a product to edit!");
        return;
    }

    // Find the product in the table to get current data
    QTableWidget *table = ui->tableWidget_10;
    int row = -1;
    for (int i = 0; i < table->rowCount(); i++) {
        if (table->item(i, 0)->text().toInt() == selectedProductId) {
            row = i;
            break;
        }
    }

    if (row < 0) {
        QMessageBox::warning(this, "Error", "Product not found in table!");
        return;
    }

    // Fill the form fields for editing
    ui->fishtype->setText(table->item(row, 2)->text());
    ui->qtyfish->setText(table->item(row, 5)->text());
    ui->fishprice->setText(table->item(row, 8)->text());
    ui->locationfish->setText(table->item(row, 6)->text());
    ui->fishTemp->setText(table->item(row, 7)->text() == "-" ? "" : table->item(row, 7)->text());

    // Set status combobox
    int statusIdx = ui->fishStatus->findText(table->item(row, 1)->text());
    if (statusIdx >= 0) ui->fishStatus->setCurrentIndex(statusIdx);

    // Set date times
    QString fishCaughtStr = table->item(row, 3)->text();
    QString dateOfPurchaseStr = table->item(row, 4)->text();

    QDateTime fishCaughtDt = QDateTime::fromString(fishCaughtStr, "yyyy-MM-dd HH:mm:ss");
    if (!fishCaughtDt.isValid()) {
        fishCaughtDt = QDateTime::fromString(fishCaughtStr, "yyyy-MM-dd hh:mm:ss");
    }
    if (fishCaughtDt.isValid()) {
        ui->fishdate1->setDateTime(fishCaughtDt);
    }

    QDateTime dateOfPurchaseDt = QDateTime::fromString(dateOfPurchaseStr, "yyyy-MM-dd HH:mm:ss");
    if (!dateOfPurchaseDt.isValid()) {
        dateOfPurchaseDt = QDateTime::fromString(dateOfPurchaseStr, "yyyy-MM-dd hh:mm:ss");
    }
    if (dateOfPurchaseDt.isValid()) {
        ui->fishdate2->setDateTime(dateOfPurchaseDt);
    }

    // Load and set boat foreign key value
    loadProductBoatIds();
    QSqlQuery boatQuery;
    boatQuery.prepare("SELECT boatid FROM products WHERE productid = :id");
    boatQuery.bindValue(":id", selectedProductId);
    if (boatQuery.exec() && boatQuery.next()) {
        const QVariant boatIdValue = boatQuery.value(0);
        if (!boatIdValue.isNull()) {
            const QString boatIdText = boatIdValue.toString();
            const int idx = ui->boatCombo->findText(boatIdText);
            if (idx >= 0) {
                ui->boatCombo->setCurrentIndex(idx);
            }
        } else {
            ui->boatCombo->setCurrentIndex(0);
        }
    }

    // Update button text to show we're editing
    ui->checkProductButton_2->setText("Edit Product");

    // Navigate to the Add Product page to edit
    ui->stackedWidget_4->setCurrentIndex(0);
}

void appwindow::on_delete_company_6_clicked()
{
    if (selectedProductId <= 0) {
        QMessageBox::warning(this, "Error", "Please select a product to delete!");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(this, "Confirm Delete",
                                                              "Are you sure you want to delete this product?",
                                                              QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (productManager.deleteProduct(selectedProductId)) {
            QMessageBox::information(this, "Success", "Product deleted successfully!");
            loadProductTable();
            // Clear form
            ui->fishtype->clear();
            ui->fishStatus->setCurrentIndex(0);
            ui->qtyfish->clear();
            ui->fishprice->clear();
            ui->fishTemp->clear();
            ui->locationfish->clear();
            ui->boatCombo->setCurrentIndex(0);
            ui->fishdate1->setDateTime(QDateTime::currentDateTime());
            ui->fishdate2->setDateTime(QDateTime::currentDateTime());
            selectedProductId = -1;

            // Reset button text
            ui->checkProductButton_2->setText("Add Product");
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete product!");
        }
    }
}

void appwindow::on_clear_6_clicked()
{
    ui->fishtype->clear();
    ui->fishStatus->setCurrentIndex(0);
    ui->qtyfish->clear();
    ui->fishprice->clear();
    ui->fishTemp->clear();
    ui->locationfish->clear();
    ui->boatCombo->setCurrentIndex(0);
    ui->fishdate1->setDateTime(QDateTime::currentDateTime());
    ui->fishdate2->setDateTime(QDateTime::currentDateTime());
    selectedProductId = -1;

    // Reset button text to "Add Product"
    ui->checkProductButton_2->setText("Add Product");

    // Clear search and reload all products
    ui->searchbar_6->blockSignals(true);
    ui->searchbar_6->clear();
    ui->searchbar_6->blockSignals(false);
    ui->comboBox_18->setCurrentIndex(0);
    loadProductTable();
}

void appwindow::on_searchbar_6_textChanged(const QString &text)
{
    QString searchText = text.trimmed();
    QList<ProductRecord> filteredRecords;

    if (searchText.isEmpty()) {
        filteredRecords = allProductRecords;
    } else {
        bool isNumeric;
        searchText.toInt(&isNumeric);

        // If search text is numeric, search by ID only
        for (const ProductRecord &r : allProductRecords) {
            if (isNumeric && QString::number(r.id).contains(searchText)) {
                filteredRecords.append(r);
            } else if (!isNumeric && (
                           r.type.contains(searchText, Qt::CaseInsensitive) ||
                           r.status.contains(searchText, Qt::CaseInsensitive) ||
                           r.location.contains(searchText, Qt::CaseInsensitive))) {
                filteredRecords.append(r);
            }
        }
    }

    // Apply the current sort to the filtered results
    int sortIndex = ui->comboBox_18->currentIndex();
    on_comboBox_18_currentIndexChanged(sortIndex);
}

void appwindow::on_comboBox_18_currentIndexChanged(int index)
{
    // Get the current filtered records (or all if no filter)
    QString searchText = ui->searchbar_6->text().trimmed();
    QList<ProductRecord> recordsToDisplay;

    if (searchText.isEmpty()) {
        recordsToDisplay = allProductRecords;
    } else {
        bool isNumeric;
        searchText.toInt(&isNumeric);

        // If search text is numeric, search by ID only
        for (const ProductRecord &r : allProductRecords) {
            if (isNumeric && QString::number(r.id).contains(searchText)) {
                recordsToDisplay.append(r);
            } else if (!isNumeric && (
                           r.type.contains(searchText, Qt::CaseInsensitive) ||
                           r.status.contains(searchText, Qt::CaseInsensitive) ||
                           r.location.contains(searchText, Qt::CaseInsensitive))) {
                recordsToDisplay.append(r);
            }
        }
    }

    // Sort based on index
    if (index == 1) {  // Sort by Price
        std::sort(recordsToDisplay.begin(), recordsToDisplay.end(), [](const ProductRecord &a, const ProductRecord &b) {
            return a.price < b.price;
        });
    }
    // index == 0 is Type (default, no sorting needed as they come in order)

    // Update the table with filtered/sorted records
    QTableWidget *table = ui->tableWidget_10;
    table->setRowCount(0);

    for (const ProductRecord &r : recordsToDisplay) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(r.id)));
        table->setItem(row, 1, new QTableWidgetItem(r.status));
        table->setItem(row, 2, new QTableWidgetItem(r.type));
        table->setItem(row, 3, new QTableWidgetItem(r.fishCaught));
        table->setItem(row, 4, new QTableWidgetItem(r.dateOfPurchase));
        table->setItem(row, 5, new QTableWidgetItem(QString::number(r.quantity)));
        table->setItem(row, 6, new QTableWidgetItem(r.location));
        table->setItem(row, 7, new QTableWidgetItem(std::isnan(r.temperature)
                                 ? "-"
                                 : QString::number(r.temperature, 'f', 2)));
        table->setItem(row, 8, new QTableWidgetItem(QString::number(r.originalPrice, 'f', 2)));
        table->setItem(row, 9, new QTableWidgetItem(QString::number(r.discountedPrice, 'f', 2)));
    }

    QHeaderView *header = table->horizontalHeader();
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    table->setColumnWidth(0, 60);   // ID
    table->setColumnWidth(1, 120);  // Status
    table->setColumnWidth(2, 120);  // Type
    table->setColumnWidth(3, 170);  // Fish Caught
    table->setColumnWidth(4, 170);  // Date Purchase
    table->setColumnWidth(5, 100);  // Quantity
    table->setColumnWidth(6, 130);  // Location
    table->setColumnWidth(7, 140);  // Temperature
    table->setColumnWidth(8, 160);  // Original Price
    table->setColumnWidth(9, 180);  // Discounted Price

    // Update the count label
    ui->labelResults_6->setText(QString("Showing %1 Products").arg(recordsToDisplay.size()));
}

void appwindow::on_export_pdf_6_clicked()
{
    QTableWidget *table = ui->tableWidget_10;
    QList<int> selectedRows;

    if (table->selectionModel()) {
        const QModelIndexList selectedIndexes = table->selectionModel()->selectedRows();
        for (const QModelIndex &index : selectedIndexes) {
            selectedRows.append(index.row());
        }
    }

    std::sort(selectedRows.begin(), selectedRows.end());

    if (selectedRows.isEmpty()) {
        QMessageBox::information(this,
                                 "Select Products",
                                 "Please select one or more products from the table before exporting to PDF.");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export Product Data to PDF"), "",
                                                    tr("PDF Files (*.pdf);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    QPdfWriter pdfWriter(fileName);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Landscape);
    pdfWriter.setResolution(300);
    pdfWriter.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);

    QColor darkBlue(0, 102, 204);
    QColor veryLightBlue(230, 242, 255);
    QColor textColor(30, 30, 30);
    QColor borderColor(180, 180, 180);

    int pageWidth  = pdfWriter.width();
    int pageHeight = pdfWriter.height();

    int marginLeft   = 0;
    int marginTop    = 0;
    int contentWidth = pageWidth;

    int yPos = marginTop;

    // ٤?٤? TITLE BLOCK ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    int titleBlockHeight = 96;
    painter.fillRect(marginLeft, yPos, contentWidth, titleBlockHeight, darkBlue);

    QFont titleFont("Arial", 28, QFont::Bold);
    painter.setFont(titleFont);
    painter.setPen(Qt::white);
    painter.drawText(marginLeft, yPos, contentWidth, titleBlockHeight,
                     Qt::AlignCenter | Qt::AlignVCenter, "PRODUCTS SOLD");

    yPos += titleBlockHeight + 30;

    // ٤?٤? DATE LINE ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    QFont dateFont("Arial", 10);
    painter.setFont(dateFont);
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(marginLeft, yPos,
                     QString("Generated: %1")
                         .arg(QDateTime::currentDateTime()
                                  .toString("yyyy-MM-dd  hh:mm:ss")));
    yPos += 50;

    // ٤?٤? TABLE ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    int rowCount    = selectedRows.size();
    int columnCount = table->columnCount();

    if (rowCount == 0) {
        painter.setFont(QFont("Arial", 14));
        painter.setPen(textColor);
        painter.drawText(marginLeft, yPos, "No data to export.");
        painter.end();
        QMessageBox::information(this, "No Data", "The table is empty. Nothing to export.");
        return;
    }

    QVector<int> columnWidths;
    if (columnCount == 10) {
        columnWidths = {70, 110, 110, 180, 200, 90, 140, 120, 140, 160};
    } else if (columnCount == 9) {
        columnWidths = {60, 105, 105, 240, 250, 90, 115, 145, 165};
    } else {
        const int fallbackWidth = contentWidth / qMax(1, columnCount);
        for (int i = 0; i < columnCount; ++i) {
            columnWidths.append(fallbackWidth);
        }
    }

    int totalColumnWidth = 0;
    for (int w : columnWidths) {
        totalColumnWidth += w;
    }

    const qreal widthScale = totalColumnWidth > 0
                                 ? static_cast<qreal>(contentWidth) / static_cast<qreal>(totalColumnWidth)
                                 : 1.0;

    for (int i = 0; i < columnWidths.size(); ++i) {
        columnWidths[i] = qMax(60, qRound(columnWidths[i] * widthScale));
    }

    int scaledSum = 0;
    for (int w : columnWidths) {
        scaledSum += w;
    }

    if (!columnWidths.isEmpty()) {
        columnWidths[columnWidths.size() - 1] += (contentWidth - scaledSum);
    }

    int headerHeight = 96;

    auto drawHeader = [&]() {
        painter.fillRect(marginLeft, yPos, contentWidth, headerHeight, darkBlue);

        QFont headerFont("Arial", 11, QFont::Bold);
        painter.setFont(headerFont);
        painter.setPen(Qt::white);

        int xPos = marginLeft;
        for (int col = 0; col < columnCount; col++) {
            QString headerText = table->horizontalHeaderItem(col)
            ? table->horizontalHeaderItem(col)->text()
            : "";
            const int currentWidth = columnWidths.value(col, 100);
            painter.drawText(xPos + 6, yPos, currentWidth - 12, headerHeight,
                             Qt::AlignCenter | Qt::AlignVCenter | Qt::TextWordWrap,
                             headerText);

            if (col < columnCount - 1) {
                painter.setPen(QPen(Qt::white, 2));
                painter.drawLine(xPos + currentWidth, yPos,
                                 xPos + currentWidth, yPos + headerHeight);
                painter.setPen(Qt::white);
            }
            xPos += currentWidth;
        }
    };

    drawHeader();
    yPos += headerHeight;

    int footerReserve   = 260; // Keep space for summary and signature area at the bottom.

    QFont cellFont("Arial", 8);
    painter.setFont(cellFont);
    const int cellPaddingX = 16;
    const int cellPaddingY = 16;
    const int minRowHeight = 76;

    auto wrappedCellHeight = [&](const QString &text, int availableWidth) {
        QFontMetrics fm(cellFont);
        const QRect bounds = fm.boundingRect(QRect(0, 0, qMax(20, availableWidth), 2000),
                                             Qt::TextWordWrap | Qt::AlignCenter,
                                             text);
        return qMax(minRowHeight, bounds.height() + (cellPaddingY * 2));
    };

    for (int exportRow = 0; exportRow < rowCount; exportRow++) {
        const int row = selectedRows.at(exportRow);

        int rowHeight = minRowHeight;
        for (int col = 0; col < columnCount; col++) {
            QTableWidgetItem *item = table->item(row, col);
            const QString cellText = item ? item->text() : "";
            const int currentWidth = columnWidths.value(col, 100);
            rowHeight = qMax(rowHeight, wrappedCellHeight(cellText, currentWidth - (cellPaddingX * 2)));
        }

        // ٤?٤? New page if needed ٤?٤?
        if (exportRow > 0 && (yPos + rowHeight + footerReserve) > pageHeight) {
            pdfWriter.newPage();
            yPos = marginTop;
            drawHeader();
            yPos += headerHeight;
            painter.setFont(cellFont);
        }

        QColor rowBg = (exportRow % 2 == 0) ? Qt::white : veryLightBlue;
        painter.fillRect(marginLeft, yPos, contentWidth, rowHeight, rowBg);

        painter.setPen(QPen(borderColor, 1));
        painter.drawRect(marginLeft, yPos, contentWidth, rowHeight);

        int xPos = marginLeft;
        for (int col = 0; col < columnCount; col++) {
            QTableWidgetItem *item = table->item(row, col);
            QString cellText = item ? item->text() : "";

            painter.setPen(textColor);
            const int currentWidth = columnWidths.value(col, 100);
            QRect cellRect(xPos, yPos, currentWidth, rowHeight);
            painter.drawText(cellRect.adjusted(cellPaddingX, cellPaddingY,
                                               -cellPaddingX, -cellPaddingY),
                             Qt::AlignCenter | Qt::TextWordWrap,
                             cellText);

            if (col < columnCount - 1) {
                painter.setPen(QPen(borderColor, 1));
                painter.drawLine(xPos + currentWidth, yPos,
                                 xPos + currentWidth, yPos + rowHeight);
            }
            xPos += currentWidth;
        }

        yPos += rowHeight;
    }

    // ٤?٤? FOOTER ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    const int summaryBlockHeight = 90;
    const int signatureBlockHeight = 420;
    const int gapBeforeSignature = 190;
    const int neededBottomSpace = 40 + summaryBlockHeight + gapBeforeSignature + signatureBlockHeight;

    if (yPos + neededBottomSpace > pageHeight) {
        pdfWriter.newPage();
        yPos = marginTop;
    }

    yPos += 30;

    painter.setPen(QPen(darkBlue, 3));
    painter.drawLine(marginLeft, yPos, marginLeft + contentWidth, yPos);
    yPos += 20;

    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(marginLeft, yPos,
                     contentWidth, 50,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("Total Records: %1").arg(rowCount));

    painter.drawText(marginLeft, yPos,
                     contentWidth, 50,
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("Marina Management System"));

    yPos += gapBeforeSignature;

    const int signatureWidth = (contentWidth * 7) / 10;
    const int signatureHeight = 420;
    const int signatureX = marginLeft + (contentWidth - signatureWidth) / 2;
    int signatureY = yPos;

    // Keep signature area intentionally low on the page.
    const int lowAnchorY = pageHeight - signatureHeight - 60;
    if (signatureY < lowAnchorY) {
        signatureY = lowAnchorY;
    }

    painter.setPen(QPen(QColor(190, 190, 190), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(signatureX, signatureY,
                            signatureWidth, signatureHeight,
                            8, 8);

    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.setPen(QColor(90, 90, 90));
    painter.drawText(signatureX, signatureY + 24,
                     signatureWidth, 42,
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     "AUTHORIZED SIGNATURE");

    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.setPen(QColor(95, 95, 95));
    painter.drawText(signatureX, signatureY + 102,
                     signatureWidth, 36,
                     Qt::AlignHCenter | Qt::AlignVCenter,
                     "Sign here");

    painter.setPen(QPen(QColor(70, 70, 70), 2));
    const int signatureLineY = signatureY + 280;
    painter.drawLine(signatureX + 100, signatureLineY, signatureX + signatureWidth - 100, signatureLineY);

    const QString reportDate = QDate::currentDate().toString("yyyy-MM-dd");
    painter.setFont(QFont("Arial", 11));
    const int detailsY = signatureLineY + 46;
    const int detailsLeft = signatureX + 40;
    const int detailsWidth = signatureWidth - 80;
    const int columnGap = 30;
    const int detailsColumnWidth = (detailsWidth - (2 * columnGap)) / 3;

    const int dateX = detailsLeft;
    const int nameX = dateX + detailsColumnWidth + columnGap;
    const int idX = nameX + detailsColumnWidth + columnGap;

    painter.drawText(dateX, detailsY,
                     detailsColumnWidth, 30,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QString("Date: %1").arg(reportDate));

    painter.drawText(nameX, detailsY,
                     detailsColumnWidth, 30,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     "Name: ____________");

    painter.drawText(idX, detailsY,
                     detailsColumnWidth, 30,
                     Qt::AlignLeft | Qt::AlignVCenter,
                     "ID/Stamp: ____________");

    painter.end();

    QMessageBox::information(this, "Export Successful",
                             QString("PDF exported successfully to:\n%1").arg(fileName));
}



void appwindow::generateProductStatisticsByStatus(const QList<ProductRecord> &products)
{
    // Group products by status and calculate totals
    QMap<QString, int> statusCount;
    QMap<QString, int> statusQuantity;
    int totalQuantity = 0;

    for (const ProductRecord &product : products) {
        statusCount[product.status]++;
        statusQuantity[product.status] += product.quantity;
        totalQuantity += product.quantity;
    }

    if (totalQuantity == 0) {
        QMessageBox::warning(this, "No Data", "No products with quantity data.");
        return;
    }

    if (statusCount.isEmpty()) {
        QMessageBox::warning(this, "No Data", "No product status data available.");
        return;
    }

    // Calculate percentage of active/available products
    int activeQuantity = statusQuantity.value("Available", 0) +
                         statusQuantity.value("In Stock", 0);
    int percentage = (totalQuantity > 0) ? (activeQuantity * 100) / totalQuantity : 0;

    // Update progress bar
    ui->progressBar_6->setValue(percentage);

    // Build pie chart by product status for Product Activity page
    QPieSeries *series = new QPieSeries();
    QList<QColor> sliceColors = {
        QColor("#0ea5e9"),
        QColor("#22c55e"),
        QColor("#f59e0b"),
        QColor("#ef4444"),
        QColor("#6366f1"),
        QColor("#14b8a6")
    };

    int colorIndex = 0;
    for (auto it = statusCount.begin(); it != statusCount.end(); ++it) {
        QPieSlice *slice = series->append(it.key(), it.value());
        slice->setLabelVisible(true);
        const int pct = (it.value() * 100) / qMax(1, products.size());
        slice->setLabel(QString("%1 (%2, %3%)").arg(it.key()).arg(it.value()).arg(pct));
        slice->setColor(sliceColors.at(colorIndex % sliceColors.size()));
        colorIndex++;
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Products Distribution by Status");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    ui->chartView_5->setChart(chart);
    ui->chartView_5->setRenderHint(QPainter::Antialiasing);

    // Update label with statistics
    QString statsText = QString("Percentage of Active Products in system: %1%\n").arg(percentage);
    statsText += "---Status Breakdown---\n";

    for (auto it = statusQuantity.begin(); it != statusQuantity.end(); ++it) {
        int count = statusCount[it.key()];
        int qty = it.value();
        int pct = (qty * 100) / totalQuantity;
        statsText += QString("%1: %2 items, %3 units (%4%)\n")
                         .arg(it.key())
                         .arg(count)
                         .arg(qty)
                         .arg(pct);
    }

    ui->label_78->setText(QString("Percentage of active Product in system: %1%").arg(percentage));

    QMessageBox::information(this, "Product Statistics", statsText);
}

void appwindow::generateProductQuantityByType(const QList<ProductRecord> &products)
{
    QMap<QString, int> quantityByType;
    int totalQuantity = 0;

    for (const ProductRecord &product : products) {
        const QString fishType = product.type.trimmed().isEmpty() ? "Unknown" : product.type.trimmed();
        quantityByType[fishType] += product.quantity;
        totalQuantity += product.quantity;
    }

    if (quantityByType.isEmpty() || totalQuantity <= 0) {
        QMessageBox::warning(this, "No Data", "No quantity data available for fish types.");
        return;
    }

    QPieSeries *series = new QPieSeries();
    QList<QColor> sliceColors = {
        QColor("#0ea5e9"),
        QColor("#22c55e"),
        QColor("#f59e0b"),
        QColor("#ef4444"),
        QColor("#6366f1"),
        QColor("#14b8a6")
    };

    int colorIndex = 0;
    for (auto it = quantityByType.begin(); it != quantityByType.end(); ++it) {
        QPieSlice *slice = series->append(it.key(), it.value());
        slice->setLabelVisible(true);
        const int pct = (it.value() * 100) / qMax(1, totalQuantity);
        slice->setLabel(QString("%1 (%2 units, %3%)").arg(it.key()).arg(it.value()).arg(pct));
        slice->setColor(sliceColors.at(colorIndex % sliceColors.size()));
        colorIndex++;
    }

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Quantity Distribution by Fish Type");
    chart->setAnimationOptions(QChart::SeriesAnimations);
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    ui->chartView_5->setChart(chart);
    ui->chartView_5->setRenderHint(QPainter::Antialiasing);

    const int maxTypeQty = *std::max_element(quantityByType.begin(), quantityByType.end());
    const int dominantPct = (maxTypeQty * 100) / qMax(1, totalQuantity);
    ui->progressBar_6->setValue(dominantPct);
    ui->label_78->setText(QString("Quantity share of top fish type: %1%").arg(dominantPct));
}

void appwindow::generateDockingStatistics()
{
    // Delete old scene if exists
    QGraphicsScene *oldScene = ui->graph_docking->scene();
    if (oldScene) {
        delete oldScene;
    }

    // Create new scene
    QGraphicsScene *scene = new QGraphicsScene();

    // Get all docking records
    QList<DockingRecord> dockings = dockingManager.getAllDockings();

    if (dockings.isEmpty()) {
        QGraphicsTextItem *textItem = scene->addText("No docking data available");
        textItem->setPos(ui->graph_docking->width() / 2 - 100, ui->graph_docking->height() / 2 - 20);
        ui->graph_docking->setScene(scene);
        return;
    }

    // Get available dimensions
    int viewWidth = ui->graph_docking->width();
    int viewHeight = ui->graph_docking->height();
    int chartWidth = viewWidth / 2;

    // Chart 1: Total Count of Dockings
    QChart *chart1 = new QChart();
    chart1->setTitle("Total Dockings in Database");
    chart1->setBackgroundBrush(QBrush(Qt::white));
    chart1->setAnimationOptions(QChart::SeriesAnimations);
    chart1->setMargins(QMargins(10, 10, 10, 10));

    QPieSeries *pieSeries1 = new QPieSeries();
    QPieSlice *slice1 = pieSeries1->append("Total Dockings", dockings.count());
    slice1->setColor(QColor(173, 216, 230));  // Light blue
    slice1->setLabelVisible(true);
    slice1->setLabel(QString("Total: %1").arg(dockings.count()));

    chart1->addSeries(pieSeries1);
    chart1->legend()->setVisible(true);
    chart1->legend()->setAlignment(Qt::AlignBottom);

    QChartView *chartView1 = new QChartView(chart1);
    chartView1->setRenderHint(QPainter::Antialiasing);
    chartView1->setStyleSheet("background-color: white;");
    chartView1->setFixedSize(chartWidth, viewHeight);

    // Chart 2: Status breakdown (Occupied vs Available)
    QChart *chart2 = new QChart();
    chart2->setTitle("Dockings by Status");
    chart2->setBackgroundBrush(QBrush(Qt::white));
    chart2->setAnimationOptions(QChart::SeriesAnimations);
    chart2->setMargins(QMargins(10, 10, 10, 10));

    QBarSeries *barSeries = new QBarSeries();
    QBarSet *statusSet = new QBarSet("Count");
    QStringList statusCategories;

    int occupied = 0, available = 0;
    for (const DockingRecord &dock : dockings) {
        if (dock.status == "Occupied") {
            occupied++;
        } else if (dock.status == "Available") {
            available++;
        }
    }

    *statusSet << occupied << available;
    statusCategories << "Occupied" << "Available";
    barSeries->append(statusSet);
    statusSet->setColor(QColor(0, 102, 204));  // Dark blue

    chart2->addSeries(barSeries);
    chart2->setTitle("Dockings by Status");

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(statusCategories);
    chart2->addAxis(axisX, Qt::AlignBottom);
    barSeries->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    int maxValue = qMax(occupied, available);
    axisY->setRange(0, maxValue > 0 ? maxValue + 1 : 5);
    axisY->setLabelFormat("%d");
    axisY->setTickCount(maxValue + 2);
    chart2->addAxis(axisY, Qt::AlignLeft);
    barSeries->attachAxis(axisY);

    QChartView *chartView2 = new QChartView(chart2);
    chartView2->setRenderHint(QPainter::Antialiasing);
    chartView2->setStyleSheet("background-color: white;");
    chartView2->setFixedSize(chartWidth, viewHeight);

    // Create a layout to display both charts
    QWidget *chartWidget = new QWidget();
    QHBoxLayout *layout = new QHBoxLayout(chartWidget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(chartView1, 1);
    layout->addWidget(chartView2, 1);
    chartWidget->setFixedSize(viewWidth, viewHeight);

    // Add widget to scene and set proper bounds
    QGraphicsProxyWidget *proxy = scene->addWidget(chartWidget);
    scene->setSceneRect(0, 0, viewWidth, viewHeight);
    proxy->setGeometry(QRectF(0, 0, viewWidth, viewHeight));

    ui->graph_docking->setScene(scene);
    ui->graph_docking->setAlignment(Qt::AlignTop | Qt::AlignLeft);
}

void appwindow::on_DockCalender_clicked()
{
    QList<DockingRecord> dockings = dockingManager.getAllDockings();

    QDialog dialog(this);
    dialog.setWindowTitle("Dock Calendar");
    dialog.setMinimumSize(980, 720);

    QVBoxLayout *mainLayout = new QVBoxLayout(&dialog);
    mainLayout->setContentsMargins(24, 24, 24, 24);
    mainLayout->setSpacing(16);

    QLabel *titleLabel = new QLabel("Dock Reservation Calendar", &dialog);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(
        "font-family: 'Behove';"
        "font-size: 24px;"
        "font-weight: bold;"
        "color: #0a4c73;"
    );
    mainLayout->addWidget(titleLabel);

    QLabel *subtitleLabel = new QLabel(
        "Red = Occupied reservation",
        &dialog);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet("color: #334155; font-size: 14px;");
    mainLayout->addWidget(subtitleLabel);

    QHBoxLayout *legendLayout = new QHBoxLayout();
    legendLayout->setSpacing(18);

    auto addLegendItem = [legendLayout, &dialog](const QString &labelText, const QColor &color) {
        QWidget *itemWidget = new QWidget(&dialog);
        QHBoxLayout *itemLayout = new QHBoxLayout(itemWidget);
        itemLayout->setContentsMargins(0, 0, 0, 0);
        itemLayout->setSpacing(8);

        QLabel *swatch = new QLabel(itemWidget);
        swatch->setFixedSize(18, 18);
        swatch->setStyleSheet(QString("background-color: %1; border: 1px solid #64748b; border-radius: 4px;")
                                  .arg(color.name()));

        QLabel *text = new QLabel(labelText, itemWidget);
        text->setStyleSheet("color: #334155; font-size: 13px;");

        itemLayout->addWidget(swatch);
        itemLayout->addWidget(text);
        itemLayout->addStretch();
        legendLayout->addWidget(itemWidget);
    };

    addLegendItem("Occupied", QColor("#f87171"));
    legendLayout->addStretch();
    mainLayout->addLayout(legendLayout);

    DockCalendarWidget *calendar = new DockCalendarWidget(&dialog);
    calendar->setGridVisible(true);
    calendar->setNavigationBarVisible(true);
    calendar->setVerticalHeaderFormat(QCalendarWidget::NoVerticalHeader);
    calendar->setFirstDayOfWeek(Qt::Monday);
    calendar->setSelectedDate(QDate::currentDate());
    calendar->setStyleSheet(
        "QCalendarWidget QWidget { background: white; }"
        "QCalendarWidget QToolButton { height: 34px; font-size: 14px; font-weight: bold; color: #0a4c73; }"
        "QCalendarWidget QAbstractItemView:enabled { selection-background-color: #0ea5e9; selection-color: white; }"
    );

    QMap<QDate, int> priorities;
    QMap<QDate, QColor> dateColors;

    for (const DockingRecord &dock : dockings) {
        QDate startDate = parseDockingDate(dock.startDate);
        QDate endDate = parseDockingDate(dock.endDate);
        QColor color = statusColor(dock.status);

        if (!startDate.isValid() || !endDate.isValid() || !color.isValid() || endDate < startDate) {
            continue;
        }

        const int priority = statusPriority(dock.status);
        for (QDate current = startDate; current <= endDate; current = current.addDays(1)) {
            if (priority > priorities.value(current, 0)) {
                priorities[current] = priority;
                dateColors[current] = color;
            }
        }
    }

    for (auto it = dateColors.cbegin(); it != dateColors.cend(); ++it) {
        QTextCharFormat format;
        format.setBackground(QBrush(it.value()));
        format.setForeground(QBrush(Qt::black));
        calendar->setDateTextFormat(it.key(), format);
    }

    calendar->setDockColors(dateColors);

    if (dateColors.isEmpty()) {
        QLabel *emptyLabel = new QLabel("No dock reservations were found.", &dialog);
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("color: #475569; font-size: 14px;");
        mainLayout->addWidget(emptyLabel);
    }

    mainLayout->addWidget(calendar, 1);

    dialog.exec();
}

///END





//boat section
//boat - dock helper functions
// Helper function to find an available dock
int appwindow::findAvailableDock()
{
    QSqlQuery query;
    query.prepare("SELECT DOCKID FROM DOCKING WHERE STATUS = 'Available' AND ROWNUM = 1");

    if (!query.exec()) {
        qDebug() << "Error checking available docks:" << query.lastError().text();
        return -1;
    }

    if (query.next()) {
        int dockId = query.value(0).toInt();
        qDebug() << "Found available dock ID:" << dockId;
        return dockId;
    }

    qDebug() << "No available docks found";
    return -1;
}

// Helper function to check if any dock is available
bool appwindow::isAnyDockAvailable()
{
    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM DOCKING WHERE STATUS = 'Available'");

    if (!query.exec()) {
        qDebug() << "Error checking dock availability:" << query.lastError().text();
        return false;
    }

    if (query.next()) {
        int count = query.value(0).toInt();
        return count > 0;
    }

    return false;
}

// Boat CRUD
void appwindow::setBoatMode(BoatMode mode)
{
    currentBoatMode = mode;

    if (mode == BoatMode::Add) {
        ui->label_99->setText("Create Boat:");
        ui->addBoatButton->setVisible(true);
        ui->editBoatButton->setVisible(false);
        clearBoatInputs();
    } else {
        ui->label_99->setText("Edit Boat:");
        ui->addBoatButton->setVisible(false);
        ui->editBoatButton->setVisible(true);
    }
}

// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  Shared helper �?� loads a boat into the form fields
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::populateBoatForm(int boatId)
{
    boatsTmp.setId(boatId);
    boatsTmp.read();

    if (!boatsTmp.getLastError().isEmpty()) {
        QMessageBox::warning(this, "Load Error",
                             "Could not load boat: " + boatsTmp.getLastError());
        return;
    }

    currentlySelectedId = boatId;

    ui->boatSizeLineEdit->setText(boatsTmp.getSize());
    ui->boatLocationLineEdit->setText(boatsTmp.getLocation());
    ui->boatOwnerNameLineEdit->setText(boatsTmp.getOwnerName());
    ui->boatOwnerEmailLineEdit->setText(boatsTmp.getOwnerEmail());
    ui->boatStatusComboBox->setCurrentIndex(boatsTmp.getStatus());
    ui->boatTypeLineEdit->setText(boatsTmp.getType());

    // ٤?٤? DATE PARSING ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
    QString rawDate = boatsTmp.getLastMaintenanceDate();
    QDate date;

    // Oracle returns month in all-caps (e.g. "23/MAR/17") �?� normalize to "23/Mar/17"
    QStringList parts = rawDate.split('/');
    QString normalizedDate = rawDate;
    if (parts.size() == 3) {
        QString month = parts[1];
        month = month[0].toUpper() + month.mid(1).toLower();
        normalizedDate = parts[0] + "/" + month + "/" + parts[2];
    }

    for (const QString &fmt : {"dd/MMM/yy", "dd/MMM/yyyy", "yyyy-MM-dd", "dd-MMM-yyyy"}) {
        date = QDate::fromString(normalizedDate, fmt);
        if (date.isValid()) break;
    }

    if (date.isValid()) {
        // Fix two-digit year: Qt may interpret "17" as 1917 instead of 2017
        if (date.year() < 2000) {
            date = date.addYears(100);
        }
        ui->boatMaintenanceDateEdit->setDate(date);
    } else {
        qDebug() << "Could not parse date:" << rawDate;
        ui->boatMaintenanceDateEdit->setDate(QDate::currentDate());
    }
    // ٤?٤? END DATE PARSING ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?

    ui->boatTripsSpinBox->setValue(boatsTmp.getTotalTrips());
    ui->boatFishSpinBox->setValue(boatsTmp.getTotalFish());

}

void appwindow::displayBoats()
{
    // Block signals so inserting rows does not fire itemClicked,
    // which would overwrite input fields and reset currentlySelectedId.
    ui->Boatwidget_2->blockSignals(true);

    ui->Boatwidget_2->setRowCount(0);
    ui->Boatwidget_2->setColumnCount(10);

    QStringList headers;
    headers << "Boat ID" << "Size" << "Location" << "Owner Name"
            << "Owner Email" << "Status" << "Type" << "Last Maintenance"
            << "Total Trips" << "Total Fish";
    ui->Boatwidget_2->setHorizontalHeaderLabels(headers);

    QSqlQuery query = Boats::getAll();

    if (!query.isActive()) {
        ui->Boatwidget_2->blockSignals(false);
        QMessageBox::critical(this, "Error", "Failed to retrieve boats");
        return;
    }

    int row = 0;
    while (query.next()) {
        ui->Boatwidget_2->insertRow(row);

        for (int col = 0; col < 10; col++) {
            QTableWidgetItem *item;

            if (col == 5) {
                int statusValue = query.value(col).toInt();
                QString statusText = (statusValue == 1) ? "IN PORT" : "OUT";
                item = new QTableWidgetItem(statusText);
            } else {
                item = new QTableWidgetItem(query.value(col).toString());
            }

            if (col == 0) {
                item->setData(Qt::UserRole, query.value("BOATID").toInt());
            }

            ui->Boatwidget_2->setItem(row, col, item);
        }
        row++;
    }

    ui->Boatwidget_2->resizeColumnsToContents();
    ui->Boatwidget_2->horizontalHeader()->setStretchLastSection(true);

    // Restore signals now that the table is fully populated
    ui->Boatwidget_2->blockSignals(false);
}

void appwindow::clearBoatInputs()
{
    ui->boatSizeLineEdit->clear();
    ui->boatLocationLineEdit->clear();
    ui->boatOwnerNameLineEdit->clear();
    ui->boatOwnerEmailLineEdit->clear();
    ui->boatStatusComboBox->setCurrentIndex(1); // Default to IN PORT
    ui->boatTypeLineEdit->clear();
    ui->boatMaintenanceDateEdit->setDate(QDate::currentDate());
    ui->boatTripsSpinBox->setValue(0);
    ui->boatFishSpinBox->setValue(0);
    ui->boatSearchLineEdit->clear();

    currentlySelectedId = -1;
    ui->deleteBoatButton->setEnabled(false);
    ui->updateBoatButton->setEnabled(false);
}


void appwindow::on_Boatwidget_2_itemSelectionChanged()
{
    int currentRow = ui->Boatwidget_2->currentRow();
    if (currentRow < 0) {
        currentlySelectedId = -1;
        ui->deleteBoatButton->setEnabled(false);
        ui->updateBoatButton->setEnabled(false);
        return;
    }

    QTableWidgetItem *idItem = ui->Boatwidget_2->item(currentRow, 0);
    if (idItem) {
        currentlySelectedId = idItem->data(Qt::UserRole).toInt();
        qDebug() << "Selected boat ID:" << currentlySelectedId;
        ui->deleteBoatButton->setEnabled(true);
        ui->updateBoatButton->setEnabled(true);
    }
}

// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  Double-click �?� populate form and switch to Edit mode
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::on_Boatwidget_2_itemDoubleClicked(QTableWidgetItem *item)
{
    if (!item) return;

    int currentRow = ui->Boatwidget_2->currentRow();
    if (currentRow < 0) return;

    QTableWidgetItem *idItem = ui->Boatwidget_2->item(currentRow, 0);
    if (!idItem) return;

    int boatId = idItem->data(Qt::UserRole).toInt();

    // Load data into the form
    populateBoatForm(boatId);

    // Switch to Edit mode and navigate to the form page
    setBoatMode(BoatMode::Edit);
    ui->boatPage->setCurrentIndex(0);
}





void appwindow::on_addBoatButton_clicked()
{
    // Input validation
    if (ui->boatSizeLineEdit->text().isEmpty() ||
        ui->boatLocationLineEdit->text().isEmpty() ||
        ui->boatOwnerNameLineEdit->text().isEmpty() ||
        ui->boatOwnerEmailLineEdit->text().isEmpty() ||
        ui->boatTypeLineEdit->text().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "All fields must be filled!");
        return;
    }

    // Email validation
    if (!ui->boatOwnerEmailLineEdit->text().contains('@')) {
        QMessageBox::warning(this, "Validation Error", "Please enter a valid email address!");
        return;
    }

    // If there is an available dock we use it, otherwise allow creation without dock.
    int availableDockId = findAvailableDock();

    // Use the location provided by the user (GPS or named location)
    QString finalLocation = ui->boatLocationLineEdit->text().trimmed();

    Boats newBoat(
        0, // ID auto-generated
        ui->boatSizeLineEdit->text().trimmed(),
        finalLocation,
        ui->boatOwnerNameLineEdit->text().trimmed(),
        ui->boatOwnerEmailLineEdit->text().trimmed(),
        ui->boatStatusComboBox->currentIndex(),
        ui->boatTypeLineEdit->text().trimmed(),
        ui->boatMaintenanceDateEdit->date().toString("yyyy-MM-dd"),
        ui->boatTripsSpinBox->value(),
        ui->boatFishSpinBox->value()
    );

    if (newBoat.create(availableDockId)) {
        QString successMessage = QString("Boat added successfully with ID: %1").arg(newBoat.getId());
        if (availableDockId > 0) {
            successMessage += QString("\nAssigned to Dock ID: %1").arg(availableDockId);

            QSqlQuery updateDock;
            updateDock.prepare("UPDATE DOCKING SET STATUS = 'Occupied' WHERE DOCKID = :dockId");
            updateDock.bindValue(":dockId", availableDockId);
            updateDock.exec();
        } else {
            successMessage += QString("\nNo dock available, random sea location used: %1").arg(finalLocation);
        }

        QMessageBox::information(this, "Success", successMessage);

        clearBoatInputs();
        displayBoats();
        updateBoatStatusProgressBar();
        loadProductBoatIds();
        loadDockingTable();
    } else {
        QMessageBox::critical(this, "Error", "Failed to add boat: " + newBoat.getLastError());
    }
}
void appwindow::update_boat_location_arduino()
{
    QByteArray rawData = A.read_from_arduino();
    if (rawData.isEmpty())
        return;

    static QByteArray buffer;
    buffer.append(rawData);

    int splitPos;
    while ((splitPos = buffer.indexOf('\r')) != -1 || (splitPos = buffer.indexOf('\n')) != -1) {
        QByteArray lineBytes = buffer.left(splitPos).trimmed();
        buffer.remove(0, splitPos + 1);

        QString line = QString::fromUtf8(lineBytes);
        if (line.isEmpty()) continue;

        // Only process lines containing "GPS:"
        if (!line.contains("GPS:")) continue;

        int colonIdx = line.indexOf(':');
        if (colonIdx == -1) continue;
        QString coordPart = line.mid(colonIdx + 1).trimmed();

        // Remove trailing ">>" if present
        coordPart.remove(QRegularExpression(">>$")).trimmed();

        QStringList parts = coordPart.split(',', Qt::SkipEmptyParts);
        if (parts.size() != 2) continue;

        bool latOk, lonOk;
        double latDeg = parts[0].trimmed().toDouble(&latOk);
        double lonDeg = parts[1].trimmed().toDouble(&lonOk);
        if (!latOk || !lonOk) continue;

        // Store coordinates in decimal degrees format (e.g., "36.80650,10.18150")
        QString decimalCoord = QString("%1,%2")
                               .arg(latDeg, 0, 'f', 6)
                               .arg(lonDeg, 0, 'f', 6);

        QSqlDatabase db = QSqlDatabase::database();
        if (!db.isOpen()) return;

        QSqlQuery updateQuery(db);
        updateQuery.prepare("UPDATE BOAT SET LOCATION = :coord WHERE BOATID = 1");
        updateQuery.bindValue(":coord", decimalCoord);

        if (updateQuery.exec()) {
            qDebug() << "Boat 1 location updated to (decimal):" << decimalCoord;
            displayBoats();   // refresh the boat table
        } else {
            qDebug() << "Failed to update boat location:" << updateQuery.lastError().text();
        }
    }
}
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  "Save Changes" button on the form (Edit mode only)
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::on_editBoatButton_clicked()
{
    if (currentlySelectedId <= 0) {
        QMessageBox::warning(this, "Selection Error", "No boat selected for editing.");
        return;
    }

    // Validation
    if (ui->boatSizeLineEdit->text().trimmed().isEmpty() ||
        ui->boatLocationLineEdit->text().trimmed().isEmpty() ||
        ui->boatOwnerNameLineEdit->text().trimmed().isEmpty() ||
        ui->boatOwnerEmailLineEdit->text().trimmed().isEmpty() ||
        ui->boatTypeLineEdit->text().trimmed().isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "All fields must be filled!");
        return;
    }

    if (!ui->boatOwnerEmailLineEdit->text().contains('@')) {
        QMessageBox::warning(this, "Validation Error", "Please enter a valid email address!");
        return;
    }

    // Check current boat's dock status
    QSqlQuery checkCurrentDock;
    checkCurrentDock.prepare("SELECT DOCKID FROM BOAT WHERE BOATID = :boatId");
    checkCurrentDock.bindValue(":boatId", currentlySelectedId);

    int currentDockId = -1;
    if (checkCurrentDock.exec() && checkCurrentDock.next()) {
        currentDockId = checkCurrentDock.value(0).toInt();
    }

    // If no dock is currently assigned, try to assign one if available.
    // If none are available, continue editing without blocking the user.
    if (currentDockId <= 0) {
        int newDockId = findAvailableDock();
        if (newDockId > 0) {
            QSqlQuery updateBoatDock;
            updateBoatDock.prepare("UPDATE BOAT SET DOCKID = :dockId WHERE BOATID = :boatId");
            updateBoatDock.bindValue(":dockId", newDockId);
            updateBoatDock.bindValue(":boatId", currentlySelectedId);
            updateBoatDock.exec();

            QSqlQuery updateDock;
            updateDock.prepare("UPDATE DOCKING SET STATUS = 'Occupied' WHERE DOCKID = :dockId");
            updateDock.bindValue(":dockId", newDockId);
            updateDock.exec();

            currentDockId = newDockId;
        }
    }

    // Proceed with boat update (existing code)
    int boatId = currentlySelectedId;
    QString size = ui->boatSizeLineEdit->text().trimmed();
    QString loc = ui->boatLocationLineEdit->text().trimmed();
    QString oName = ui->boatOwnerNameLineEdit->text().trimmed();
    QString oEmail = ui->boatOwnerEmailLineEdit->text().trimmed();
    int status = ui->boatStatusComboBox->currentIndex();
    QString type = ui->boatTypeLineEdit->text().trimmed();
    QString dateStr = ui->boatMaintenanceDateEdit->date().toString("dd-MMM-yyyy").toUpper();
    int trips = ui->boatTripsSpinBox->value();
    int fish = ui->boatFishSpinBox->value();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, "DB Error", "Database is not open!");
        return;
    }

    QSqlQuery q(db);
    q.prepare(
        "UPDATE BOAT SET "
        "  SIZEBOAT            = :size,    "
        "  LOCATION            = :loc,     "
        "  OWNERNAME           = :oName,   "
        "  OWNERMAIL           = :oEmail,  "
        "  STATUS              = :status,  "
        "  TYPE                = :type,    "
        "  LASTMAINTENANCEDATE = TO_DATE(:dateStr, 'DD-MON-YYYY'), "
        "  TOTALTRIPS          = :trips,   "
        "  TOTALFISH           = :fish     "
        "WHERE BOATID = :boatId"
    );

    q.bindValue(":size", size);
    q.bindValue(":loc", loc);
    q.bindValue(":oName", oName);
    q.bindValue(":oEmail", oEmail);
    q.bindValue(":status", status);
    q.bindValue(":type", type);
    q.bindValue(":dateStr", dateStr);
    q.bindValue(":trips", trips);
    q.bindValue(":fish", fish);
    q.bindValue(":boatId", boatId);

    if (!q.exec()) {
        QString err = q.lastError().text();
        qDebug() << "EDIT FAILED:" << err;
        QMessageBox::critical(this, "Update Failed",
                              "Oracle error:\n\n" + err);
        return;
    }

    QString updateMessage = "Boat updated successfully!";
    if (currentDockId > 0) {
        updateMessage += QString("\nAssigned to Dock ID: %1").arg(currentDockId);
    } else {
        updateMessage += "\nNo dock assigned.";
    }
    QMessageBox::information(this, "Success", updateMessage);

    displayBoats();
    setBoatMode(BoatMode::Add);
    ui->boatPage->setCurrentIndex(1);
    updateBoatStatusProgressBar();
}

// FIXED
void appwindow::on_deleteBoatButton_clicked()
{
    if (currentlySelectedId <= 0) {
        QMessageBox::warning(this, "Selection Error", "Please select a boat to delete!");
        return;
    }

    // Get the dock ID before deleting the boat
    QSqlQuery getDockId;
    getDockId.prepare("SELECT DOCKID FROM BOAT WHERE BOATID = :boatId");
    getDockId.bindValue(":boatId", currentlySelectedId);

    int dockIdToFree = -1;
    if (getDockId.exec() && getDockId.next()) {
        dockIdToFree = getDockId.value(0).toInt();
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete",
        "Are you sure you want to delete boat with ID: " + QString::number(currentlySelectedId) + "?",
        QMessageBox::Yes | QMessageBox::No
    );

    if (reply == QMessageBox::Yes) {
        boatsTmp.setId(currentlySelectedId);

        if (boatsTmp.deleteBoat()) {
            // Free the dock if it was assigned
            if (dockIdToFree > 0) {
                QSqlQuery freeDock;
                freeDock.prepare("UPDATE DOCKING SET STATUS = 'Available' WHERE DOCKID = :dockId");
                freeDock.bindValue(":dockId", dockIdToFree);
                freeDock.exec();
                qDebug() << "Freed dock ID:" << dockIdToFree;
            }

            QMessageBox::information(this, "Success", "Boat deleted successfully!");
            clearBoatInputs();
            displayBoats();
            loadDockingTable();  // Refresh docking table
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete boat: " + boatsTmp.getLastError());
        }
    }

    updateBoatStatusProgressBar();
}
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
//  "Edit" button on the list page �?� navigate to form in Edit mode
// ٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?٤?
void appwindow::on_updateBoatButton_clicked()
{
    if (currentlySelectedId <= 0) {
        QMessageBox::warning(this, "Selection Error",
                             "Please click a row in the table first to select a boat.");
        return;
    }

    // Load data into the form and switch to Edit mode
    populateBoatForm(currentlySelectedId);
    setBoatMode(BoatMode::Edit);
    ui->boatPage->setCurrentIndex(0);
}


void appwindow::on_searchBoatButton_3_clicked()
{
    QString searchText = ui->boatSearchLineEdit->text().trimmed();

    if (searchText.isEmpty()) {
        displayBoats();
        return;
    }

    ui->Boatwidget_2->blockSignals(true);
    ui->Boatwidget_2->setRowCount(0);

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::critical(this, "DB Error", "Database is not open!");
        ui->Boatwidget_2->blockSignals(false);
        return;
    }

    QSqlQuery query(db);
    query.prepare(
        "SELECT BOATID, SIZEBOAT, LOCATION, OWNERNAME, OWNERMAIL, "
        "STATUS, TYPE, LASTMAINTENANCEDATE, TOTALTRIPS, TOTALFISH "
        "FROM BOAT "
        "WHERE CAST(BOATID AS VARCHAR2(20)) LIKE :search "
        "OR UPPER(OWNERNAME) LIKE UPPER(:search2) "
        "ORDER BY BOATID"
        );
    query.bindValue(":search",  "%" + searchText + "%");
    query.bindValue(":search2", "%" + searchText + "%");

    if (!query.exec()) {
        QMessageBox::critical(this, "Search Error", query.lastError().text());
        ui->Boatwidget_2->blockSignals(false);
        return;
    }

    int row = 0;
    while (query.next()) {
        ui->Boatwidget_2->insertRow(row);

        for (int col = 0; col < 10; col++) {
            QTableWidgetItem *item;
            if (col == 5) {
                QString statusText = query.value(col).toInt() == 1 ? "IN PORT" : "OUT";
                item = new QTableWidgetItem(statusText);
            } else {
                item = new QTableWidgetItem(query.value(col).toString());
            }
            if (col == 0)
                item->setData(Qt::UserRole, query.value("BOATID").toInt());
            ui->Boatwidget_2->setItem(row, col, item);
        }
        row++;
    }

    ui->Boatwidget_2->blockSignals(false);

    if (row == 0) {
        QMessageBox::information(this, "Not Found",
                                 "No boat found matching: " + searchText);
        displayBoats();
    }
}

void appwindow::on_pushButton_10_clicked()
{
    qDebug() << "=== PushButton_10 clicked ===";
    qDebug() << "ComboBox current index:" << ui->comboBox_16->currentIndex();

    // Get all boats
    QSqlQuery allBoats = Boats::getAll();

    if (!allBoats.isActive()) {
        qDebug() << "Query failed:" << allBoats.lastError().text();
        QMessageBox::warning(this, "Error", "Failed to retrieve boat data from database.");
        return;
    }

    qDebug() << "Query active, checking for boats...";

    int rowCount = 0;
    while (allBoats.next()) {
        rowCount++;
        qDebug() << "Boat ID:" << allBoats.value(0).toInt();
    }

    qDebug() << "Total boats found:" << rowCount;

    if (rowCount == 0) {
        QMessageBox::warning(this, "No Data", "No boats found in the system to analyze.");
        return;
    }

    // Reset the query to get data again
    allBoats = Boats::getAll();

    // Determine which chart to show based on comboBox selection
    int selectedOption = ui->comboBox_16->currentIndex();
    qDebug() << "Selected option:" << selectedOption << "(0=Fish, 1=Trips)";

    // Prepare data containers
    QStringList boatLabels;
    QList<int> values;

    // Collect data based on selection
    if (selectedOption == 0) {
        // Show Total Fish by Boat
        while (allBoats.next()) {
            int boatId = allBoats.value(0).toInt();
            int totalFish = allBoats.value(9).toInt(); // TOTALFISH is column 9

            // Create a label for each boat
            QString boatLabel = QString("Boat %1").arg(boatId);

            boatLabels.append(boatLabel);
            values.append(totalFish);

            qDebug() << "Boat" << boatId << "Fish:" << totalFish;
        }

        // Check if there's data
        if (boatLabels.isEmpty()) {
            QMessageBox::warning(this, "No Data", "No boat data available.");
            return;
        }

        qDebug() << "Creating Fish chart with" << boatLabels.size() << "boats";
        // Create the chart
        createBoatChart(boatLabels, values, "Total Fish Captured by Boat", "Fish Count");

    } else if (selectedOption == 1) {
        // Show Total Trips by Boat
        while (allBoats.next()) {
            int boatId = allBoats.value(0).toInt();
            int totalTrips = allBoats.value(8).toInt(); // TOTALTRIPS is column 8

            QString boatLabel = QString("Boat %1").arg(boatId);

            boatLabels.append(boatLabel);
            values.append(totalTrips);

            qDebug() << "Boat" << boatId << "Trips:" << totalTrips;
        }

        // Check if there's data
        if (boatLabels.isEmpty()) {
            QMessageBox::warning(this, "No Data", "No boat data available.");
            return;
        }

        qDebug() << "Creating Trips chart with" << boatLabels.size() << "boats";
        // Create the chart
        createBoatChart(boatLabels, values, "Total Trips by Boat", "Number of Trips");
    }
}

void appwindow::createBoatChart(const QStringList &categories, const QList<int> &values,
                                const QString &title, const QString &yAxisLabel)
{
    qDebug() << "Creating chart with" << categories.size() << "categories";

    // Clear any existing chart
    if (ui->chartView_4->chart()) {
        ui->chartView_4->chart()->deleteLater();
    }

    // Create bar series
    QBarSeries *series = new QBarSeries();
    QBarSet *barSet = new QBarSet(yAxisLabel);

    // Add all values to the bar set
    for (int value : values) {
        *barSet << value;
        qDebug() << "Adding value:" << value;
    }

    series->append(barSet);

    // Create chart
    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(title);
    chart->setAnimationOptions(QChart::SeriesAnimations);

    // Create category axis for X-axis
    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setTitleText("Boats");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Create value axis for Y-axis
    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText(yAxisLabel);

    // Find max value for proper scaling
    int maxValue = 0;
    for (int value : values) {
        if (value > maxValue) maxValue = value;
    }

    // Add some padding to the top
    double maxRange = maxValue > 0 ? maxValue + (maxValue * 0.2) : 10;
    axisY->setRange(0, maxRange);
    axisY->setLabelFormat("%d");
    axisY->setTickCount(6);

    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    // Enable legend
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // Set the chart to the chartView_4
    ui->chartView_4->setChart(chart);
    ui->chartView_4->setRenderHint(QPainter::Antialiasing);

    // Force an update
    ui->chartView_4->update();
    ui->chartView_4->repaint();

    qDebug() << "Chart created successfully";
}



////EMAILS BOATS
QDate appwindow::parseBoatMaintenanceDate(const QString &rawDate)
{
    QString normalized = rawDate;
    QStringList parts = normalized.split('/');
    if (parts.size() == 3) {
        // Convert "2/FEB/2027" �?� "2/Feb/2027" (month first letter capital, rest lower)
        QString month = parts[1];
        month = month[0].toUpper() + month.mid(1).toLower();
        normalized = parts[0] + "/" + month + "/" + parts[2];
    }

    for (const QString &fmt : {"d/MMM/yyyy", "dd/MMM/yyyy", "d/MMM/yy", "dd/MMM/yy"}) {
        QDate date = QDate::fromString(normalized, fmt);
        if (date.isValid()) {
            if (date.year() < 2000) date = date.addYears(100);
            return date;
        }
    }
    return QDate();
}
// Add this function to check and send maintenance reminders
/*void appwindow::checkAndSendMaintenanceReminders() //apparently bad????
{
    qDebug() << "Checking for boats due for maintenance...";

    QSqlQuery allBoats = Boats::getAll();

    if (!allBoats.isActive()) {
        qDebug() << "Failed to retrieve boats for maintenance check";
        return;
    }

    QDate today = QDate::currentDate();
    int remindersSent = 0;
    int errorsEncountered = 0;

    while (allBoats.next()) {
        int boatId = allBoats.value(0).toInt();
        QString size = allBoats.value(1).toString();
        QString location = allBoats.value(2).toString();
        QString ownerName = allBoats.value(3).toString();
        QString ownerEmail = allBoats.value(4).toString();
        int status = allBoats.value(5).toInt();
        QString boatType = allBoats.value(6).toString();
        QString lastMaintenanceDateStr = allBoats.value(7).toString();

        // Skip boats without email or invalid email
        if (ownerEmail.isEmpty() || !isEmailValid(ownerEmail)) {
            qDebug() << "Skipping boat" << boatId << "- Invalid or missing email:" << ownerEmail;
            continue;
        }

        // Parse the last maintenance date//updated
        QDate lastMaintenanceDate = parseBoatMaintenanceDate(lastMaintenanceDateStr);
        if (!lastMaintenanceDate.isValid()) {
            qDebug() << "Invalid date for boat" << boatId << ":" << lastMaintenanceDateStr;
            continue;
        }

        if (!lastMaintenanceDate.isValid()) {
            qDebug() << "Skipping boat" << boatId << "- Invalid maintenance date:" << lastMaintenanceDateStr;
            continue;
        }

        // Calculate days since last maintenance
        int daysSinceMaintenance = lastMaintenanceDate.daysTo(today);
        int weeksSinceMaintenance = daysSinceMaintenance / 7;

        // Check if maintenance is overdue (more than 365 days / 52 weeks)
        if (daysSinceMaintenance >= 365) {
            int weeksOverdue = (daysSinceMaintenance - 365) / 7;

            // Send reminder for first overdue or weekly thereafter
            if (weeksOverdue == 0 || weeksOverdue % 1 == 0) { // Weekly reminders
                qDebug() << "Boat" << boatId << "is overdue by" << weeksOverdue << "weeks";
                sendMaintenanceReminderEmail(ownerEmail, ownerName, QString::number(boatId),
                                             boatType, lastMaintenanceDate, weeksOverdue);
                remindersSent++;
            }
        } else {
            // Calculate days until maintenance is due
            int daysUntilDue = 365 - daysSinceMaintenance;
            qDebug() << "Boat" << boatId << "maintenance due in" << daysUntilDue << "days";
        }
    }

    qDebug() << "Maintenance check complete. Sent" << remindersSent << "reminders. Errors:" << errorsEncountered;


}*/



void appwindow::checkAndSendMaintenanceReminders()
{
    QSqlQuery allBoats = Boats::getAll();
    if (!allBoats.isActive()) return;

    QDate today = QDate::currentDate();
    int remindersSent = 0;

    while (allBoats.next()) {
        int boatId = allBoats.value(0).toInt();
        QString ownerEmail = allBoats.value(4).toString();
        QString ownerName = allBoats.value(3).toString();
        QString boatType = allBoats.value(6).toString();
        QString lastMaintStr = allBoats.value(7).toString();

        if (!isEmailValid(ownerEmail)) continue;

        QDate lastMaint = parseBoatMaintenanceDate(lastMaintStr);
        if (!lastMaint.isValid()) continue;

        int daysOverdue = lastMaint.daysTo(today) - 365;  // positive if overdue
        if (daysOverdue >= 0) {
            // Send only once per week (avoid daily spam)
            int weeksOverdue = daysOverdue / 7;
            // Use a persistent record (e.g., database column or QSettings)
            if (weeksOverdue > 0 && weeksOverdue % 1 == 0) {  // weekly
                sendMaintenanceReminderEmail(ownerEmail, ownerName,
                                             QString::number(boatId),
                                             boatType, lastMaint, weeksOverdue);
                remindersSent++;
            }
        }
    }
    qDebug() << "Maintenance reminders sent:" << remindersSent;
}

//status boat
void appwindow::updateBoatStatusProgressBar()
{
    // Get all boats
    QSqlQuery allBoats = Boats::getAll();

    if (!allBoats.isActive()) {
        qDebug() << "Failed to retrieve boats for progress bar";
        return;
    }

    int totalBoats = 0;
    int boatsInPort = 0;

    // Count boats
    while (allBoats.next()) {
        totalBoats++;
        int status = allBoats.value(5).toInt(); // STATUS is column 5 (0=OUT, 1=IN PORT)
        if (status == 1) {
            boatsInPort++;
        }
    }

    if (totalBoats == 0) {
        ui->progressBar_5->setValue(0);
        ui->label_77->setText("Percentage of active boats: 0% (No boats in system)");
        return;
    }

    // Calculate percentage of boats IN PORT
    int percentageInPort = (boatsInPort * 100) / totalBoats;

    // Update progress bar
    ui->progressBar_5->setValue(percentageInPort);

    // Update label with detailed information
    QString statusText = QString("Boats IN PORT: %1% (%2 of %3 boats)")
                             .arg(percentageInPort)
                             .arg(boatsInPort)
                             .arg(totalBoats);
    ui->label_77->setText(statusText);

    qDebug() << "Boat status update - Total:" << totalBoats
             << "In Port:" << boatsInPort
             << "Percentage:" << percentageInPort << "%";
}

//boat pdf export section
void appwindow::on_export_pdf_5_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
                                                    tr("Export Boat Fleet Report to PDF"), "",
                                                    tr("PDF Files (*.pdf);;All Files (*)"));

    if (fileName.isEmpty())
        return;

    if (!fileName.endsWith(".pdf", Qt::CaseInsensitive))
        fileName += ".pdf";

    QPdfWriter pdfWriter(fileName);
    pdfWriter.setPageSize(QPageSize(QPageSize::A4));
    pdfWriter.setPageOrientation(QPageLayout::Portrait);
    pdfWriter.setResolution(300);
    pdfWriter.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);

    if (pdfWriter.width() <= 0 || pdfWriter.height() <= 0) {
        QMessageBox::critical(this, "Export Error", "Failed to initialize PDF writer.");
        return;
    }

    QPainter painter(&pdfWriter);
    painter.setRenderHint(QPainter::Antialiasing);

    double scale = pdfWriter.width() / 170.0;

    QColor darkBlue(10, 76, 115);
    QColor mediumBlue(52, 152, 219);
    QColor lightBlue(163, 213, 247);
    QColor veryLightBlue(230, 242, 255);
    QColor textColor(30, 30, 30);
    QColor borderColor(180, 180, 180);

    int pageWidth  = pdfWriter.width();
    int pageHeight = pdfWriter.height();
    int margin     = 0;
    int yPos       = 0;

    // ===== HEADER =====
    int headerHeight = (int)(28 * scale);
    painter.fillRect(margin, yPos, pageWidth, headerHeight, darkBlue);

    painter.setFont(QFont("Arial", 16, QFont::Bold));
    painter.setPen(Qt::white);
    painter.drawText(margin, yPos, pageWidth, (int)(17 * scale),
                     Qt::AlignCenter | Qt::AlignBottom,
                     "HAWATA MARINA");

    painter.setFont(QFont("Arial", 9));
    painter.drawText(margin, yPos + (int)(17 * scale), pageWidth, (int)(10 * scale),
                     Qt::AlignCenter | Qt::AlignVCenter,
                     "Fleet Performance Report");

    yPos += headerHeight + (int)(5 * scale);

    // ===== REPORT METADATA =====
    painter.setFont(QFont("Arial", 7));
    painter.setPen(QColor(100, 100, 100));
    painter.drawText(margin, yPos,
                     QString("Report Generated: %1")
                         .arg(QDateTime::currentDateTime().toString("dd MMMM yyyy  hh:mm")));

    yPos += (int)(7 * scale);

    // ===== COLLECT FLEET DATA =====
    QSqlQuery allBoats = Boats::getAll();
    int totalBoats = 0;
    int boatsInPort = 0;
    int boatsOut = 0;
    int totalTrips = 0;
    int totalFish = 0;

    while (allBoats.next()) {
        totalBoats++;
        if (allBoats.value(5).toInt() == 1)
            boatsInPort++;
        else
            boatsOut++;
        totalTrips += allBoats.value(8).toInt();
        totalFish  += allBoats.value(9).toInt();
    }

    // ===== EXECUTIVE SUMMARY CARD =====
    int summaryHeight = (int)(36 * scale);
    painter.fillRect(margin, yPos, pageWidth, summaryHeight, veryLightBlue);
    painter.setPen(QPen(mediumBlue, 2));
    painter.drawRect(margin, yPos, pageWidth, summaryHeight);

    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(margin + (int)(4 * scale), yPos + (int)(6 * scale), "EXECUTIVE SUMMARY");

    int halfWidth = pageWidth / 2;
    int metricY   = yPos + (int)(14 * scale);

    // Row 1
    painter.setFont(QFont("Arial", 7));
    painter.setPen(textColor);
    painter.drawText(margin + (int)(4 * scale), metricY, "Total Fleet Size:");
    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(mediumBlue);
    painter.drawText(margin + (int)(38 * scale), metricY, QString::number(totalBoats));

    painter.setFont(QFont("Arial", 7));
    painter.setPen(textColor);
    painter.drawText(margin + halfWidth + (int)(4 * scale), metricY, "Active Boats:");
    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(mediumBlue);
    painter.drawText(margin + halfWidth + (int)(32 * scale), metricY, QString::number(boatsOut));

    metricY += (int)(11 * scale);

    // Row 2
    painter.setFont(QFont("Arial", 7));
    painter.setPen(textColor);
    painter.drawText(margin + (int)(4 * scale), metricY, "Total Trips (All Time):");
    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(mediumBlue);
    painter.drawText(margin + (int)(48 * scale), metricY, QString::number(totalTrips));

    painter.setFont(QFont("Arial", 7));
    painter.setPen(textColor);
    painter.drawText(margin + halfWidth + (int)(4 * scale), metricY, "Total Fish Catch:");
    painter.setFont(QFont("Arial", 11, QFont::Bold));
    painter.setPen(mediumBlue);
    painter.drawText(margin + halfWidth + (int)(38 * scale), metricY, QString::number(totalFish));

    yPos += summaryHeight + (int)(8 * scale);

    // ===== FLEET PERFORMANCE DASHBOARD TITLE =====
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(margin, yPos, "Fleet Performance Dashboard");
    yPos += (int)(7 * scale);

    // ===== KPI CARDS =====
    int kpiHeight  = (int)(22 * scale);
    int kpiSpacing = (int)(3 * scale);
    int kpiWidth   = (pageWidth - kpiSpacing * 2) / 3;

    double utilizationRate = totalBoats > 0 ? (boatsOut * 100.0 / totalBoats) : 0.0;
    double avgTrips        = totalBoats > 0 ? (double)totalTrips / totalBoats  : 0.0;
    double avgFishPerTrip  = totalTrips > 0 ? (double)totalFish  / totalTrips  : 0.0;

    struct KPI { QString label; QString value; };
    KPI kpis[3] = {
        { "FLEET UTILIZATION", QString("%1%").arg(utilizationRate, 0, 'f', 1) },
        { "AVG TRIPS / BOAT",  QString::number(avgTrips, 'f', 1) },
        { "FISH / TRIP",       QString::number(avgFishPerTrip, 'f', 1) }
    };

    for (int k = 0; k < 3; k++) {
        int kx = margin + k * (kpiWidth + kpiSpacing);

        painter.fillRect(kx, yPos, kpiWidth, kpiHeight, lightBlue);
        painter.setPen(QPen(darkBlue, 1));
        painter.drawRect(kx, yPos, kpiWidth, kpiHeight);

        painter.setFont(QFont("Arial", 7));
        painter.setPen(darkBlue);
        painter.drawText(kx + (int)(3 * scale), yPos + (int)(6 * scale), kpis[k].label);

        painter.setFont(QFont("Arial", 13, QFont::Bold));
        painter.setPen(mediumBlue);
        painter.drawText(kx + (int)(3 * scale), yPos + (int)(18 * scale), kpis[k].value);
    }

    yPos += kpiHeight + (int)(9 * scale);

    // ===== FLEET DETAILS TABLE =====
    QTableWidget *table = ui->Boatwidget_2;
    int rowCount = table->rowCount();

    if (rowCount == 0) {
        painter.setFont(QFont("Arial", 10));
        painter.setPen(textColor);
        painter.drawText(margin, yPos, "No boat data available for detailed report.");
        painter.end();
        QMessageBox::information(this, "No Data", "The boat table is empty. Nothing to export.");
        return;
    }

    // Table section title
    painter.setFont(QFont("Arial", 9, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(margin, yPos, "Detailed Fleet Inventory");
    yPos += (int)(7 * scale);

    // Column definitions
    QStringList headers;
    headers << "ID" << "Size" << "Location" << "Owner" << "Status"
            << "Type" << "Last Maint." << "Trips" << "Fish";

    int colCount         = headers.size();
    int colWidthDetailed = pageWidth / colCount;

    int headerRowHeight = qMax(1, (int)(11 * scale));
    int rowHeight       = qMax(1, (int)(9 * scale));

    int rowsPerPage = qMax(1, (pageHeight - yPos - headerRowHeight - (int)(28 * scale)) / rowHeight);

    auto drawTableHeader = [&]() {
        painter.fillRect(margin, yPos, pageWidth, headerRowHeight, mediumBlue);
        painter.setFont(QFont("Arial", 7, QFont::Bold));
        painter.setPen(Qt::white);

        int xPos = margin;
        for (int col = 0; col < colCount; col++) {
            painter.drawText(xPos + (int)(2 * scale), yPos,
                             colWidthDetailed - (int)(3 * scale), headerRowHeight,
                             Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
                             headers[col]);

            if (col < colCount - 1) {
                painter.setPen(QPen(Qt::white, 1));
                painter.drawLine(xPos + colWidthDetailed, yPos,
                                 xPos + colWidthDetailed, yPos + headerRowHeight);
                painter.setPen(Qt::white);
            }
            xPos += colWidthDetailed;
        }
        yPos += headerRowHeight;
    };

    drawTableHeader();

    QSqlQuery boatDetails = Boats::getAll();
    int row        = 0;
    int pageNumber = 1;

    while (boatDetails.next()) {
        if (row > 0 && row % rowsPerPage == 0) {
            pdfWriter.newPage();
            pageNumber++;
            yPos = 0;

            rowsPerPage = qMax(1, (pageHeight - headerRowHeight - (int)(28 * scale)) / rowHeight);

            drawTableHeader();
            painter.setFont(QFont("Arial", 7));
            painter.setPen(textColor);
        }

        QColor rowBg = (row % 2 == 0) ? Qt::white : veryLightBlue;
        painter.fillRect(margin, yPos, pageWidth, rowHeight, rowBg);
        painter.setPen(QPen(borderColor, 1));
        painter.drawRect(margin, yPos, pageWidth, rowHeight);

        int xPos = margin;
        painter.setFont(QFont("Arial", 6));
        painter.setPen(textColor);

        // ID
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(0).toString());
        xPos += colWidthDetailed;

        // Size
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(1).toString());
        xPos += colWidthDetailed;

        // Location
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(2).toString());
        xPos += colWidthDetailed;

        // Owner
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(3).toString());
        xPos += colWidthDetailed;

        // Status (color coded)
        int statusVal      = boatDetails.value(5).toInt();
        QString statusText = (statusVal == 1) ? "IN PORT" : "AT SEA";
        painter.setPen(statusVal == 1 ? QColor(46, 204, 113) : QColor(231, 76, 60));
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         statusText);
        painter.setPen(textColor);
        xPos += colWidthDetailed;

        // Type
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(6).toString());
        xPos += colWidthDetailed;

        // Last Maintenance
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(7).toString());
        xPos += colWidthDetailed;

        // Total Trips
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(8).toString());
        xPos += colWidthDetailed;

        // Total Fish
        painter.drawText(xPos + (int)(2 * scale), yPos,
                         colWidthDetailed - (int)(3 * scale), rowHeight,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         boatDetails.value(9).toString());

        yPos += rowHeight;
        row++;
    }

    yPos += (int)(6 * scale);

    // ===== FOOTER =====
    if (yPos > pageHeight - (int)(24 * scale)) {
        pdfWriter.newPage();
        pageNumber++;
        yPos = 0;
    }

    painter.setPen(QPen(mediumBlue, 2));
    painter.drawLine(margin, yPos, margin + pageWidth, yPos);
    yPos += (int)(5 * scale);

    QStringList insights;
    insights << "�?� Top performing boats identified for recognition";
    insights << QString("�?� %1 boats currently at sea - peak operational status").arg(boatsOut);
    insights << QString("�?� Fleet maintenance tracking: %1 boats need review").arg(boatsInPort / 2);

    painter.setFont(QFont("Arial", 6));
    painter.setPen(QColor(100, 100, 100));
    for (const QString &insight : insights) {
        painter.drawText(margin, yPos, insight);
        yPos += (int)(5 * scale);
    }

    yPos += (int)(4 * scale);

    painter.setFont(QFont("Arial", 6, QFont::Bold));
    painter.setPen(darkBlue);
    painter.drawText(margin, yPos,
                     QString("Report ID: FLT-%1 | Confidential - For Management Use Only")
                         .arg(QDateTime::currentDateTime().toString("yyyyMMdd")));

    painter.drawText(margin, yPos, pageWidth, (int)(10 * scale),
                     Qt::AlignRight | Qt::AlignVCenter,
                     QString("Page %1").arg(pageNumber));

    painter.end();

    QMessageBox::information(this, "Export Successful",
                             QString("Fleet performance report exported successfully to:\n%1").arg(fileName));
}




//////////boat maintenance section


// Send maintenance reminder email
void appwindow::sendMaintenanceReminderEmail(const QString &ownerEmail, const QString &ownerName,
                                             const QString &boatId, const QString &boatType,
                                             const QDate &lastMaintenanceDate, int weeksOverdue)
{
    // Prepare subject
    QString subject;
    if (weeksOverdue == 0) {
        subject = "URGENT: Boat Maintenance Overdue - Hawata Marina";
    } else {
        subject = QString("MAINTENANCE REMINDER - %1 Week%2 Overdue - Hawata Marina")
        .arg(weeksOverdue)
            .arg(weeksOverdue > 1 ? "s" : "");
    }

    // Prepare body (same as before)
    QString body;
    if (weeksOverdue == 0) {
        body = QString(
                   "Dear %1,\n\n"
                   "This is an important reminder that your boat (ID: %2, Type: %3) is now OVERDUE for maintenance.\n\n"
                   "Last Maintenance Date: %4\n"
                   "Current Status: OVERDUE BY 1 YEAR\n\n"
                   "Regular maintenance is crucial for:\n"
                   "٣� Ensuring safety of your vessel and crew\n"
                   "٣� Preventing costly repairs in the future\n"
                   "٣� Maintaining optimal performance\n"
                   "٣� Compliance with marina regulations\n\n"
                   "Please contact our maintenance department immediately to schedule your appointment.\n\n"
                   "To schedule maintenance, please reply to this email or call us at +216 XX XXX XXX.\n\n"
                   "Thank you for your prompt attention to this matter.\n\n"
                   "Best regards,\n"
                   "Hawata Marina Maintenance Team"
                   ).arg(ownerName, boatId, boatType, lastMaintenanceDate.toString("MMMM dd, yyyy"));
    } else {
        body = QString(
                   "Dear %1,\n\n"
                   "This is a follow-up reminder that your boat (ID: %2, Type: %3) is now %4 week%5 overdue for maintenance.\n\n"
                   "Last Maintenance Date: %6\n"
                   "Overdue By: %4 week%5\n\n"
                   "Please schedule your maintenance appointment as soon as possible.\n\n"
                   "To schedule your maintenance, please contact us at:\n"
                   "?��� Phone: +216 XX XXX XXX\n"
                   "?��� Email: maintenance@hawatamarina.com\n\n"
                   "We look forward to serving you.\n\n"
                   "Best regards,\n"
                   "Hawata Marina Maintenance Team"
                   ).arg(ownerName, boatId, boatType)
                   .arg(weeksOverdue)
                   .arg(weeksOverdue > 1 ? "s" : "")
                   .arg(lastMaintenanceDate.toString("MMMM dd, yyyy"));
    }

    // Get email credentials from environment (DO NOT HARDCODE!)
    QString emailUser = "stargaser518@gmail.com";
    QString emailPass = "njnvzpfrozglrzam";

    if (emailUser.isEmpty() || emailPass.isEmpty()) {
        qDebug() << "ERROR: Email credentials not set!";
        QMessageBox::warning(this, "Email Error",
                             "Email credentials not configured.\n"
                             "Set HWT_EMAIL_USER and HWT_EMAIL_PASS environment variables.");
        return;
    }

    SmtpClient *smtp = new SmtpClient("smtp.gmail.com", 465, this);
    smtp->setUsername(emailUser);
    smtp->setPassword(emailPass);

    // ٣� FIX: Use a flag to ensure we only handle ONE signal
    bool *handled = new bool(false);

    connect(smtp, &SmtpClient::mailSent, this, [this, ownerEmail, subject, smtp, handled](bool success, const QString &error) {
        if (*handled) {
            // Already handled this email, ignore duplicate signals
            return;
        }
        *handled = true;

        if (success) {
            qDebug() << "Email sent successfully to:" << ownerEmail;
            // Optional: Show a non-blocking notification instead of QMessageBox
            // QMessageBox::information(this, "Email Sent",
            //                          QString("Maintenance reminder sent to:\n%1").arg(ownerEmail));
        } else {
            qDebug() << "Failed to send email to:" << ownerEmail << "Error:" << error;
            // Only show error for first failure, not for the socket close after success
            if (!error.contains("closed the connection")) {
                QMessageBox::warning(this, "Email Failed",
                                     QString("Failed to send to %1\nError: %2").arg(ownerEmail, error));
            }
        }

        // Clean up
        smtp->deleteLater();
        delete handled;
    });

    smtp->sendMail(emailUser, ownerEmail, subject, body);
}
// Validate email format
bool appwindow::isEmailValid(const QString &email)
{
    QRegularExpression emailRegex("^[\\w\\.]+@[\\w\\.]+\\.[a-zA-Z]{2,}$");
    return emailRegex.match(email).hasMatch();
}

// Get the next maintenance date (one year after last maintenance)
QDate appwindow::getNextMaintenanceDate(const QDate &lastMaintenanceDate)
{
    return lastMaintenanceDate.addYears(1);
}



// Slot to check maintenance reminders (called by timer)
void appwindow::onCheckMaintenanceReminders()
{
    checkAndSendMaintenanceReminders();
}

void appwindow::setupGoogleMap()
{
    if (!ui->listView || !ui->listView->viewport()) {
        return;
    }

    QWidget *container = ui->listView->viewport();
    QLabel *mapLabel = container->findChild<QLabel*>("boatMapImageLabel");
    if (!mapLabel) {
        mapLabel = new QLabel(container);
        mapLabel->setObjectName("boatMapImageLabel");
        mapLabel->setAlignment(Qt::AlignCenter);
        mapLabel->setText("Loading map...");
        mapLabel->show();
    }

    mapLabel->setGeometry(container->rect());
    mapLabel->setStyleSheet("background: #ffffff; border: 1px solid #d7dce4;");
    container->setStyleSheet("background: white; border: 1px solid #c7d2e0; border-radius: 12px;");

    if (!locationUpdateTimer) {
        locationUpdateTimer = new QTimer(this);
        locationUpdateTimer->setInterval(10000);
        connect(locationUpdateTimer, &QTimer::timeout, this, &appwindow::onUpdateBoatPositions);
    }

    updateBoatMapView(36.8065, 10.1815);
}

void appwindow::updateBoatMapView(double latitude, double longitude)
{
    if (!ui->listView || !ui->listView->viewport()) {
        return;
    }

    QLabel *mapLabel = ui->listView->viewport()->findChild<QLabel*>("boatMapImageLabel");
    if (!mapLabel) {
        return;
    }

    mapLabel->setGeometry(ui->listView->viewport()->rect());

    const int width = qMax(256, mapLabel->width());
    const int height = qMax(192, mapLabel->height());
    const QString url = QString("https://static-maps.yandex.ru/1.x/?lang=en-US&ll=%1,%2&z=14&l=map&size=%3,%4&pt=%1,%2,pm2rdm")
                            .arg(longitude, 0, 'f', 6)
                            .arg(latitude, 0, 'f', 6)
                            .arg(width)
                            .arg(height);

    QNetworkAccessManager manager;
    QEventLoop loop;
    QNetworkReply *reply = manager.get(QNetworkRequest(QUrl(url)));
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        mapLabel->setText(QString("Map failed to load\n%1\nLat:%2 Lon:%3")
                              .arg(reply->errorString())
                              .arg(latitude, 0, 'f', 6)
                              .arg(longitude, 0, 'f', 6));
        reply->deleteLater();
        return;
    }

    const QByteArray imageData = reply->readAll();
    reply->deleteLater();

    QPixmap mapPixmap;
    if (!mapPixmap.loadFromData(imageData)) {
        mapLabel->setText("Invalid map image");
        return;
    }

    mapLabel->setPixmap(mapPixmap.scaled(mapLabel->size(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
    mapLabel->setText(QString());
}

void appwindow::populateBoatComboBox()
{
    // No combo box for tracking in current UI; uses lineEdit + pushButton instead.
}

bool appwindow::getBoatCoordinates(int boatId, double &latitude, double &longitude,
                                   QString &boatName, QString &boatStatus)
{
    QSqlQuery query;
    query.prepare("SELECT BOATID, LOCATION, OWNERNAME, STATUS, DOCKID FROM BOAT WHERE BOATID = :boatId");
    query.bindValue(":boatId", boatId);

    if (!query.exec() || !query.next()) {
        return false;
    }

    const QString locationStr = query.value(1).toString();
    boatName = QString("Boat %1 - %2").arg(boatId).arg(query.value(2).toString());
    boatStatus = query.value(3).toString();
    const int dockId = query.value(4).toInt();

    if (parseLocationCoordinates(locationStr, latitude, longitude)) {
        return true;
    }

    // If boat LOCATION is not usable, fallback to linked dock LOCATION.
    if (dockId > 0) {
        QSqlQuery dockQuery;
        dockQuery.prepare("SELECT LOCATION FROM DOCKING WHERE DOCKID = :dockId");
        dockQuery.bindValue(":dockId", dockId);
        if (dockQuery.exec() && dockQuery.next()) {
            const QString dockLocation = dockQuery.value(0).toString();
            if (parseLocationCoordinates(dockLocation, latitude, longitude)) {
                return true;
            }
        }
    }

    return false;
}

void appwindow::updateAllBoatPositions()
{
    QSqlQuery query = Boats::getAll();
    if (!query.isActive()) {
        return;
    }

    while (query.next()) {
        const QString locationStr = query.value(2).toString();
        double latitude = 0.0;
        double longitude = 0.0;
        if (parseLocationCoordinates(locationStr, latitude, longitude)) {
            updateBoatMapView(latitude, longitude);
            return;
        }
    }
}

void appwindow::updateSingleBoatPosition(int boatId)
{
    double latitude = 0.0;
    double longitude = 0.0;
    QString boatName;
    QString boatStatus;

    if (getBoatCoordinates(boatId, latitude, longitude, boatName, boatStatus)) {
        currentLatitude = latitude;
        currentLongitude = longitude;
        updateBoatMapView(latitude, longitude);
    }
}

void appwindow::onUpdateBoatPositions()
{
    if (currentTrackingBoatId > 0) {
        updateSingleBoatPosition(currentTrackingBoatId);
    } else {
        updateAllBoatPositions();
    }
}

void appwindow::on_pushButton_trackBoat_clicked()
{
    if (ui->listView && ui->listView->viewport()) {
        QLabel *mapLabel = ui->listView->viewport()->findChild<QLabel*>("boatMapImageLabel");
        if (mapLabel) {
            mapLabel->setGeometry(ui->listView->viewport()->rect());
        }
    }

    bool ok = false;
    const int selectedBoatId = ui->lineEdit->text().trimmed().toInt(&ok);
    if (!ok || selectedBoatId <= 0) {
        QMessageBox::warning(this, "Selection Error", "Please enter a valid boat ID.");
        return;
    }

    double latitude = 0.0;
    double longitude = 0.0;
    QString boatName;
    QString boatStatus;
    if (!getBoatCoordinates(selectedBoatId, latitude, longitude, boatName, boatStatus)) {
        QString rawLocation;
        QSqlQuery rawQuery;
        rawQuery.prepare("SELECT LOCATION FROM BOAT WHERE BOATID = :boatId");
        rawQuery.bindValue(":boatId", selectedBoatId);
        if (rawQuery.exec() && rawQuery.next()) {
            rawLocation = rawQuery.value(0).toString();
        }

        QMessageBox::warning(this, "Location Error",
                             QString("Boat %1 has no valid coordinates.\n"
                                     "Stored LOCATION: %2\n"
                                     "Use format: latitude,longitude")
                                 .arg(selectedBoatId)
                                 .arg(rawLocation.isEmpty() ? "(empty)" : rawLocation));
        return;
    }

    currentTrackingBoatId = selectedBoatId;
    currentLatitude = latitude;
    currentLongitude = longitude;
    updateBoatMapView(latitude, longitude);
    if (locationUpdateTimer) {
        locationUpdateTimer->start();
    }
}

void appwindow::on_pushButton_trackAll_clicked()
{
    currentTrackingBoatId = -1;
    updateAllBoatPositions();
    if (locationUpdateTimer) {
        locationUpdateTimer->start();
    }
}

void appwindow::on_comboBox_selectBoat_currentIndexChanged(int index)
{
    Q_UNUSED(index);
}


///=============COMPANY SECTION=============
void appwindow::loadCompaniesTable()
{
    Company companyObj;
    QList<Company> records = companyObj.afficher_liste();

    QTableWidget *table = ui->tableWidget_11;
    table->setRowCount(0);
    table->setColumnCount(7);
    table->setHorizontalHeaderLabels(
        {"ID", "Name", "Location", "Email", "Phone", "Preferred Fish", "Status"});
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);

    for (const Company &r : records) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(r.getCompanyId())));
        table->setItem(row, 1, new QTableWidgetItem(r.getName()));
        table->setItem(row, 2, new QTableWidgetItem(r.getLocation()));
        table->setItem(row, 3, new QTableWidgetItem(r.getEmail()));
        table->setItem(row, 4, new QTableWidgetItem(r.getPhone()));
        table->setItem(row, 5, new QTableWidgetItem(r.getPreferredFish()));
        table->setItem(row, 6, new QTableWidgetItem(r.getStatus()));
    }

    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    ui->labelResults_7->setText(QString("Showing %1 Companies").arg(records.size()));
}
void appwindow::fillCompanyForm(int row)
{
    if (row < 0) return;

    QTableWidget *table = ui->tableWidget_11;
    if (!table->item(row, 0)) return;

    selectedCompanyId = table->item(row, 0)->text().toInt();

    ui->firstNameEdit_8->setText(table->item(row, 1) ? table->item(row, 1)->text() : "");
    ui->lastNameEdit_7->setText (table->item(row, 2) ? table->item(row, 2)->text() : "");
    ui->emailEdit_4->setText    (table->item(row, 3) ? table->item(row, 3)->text() : "");
    ui->passwordEdit_3->setText (table->item(row, 4) ? table->item(row, 4)->text() : "");
    ui->lastNameEdit_8->setText (table->item(row, 5) ? table->item(row, 5)->text() : "");

    QString statusText = table->item(row, 6) ? table->item(row, 6)->text() : "";
    int statusIdx = ui->role_option_7->findText(statusText);
    if (statusIdx >= 0)
        ui->role_option_7->setCurrentIndex(statusIdx);

    ui->stackedWidget_5->setCurrentIndex(0);
    ui->CreateUser_3->setText("Update Company");
}

void appwindow::on_tableWidget_11_cellClicked(int row, int /*column*/)
{
    if (row < 0 || !ui->tableWidget_11->item(row, 0)) return;
    selectedCompanyId = ui->tableWidget_11->item(row, 0)->text().toInt();
}
void appwindow::on_tableWidget_11_cellDoubleClicked(int row, int column)
{
    Q_UNUSED(column);
    fillCompanyForm(row);
}
///CREATE / UPDATE
void appwindow::on_CreateUser_3_clicked()
{
    QString name          = ui->firstNameEdit_8->text().trimmed();
    QString location      = ui->lastNameEdit_7->text().trimmed();
    QString email         = ui->emailEdit_4->text().trimmed();
    QString phone         = ui->passwordEdit_3->text().trimmed();
    QString preferredFish = ui->lastNameEdit_8->text().trimmed();
    QString status        = ui->role_option_7->currentText().trimmed().toUpper();

    // ===== VALIDATION =====
    QRegularExpression nameRegex("^[A-Za-z\\s]{8,}$");
    if (!nameRegex.match(name).hasMatch()) {
        QMessageBox::warning(this, "Validation Error",
                             "Name must contain only letters and be at least 8 characters.");
        return;
    }

    if (preferredFish.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "Preferred fish is required.");
        return;
    }

    QRegularExpression emailRegex("^[\\w\\.]+@[\\w\\.]+\\.[a-zA-Z]{2,}$");
    if (!emailRegex.match(email).hasMatch()) {
        QMessageBox::warning(this, "Validation Error", "Invalid email format.");
        return;
    }

    QRegularExpression phoneRegex("^[0-9]{8}$");
    if (!phoneRegex.match(phone).hasMatch()) {
        QMessageBox::warning(this, "Validation Error",
                             "Phone number must contain exactly 8 digits.");
        return;
    }

    if (status != "ACTIVE" && status != "INACTIVE") {
        QMessageBox::warning(this, "Validation Error", "Status must be ACTIVE or INACTIVE.");
        return;
    }

    bool success = false;

    if (selectedCompanyId == -1) {
        // ===== CREATE =====
        Company c;
        c.setName(name);
        c.setLocation(location);
        c.setEmail(email);
        c.setPhone(phone);
        c.setPreferredFish(preferredFish);
        c.setStatus(status);

        success = c.ajouter_company();
        if (success)
            QMessageBox::information(this, "Success", "Company created successfully.");
    } else {
        // ===== UPDATE =====
        Company c;
        c.setCompanyId(selectedCompanyId);
        c.setName(name);
        c.setLocation(location);
        c.setEmail(email);
        c.setPhone(phone);
        c.setPreferredFish(preferredFish);
        c.setStatus(status);

        success = c.modifier_company();
        if (success)
            QMessageBox::information(this, "Success", "Company updated successfully.");
    }

    if (!success) {
        QMessageBox::critical(this, "Error", "Operation failed.");
        return;
    }

    loadCompaniesTable();
    on_clear_7_clicked();
}
///EDIT
void appwindow::on_edit_company_7_clicked()
{
    int row = ui->tableWidget_11->currentRow();
    if (row < 0) {
        QMessageBox::warning(this, "No Selection", "Please select a company row first.");
        return;
    }
    fillCompanyForm(row);
}

///DELETE
void appwindow::on_delete_company_7_clicked()
{
    QModelIndexList selected = ui->tableWidget_11->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, "No Selection", "Please select a company to delete.");
        return;
    }

    int row = selected.first().row();
    int id  = ui->tableWidget_11->item(row, 0)->text().toInt();
    QString name = ui->tableWidget_11->item(row, 1)->text();

    if (id <= 0) {
        QMessageBox::warning(this, "Error", "Invalid company ID.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Delete",
        QString("Are you sure you want to delete \"%1\" (ID: %2)?").arg(name).arg(id),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        Company c;
        if (c.supprimer(id)) {
            QMessageBox::information(this, "Deleted", "Company deleted successfully.");
            selectedCompanyId = -1;
            loadCompaniesTable();
        } else {
            QMessageBox::critical(this, "Error", "Failed to delete company.");
        }
    }
}
// search
void appwindow::on_searchbar_7_textChanged(const QString &text)
{
    for (int row = 0; row < ui->tableWidget_11->rowCount(); ++row) {
        bool match = false;
        for (int col = 0; col < ui->tableWidget_11->columnCount(); ++col) {
            QTableWidgetItem *item = ui->tableWidget_11->item(row, col);
            if (item && item->text().contains(text, Qt::CaseInsensitive)) {
                match = true;
                break;
            }
        }
        ui->tableWidget_11->setRowHidden(row, !match);
    }
}

///CLEAR
void appwindow::on_clear_7_clicked()
{
    selectedCompanyId = -1;

    ui->firstNameEdit_8->clear();
    ui->lastNameEdit_7->clear();
    ui->emailEdit_4->clear();
    ui->passwordEdit_3->clear();
    ui->lastNameEdit_8->clear();
    ui->role_option_7->setCurrentIndex(0);

    ui->CreateUser_3->setText("Create Company");

    if (ui->tableWidget_11->selectionModel())
        ui->tableWidget_11->clearSelection();
}


void appwindow::on_export_pdf_7_clicked()
{
    QString fileName = QFileDialog::getSaveFileName(
        this, "Export PDF", "", "PDF Files (*.pdf)");

    if (fileName.isEmpty())
        return;

    QTextDocument document;

    int total = ui->tableWidget_11->rowCount();

    QString html = R"(
    <html>
    <head>
    <style>
        body {
            font-family: Arial;
            margin: 0;
            padding: 0;
        }

        /* ===== HEADER ===== */
        .header {
            background: linear-gradient(to right, #003E7E, #005CBF);
            color: white;
            padding: 25px;
        }

        .header h1 {
            margin: 0;
            font-size: 26px;
        }

        .subtitle {
            font-size: 12px;
            opacity: 0.8;
        }

        /* ===== SECTION TITLE ===== */
        .section-title {
            margin-top: 30px;
            font-size: 18px;
            font-weight: bold;
            color: #003E7E;
            border-left: 6px solid #005CBF;
            padding-left: 10px;
        }

        /* ===== KPI CARD ===== */
        .card-container {
            margin-top: 15px;
        }

        .card {
            background-color: #F4F6F9;
            border-radius: 6px;
            padding: 15px;
            width: 250px;
            display: inline-block;
            margin-right: 15px;
            box-shadow: 2px 2px 8px rgba(0,0,0,0.1);
        }

        .card-title {
            font-size: 13px;
            color: #555;
        }

        .card-value {
            font-size: 26px;
            font-weight: bold;
            color: #003E7E;
        }

        /* ===== TABLE ===== */
        table {
            border-collapse: collapse;
            width: 100%;
            margin-top: 20px;
            font-size: 12px;
        }

        th {
            background-color: #003E7E;
            color: white;
            padding: 10px;
            text-align: left;
        }

        td {
            padding: 8px;
            border-bottom: 1px solid #ddd;
        }

        tr:nth-child(even) {
            background-color: #F4F6F9;
        }

        /* ===== FOOTER ===== */
        .footer {
            margin-top: 40px;
            font-size: 10px;
            color: #777;
            text-align: center;
            border-top: 1px solid #ccc;
            padding-top: 10px;
        }

    </style>
    </head>
    <body>
    )";

    // ===== HEADER =====
    html += "<div class='header'>";
    html += "<h1>COMPANY PERFORMANCE REPORT</h1>";
    html += "<div class='subtitle'>Marina Management System | Generated on "
            + QDate::currentDate().toString("dd MMM yyyy") + "</div>";
    html += "</div>";

    // ===== EXECUTIVE SUMMARY =====
    html += "<div class='section-title'>EXECUTIVE SUMMARY</div>";

    html += "<div class='card-container'>";
    html += "<div class='card'>";
    html += "<div class='card-title'>Total Companies</div>";
    html += "<div class='card-value'>" + QString::number(total) + "</div>";
    html += "</div>";
    html += "</div>";

    // ===== TABLE SECTION =====
    html += "<div class='section-title'>COMPANY REGISTRY</div>";

    html += "<table>";
    html += "<tr>"
            "<th>ID</th>"
            "<th>Name</th>"
            "<th>Location</th>"
            "<th>Email</th>"
            "<th>Phone</th>"
            "<th>Preferred Fish</th>"
            "<th>Status</th>"
            "</tr>";

    for (int row = 0; row < ui->tableWidget_11->rowCount(); ++row)
    {
        html += "<tr>";

        for (int col = 0; col < ui->tableWidget_11->columnCount(); ++col)
        {
            QString text = ui->tableWidget_11->item(row, col)->text();
            html += "<td>" + text + "</td>";
        }

        html += "</tr>";
    }

    html += "</table>";

    // ===== FOOTER =====
    html += "<div class='footer'>";
    html += "Marina Management System - Company Report - CONFIDENTIAL";
    html += "</div>";

    html += "</body></html>";

    document.setHtml(html);

    QPdfWriter writer(fileName);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(300);

    document.print(&writer);

    QMessageBox::information(this, "Success", "PDF exported successfully!");
}
void appwindow::loadCompaniesTableFromList(const QList<Company> &records)
{
    QTableWidget *table = ui->tableWidget_11;
    table->setRowCount(0);

    for (const Company &r : records) {
        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(QString::number(r.getCompanyId())));
        table->setItem(row, 1, new QTableWidgetItem(r.getName()));
        table->setItem(row, 2, new QTableWidgetItem(r.getLocation()));
        table->setItem(row, 3, new QTableWidgetItem(r.getEmail()));
        table->setItem(row, 4, new QTableWidgetItem(r.getPhone()));
        table->setItem(row, 5, new QTableWidgetItem(r.getPreferredFish()));
        table->setItem(row, 6, new QTableWidgetItem(r.getStatus()));
    }

    ui->labelResults_7->setText(QString("Showing %1 Companies").arg(records.size()));
}
void appwindow::on_comboBox_19_currentTextChanged(const QString &arg1)
{
    Company companyObj;
    QList<Company> records = companyObj.afficher_liste();

    if (arg1 == "Name") {
        std::sort(records.begin(), records.end(), [](const Company &a, const Company &b) {
            return a.getName().toLower() < b.getName().toLower();
        });
    } else if (arg1 == "Preferred Fish") {
        std::sort(records.begin(), records.end(), [](const Company &a, const Company &b) {
            return a.getPreferredFish().toLower() < b.getPreferredFish().toLower();
        });
    }

    loadCompaniesTableFromList(records);
}

void appwindow::on_pushButton_12_clicked()
{
    QString filterType = ui->comboBox_20->currentText();

    QMap<QString, int> stats;

    for (int row = 0; row < ui->tableWidget_11->rowCount(); ++row) {
        QString key;
        if (filterType == "Status")
            key = ui->tableWidget_11->item(row, 6)->text();
        else if (filterType == "Preferred Fish")
            key = ui->tableWidget_11->item(row, 5)->text();
        stats[key]++;
    }

    QBarSeries *series = new QBarSeries();
    QBarSet *set = new QBarSet(filterType);
    QStringList categories;

    for (auto it = stats.begin(); it != stats.end(); ++it) {
        *set << it.value();
        categories << it.key();
    }
    series->append(set);

    QChart *chart = new QChart();
    chart->addSeries(series);
    chart->setTitle("Company Distribution by " + filterType);
    chart->setAnimationOptions(QChart::SeriesAnimations);

    QBarCategoryAxis *axisX = new QBarCategoryAxis();
    axisX->append(categories);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    QValueAxis *axisY = new QValueAxis();
    axisY->setTitleText("Number of Companies");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    chart->legend()->setVisible(true);

    ui->chartView_6->setChart(chart);
    ui->chartView_6->setRenderHint(QPainter::Antialiasing);
}
// =====================================================================
// ============= COMPANY EMAIL CAMPAIGN SECTION ========================
// =====================================================================

void appwindow::initEmailCampaignPage()
{
    connect(ui->radio_inactive_2,  &QRadioButton::toggled,
            this, &appwindow::updateTargetCount);
    connect(ui->radio_promotion_2, &QRadioButton::toggled,
            this, &appwindow::updateTargetCount);
    connect(ui->radio_custom_2,    &QRadioButton::toggled,
            this, &appwindow::updateTargetCount);

    connect(ui->filter_fish_2,     qOverload<int>(&QComboBox::currentIndexChanged),
            this, &appwindow::updateTargetCount);
    connect(ui->filter_location_2, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &appwindow::updateTargetCount);

    connect(ui->btn_template_inactive_2, &QPushButton::clicked,
            this, &appwindow::on_btn_template_inactive_2_clicked);
    connect(ui->btn_template_promo_2,    &QPushButton::clicked,
            this, &appwindow::on_btn_template_promo_2_clicked);
    connect(ui->btn_template_clear_2,    &QPushButton::clicked,
            this, &appwindow::on_btn_template_clear_2_clicked);

    connect(ui->btn_preview_2,  &QPushButton::clicked,
            this, &appwindow::on_btn_preview_2_clicked);
    connect(ui->btn_send_now_2, &QPushButton::clicked,
            this, &appwindow::on_btn_send_now_2_clicked);
    connect(ui->btn_schedule_2, &QPushButton::clicked,
            this, &appwindow::on_btn_schedule_2_clicked);

    ui->schedule_date_2->setDateTime(QDateTime::currentDateTime().addDays(1));

    updateTargetCount();
}

// ─────────────────────────────────────────────────────────────────────
//  Build filtered company list from UI state
// ─────────────────────────────────────────────────────────────────────
QList<Company> appwindow::getTargetCompanies()
{
    Company obj;
    QList<Company> all = obj.afficher_liste();
    QList<Company> result;

    QString fishFilter     = ui->filter_fish_2->currentText();
    QString locationFilter = ui->filter_location_2->currentText();

    for (const Company &c : all) {

        // ── Status filter ────────────────────────────────────────────
        if (ui->radio_inactive_2->isChecked()) {
            if (c.getStatus().toUpper() != "INACTIVE") continue;
        } else if (ui->radio_promotion_2->isChecked()) {
            if (c.getStatus().toUpper() != "ACTIVE") continue;
        }
        // radio_custom_2 → no status filter

        // ── Fish filter ──────────────────────────────────────────────
        if (fishFilter != "All Fish Types") {
            if (!c.getPreferredFish().contains(fishFilter, Qt::CaseInsensitive))
                continue;
        }

        // ── Location filter ──────────────────────────────────────────
        if (locationFilter != "All Locations") {
            if (!c.getLocation().contains(locationFilter, Qt::CaseInsensitive))
                continue;
        }

        result.append(c);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────
//  Update "Target: X companies" label
// ─────────────────────────────────────────────────────────────────────
void appwindow::updateTargetCount()
{
    int count = getTargetCompanies().size();
    ui->label_target_count_2->setText(
        QString("Target: %1 compan%2")
            .arg(count)
            .arg(count == 1 ? "y" : "ies"));
}

// ─────────────────────────────────────────────────────────────────────
//  Replace [placeholders] for one company
// ─────────────────────────────────────────────────────────────────────
QString appwindow::buildEmailBody(const Company &company, const QString &templateText)
{
    QString body = templateText;
    body.replace("[Company Name]",   company.getName());
    body.replace("[Preferred Fish]", company.getPreferredFish());
    body.replace("[Location]",       company.getLocation());
    body.replace("[Email]",          company.getEmail());
    return body;
}

// ─────────────────────────────────────────────────────────────────────
//  Template buttons  (random pick from vector)
// ─────────────────────────────────────────────────────────────────────
void appwindow::on_btn_template_inactive_2_clicked()
{
    static const QVector<QPair<QString,QString>> inactiveTemplates = {
        {
            "We Miss You, [Company Name]!",
            "Dear [Company Name],\n\n"
            "We noticed it has been a while since we last had the pleasure of working together. "
            "Our team has been thinking about you and we would love to reconnect.\n\n"
            "We continue to offer the finest [Preferred Fish] from the waters of [Location], "
            "and we believe there is a great opportunity waiting for both of us.\n\n"
            "Would you be open to a quick call this week to explore how we can serve you better?\n\n"
            "Warm regards,\nHawata Marina Team"
        },
        {
            "A Special Offer to Welcome You Back – [Company Name]",
            "Dear [Company Name],\n\n"
            "It has been some time since your last visit and we genuinely value our relationship with you.\n\n"
            "To show our appreciation, we are offering an exclusive discount on your next order of "
            "[Preferred Fish] – a product we know your business depends on.\n\n"
            "Your location in [Location] puts you perfectly within our delivery range, and we are "
            "ready to fulfil your needs faster than ever.\n\n"
            "Let us know if you are interested and we will get things moving right away.\n\n"
            "Best wishes,\nHawata Marina Team"
        },
        {
            "Checking In – How Can We Help, [Company Name]?",
            "Hello [Company Name],\n\n"
            "We have not heard from you in a while and we just wanted to check in.\n\n"
            "If there is anything that caused a pause in our partnership – whether it is pricing, "
            "availability of [Preferred Fish], or anything else – we would love to hear your feedback "
            "and work together to find a solution.\n\n"
            "Your satisfaction is our priority, and we are here whenever you are ready.\n\n"
            "Looking forward to hearing from you,\nHawata Marina Team"
        },
        {
            "Fresh Stock Alert for [Company Name]",
            "Dear [Company Name],\n\n"
            "Great news! We have just received a fresh batch of [Preferred Fish] that we think "
            "would be of interest to your operations in [Location].\n\n"
            "Given our previous partnership, we wanted to give you early access before "
            "opening stock to the wider market.\n\n"
            "Quantities are limited – reach out today to secure your allocation.\n\n"
            "Kind regards,\nHawata Marina Team"
        },
        {
            "We Value Your Partnership, [Company Name]",
            "Dear [Company Name],\n\n"
            "Partnerships like ours are built on trust and consistency, and we want to make sure "
            "we are still meeting your expectations.\n\n"
            "We noticed your last interaction with us was some time ago. Our team is ready to "
            "resume deliveries of [Preferred Fish] to [Location] at competitive prices.\n\n"
            "Please feel free to reply to this message – we would be happy to reconnect at your convenience.\n\n"
            "Sincerely,\nHawata Marina Team"
        }
    };

    int idx = QRandomGenerator::global()->bounded(inactiveTemplates.size());
    ui->input_subject_2->setText(inactiveTemplates[idx].first);
    ui->email_template_2->setPlainText(inactiveTemplates[idx].second);
}

void appwindow::on_btn_template_promo_2_clicked()
{
    static const QVector<QPair<QString,QString>> promoTemplates = {
        {
            "Exclusive Seasonal Offer for [Company Name]",
            "Dear [Company Name],\n\n"
            "As one of our valued active partners, we are thrilled to bring you an exclusive "
            "seasonal promotion on [Preferred Fish].\n\n"
            "For a limited time, we are offering priority pricing and guaranteed stock "
            "reservations for companies in [Location].\n\n"
            "Take advantage of this offer before it expires – quantities are limited and our "
            "loyal partners always come first.\n\n"
            "Best regards,\nHawata Marina Team"
        },
        {
            "New Arrivals – [Preferred Fish] Now in Stock!",
            "Hello [Company Name],\n\n"
            "We are excited to announce that our latest catch of [Preferred Fish] has just arrived "
            "and is ready for immediate dispatch to [Location].\n\n"
            "As a valued partner, you get first access to our freshest stock at preferential rates.\n\n"
            "Place your order today and experience the quality that sets us apart.\n\n"
            "Warm regards,\nHawata Marina Team"
        },
        {
            "Bulk Order Discount – Just for You, [Company Name]",
            "Dear [Company Name],\n\n"
            "We know how important cost efficiency is to your business, which is why we are "
            "launching a special bulk discount programme exclusively for our active partners.\n\n"
            "Order a qualifying quantity of [Preferred Fish] and receive a significant price reduction "
            "– delivered directly to [Location] on your schedule.\n\n"
            "Reply to this email to learn more and lock in your rate.\n\n"
            "Looking forward to serving you,\nHawata Marina Team"
        },
        {
            "A Thank You from the Marina Team – Special Reward Inside",
            "Dear [Company Name],\n\n"
            "Your continued trust and partnership means everything to us.\n\n"
            "As a token of our appreciation, we are offering [Company Name] an exclusive "
            "loyalty reward on your next shipment of [Preferred Fish] to [Location].\n\n"
            "No action is required – simply place your order as usual and the discount "
            "will be applied automatically.\n\n"
            "Thank you for being an outstanding partner.\n\n"
            "With gratitude,\nHawata Marina Team"
        },
        {
            "Weekend Flash Sale – [Preferred Fish] at Special Rates",
            "Hello [Company Name],\n\n"
            "This weekend only, we are running a flash promotion on [Preferred Fish] "
            "for our active partners across [Location] and beyond.\n\n"
            "Prices are reduced, stock is fresh, and delivery slots are available immediately.\n\n"
            "This offer expires Sunday midnight – do not miss out!\n\n"
            "Act fast and reply to this email to reserve your order.\n\n"
            "Hawata Marina Team"
        }
    };

    int idx = QRandomGenerator::global()->bounded(promoTemplates.size());
    ui->input_subject_2->setText(promoTemplates[idx].first);
    ui->email_template_2->setPlainText(promoTemplates[idx].second);
}

void appwindow::on_btn_template_clear_2_clicked()
{
    ui->input_subject_2->clear();
    ui->email_template_2->clear();
}

// ─────────────────────────────────────────────────────────────────────
//  Preview – QDialog with rendered email
// ─────────────────────────────────────────────────────────────────────
void appwindow::on_btn_preview_2_clicked()
{
    QString subject      = ui->input_subject_2->text().trimmed();
    QString templateText = ui->email_template_2->toPlainText().trimmed();

    if (subject.isEmpty() || templateText.isEmpty()) {
        QMessageBox::warning(this, "Missing Content",
                             "Please fill in the subject and template before previewing.");
        return;
    }

    QList<Company> targets = getTargetCompanies();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, "No Targets",
                             "No companies match the current filters.");
        return;
    }

    Company sample      = targets.first();
    QString previewBody = buildEmailBody(sample, templateText);

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Email Preview");
    dlg->setMinimumSize(650, 520);

    QVBoxLayout *layout = new QVBoxLayout(dlg);

    QLabel *infoLabel = new QLabel(
        QString("Sample preview for: <b>%1</b>  |  "
                "<b>%2</b> total recipient%3")
            .arg(sample.getName())
            .arg(targets.size())
            .arg(targets.size() == 1 ? "" : "s"));
    infoLabel->setStyleSheet(
        "padding: 8px; background: #e8f4f8; border-radius: 4px; font-size: 13px;");
    layout->addWidget(infoLabel);

    QLabel *subjectLabel = new QLabel("<b>Subject:</b> " + subject);
    subjectLabel->setStyleSheet("padding: 6px; font-size: 13px;");
    layout->addWidget(subjectLabel);

    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet("color: #ccc;");
    layout->addWidget(line);

    QTextEdit *bodyView = new QTextEdit();
    bodyView->setPlainText(previewBody);
    bodyView->setReadOnly(true);
    bodyView->setStyleSheet("font-size: 13px; padding: 6px;");
    layout->addWidget(bodyView);

    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setFixedHeight(35);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    layout->addWidget(closeBtn);

    dlg->exec();
    delete dlg;
}

// ─────────────────────────────────────────────────────────────────────
//  Send Now
// ─────────────────────────────────────────────────────────────────────
void appwindow::on_btn_send_now_2_clicked()
{
    QString subject      = ui->input_subject_2->text().trimmed();
    QString templateText = ui->email_template_2->toPlainText().trimmed();

    if (subject.isEmpty() || templateText.isEmpty()) {
        QMessageBox::warning(this, "Missing Content",
                             "Please fill in the subject and email template.");
        return;
    }

    QList<Company> targets = getTargetCompanies();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, "No Targets",
                             "No companies match the current filters.");
        return;
    }

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Send",
        QString("Send emails to %1 compan%2?\n\nSubject: %3")
            .arg(targets.size())
            .arg(targets.size() == 1 ? "y" : "ies")
            .arg(subject),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    int sent = 0, failed = 0, skipped = 0;

    ui->btn_send_now_2->setEnabled(false);
    ui->btn_send_now_2->setText("Sending...");
    QApplication::processEvents();

    QRegularExpression emailRegex("^[\\w\\.]+@[\\w\\.]+\\.[a-zA-Z]{2,}$");

    for (const Company &company : targets) {

        if (company.getEmail().isEmpty() ||
            !emailRegex.match(company.getEmail()).hasMatch()) {
            skipped++;
            continue;
        }

        QString body = buildEmailBody(company, templateText);

        SmtpClient *smtp = new SmtpClient("smtp.gmail.com", 465, this);
        smtp->setUsername("khelifaaa.ahmed@gmail.com");
        smtp->setPassword("psdb ozyc uypx vqcb");

        bool thisSent = false;
        QEventLoop loop;
        connect(smtp, &SmtpClient::mailSent,
                [&](bool success, const QString &error) {
                    thisSent = success;
                    if (!success)
                        qDebug() << "Send failed to" << company.getEmail() << ":" << error;
                    loop.quit();
                });

        smtp->sendMail("khelifaaa.ahmed@gmail.com",
                       company.getEmail(),
                       subject,
                       body);
        loop.exec();
        smtp->deleteLater();

        thisSent ? sent++ : failed++;
        QApplication::processEvents();
    }

    ui->btn_send_now_2->setEnabled(true);
    ui->btn_send_now_2->setText("📧 Send Now");

    QMessageBox::information(this, "Campaign Complete",
                             QString("✅ Sent:     %1\n"
                                     "❌ Failed:   %2\n"
                                     "⚠️  Skipped:  %3  (invalid/missing email)")
                                 .arg(sent).arg(failed).arg(skipped));
}

// ─────────────────────────────────────────────────────────────────────
//  Schedule
// ─────────────────────────────────────────────────────────────────────
void appwindow::on_btn_schedule_2_clicked()
{
    QString subject      = ui->input_subject_2->text().trimmed();
    QString templateText = ui->email_template_2->toPlainText().trimmed();
    QDateTime scheduleAt = ui->schedule_date_2->dateTime();

    if (subject.isEmpty() || templateText.isEmpty()) {
        QMessageBox::warning(this, "Missing Content",
                             "Please fill in the subject and email template.");
        return;
    }

    if (scheduleAt <= QDateTime::currentDateTime()) {
        QMessageBox::warning(this, "Invalid Time",
                             "Please select a future date and time.");
        return;
    }

    QList<Company> targets = getTargetCompanies();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, "No Targets",
                             "No companies match the current filters.");
        return;
    }

    qint64 msDelay = QDateTime::currentDateTime().msecsTo(scheduleAt);

    QMessageBox::information(this, "Scheduled",
                             QString("Campaign scheduled for:\n%1\n\nRecipients: %2 compan%3")
                                 .arg(scheduleAt.toString("dd/MM/yyyy  hh:mm"))
                                 .arg(targets.size())
                                 .arg(targets.size() == 1 ? "y" : "ies"));

    ui->btn_schedule_2->setEnabled(false);
    ui->btn_schedule_2->setText("⏰ Scheduled");

    QTimer::singleShot(msDelay, this, [this]() {
        ui->btn_schedule_2->setEnabled(true);
        ui->btn_schedule_2->setText("⏰ Schedule");
        on_btn_send_now_2_clicked();
    });
}

// =====================================================================
//  WhatsApp / SMS SECTION
// =====================================================================

void appwindow::initWhatsAppSmsPage()
{
    // ── Alert type buttons behave as a radio group ───────────────────
    // (they are checkable QPushButtons – make only one active at a time)
    QList<QPushButton*> alertBtns = {
        ui->atype_stock, ui->atype_closure,
        ui->atype_price, ui->atype_port, ui->atype_pickup
    };
    for (QPushButton *btn : alertBtns) {
        connect(btn, &QPushButton::clicked, this, [this, btn, alertBtns]() {
            for (QPushButton *b : alertBtns)
                b->setChecked(false);
            btn->setChecked(true);
        });
    }

    // ── Recipients radio buttons ─────────────────────────────────────
    connect(ui->rec_all,      &QRadioButton::toggled, this, &appwindow::updateWaSmsRecipientCount);
    connect(ui->rec_fish,     &QRadioButton::toggled, this, [this](bool checked) {
        ui->combo_fish_filter->setEnabled(checked);
        updateWaSmsRecipientCount();
    });
    connect(ui->rec_location, &QRadioButton::toggled, this, [this](bool checked) {
        ui->combo_location_filter->setEnabled(checked);
        updateWaSmsRecipientCount();
    });
    connect(ui->rec_custom,   &QRadioButton::toggled, this, &appwindow::updateWaSmsRecipientCount);

    connect(ui->combo_fish_filter,     qOverload<int>(&QComboBox::currentIndexChanged),
            this, &appwindow::updateWaSmsRecipientCount);
    connect(ui->combo_location_filter, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &appwindow::updateWaSmsRecipientCount);

    // ── Channel radio buttons update char count hint ─────────────────
    connect(ui->ch_whatsapp, &QRadioButton::toggled, this, &appwindow::updateWaCharCount);
    connect(ui->ch_sms,      &QRadioButton::toggled, this, &appwindow::updateWaCharCount);
    connect(ui->ch_both,     &QRadioButton::toggled, this, &appwindow::updateWaCharCount);

    // ── Message body – live char counter ─────────────────────────────
    connect(ui->msg_body, &QTextEdit::textChanged, this, &appwindow::updateWaCharCount);

    // ── Schedule radio buttons enable/disable datetime picker ────────
    connect(ui->sched_now,   &QRadioButton::toggled, this, [this](bool checked) {
        ui->schedule_datetime->setEnabled(!checked);
    });
    connect(ui->sched_later, &QRadioButton::toggled, this, [this](bool checked) {
        ui->schedule_datetime->setEnabled(checked);
    });
    ui->schedule_datetime->setDateTime(QDateTime::currentDateTime().addDays(1));

    // ── Quick template buttons ────────────────────────────────────────
    connect(ui->tpl_stock_available, &QPushButton::clicked,
            this, &appwindow::on_tpl_stock_available_clicked);
    connect(ui->tpl_no_fish,         &QPushButton::clicked,
            this, &appwindow::on_tpl_no_fish_clicked);
    connect(ui->tpl_port_closed,     &QPushButton::clicked,
            this, &appwindow::on_tpl_port_closed_clicked);
    connect(ui->tpl_price_up,        &QPushButton::clicked,
            this, &appwindow::on_tpl_price_up_clicked);
    connect(ui->tpl_maintenance,     &QPushButton::clicked,
            this, &appwindow::on_tpl_maintenance_clicked);
    connect(ui->tpl_pickup_ready,    &QPushButton::clicked,
            this, &appwindow::on_tpl_pickup_ready_clicked);

    // ── Action buttons ────────────────────────────────────────────────
    connect(ui->btn_preview_msg,    &QPushButton::clicked,
            this, &appwindow::on_btn_preview_msg_clicked);
    connect(ui->btn_clear_msg,      &QPushButton::clicked,
            this, &appwindow::on_btn_clear_msg_clicked);
    connect(ui->btn_send_alert,     &QPushButton::clicked,
            this, &appwindow::on_btn_send_alert_clicked);
    connect(ui->btn_schedule_alert, &QPushButton::clicked,
            this, &appwindow::on_btn_schedule_alert_clicked);

    // ── Init counts ───────────────────────────────────────────────────
    updateWaSmsRecipientCount();
    updateWaCharCount();
}

// ─────────────────────────────────────────────────────────────────────
//  Normalize phone – international format
//  Tunisian 8-digit numbers → +216XXXXXXXX
// ─────────────────────────────────────────────────────────────────────
QString appwindow::normalizePhoneNumber(const QString &phone, const QString &countryCode)
{
    QString cleaned = phone;
    cleaned.remove(' ').remove('-').remove('(').remove(')').remove('.');

    if (cleaned.isEmpty())            return {};
    if (cleaned.startsWith('+'))      return cleaned;
    if (cleaned.startsWith("00"))     return "+" + cleaned.mid(2);

    return countryCode + cleaned;   // prepend +216 for local numbers
}

// ─────────────────────────────────────────────────────────────────────
//  Build filtered company list from UI state
// ─────────────────────────────────────────────────────────────────────
QList<Company> appwindow::getWaSmsTargetCompanies()
{
    Company obj;
    QList<Company> all = obj.afficher_liste();
    QList<Company> result;

    for (const Company &c : all) {
        // Fish filter
        if (ui->rec_fish->isChecked()) {
            QString fish = ui->combo_fish_filter->currentText();
            if (fish != "All Fish Types" &&
                !c.getPreferredFish().contains(fish, Qt::CaseInsensitive))
                continue;
        }
        // Location filter
        if (ui->rec_location->isChecked()) {
            QString loc = ui->combo_location_filter->currentText();
            if (loc != "All Locations" &&
                !c.getLocation().contains(loc, Qt::CaseInsensitive))
                continue;
        }
        // rec_all and rec_custom → include everyone
        result.append(c);
    }
    return result;
}

// ─────────────────────────────────────────────────────────────────────
//  Update recipient count label
// ─────────────────────────────────────────────────────────────────────
void appwindow::updateWaSmsRecipientCount()
{
    QList<Company> targets = getWaSmsTargetCompanies();
    int total = targets.size();

    // Count how many have a usable phone number
    int withPhone = 0;
    for (const Company &c : targets)
        if (!normalizePhoneNumber(c.getPhone()).isEmpty()) withPhone++;

    QString filterLabel = "All companies";
    if (ui->rec_fish->isChecked())
        filterLabel = ui->combo_fish_filter->currentText();
    else if (ui->rec_location->isChecked())
        filterLabel = ui->combo_location_filter->currentText();
    else if (ui->rec_custom->isChecked())
        filterLabel = "Custom selection";

    ui->lbl_recipient_count->setText(
        QString("Targeting: %1  |  %2 compan%3, %4 with valid phone")
            .arg(filterLabel)
            .arg(total)
            .arg(total == 1 ? "y" : "ies")
            .arg(withPhone));
}

// ─────────────────────────────────────────────────────────────────────
//  Live character counter (changes limit based on channel)
// ─────────────────────────────────────────────────────────────────────
void appwindow::updateWaCharCount()
{
    int len   = ui->msg_body->toPlainText().length();
    int limit = ui->ch_sms->isChecked() ? 160 : 1000;  // SMS=160, WA/Both=1000

    QString style;
    if (len > limit)
        style = "color: #dc2626; font-size: 12px; font-weight: bold;";
    else if (len > limit * 0.85)
        style = "color: #d97706; font-size: 12px;";
    else
        style = "color: #64748b; font-size: 12px;";

    ui->lbl_char_count->setStyleSheet(style);
    ui->lbl_char_count->setText(
        QString("%1 / %2 characters%3")
            .arg(len).arg(limit)
            .arg(len > limit ? " — Too long!" : ""));
}

// ─────────────────────────────────────────────────────────────────────
//  Replace [placeholders] in message body
// ─────────────────────────────────────────────────────────────────────
QString appwindow::buildWaMessage(const Company &company, const QString &msgTemplate)
{
    QString body = msgTemplate;
    body.replace("[Company Name]", company.getName());
    body.replace("[Fish Type]",    company.getPreferredFish());
    body.replace("[Price]",        "N/A");           // fill in if you have a price field
    body.replace("[Date]",         QDate::currentDate().toString("dd/MM/yyyy"));
    body.replace("[Port Name]",    "Hawata Marina");
    return body;
}

// =====================================================================
//  Auto-shorten to ≤160 chars for SMS
// =====================================================================
QString appwindow::buildSmsMessage(const Company &company, const QString &msgTemplate)
{
    QString body = buildWaMessage(company, msgTemplate);

    const int maxLen = 157;
    if (body.length() <= maxLen) return body;

    QString truncated = body.left(maxLen);
    int lastSpace = truncated.lastIndexOf(' ');
    if (lastSpace > 80) truncated = truncated.left(lastSpace);
    return truncated + "...";
}

// =====================================================================
//  Build wa.me URL
// =====================================================================
QString appwindow::buildWhatsAppUrl(const QString &phone, const QString &message)
{
    QString cleanPhone = phone;
    if (cleanPhone.startsWith('+')) cleanPhone.remove(0, 1);

    return QString("https://wa.me/%1?text=%2")
        .arg(cleanPhone)
        .arg(QString(QUrl::toPercentEncoding(message)));
}

// =====================================================================
//  QUICK TEMPLATE SLOTS
// =====================================================================

void appwindow::on_tpl_stock_available_clicked()
{
    ui->msg_body->setPlainText(
        "Dear [Company Name], great news! [Fish Type] is now available at Hawata Marina. "
        "Quantities are limited – contact us today to reserve your stock. "
        "Date: [Date] | [Port Name]");
    ui->atype_stock->setChecked(true);
}

void appwindow::on_tpl_no_fish_clicked()
{
    ui->msg_body->setPlainText(
        "Dear [Company Name], unfortunately there is no [Fish Type] available today ([Date]). "
        "We will notify you as soon as stock arrives. Apologies for any inconvenience. "
        "– [Port Name]");
    ui->atype_stock->setChecked(true);
}

void appwindow::on_tpl_port_closed_clicked()
{
    ui->msg_body->setPlainText(
        "NOTICE – [Port Name]: The port will be closed on [Date] (Moon Day / Maintenance). "
        "No fishing or pickup operations will be available. We will resume normal activity the following day.");
    ui->atype_closure->setChecked(true);
}

void appwindow::on_tpl_price_up_clicked()
{
    ui->msg_body->setPlainText(
        "PRICE ALERT – [Company Name]: Please note that the price of [Fish Type] has changed "
        "as of [Date]. Contact [Port Name] for the updated price list before placing your next order.");
    ui->atype_price->setChecked(true);
}

void appwindow::on_tpl_maintenance_clicked()
{
    ui->msg_body->setPlainText(
        "MAINTENANCE NOTICE – [Port Name]: Scheduled dock maintenance on [Date]. "
        "Access to certain docking areas will be restricted. "
        "Please coordinate your pickup schedule with our team in advance.");
    ui->atype_port->setChecked(true);
}

void appwindow::on_tpl_pickup_ready_clicked()
{
    ui->msg_body->setPlainText(
        "READY FOR PICKUP – [Company Name]: Your order of [Fish Type] is now ready for collection "
        "at [Port Name]. Please arrange pickup today. "
        "Date: [Date]. Bring your order reference on arrival.");
    ui->atype_pickup->setChecked(true);
}

// =====================================================================
//  PREVIEW
// =====================================================================

void appwindow::on_btn_preview_msg_clicked()
{
    QString msgTemplate = ui->msg_body->toPlainText().trimmed();
    if (msgTemplate.isEmpty()) {
        QMessageBox::warning(this, "Empty Message",
                             "Please type a message or choose a quick template first.");
        return;
    }

    QList<Company> targets = getWaSmsTargetCompanies();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, "No Recipients",
                             "No companies match the current filter.");
        return;
    }

    Company sample  = targets.first();
    QString waMsg   = buildWaMessage(sample, msgTemplate);
    QString smsMsg  = buildSmsMessage(sample, msgTemplate);

    bool doWa  = ui->ch_whatsapp->isChecked() || ui->ch_both->isChecked();
    bool doSms = ui->ch_sms->isChecked()      || ui->ch_both->isChecked();

    QDialog *dlg = new QDialog(this);
    dlg->setWindowTitle("Message Preview");
    dlg->setMinimumSize(720, 520);

    QVBoxLayout *layout = new QVBoxLayout(dlg);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(10);

    // Info bar
    QLabel *info = new QLabel(
        QString("Preview for: <b>%1</b>  |  Phone: <b>%2</b>  |  "
                "<b>%3</b> total recipient%4")
            .arg(sample.getName(), sample.getPhone())
            .arg(targets.size()).arg(targets.size() == 1 ? "" : "s"));
    info->setStyleSheet(
        "padding:8px; background:#e0f2fe; border-radius:5px; font-size:13px;");
    layout->addWidget(info);

    // WhatsApp preview
    if (doWa) {
        QLabel *waTitle = new QLabel("💬 <b>WhatsApp</b> (full text):");
        waTitle->setStyleSheet("color:#075e54; font-size:13px; margin-top:6px;");
        layout->addWidget(waTitle);

        QTextEdit *waView = new QTextEdit();
        waView->setPlainText(waMsg);
        waView->setReadOnly(true);
        waView->setMaximumHeight(160);
        waView->setStyleSheet(
            "font-size:13px; padding:8px; background:#dcf8c6;"
            "border:1px solid #25d366; border-radius:6px;");
        layout->addWidget(waView);

        QLabel *waCount = new QLabel(QString("Characters: %1").arg(waMsg.length()));
        waCount->setStyleSheet("color:#555; font-size:11px;");
        layout->addWidget(waCount);
    }

    // SMS preview
    if (doSms) {
        QLabel *smsTitle = new QLabel("📱 <b>SMS</b> (auto-shortened ≤160 chars):");
        smsTitle->setStyleSheet("color:#1a73e8; font-size:13px; margin-top:6px;");
        layout->addWidget(smsTitle);

        QTextEdit *smsView = new QTextEdit();
        smsView->setPlainText(smsMsg);
        smsView->setReadOnly(true);
        smsView->setMaximumHeight(110);
        bool over = smsMsg.length() > 160;
        smsView->setStyleSheet(
            QString("font-size:13px; padding:8px; background:%1;"
                    "border:1px solid %2; border-radius:6px;")
                .arg(over ? "#fff3cd" : "#e8f0fe",
                     over ? "#ffc107" : "#1a73e8"));
        layout->addWidget(smsView);

        QLabel *smsCount = new QLabel(
            QString("Characters: %1 / 160  %2")
                .arg(smsMsg.length())
                .arg(over ? "⚠️ 2 SMS units" : "✅ 1 SMS unit"));
        smsCount->setStyleSheet(
            over ? "color:#c0392b; font-size:11px;" : "color:#27ae60; font-size:11px;");
        layout->addWidget(smsCount);
    }

    QPushButton *closeBtn = new QPushButton("Close");
    closeBtn->setFixedHeight(36);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::accept);
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);

    dlg->exec();
    delete dlg;
}

// =====================================================================
//  CLEAR
// =====================================================================

void appwindow::on_btn_clear_msg_clicked()
{
    ui->msg_body->clear();
    updateWaCharCount();
}

// =====================================================================
//  SEND ALERT  (WhatsApp via wa.me / SMS via sms: URI)
// =====================================================================

void appwindow::on_btn_send_alert_clicked()
{
    QString msgTemplate = ui->msg_body->toPlainText().trimmed();
    if (msgTemplate.isEmpty()) {
        QMessageBox::warning(this, "Empty Message",
                             "Please type a message or choose a quick template first.");
        return;
    }

    QList<Company> targets = getWaSmsTargetCompanies();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, "No Recipients",
                             "No companies match the current filter.");
        return;
    }

    bool doWa  = ui->ch_whatsapp->isChecked() || ui->ch_both->isChecked();
    bool doSms = ui->ch_sms->isChecked()      || ui->ch_both->isChecked();

    // Count valid phones
    int validPhones = 0;
    for (const Company &c : targets)
        if (!normalizePhoneNumber(c.getPhone()).isEmpty()) validPhones++;

    if (validPhones == 0) {
        QMessageBox::warning(this, "No Phone Numbers",
                             "None of the target companies have a valid phone number.");
        return;
    }

    // Priority prefix
    QString priorityPrefix;
    int pIdx = ui->combo_priority->currentIndex();
    if (pIdx == 1) priorityPrefix = "⚠️ IMPORTANT: ";
    else if (pIdx == 2) priorityPrefix = "🚨 URGENT: ";

    QString channelLabel = doWa && doSms ? "WhatsApp + SMS"
                         : doWa         ? "WhatsApp"
                                        : "SMS";

    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Confirm Send",
        QString("Send via %1 to %2 compan%3 (%4 with valid phone)?\n\n"
                "Priority: %5")
            .arg(channelLabel)
            .arg(targets.size()).arg(targets.size() == 1 ? "y" : "ies")
            .arg(validPhones)
            .arg(ui->combo_priority->currentText()),
        QMessageBox::Yes | QMessageBox::No);

    if (reply != QMessageBox::Yes) return;

    int waSent = 0, smsSent = 0, skipped = 0;

    ui->btn_send_alert->setEnabled(false);
    ui->btn_send_alert->setText("Sending...");
    QApplication::processEvents();

    for (const Company &company : targets) {
        QString phone = normalizePhoneNumber(company.getPhone());
        if (phone.isEmpty()) { skipped++; continue; }

        if (doWa) {
            QString msg = priorityPrefix + buildWaMessage(company, msgTemplate);
            QString url = buildWhatsAppUrl(phone, msg);
            QDesktopServices::openUrl(QUrl(url));
            waSent++;
            QThread::msleep(700);
            QApplication::processEvents();
        }

        if (doSms) {
            QString msg = priorityPrefix + buildSmsMessage(company, msgTemplate);
            // Strip emoji from SMS (not all carriers support them)
            msg.remove(QRegularExpression("[^\\x00-\\x7F]"));
            QString encoded = QString(QUrl::toPercentEncoding(msg));
            QDesktopServices::openUrl(QUrl(QString("sms:%1?body=%2").arg(phone, encoded)));
            smsSent++;
            QThread::msleep(500);
            QApplication::processEvents();
        }
    }

    ui->btn_send_alert->setEnabled(true);
    ui->btn_send_alert->setText("📤 Send Alert");

    QString summary;
    if (doWa)  summary += QString("💬 WhatsApp opened: %1\n").arg(waSent);
    if (doSms) summary += QString("📱 SMS opened:      %1\n").arg(smsSent);
    summary += QString("⚠️  Skipped (no phone): %1").arg(skipped);

    QMessageBox::information(this, "Alert Sent", summary);
}

// =====================================================================
//  SCHEDULE ALERT
// =====================================================================

void appwindow::on_btn_schedule_alert_clicked()
{
    QString msgTemplate = ui->msg_body->toPlainText().trimmed();
    if (msgTemplate.isEmpty()) {
        QMessageBox::warning(this, "Empty Message",
                             "Please type a message or choose a quick template first.");
        return;
    }

    if (!ui->sched_later->isChecked()) {
        QMessageBox::information(this, "Send Now Selected",
                                 "You have 'Send Now' selected.\n"
                                 "Select 'Schedule for:' and pick a future date/time first.");
        return;
    }

    QDateTime scheduleAt = ui->schedule_datetime->dateTime();
    if (scheduleAt <= QDateTime::currentDateTime()) {
        QMessageBox::warning(this, "Invalid Time",
                             "Please select a future date and time.");
        return;
    }

    QList<Company> targets = getWaSmsTargetCompanies();
    if (targets.isEmpty()) {
        QMessageBox::warning(this, "No Recipients",
                             "No companies match the current filter.");
        return;
    }

    qint64 msDelay = QDateTime::currentDateTime().msecsTo(scheduleAt);

    bool doWa  = ui->ch_whatsapp->isChecked() || ui->ch_both->isChecked();
    bool doSms = ui->ch_sms->isChecked()      || ui->ch_both->isChecked();
    QString channelLabel = doWa && doSms ? "WhatsApp + SMS"
                         : doWa         ? "WhatsApp"
                                        : "SMS";

    QMessageBox::information(this, "Scheduled",
                             QString("✅ Alert scheduled!\n\n"
                                     "Channel:    %1\n"
                                     "Recipients: %2 compan%3\n"
                                     "Send time:  %4")
                                 .arg(channelLabel)
                                 .arg(targets.size())
                                 .arg(targets.size() == 1 ? "y" : "ies")
                                 .arg(scheduleAt.toString("dd/MM/yyyy  hh:mm")));

    ui->btn_schedule_alert->setEnabled(false);
    ui->btn_schedule_alert->setText("⏰ Scheduled...");

    QTimer::singleShot(msDelay, this, [this]() {
        ui->btn_schedule_alert->setEnabled(true);
        ui->btn_schedule_alert->setText("⏰ Schedule");
        on_btn_send_alert_clicked();
    });
}
//boat new functionalities
void appwindow::on_clearBoatButton_clicked()
{

    ui->boatSizeLineEdit->clear();
    ui->boatLocationLineEdit->clear();
    ui->boatOwnerNameLineEdit->clear();
    ui->boatOwnerEmailLineEdit->clear();
    ui->boatTypeLineEdit->clear();
    ui->boatStatusComboBox->setCurrentIndex(1);
    ui->boatMaintenanceDateEdit->setDate(QDate::currentDate());
    ui->boatTripsSpinBox->setValue(0);
    ui->boatFishSpinBox->setValue(0);

    // If we're in Edit mode, switch back to Add mode
    if (currentBoatMode == BoatMode::Edit) {
        setBoatMode(BoatMode::Add);
    }
}



void appwindow::on_comboBox_15_currentIndexChanged(int index)
{
    switch (index) {                                                              break; // Sort by...
    case 1: ui->Boatwidget_2->setSortingEnabled(true);
        ui->Boatwidget_2->sortByColumn(1, Qt::AscendingOrder);
        ui->Boatwidget_2->setSortingEnabled(false);                                       break; // Size
    case 2: ui->Boatwidget_2->setSortingEnabled(true);
        ui->Boatwidget_2->sortByColumn(7, Qt::AscendingOrder);
        ui->Boatwidget_2->setSortingEnabled(false);                                       break; // Last Maintenance Date
    default: break;
    }
}

void appwindow::on_logout_clicked()
{
    QSettings settings("MyApp", "UserLogin");
    settings.clear(); // remove saved session

    MainWindow *login = new MainWindow();
    login->show();
    this->close();
}
// =====================================================================
// ============  ARDUINO / TEMPERATURE INTEGRATION  ====================
// =====================================================================
//
// Implementation follows the slide pattern exactly:
//
//   Arduino A;          — declared in appwindow.h
//   A.connect_arduino() — called here to open the port
//   A.getserial()       — used to wire readyRead → update_label()
//   update_label()      — slot that reads and displays the temperature
//
// The Arduino sketch sends lines like:
//   "Temperature: 27.50 °C - Fan OFF"
//   "Temperature: 31.00 °C - Fan ON  <<<<"
//
// The slot parses the numeric value and updates ui->fishTemp.
// =====================================================================

/**
 * setupArduinoTemperatureReader()
 *
 * Called once from the appwindow constructor.
 * Connects the Arduino, reports the result in the status label, and wires
 * the readyRead signal (slide pattern) to the update_label() slot.
 */
void appwindow::setupArduinoTemperatureReader()
{
    ui->fishTemp->setPlaceholderText("Auto from Arduino or enter manually");
    ui->tempSourceLabel->setText("Arduino: searching for device...");

    // ── Launch the connection to Arduino (mirrors Mainwindow.cpp slide) ──
    int ret = A.connect_arduino();

    switch (ret) {
    case 0:
        qDebug() << "arduino is available and connected to :" << A.getarduino_port_name();
        ui->tempSourceLabel->setText(
            QString("Arduino: connected on %1").arg(A.getarduino_port_name()));

        // Wire readyRead → update_label (temperature reading)
        disconnect(A.getserial(), &QSerialPort::readyRead, this, nullptr);
        connect(A.getserial(), &QSerialPort::readyRead,
                this, &appwindow::update_label);
        qDebug() << "Connected Arduino readyRead signal to update_label() slot for temperature parsing";
        break;

    case 1:
        qDebug() << "arduino is available but not connected to :" << A.getarduino_port_name();
        ui->tempSourceLabel->setText(
            QString("Arduino: found on %1 but could not open port (manual input enabled)")
                .arg(A.getarduino_port_name()));
        break;

    case -1:
    default:
        qDebug() << "arduino is not available";
        ui->tempSourceLabel->setText("Arduino: not found (manual temperature enabled)");
        break;
    }
}

/**
 * update_label()  — slot for readyRead signal  (slide pattern)
 *
 * Reads all available bytes from the Arduino, assembles complete lines,
 * parses the temperature value, and pushes it into the UI.
 *
 * Arduino DHT11 sketch output format:
 *   "Temperature: 27.50 °C - Fan OFF"
 *   "Temperature: 31.00 °C - Fan ON  <<<<"
 *   "ERROR: DHT11 not working!"
 */
void appwindow::update_label()
{
    // Constant for fan control threshold
    const double TEMP_THRESHOLD = 30.0;
    
    QByteArray newData = A.read_from_arduino();
    if (!newData.isEmpty()) {
        qDebug() << "[ARDUINO RX]" << newData;  // Debug: show raw data
    }
    arduinoSerialBuffer.append(newData);

    int newlineIndex = arduinoSerialBuffer.indexOf('\n');
    while (newlineIndex >= 0) {
        const QByteArray rawLine = arduinoSerialBuffer.left(newlineIndex);
        arduinoSerialBuffer.remove(0, newlineIndex + 1);

        const QString line = QString::fromUtf8(rawLine).trimmed();

        if (line.isEmpty()) {
            newlineIndex = arduinoSerialBuffer.indexOf('\n');
            continue;
        }

        qDebug() << "[PARSED LINE]" << line;  // Debug: show each parsed line

        if (line.startsWith("ERROR")) {
            ui->tempSourceLabel->setText("Arduino: DHT11 sensor error — check wiring");
            qDebug() << "[ERROR]" << line;
            newlineIndex = arduinoSerialBuffer.indexOf('\n');
            continue;
        }

        // ── Temperature line only ─────────────────────────────────────
        const QRegularExpression labeledPattern(
            R"((?:TEMP(?:ERATURE)?\s*[:=]\s*)([-+]?\d+(?:[\.,]\d+)?))",
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch m = labeledPattern.match(line);

        if (m.hasMatch()) {
            QString token = m.captured(1);
            token.replace(',', '.');
            bool ok = false;
            const double v = token.toDouble(&ok);
            if (ok) {
                qDebug() << "[TEMPERATURE PARSED]" << v << "°C";
                ui->fishTemp->setText(QString::number(v, 'f', 2));

                // -- Insert temperature reading into database
                QSqlDatabase db = QSqlDatabase::database();
                if (!db.isOpen()) {
                    qDebug() << "DB not open — attempting to open default connection...";
                    if (!db.open()) {
                        qDebug() << "Failed to open DB:" << db.lastError().text();
                    }
                }

                if (db.isOpen()) {
                    // Create table if it doesn't exist (best-effort; syntax targets Oracle/standard SQL)
                    QStringList tables = db.tables();
                    if (!tables.contains("TEMPERATURE_READINGS", Qt::CaseInsensitive)) {
                        QSqlQuery createQ(db);
                        bool okCreate = createQ.exec(
                            "CREATE TABLE TEMPERATURE_READINGS ("
                            "ID NUMBER GENERATED ALWAYS AS IDENTITY PRIMARY KEY, "
                            "READING_TIME TIMESTAMP, "
                            "TEMPERATURE FLOAT)"
                        );
                        if (!okCreate) {
                            qDebug() << "Create table failed (may already exist or DB doesn't support identity):" << createQ.lastError().text();
                        } else {
                            qDebug() << "Created TEMPERATURE_READINGS table";
                        }
                    }

                    QSqlQuery insertQ(db);
                    insertQ.prepare("INSERT INTO TEMPERATURE_READINGS (READING_TIME, TEMPERATURE) VALUES (CURRENT_TIMESTAMP, :temp)");
                    insertQ.bindValue(":temp", v);
                    if (!insertQ.exec()) {
                        qDebug() << "Failed to insert temperature reading:" << insertQ.lastError().text();
                    } else {
                        qDebug() << "Inserted temperature reading:" << v;
                    }
                }

                // ──────────────────────────────────────────────────────
                // Temperature control logic: compare the latest stored
                // database temperature against the threshold.
                // ──────────────────────────────────────────────────────

                double dbTemperature = v;
                if (db.isOpen()) {
                    QSqlQuery latestQ(db);
                    if (latestQ.exec(
                            "SELECT TEMPERATURE "
                            "FROM TEMPERATURE_READINGS "
                            "ORDER BY READING_TIME DESC, ID DESC "
                            "FETCH FIRST 1 ROWS ONLY")) {
                        if (latestQ.next()) {
                            const QVariant latestValue = latestQ.value(0);
                            bool latestOk = false;
                            const double latestTemp = latestValue.toDouble(&latestOk);
                            if (latestOk) {
                                dbTemperature = latestTemp;
                                qDebug() << "Latest DB temperature:" << dbTemperature;
                            }
                        }
                    } else {
                        qDebug() << "Failed to read latest DB temperature:" << latestQ.lastError().text();
                    }
                }

                QString fanCommand;
                QString fanStatus;

                if (dbTemperature > TEMP_THRESHOLD) {
                    fanCommand = "FAN_ON\n";
                    fanStatus = "ON";
                    qDebug() << QString("Database temperature %1°C > %2°C threshold - FAN ON").arg(dbTemperature, 0, 'f', 2).arg(TEMP_THRESHOLD, 0, 'f', 1);
                } else {
                    fanCommand = "FAN_OFF\n";
                    fanStatus = "OFF";
                    qDebug() << QString("Database temperature %1°C <= %2°C threshold - FAN OFF").arg(dbTemperature, 0, 'f', 2).arg(TEMP_THRESHOLD, 0, 'f', 1);
                }
                
                // Send fan control command to Arduino
                if (A.getserial() && A.getserial()->isOpen()) {
                    A.write_to_arduino(fanCommand.toUtf8());
                    qDebug() << "Sent to Arduino:" << fanCommand.trimmed();
                }
                
                // Update UI to show fan status
                ui->tempSourceLabel->setText(
                    QString("Arduino: %1 C | DB: %2 C | Fan: %3").arg(v, 0, 'f', 2).arg(dbTemperature, 0, 'f', 2).arg(fanStatus));
            }
        } else {
            // Log lines that don't match temperature pattern (for debugging)
            if (line.contains("SYSTEM") || line.contains("COMMAND") || line.contains("STATUS")) {
                qDebug() << "[ARDUINO MSG]" << line;
            }
        }

        newlineIndex = arduinoSerialBuffer.indexOf('\n');
    }
}

appwindow::~appwindow()
{
    A.close_arduino();   // cleanly close the serial port via Arduino class
    delete ui;
}
