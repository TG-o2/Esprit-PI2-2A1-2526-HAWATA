#include "chatbotdialog.h"
#include "ui_chatbotdialog.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QStringList>

#include <QKeyEvent>
#include <QMouseEvent>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QRegularExpression>
#include <QVariant>
#include <QTimer>
#include <QScrollBar>
#include <QApplication>

#include <QDate>
#include <QDateTime>
#include <cmath>
#include <limits>

namespace {
struct NamedPoint {
    const char *name;
    double lat;
    double lon;
};

static const NamedPoint kTunisiaSeaPoints[] = {
    {"Tunis", 36.900000, 10.380000},
    {"La Goulette", 36.819000, 10.305000},
    {"La Marsa", 36.878000, 10.325000},
    {"Sidi Bou Said", 36.870000, 10.345000},
    {"Rades", 36.770000, 10.290000},
    {"Bizerte", 37.315000, 9.930000},
    {"Tabarka", 36.995000, 8.760000},
    {"Kelibia", 36.865000, 11.120000},
    {"Nabeul", 36.485000, 10.790000},
    {"Hammamet", 36.340000, 10.590000},
    {"Sousse", 35.925000, 10.700000},
    {"Monastir", 35.790000, 10.850000},
    {"Mahdia", 35.535000, 11.070000},
    {"Sfax", 34.745000, 10.780000},
    {"Kerkennah", 34.700000, 11.200000},
    {"Gabes", 33.980000, 10.300000},
    {"Djerba", 33.930000, 10.920000},
    {"Houmt Souk", 33.880000, 10.860000},
    {"Zarzis", 33.520000, 11.130000},
    {"Esprit College", 36.845000, 10.190000},
    {"Malles", 36.840000, 10.170000}
};

bool convertDdmmToDecimal(const QString &ddmm, double &decimalOut)
{
    QString normalized = ddmm.trimmed();
    normalized.replace(',', '.');

    bool ok = false;
    const double value = normalized.toDouble(&ok);
    if (!ok || value < 0) return false;

    const int dotPos = normalized.indexOf('.');
    const int minutesStart = (dotPos < 0) ? normalized.length() - 2 : dotPos - 2;
    if (minutesStart <= 0) return false;

    bool degOk = false;
    bool minOk = false;
    const int degrees = normalized.left(minutesStart).toInt(&degOk);
    const double minutes = normalized.mid(minutesStart).toDouble(&minOk);
    if (!degOk || !minOk || minutes < 0.0 || minutes >= 60.0) return false;

    decimalOut = static_cast<double>(degrees) + (minutes / 60.0);
    return true;
}

bool parseLocationToLatLon(const QString &location, double &lat, double &lon)
{
    const QString trimmed = location.trimmed();
    if (trimmed.isEmpty()) return false;

    const QRegularExpression pairPattern(R"((\d+\.?\d*)\s*[,\s]\s*(\d+\.?\d*))");
    const QRegularExpressionMatch m = pairPattern.match(trimmed);
    if (!m.hasMatch()) return false;

    QString first = m.captured(1);
    QString second = m.captured(2);
    first.replace(',', '.');
    second.replace(',', '.');

    double latDdmm = 0.0;
    double lonDdmm = 0.0;
    if (convertDdmmToDecimal(first, latDdmm) && convertDdmmToDecimal(second, lonDdmm)) {
        lat = latDdmm;
        lon = lonDdmm;
        return true;
    }

    bool ok1 = false;
    bool ok2 = false;
    const double v1 = first.toDouble(&ok1);
    const double v2 = second.toDouble(&ok2);
    if (!ok1 || !ok2) return false;

    auto validLatLon = [](double a, double b) {
        return a >= -90.0 && a <= 90.0 && b >= -180.0 && b <= 180.0;
    };

    if (validLatLon(v1, v2)) {
        lat = v1;
        lon = v2;
        return true;
    }

    if (validLatLon(v2, v1)) {
        lat = v2;
        lon = v1;
        return true;
    }

    return false;
}

QString resolveLocationName(const QString &rawLocation)
{
    const QString trimmed = rawLocation.trimmed();
    if (trimmed.isEmpty()) return "Unknown area";

    const bool containsLetter = trimmed.contains(QRegularExpression("[A-Za-z]"));
    if (containsLetter) return trimmed;

    double lat = 0.0;
    double lon = 0.0;
    if (!parseLocationToLatLon(trimmed, lat, lon)) return trimmed;

    QString bestName = "Tunis";
    double bestDistance = std::numeric_limits<double>::max();

    for (const NamedPoint &p : kTunisiaSeaPoints) {
        const double dLat = lat - p.lat;
        const double dLon = lon - p.lon;
        const double dist = (dLat * dLat) + (dLon * dLon);
        if (dist < bestDistance) {
            bestDistance = dist;
            bestName = QString::fromLatin1(p.name);
        }
    }

    return bestName;
}

QDate parseProductDateForChat(const QString &value)
{
    const QString trimmed = value.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    const QStringList dateFormats = {
        "yyyy-MM-dd",
        "yyyy-MM-ddThh:mm:ss",
        "yyyy-MM-dd hh:mm:ss",
        "yyyy-MM-ddTHH:mm:ss",
        "yyyy-MM-dd HH:mm:ss",
        "dd/MM/yyyy",
        "dd-MM-yyyy"
    };

    for (const QString &format : dateFormats) {
        QDateTime dt = QDateTime::fromString(trimmed, format);
        if (dt.isValid()) return dt.date();

        QDate d = QDate::fromString(trimmed, format);
        if (d.isValid()) return d;
    }

    const QDateTime iso = QDateTime::fromString(trimmed, Qt::ISODate);
    if (iso.isValid()) return iso.date();

    return {};
}

double discountedPriceForChat(double originalPrice, const QString &purchaseDateText)
{
    const QDate purchaseDate = parseProductDateForChat(purchaseDateText);
    if (!purchaseDate.isValid()) {
        return originalPrice;
    }

    const int ageDays = purchaseDate.daysTo(QDate::currentDate());
    if (ageDays >= 30) return originalPrice * 0.70;
    if (ageDays >= 20) return originalPrice * 0.80;
    if (ageDays >= 10) return originalPrice * 0.90;
    return originalPrice;
}
}

// ─────────────────────────────────────────────
//  Constructor / Destructor
// ─────────────────────────────────────────────
ChatbotDialog::ChatbotDialog(QWidget *parent, int currentUserId, const QString &currentUserRole)
    : QDialog(parent)
    , ui(new Ui::chatbotdialog)
    , m_currentUserId(currentUserId)
    , m_currentUserRole(currentUserRole)
{
    ui->setupUi(this);

    setWindowFlags(Qt::FramelessWindowHint | Qt::Dialog | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);

    if (parent) {
        QRect parentRect = parent->geometry();
        int x = parentRect.x() + (parentRect.width()  - width())  / 2;
        int y = parentRect.y() + (parentRect.height() - height()) / 2;
        move(x, y);
    }

    connect(ui->closeChatBtn, &QPushButton::clicked, this, &ChatbotDialog::hide);
    connect(ui->minimizeBtn,  &QPushButton::clicked, this, &ChatbotDialog::hide);
    connect(ui->sendBtn,      &QPushButton::clicked, this, &ChatbotDialog::onSendClicked);

    ui->messageInput->installEventFilter(this);

    connect(ui->chip1, &QPushButton::clicked, this, &ChatbotDialog::onChip1Clicked);
    connect(ui->chip2, &QPushButton::clicked, this, &ChatbotDialog::onChip2Clicked);
    connect(ui->chip3, &QPushButton::clicked, this, &ChatbotDialog::onChip3Clicked);

    addMessage("Hello! I'm your Harbor Assistant ⚓\n"
               "Ask me about docks, boats, fish stock, users, or companies.\n"
               "Try: \"low stock\", \"available docks\", \"boats at sea\", \"active companies\"", false);
}

ChatbotDialog::~ChatbotDialog()
{
    delete ui;
}

// ─────────────────────────────────────────────
//  Drag to move
// ─────────────────────────────────────────────
void ChatbotDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && event->pos().y() <= 60) {
        m_dragging   = true;
        m_dragOffset = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QDialog::mousePressEvent(event);
}

void ChatbotDialog::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
        move(event->globalPosition().toPoint() - m_dragOffset);
    QDialog::mouseMoveEvent(event);
}

void ChatbotDialog::mouseReleaseEvent(QMouseEvent *event)
{
    m_dragging = false;
    QDialog::mouseReleaseEvent(event);
}

// ─────────────────────────────────────────────
//  Enter key support
// ─────────────────────────────────────────────
bool ChatbotDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == ui->messageInput && event->type() == QEvent::KeyPress) {
        QKeyEvent *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            onSendClicked();
            return true;
        }
    }
    return QDialog::eventFilter(obj, event);
}

// ─────────────────────────────────────────────
//   Call Ollama ai
// ─────────────────────────────────────────────
void ChatbotDialog::callOllama(const QString &input)
{
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);

    // Put model in query string (common Ollama pattern) and send JSON body
    QUrl url("http://localhost:11434/api/generate");
    QUrlQuery q;
    q.addQueryItem("model", "llama3");
    url.setQuery(q);

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    // Provide clear system/context and the user message
    QString prompt =
        "You are a helpful harbor assistant for a fishing port system.\n"
        "Answer concisely and use friendly emoji when helpful.\n"
        "You have access to information about boats, docks, fish stock, users, and companies.\n\n";
    prompt += QString("Current session role: %1\n").arg(m_currentUserRole.isEmpty() ? "unknown" : m_currentUserRole);
    prompt += QString("Current session user ID: %1\n\n").arg(m_currentUserId);
    prompt += QString("User: %1\nAssistant:").arg(input);

    json["prompt"] = prompt;
    json["stream"] = false;
    json["temperature"] = 0.2;
    json["max_tokens"] = 512;

    // Try a sequence of common Ollama/completion endpoints if one returns 404
    QStringList endpoints = {
        "http://localhost:11434/api/generate?model=llama3",
        "http://localhost:11434/api/generate",
        "http://localhost:11434/api/completions",
        "http://localhost:11434/v1/completions"
    };

    std::function<void(int)> doRequest;
    doRequest = [=](int idx) mutable {
        if (idx >= endpoints.size()) {
            addMessage("Error: All endpoints tried and returned errors.", false);
            return;
        }

        QUrl u(endpoints[idx]);
        QNetworkRequest r(u);
        r.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

        // include model in body too for endpoints that expect it there
        QJsonObject body = json;
        body["model"] = "llama3";

        QNetworkReply *rep = manager->post(r, QJsonDocument(body).toJson());

        connect(rep, &QNetworkReply::errorOccurred, this, [=](QNetworkReply::NetworkError){
            QString err = QString("Network error: %1").arg(rep->errorString());
            addMessage(err, false);
            rep->deleteLater();
        });

        connect(rep, &QNetworkReply::finished, this, [=]() mutable {
            int status = rep->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            QByteArray responseData = rep->readAll();

            if (status >= 200 && status < 300) {
                // parse success
                QString aiResponse;

                QJsonParseError parseError;
                QJsonDocument doc = QJsonDocument::fromJson(responseData, &parseError);

                if (parseError.error == QJsonParseError::NoError) {
                    if (doc.isObject()) {
                        QJsonObject obj = doc.object();

                        if (obj.contains("response") && obj["response"].isString())
                            aiResponse = obj["response"].toString();
                        else if (obj.contains("text") && obj["text"].isString())
                            aiResponse = obj["text"].toString();
                        else if (obj.contains("output") && obj["output"].isString())
                            aiResponse = obj["output"].toString();
                        else if (obj.contains("choices") && obj["choices"].isArray()) {
                            QJsonArray choices = obj["choices"].toArray();
                            if (!choices.isEmpty() && choices[0].isObject()) {
                                QJsonObject c0 = choices[0].toObject();
                                if (c0.contains("text")) aiResponse = c0["text"].toString();
                                else if (c0.contains("message") && c0["message"].isObject()) {
                                    QJsonObject msg = c0["message"].toObject();
                                    if (msg.contains("content")) aiResponse = msg["content"].toString();
                                }
                            }
                        } else if (obj.contains("results") && obj["results"].isArray()) {
                            QJsonArray res = obj["results"].toArray();
                            QStringList parts;
                            for (auto v : res) {
                                if (v.isObject()) {
                                    QJsonObject r = v.toObject();
                                    if (r.contains("content") && r["content"].isArray()) {
                                        QJsonArray cont = r["content"].toArray();
                                        for (auto c : cont) if (c.isObject()) {
                                            QJsonObject co = c.toObject();
                                            if (co.contains("text")) parts << co["text"].toString();
                                        }
                                    }
                                }
                            }
                            aiResponse = parts.join("\n");
                        }
                    } else if (doc.isArray()) {
                        QJsonArray arr = doc.array();
                        QStringList parts;
                        for (auto v : arr) {
                            if (v.isString()) parts << v.toString();
                            else if (v.isObject()) {
                                QJsonObject o = v.toObject();
                                if (o.contains("text")) parts << o["text"].toString();
                            }
                        }
                        aiResponse = parts.join("\n");
                    }
                }

                if (aiResponse.trimmed().isEmpty()) {
                    aiResponse = QString::fromUtf8(responseData).trimmed();
                    if (aiResponse.isEmpty()) aiResponse = "(No response from Ollama)";
                }

                addMessage(aiResponse, false);
                rep->deleteLater();
            } else if (status == 404) {
                // try next endpoint
                rep->deleteLater();
                doRequest(idx + 1);
            } else {
                // final error
                QString body = QString::fromUtf8(responseData).trimmed();
                QString msg = QString("HTTP %1: %2").arg(status).arg(body);
                addMessage(msg, false);
                rep->deleteLater();
            }
        });
    };

    doRequest(0);
}



// ─────────────────────────────────────────────
//  Send / Chip handlers
// ─────────────────────────────────────────────
void ChatbotDialog::onSendClicked()
{
    QString text = ui->messageInput->text().trimmed();
    if (text.isEmpty()) return;
    ui->messageInput->clear();
    addMessage(text, true);

    QString reply = processQuery(text.toLower());

    if (reply.contains("I'm not sure")) {
        // fallback to AI
        callOllama(text);
    } else {
        addMessage(reply, false);
    }
}

void ChatbotDialog::onChip1Clicked() {
    addMessage("🐟 Stock levels", true);
    addMessage(queryStock("summary"), false);
}
void ChatbotDialog::onChip2Clicked() {
    addMessage("⚓ Dock status", true);
    addMessage(queryDocks("summary"), false);
}
void ChatbotDialog::onChip3Clicked() {
    addMessage("🚢 Active boats", true);
    addMessage(queryBoats("summary"), false);
}

// ─────────────────────────────────────────────
//  Core keyword dispatcher
// ─────────────────────────────────────────────
QString ChatbotDialog::processQuery(const QString &input)
{
    auto hasWord = [&input](const QString &word) {
        const QRegularExpression re(QString("\\b%1\\b").arg(QRegularExpression::escape(word)));
        return re.match(input).hasMatch();
    };

    if (input.contains("who am i") || input.contains("what type of user") ||
        input.contains("my role") || input.contains("what am i") ||
        input.contains("am i")) {
        if (!m_currentUserRole.trimmed().isEmpty()) {
            return QString("You are logged in as %1 user #%2.")
                .arg(m_currentUserRole, QString::number(m_currentUserId));
        }

        return "I don't have the current session role yet. Please log in again so I can identify your account.";
    }

    // ── Greetings ────────────────────────────────────────
    if (hasWord("hello") || hasWord("hi") ||
        hasWord("hey")   || hasWord("salut") ||
        hasWord("bonjour"))
        return "Hello! 👋 How can I help you today?\n"
               "Ask me about stock, docks, boats, users, or companies.";

    if (input.contains("help") || input.contains("what can you"))
        return "I can answer questions about:\n"
               "🐟 Fish stock & products\n"
               "⚓ Dock availability\n"
               "🚢 Boat status & maintenance\n"
               "👤 Users & employees\n"
               "🏢 Companies\n"
               "📊 General summary\n\n"
               "Just ask naturally, e.g. \"how many boats are at sea?\"";

    // ── Summary ──────────────────────────────────────────
    if (input.contains("summary") || input.contains("overview") ||
        input.contains("dashboard") || input.contains("all"))
        return querySummary();

    // ── List / Dump tables (admin) ─────────────────────────
    if (input.contains("list tables") || input.contains("show tables") ||
        (input.contains("tables") && input.contains("show"))) {
        return queryListTables();
    }

    if (input.contains("dump table") || input.contains("show table") || input.contains("read table")) {
        // simple extraction: take the token after "table"
        QStringList parts = input.split(QRegularExpression("\\s+"));
        int idx = parts.indexOf("table");
        if (idx >= 0 && idx + 1 < parts.size()) {
            QString tbl = parts[idx + 1];
            // strip punctuation
            tbl.remove(QRegularExpression("[^A-Za-z0-9_\.:]") );
            if (!tbl.isEmpty()) return dumpTable(tbl, 12);
        }
        return "Usage: 'dump table <TABLE_NAME>' — e.g. 'dump table SMARTFISH.USERS'";
    }

    // ── Stock / Products ─────────────────────────────────
    if (input.contains("stock")   || input.contains("fish")    ||
        input.contains("product") || input.contains("tuna")    ||
        input.contains("sardine") || input.contains("low")     ||
        input.contains("quantity")|| input.contains("price")   ||
        input.contains("inventory") || input.contains("discount") ||
        input.contains("temperature") || input.contains("tempreture") ||
        input.contains("hottest") || input.contains("coldest") ||
        input.contains("hot") || input.contains("cold"))
        return queryStock(input);

    // ── Docks ────────────────────────────────────────────
    if (input.contains("dock")    || input.contains("port")    ||
        input.contains("berth")   || input.contains("slip")    ||
        input.contains("available")|| input.contains("occupied") ||
        input.contains("capacity"))
        return queryDocks(input);

    // ── Boats ────────────────────────────────────────────
    if (input.contains("boat")    || input.contains("vessel")  ||
        input.contains("ship")    || input.contains("sea")     ||
        input.contains("sailing") || input.contains("maintenance") ||
        input.contains("repair")  || input.contains("trip")    ||
        input.contains("docked"))
        return queryBoats(input);

    // ── Users ────────────────────────────────────────────
    if (input.contains("user")    || input.contains("employee") ||
        input.contains("staff")   || input.contains("admin")    ||
        input.contains("manager") || input.contains("salary")   ||
        input.contains("shift")   || input.contains("worker"))
        return queryUsers(input);

    // ── Companies ────────────────────────────────────────
    if (input.contains("compan")  || input.contains("client")  ||
        input.contains("customer")|| input.contains("partner")  ||
        input.contains("active")  || input.contains("inactive") ||
        input.contains("carrefour"))
        return queryCompanies(input);

    // ── Fallback ─────────────────────────────────────────
    return "🤔 I'm not sure what you mean. Try asking about:\n"
           "• \"low stock products\"\n"
           "• \"available docks\"\n"
           "• \"boats at sea\"\n"
           "• \"how many users\"\n"
           "• \"inactive companies\"\n"
           "• \"summary\"";
}

QString ChatbotDialog::queryListTables()
{
    if (!QSqlDatabase::database().isOpen()) return "Database not connected.";
    QStringList tables = QSqlDatabase::database().tables();
    if (tables.isEmpty()) return "No tables found in the current connection.";
    QString res = QString("Found %1 tables:\n").arg(tables.size());
    int c = 0;
    for (const QString &t : tables) {
        res += QString("• %1\n").arg(t);
        if (++c >= 40) { res += "...and more\n"; break; }
    }
    return res.trimmed();
}

QString ChatbotDialog::actualTable(const QString &baseName) const
{
    QSqlDatabase db = QSqlDatabase::database();
    QStringList tables = db.tables();

    // Exact match
    for (const QString &t : tables) if (QString::compare(t, baseName, Qt::CaseInsensitive) == 0) return t;

    // With SMARTFISH prefix
    QString pref = QString("SMARTFISH.%1").arg(baseName);
    for (const QString &t : tables) if (QString::compare(t, pref, Qt::CaseInsensitive) == 0) return t;

    // Table names that end with .BASE
    for (const QString &t : tables) {
        if (t.endsWith("." + baseName, Qt::CaseInsensitive)) return t;
    }

    // not found, return baseName (best-effort)
    return baseName;
}

QString ChatbotDialog::dumpTable(const QString &tableName, int limit)
{
    if (!QSqlDatabase::database().isOpen()) return "Database not connected.";

    // Basic safety: allow only alnum, underscore, dot, and optionally schema qualifier
    if (tableName.contains(QRegularExpression("[^A-Za-z0-9_\.:]")))
        return "Invalid table name.";

    // Resolve actual table name if user provided short name
    QString resolved = tableName;
    if (!tableName.contains('.')) resolved = actualTable(tableName);

    QString qsql = QString("SELECT * FROM %1 FETCH FIRST %2 ROWS ONLY").arg(resolved).arg(limit);
    QSqlQuery q;
    if (!q.exec(qsql)) {
        // Try MySQL-style LIMIT as a fallback
        qsql = QString("SELECT * FROM %1 LIMIT %2").arg(tableName).arg(limit);
        if (!q.exec(qsql)) {
            return QString("Error querying %1: %2").arg(tableName, q.lastError().text());
        }
    }

    // Build header
    QStringList cols;
    QSqlRecord r = q.record();
    for (int i = 0; i < r.count(); ++i) cols << r.fieldName(i);

    if (cols.isEmpty()) return QString("%1: no columns or empty table.").arg(tableName);

    QString out = QString("%1 — columns: %2\n").arg(tableName).arg(cols.join(", "));
    int rows = 0;
    while (q.next() && rows < limit) {
        QStringList vals;
        for (int i = 0; i < r.count(); ++i) {
            QVariant v = q.value(i);
            QString vs = v.isNull() ? "(null)" : v.toString();
            if (vs.length() > 80) vs = vs.left(77) + "...";
            vals << vs;
        }
        out += QString("%1) %2\n").arg(rows + 1).arg(vals.join(" | "));
        rows++;
    }
    if (rows == 0) out += "(no rows returned)";
    return out.trimmed();
}

// ─────────────────────────────────────────────
//  Summary — all tables at a glance
// ─────────────────────────────────────────────
QString ChatbotDialog::querySummary()
{
    QString result = "📊 Harbor Summary\n";
    result += "─────────────────\n";


    QSqlQuery q;

    // Resolve table names according to the connected DB
    QString productsTable = actualTable("PRODUCTS");
    QString dockingTable  = actualTable("DOCKING");
    QString boatTable     = actualTable("BOAT");
    QString usersTable    = actualTable("USERS");
    QString companiesTable= actualTable("COMPANIES");

    // Products
    q.exec(QString("SELECT COUNT(*) FROM %1").arg(productsTable));
    int prodTotal = q.next() ? q.value(0).toInt() : 0;
    q.exec(QString("SELECT COUNT(*) FROM %1 WHERE QUANTITY < 30").arg(productsTable));
    int prodLow = q.next() ? q.value(0).toInt() : 0;
    result += QString("🐟 Products: %1 total, %2 low stock\n").arg(prodTotal).arg(prodLow);

    // Docks
    q.exec(QString("SELECT COUNT(*) FROM %1").arg(dockingTable));
    int dockTotal = q.next() ? q.value(0).toInt() : 0;
    q.exec(QString("SELECT COUNT(*) FROM %1 WHERE STATUS = 'Available'").arg(dockingTable));
    int dockFree = q.next() ? q.value(0).toInt() : 0;
    result += QString("⚓ Docks: %1 total, %2 available\n").arg(dockTotal).arg(dockFree);

    // Boats
    q.exec(QString("SELECT COUNT(*) FROM %1").arg(boatTable));
    int boatTotal = q.next() ? q.value(0).toInt() : 0;
    q.exec(QString("SELECT COUNT(*) FROM %1 WHERE STATUS = 0").arg(boatTable));
    int boatSea = q.next() ? q.value(0).toInt() : 0;
    result += QString("🚢 Boats: %1 total, %2 at sea\n").arg(boatTotal).arg(boatSea);

    // Users
    q.exec(QString("SELECT COUNT(*) FROM %1").arg(usersTable));
    int userTotal = q.next() ? q.value(0).toInt() : 0;
    result += QString("👤 Users: %1\n").arg(userTotal);

    // Companies
    q.exec(QString("SELECT COUNT(*) FROM %1").arg(companiesTable));
    int compTotal = q.next() ? q.value(0).toInt() : 0;
    q.exec(QString("SELECT COUNT(*) FROM %1 WHERE STATUS = 'ACTIVE'").arg(companiesTable));
    int compActive = q.next() ? q.value(0).toInt() : 0;
    result += QString("🏢 Companies: %1 total, %2 active\n").arg(compTotal).arg(compActive);

    return result.trimmed();
}

// ─────────────────────────────────────────────
//  Stock / Products
// ─────────────────────────────────────────────
QString ChatbotDialog::queryStock(const QString &input)
{
    QSqlQuery q;
    QString productsTable = actualTable("PRODUCTS");

    const bool asksDiscount = input.contains("discount") || input.contains("reduction") ||
                              input.contains("promo") || input.contains("sale");
    const bool asksTemperature = input.contains("temperature") || input.contains("tempreture") ||
                                 input.contains("hottest") || input.contains("coldest") ||
                                 input.contains("hot") || input.contains("cold");

    // Count
    if (input.contains("how many") || input.contains("count") || input.contains("total")) {
        q.exec(QString("SELECT COUNT(*) FROM %1").arg(productsTable));
        if (q.next())
            return QString("📦 %1 products in the system.").arg(q.value(0).toInt());
    }

    // Discount insights (computed from DATEOFPURCHASE using same logic as product list)
    if (asksDiscount) {
        q.exec(QString("SELECT TYPE, PRICE, DATEOFPURCHASE FROM %1").arg(productsTable));
        if (!q.next()) return "No discount data found.";

        struct DiscountRow {
            QString type;
            double originalPrice;
            double discountedPrice;
            double discountPct;
        };

        QList<DiscountRow> rows;
        do {
            const QString type = q.value("TYPE").toString();
            const double originalPrice = q.value("PRICE").toDouble();
            const QString purchaseDate = q.value("DATEOFPURCHASE").toString();
            const double discountedPrice = discountedPriceForChat(originalPrice, purchaseDate);
            const double discountPct = (originalPrice > 0.0)
                ? ((originalPrice - discountedPrice) / originalPrice) * 100.0
                : 0.0;
            rows.append({type, originalPrice, discountedPrice, discountPct});
        } while (q.next());

        std::sort(rows.begin(), rows.end(), [](const DiscountRow &a, const DiscountRow &b) {
            return a.discountPct > b.discountPct;
        });

        if (input.contains("highest") || input.contains("higest") || input.contains("biggest") ||
            input.contains("max") || input.contains("best")) {
            const double maxPct = rows.first().discountPct;
            if (maxPct <= 0.0) {
                return "No discounted products right now.";
            }

            QString result = QString("🏷️ Highest discount: %1%%\n").arg(maxPct, 0, 'f', 0);
            int shown = 0;
            for (const DiscountRow &r : rows) {
                if (std::fabs(r.discountPct - maxPct) > 0.01) break;
                result += QString("• %1 | %2 → %3 TND\n")
                              .arg(r.type)
                              .arg(r.originalPrice, 0, 'f', 2)
                              .arg(r.discountedPrice, 0, 'f', 2);
                if (++shown >= 5) break;
            }
            return result.trimmed();
        }

        QString result = "🏷️ Products by discount:\n";
        int shown = 0;
        for (const DiscountRow &r : rows) {
            result += QString("• %1 | %2%% off | %3 → %4 TND\n")
                          .arg(r.type)
                          .arg(r.discountPct, 0, 'f', 0)
                          .arg(r.originalPrice, 0, 'f', 2)
                          .arg(r.discountedPrice, 0, 'f', 2);
            if (++shown >= 8) break;
        }
        return result.trimmed();
    }

    // Temperature insights
    if (asksTemperature) {
        q.exec(QString("SELECT TYPE, LOCATION, TEMPERATURE FROM %1 WHERE TEMPERATURE IS NOT NULL").arg(productsTable));
        if (!q.next()) return "No product temperature data found.";

        struct TempRow {
            QString type;
            QString location;
            double temperature;
        };

        QList<TempRow> rows;
        do {
            rows.append({
                q.value("TYPE").toString(),
                q.value("LOCATION").toString(),
                q.value("TEMPERATURE").toDouble()
            });
        } while (q.next());

        const bool asksColdest = input.contains("coldest") || input.contains("lowest") || input.contains("minimum");
        std::sort(rows.begin(), rows.end(), [asksColdest](const TempRow &a, const TempRow &b) {
            return asksColdest ? (a.temperature < b.temperature) : (a.temperature > b.temperature);
        });

        if (input.contains("highest") || input.contains("hottest") || input.contains("max") ||
            input.contains("coldest") || input.contains("lowest") || input.contains("minimum")) {
            const TempRow &top = rows.first();
            return QString("🌡️ %1 product: %2 at %3 (%4 °C).")
                .arg(asksColdest ? "Coldest" : "Hottest")
                .arg(top.type)
                .arg(top.location)
                .arg(top.temperature, 0, 'f', 2);
        }

        QString result = QString("🌡️ Products by %1 temperature:\n")
                             .arg(asksColdest ? "lowest" : "highest");
        int shown = 0;
        for (const TempRow &r : rows) {
            result += QString("• %1 | %2 | %3 °C\n")
                          .arg(r.type)
                          .arg(r.location)
                          .arg(r.temperature, 0, 'f', 2);
            if (++shown >= 8) break;
        }
        return result.trimmed();
    }

    // Low stock
    if (input.contains("low") || input.contains("running out") || input.contains("shortage")) {
        q.exec(QString("SELECT TYPE, QUANTITY FROM %1 WHERE QUANTITY < 30 ORDER BY QUANTITY ASC").arg(productsTable));
        if (!q.next()) return "✅ All products have sufficient stock (above 30 units).";
        QString result = "⚠️ Low stock (< 30 units):\n";
        do {
            result += QString("• %1 — %2 units\n")
                          .arg(q.value("TYPE").toString())
                          .arg(q.value("QUANTITY").toInt());
        } while (q.next());
        return result.trimmed();
    }

    // Price
    if (input.contains("price") || input.contains("cost") || input.contains("expensive")) {
        q.exec(QString("SELECT TYPE, PRICE FROM %1 ORDER BY PRICE DESC").arg(productsTable));
        if (!q.next()) return "No price data found.";
        QString result = "💰 Products by price:\n";
        int c = 0;
        do {
            result += QString("• %1 — %2 TND\n")
                          .arg(q.value("TYPE").toString())
                          .arg(q.value("PRICE").toDouble(), 0, 'f', 2);
        } while (q.next() && ++c < 8);
        return result.trimmed();
    }

    // Status breakdown
    if (input.contains("status") || input.contains("sold") || input.contains("available")) {
        q.exec(QString("SELECT STATUS, COUNT(*) AS CNT FROM %1 GROUP BY STATUS").arg(productsTable));
        QString result = "📊 Products by status:\n";
        while (q.next())
            result += QString("• %1: %2\n")
                          .arg(q.value("STATUS").toString())
                          .arg(q.value("CNT").toInt());
        return result.trimmed();
    }

    // Default — full list
    q.exec(QString("SELECT TYPE, STATUS, QUANTITY, PRICE FROM %1 ORDER BY PRODUCTID DESC").arg(productsTable));
    if (!q.next()) return "No products found in the database.";
    QString result = "🐟 Products:\n";
    int count = 0;
    do {
        result += QString("• %1 | %2 | Qty: %3 | %4 TND\n")
                      .arg(q.value("TYPE").toString())
                      .arg(q.value("STATUS").toString())
                      .arg(q.value("QUANTITY").toInt())
                      .arg(q.value("PRICE").toDouble(), 0, 'f', 2);
        count++;
    } while (q.next() && count < 10);
    return result.trimmed();
}

// ─────────────────────────────────────────────
//  Docks
// ─────────────────────────────────────────────
QString ChatbotDialog::queryDocks(const QString &input)
{
    QSqlQuery q;

    QString dockingTable = actualTable("DOCKING");
    if (input.contains("available") || input.contains("free")) {
        q.exec(QString("SELECT COUNT(*) FROM %1 WHERE STATUS = 'Available'").arg(dockingTable));
        if (q.next())
            return QString("✅ %1 dock(s) currently available.").arg(q.value(0).toInt());
    }

    if (input.contains("occupied") || input.contains("full") || input.contains("taken")) {
        q.exec(QString("SELECT COUNT(*) FROM %1 WHERE STATUS = 'Occupied'").arg(dockingTable));
        if (q.next())
            return QString("🔴 %1 dock(s) currently occupied.").arg(q.value(0).toInt());
    }

    if (input.contains("how many") || input.contains("count") || input.contains("total")) {
        q.exec(QString("SELECT COUNT(*) FROM %1").arg(dockingTable));
        if (q.next())
            return QString("⚓ %1 docks registered in total.").arg(q.value(0).toInt());
    }

    if (input.contains("capacity")) {
        q.exec(QString("SELECT DOCKID, LOCATION, CAPACITY, STATUS FROM %1 ORDER BY CAPACITY DESC").arg(dockingTable));
        if (!q.next()) return "No dock capacity data found.";
        QString result = "⚓ Dock capacities:\n";
        do {
            result += QString("• Dock #%1 | %2 | Cap: %3 | %4\n")
                          .arg(q.value("DOCKID").toInt())
                          .arg(q.value("LOCATION").toString())
                          .arg(q.value("CAPACITY").toString())
                          .arg(q.value("STATUS").toString());
        } while (q.next());
        return result.trimmed();
    }

    // Default — full list
    q.exec(QString("SELECT DOCKID, LOCATION, STATUS, CAPACITY FROM %1 ORDER BY DOCKID").arg(dockingTable));
    if (!q.next()) return "No docking stations found in the database.";
    QString result = "⚓ Docking stations:\n";
    do {
        result += QString("• Dock #%1 | %2 | %3 | Cap: %4\n")
                      .arg(q.value("DOCKID").toInt())
                      .arg(q.value("LOCATION").toString())
                      .arg(q.value("STATUS").toString())
                      .arg(q.value("CAPACITY").toString());
    } while (q.next());
    return result.trimmed();
}

// ─────────────────────────────────────────────
//  Boats
// ─────────────────────────────────────────────
QString ChatbotDialog::queryBoats(const QString &input)
{
    QSqlQuery q;

    QString boatTable = actualTable("BOAT");
    if (input.contains("sea") || input.contains("outside") || input.contains("sailing")) {
        q.exec(QString("SELECT BOATID, OWNERNAME, TYPE FROM %1 WHERE STATUS = 0").arg(boatTable));
        if (!q.next()) return "🚢 No boats currently at sea.";
        QString result = "🌊 Boats at sea:\n";
        do {
            result += QString("• #%1 | %2 | %3\n")
                          .arg(q.value("BOATID").toInt())
                          .arg(q.value("OWNERNAME").toString())
                          .arg(q.value("TYPE").toString());
        } while (q.next());
        return result.trimmed();
    }

    if (input.contains("docked") || input.contains("inside") || input.contains("port")) {
        q.exec(QString("SELECT BOATID, OWNERNAME, TYPE FROM %1 WHERE STATUS = 1").arg(boatTable));
        if (!q.next()) return "⚓ No boats currently docked.";
        QString result = "⚓ Boats in port:\n";
        do {
            result += QString("• #%1 | %2 | %3\n")
                          .arg(q.value("BOATID").toInt())
                          .arg(q.value("OWNERNAME").toString())
                          .arg(q.value("TYPE").toString());
        } while (q.next());
        return result.trimmed();
    }

    if (input.contains("maintenance") || input.contains("repair")) {
        q.exec(QString("SELECT OWNERNAME, TYPE, LASTMAINTENANCEDATE FROM %1 ORDER BY LASTMAINTENANCEDATE ASC").arg(boatTable));
        if (!q.next()) return "No maintenance records found.";
        QString result = "🔧 Boats by last maintenance (oldest first):\n";
        int c = 0;
        do {
            result += QString("• %1 (%2) — Last: %3\n")
                          .arg(q.value("OWNERNAME").toString())
                          .arg(q.value("TYPE").toString())
                          .arg(q.value("LASTMAINTENANCEDATE").toString());
        } while (q.next() && ++c < 6);
        return result.trimmed();
    }

    if (input.contains("trip") || input.contains("fish caught")) {
        q.exec(QString("SELECT OWNERNAME, TOTALTRIPS, TOTALFISH FROM %1 ORDER BY TOTALFISH DESC").arg(boatTable));
        if (!q.next()) return "No trip data found.";
        QString result = "📈 Boat performance:\n";
        do {
            result += QString("• %1 | Trips: %2 | Fish: %3\n")
                          .arg(q.value("OWNERNAME").toString())
                          .arg(q.value("TOTALTRIPS").toInt())
                          .arg(q.value("TOTALFISH").toInt());
        } while (q.next());
        return result.trimmed();
    }

    if (input.contains("location") || input.contains("where")) {
        const QRegularExpression idPattern(R"((?:boat|vessel|ship)\s*#?\s*(\d+))");
        const QRegularExpressionMatch idMatch = idPattern.match(input);

        if (idMatch.hasMatch()) {
            const int boatId = idMatch.captured(1).toInt();
            q.prepare(QString("SELECT BOATID, OWNERNAME, LOCATION, STATUS FROM %1 WHERE BOATID = :id").arg(boatTable));
            q.bindValue(":id", boatId);
            if (!q.exec() || !q.next())
                return QString("I couldn't find boat #%1.").arg(boatId);

            const QString locationName = resolveLocationName(q.value("LOCATION").toString());
            const QString st = q.value("STATUS").toInt() == 1 ? "In Port" : "At Sea";
            return QString("📍 Boat #%1 (%2) is currently %3 near %4.")
                .arg(q.value("BOATID").toInt())
                .arg(q.value("OWNERNAME").toString())
                .arg(st)
                .arg(locationName);
        }

        q.exec(QString("SELECT BOATID, OWNERNAME, LOCATION, STATUS FROM %1 ORDER BY BOATID").arg(boatTable));
        if (!q.next()) return "No boats found in the database.";
        QString result = "📍 Boat locations:\n";
        do {
            const QString locationName = resolveLocationName(q.value("LOCATION").toString());
            const QString st = q.value("STATUS").toInt() == 1 ? "In Port" : "At Sea";
            result += QString("• #%1 | %2 | %3 | %4\n")
                          .arg(q.value("BOATID").toInt())
                          .arg(q.value("OWNERNAME").toString())
                          .arg(st)
                          .arg(locationName);
        } while (q.next());
        return result.trimmed();
    }

    if (input.contains("how many") || input.contains("count") || input.contains("total")) {
        q.exec(QString("SELECT COUNT(*) FROM %1").arg(boatTable));
        if (q.next())
            return QString("🚢 %1 boats registered.").arg(q.value(0).toInt());
    }

    // Default — full list
    q.exec(QString("SELECT BOATID, OWNERNAME, SIZEBOAT, LOCATION, STATUS, TYPE, TOTALTRIPS, TOTALFISH FROM %1 ORDER BY BOATID").arg(boatTable));
    if (!q.next()) return "No boats found in the database.";
    QString result = "🚢 All boats:\n";
    do {
        QString st = q.value("STATUS").toInt() == 1 ? "In Port" : "At Sea";
        const QString locationName = resolveLocationName(q.value("LOCATION").toString());
        result += QString("• #%1 | %2 | %3 | %4 | %5 | Trips:%6 | Fish:%7\n")
                      .arg(q.value("BOATID").toInt())
                      .arg(q.value("OWNERNAME").toString())
                      .arg(q.value("TYPE").toString())
                      .arg(st)
                      .arg(locationName)
                      .arg(q.value("TOTALTRIPS").toInt())
                      .arg(q.value("TOTALFISH").toInt());
    } while (q.next());
    return result.trimmed();
}

// ─────────────────────────────────────────────
//  Users
// ─────────────────────────────────────────────
QString ChatbotDialog::queryUsers(const QString &input)
{
    QSqlQuery q;

    QString usersTable = actualTable("USERS");
    if (input.contains("how many") || input.contains("count") || input.contains("total")) {
        q.exec(QString("SELECT COUNT(*) FROM %1").arg(usersTable));
        if (q.next())
            return QString("👤 %1 users in the system.").arg(q.value(0).toInt());
    }

    if (input.contains("role") || input.contains("admin") ||
        input.contains("manager") || input.contains("employee")) {
        q.exec(QString("SELECT ROLE, COUNT(*) AS CNT FROM %1 GROUP BY ROLE ORDER BY CNT DESC").arg(usersTable));
        QString result = "👥 Users by role:\n";
        while (q.next())
            result += QString("• %1: %2\n")
                          .arg(q.value("ROLE").toString())
                          .arg(q.value("CNT").toInt());
        return result.trimmed();
    }

    if (input.contains("salary")) {
        q.exec(QString("SELECT FIRST_NAME, LAST_NAME, SALARY FROM %1 ORDER BY SALARY DESC").arg(usersTable));
        if (!q.next()) return "No salary data found.";
        QString result = "💰 Salaries:\n";
        do {
            result += QString("• %1 %2 — %3 TND\n")
                          .arg(q.value("FIRST_NAME").toString())
                          .arg(q.value("LAST_NAME").toString())
                          .arg(q.value("SALARY").toDouble(), 0, 'f', 0);
        } while (q.next());
        return result.trimmed();
    }

    // Default — full list
    q.exec(QString("SELECT USERID, FIRST_NAME, LAST_NAME, ROLE, EMAIL FROM %1 ORDER BY USERID").arg(usersTable));
    if (!q.next()) {
        if (!m_currentUserRole.trimmed().isEmpty()) {
            return QString("I can see your session as %1 user #%2, but the users table returned no rows.")
                .arg(m_currentUserRole, QString::number(m_currentUserId));
        }
        return "No users found in the database.";
    }
    QString result = "👤 Users:\n";
    do {
        result += QString("• #%1 | %2 %3 | %4 | %5\n")
                      .arg(q.value("USERID").toInt())
                      .arg(q.value("FIRST_NAME").toString())
                      .arg(q.value("LAST_NAME").toString())
                      .arg(q.value("ROLE").toString())
                      .arg(q.value("EMAIL").toString());
    } while (q.next());
    return result.trimmed();
}

// ─────────────────────────────────────────────
//  Companies
// ─────────────────────────────────────────────
QString ChatbotDialog::queryCompanies(const QString &input)
{
    QSqlQuery q;

    QString companiesTable = actualTable("COMPANIES");
    if (input.contains("active") && !input.contains("inactive")) {
        q.exec(QString("SELECT NAME, LOCATION, PREFERRED_FISH FROM %1 WHERE STATUS = 'ACTIVE'").arg(companiesTable));
        if (!q.next()) return "No active companies found.";
        QString result = "✅ Active companies:\n";
        do {
            result += QString("• %1 | %2 | Prefers: %3\n")
                          .arg(q.value("NAME").toString())
                          .arg(q.value("LOCATION").toString())
                          .arg(q.value("PREFERRED_FISH").toString());
        } while (q.next());
        return result.trimmed();
    }

    if (input.contains("inactive")) {
        q.exec(QString("SELECT NAME, EMAIL, LOCATION FROM %1 WHERE STATUS = 'INACTIVE'").arg(companiesTable));
        if (!q.next()) return "✅ No inactive companies — all partners are active!";
        QString result = "🔴 Inactive companies:\n";
        do {
            result += QString("• %1 | %2 | %3\n")
                          .arg(q.value("NAME").toString())
                          .arg(q.value("LOCATION").toString())
                          .arg(q.value("EMAIL").toString());
        } while (q.next());
        return result.trimmed();
    }

    if (input.contains("how many") || input.contains("count") || input.contains("total")) {
        q.exec(QString("SELECT COUNT(*) FROM %1").arg(companiesTable));
        if (q.next())
            return QString("🏢 %1 companies registered.").arg(q.value(0).toInt());
    }

    if (input.contains("fish") || input.contains("prefer") || input.contains("tuna") ||
        input.contains("sardine")) {
        q.exec(QString("SELECT PREFERRED_FISH, COUNT(*) AS CNT FROM %1 GROUP BY PREFERRED_FISH ORDER BY CNT DESC").arg(companiesTable));
        QString result = "🐟 Companies by preferred fish:\n";
        while (q.next())
            result += QString("• %1: %2 compan%3\n")
                          .arg(q.value("PREFERRED_FISH").toString())
                          .arg(q.value("CNT").toInt())
                          .arg(q.value("CNT").toInt() == 1 ? "y" : "ies");
        return result.trimmed();
    }

    // Default — full list
    q.exec(QString("SELECT NAME, LOCATION, PREFERRED_FISH, STATUS FROM %1 ORDER BY COMPANY_ID").arg(companiesTable));
    if (!q.next()) return "No companies found in the database.";
    QString result = "🏢 Companies:\n";
    do {
        result += QString("• %1 | %2 | Prefers: %3 | %4\n")
                      .arg(q.value("NAME").toString())
                      .arg(q.value("LOCATION").toString())
                      .arg(q.value("PREFERRED_FISH").toString())
                      .arg(q.value("STATUS").toString());
    } while (q.next());
    return result.trimmed();
}

// ─────────────────────────────────────────────
//  UI helpers
// ─────────────────────────────────────────────
void ChatbotDialog::addMessage(const QString &text, bool isUser)
{
    QLabel *bubble = new QLabel(text);
    bubble->setWordWrap(true);
    bubble->setMaximumWidth(270);
    bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);

    if (isUser) {
        bubble->setStyleSheet(
            "background-color: #2EC4B6;"
            "color: #0A2540;"
            "border-radius: 14px;"
            "padding: 10px 13px;"
            "font-size: 13px;"
            "font-weight: 500;"
            );
    } else {
        bubble->setStyleSheet(
            "background-color: #0D3060;"
            "color: #C8E0F4;"
            "border: 1px solid rgba(46,196,182,0.2);"
            "border-radius: 14px;"
            "padding: 10px 13px;"
            "font-size: 13px;"
            );
    }

    QHBoxLayout *row = new QHBoxLayout();
    row->setContentsMargins(0, 0, 0, 0);
    if (isUser) {
        row->addStretch();
        row->addWidget(bubble);
    } else {
        row->addWidget(bubble);
        row->addStretch();
    }

    int idx = ui->messagesLayout->count() - 1;
    ui->messagesLayout->insertLayout(idx, row);
    scrollToBottom();
}

void ChatbotDialog::scrollToBottom()
{
    QTimer::singleShot(50, this, [this]() {
        ui->chatScrollArea->verticalScrollBar()->setValue(
            ui->chatScrollArea->verticalScrollBar()->maximum()
            );
    });
}
