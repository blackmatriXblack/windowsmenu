#include <QtWidgets>
#include <QApplication>
#include <QStyle>
#include <QStyleHints>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QScreen>
#include <QDesktopServices>
#include <QStandardPaths>
#include <QDirIterator>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <tlhelp32.h>
#include <winsvc.h>
#include <psapi.h>
#include <iphlpapi.h>
#include <cstring>
#include <cstdio>
#include <vector>
#include <set>
#include <algorithm>

#ifndef MIB_TCP_STATE_ESTABLISHED
#define MIB_TCP_STATE_ESTABLISHED 5
#endif

extern "C" {
    int QcCategoryCount(void);
    const char *QcCategoryName(int);
    int QcCategoryCommandCount(int);
    const char *QcCommandName(int, int);
    const char *QcCommandDesc(int, int);
    int QcCommandNeedsArg(int, int);
    const char *QcCommandDetail(int, int);
    int QcTotalCommands(void);
    void QcRunCommand(int, int, const char *);
    void QcSample(void);
    double QcCpuPct(void);
    double QcRamPct(void);
    double QcDiskPct(void);
    double QcNetDown(void);
    double QcNetUp(void);
    const char *QcUptime(void);
    void QcOpenTool(int);
    void QcTermExec(const char *, void (*)(const char *));
    extern int g_qcTermClose;
}

static const char *APP_TITLE = "Windows Control Center";
static bool g_darkMode = true;

struct CmdRef {
    int cat;
    int idx;
    QString name;
    QString desc;
    bool needsArg;
    QString detail;
};

static QPalette makePalette(bool dark) {
    QPalette p;
    if (dark) {
        p.setColor(QPalette::Window, QColor(0x20, 0x20, 0x20));
        p.setColor(QPalette::WindowText, QColor(0xf0, 0xf0, 0xf0));
        p.setColor(QPalette::Base, QColor(0x2b, 0x2b, 0x2b));
        p.setColor(QPalette::AlternateBase, QColor(0x30, 0x30, 0x30));
        p.setColor(QPalette::Text, QColor(0xf0, 0xf0, 0xf0));
        p.setColor(QPalette::Button, QColor(0x33, 0x33, 0x33));
        p.setColor(QPalette::ButtonText, QColor(0xf0, 0xf0, 0xf0));
        p.setColor(QPalette::Highlight, QColor(0x3c, 0x7d, 0xf2));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::PlaceholderText, QColor(0x9a, 0x9a, 0x9a));
        p.setColor(QPalette::ToolTipBase, QColor(0x2b, 0x2b, 0x2b));
        p.setColor(QPalette::ToolTipText, QColor(0xf0, 0xf0, 0xf0));
    } else {
        p.setColor(QPalette::Window, QColor(0xf6, 0xf6, 0xf6));
        p.setColor(QPalette::WindowText, QColor(0x1b, 0x1b, 0x1b));
        p.setColor(QPalette::Base, Qt::white);
        p.setColor(QPalette::AlternateBase, QColor(0xf0, 0xf0, 0xf0));
        p.setColor(QPalette::Text, QColor(0x1b, 0x1b, 0x1b));
        p.setColor(QPalette::Button, QColor(0xe8, 0xe8, 0xe8));
        p.setColor(QPalette::ButtonText, QColor(0x1b, 0x1b, 0x1b));
        p.setColor(QPalette::Highlight, QColor(0x00, 0x66, 0xcc));
        p.setColor(QPalette::HighlightedText, Qt::white);
        p.setColor(QPalette::PlaceholderText, QColor(0x8a, 0x8a, 0x8a));
        p.setColor(QPalette::ToolTipBase, QColor(0xff, 0xff, 0xf0));
        p.setColor(QPalette::ToolTipText, QColor(0x1b, 0x1b, 0x1b));
    }
    return p;
}

static bool systemIsDarkMode() {
    QColor c = QApplication::palette().color(QPalette::Window);
    return (c.red() * 299 + c.green() * 587 + c.blue() * 114) / 1000 < 128;
}

static QString favPath() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/favorites.json";
}

static QStringList loadFavorites() {
    QFile f(favPath());
    if (!f.open(QIODevice::ReadOnly)) return {};
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
    QStringList out;
    for (auto v : doc.array()) out << v.toString();
    return out;
}

static void saveFavorites(const QStringList &list) {
    QJsonArray arr;
    for (auto &s : list) arr.append(s);
    QFile f(favPath());
    QFileInfo fi(f);
    QDir().mkpath(fi.absolutePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(arr).toJson());
}class StatsChip : public QFrame {
public:
    explicit StatsChip(const QString &title, QWidget *parent = nullptr)
        : QFrame(parent), m_title(title) {
        setObjectName("chip");
        setMinimumHeight(52);
        QHBoxLayout *lay = new QHBoxLayout(this);
        lay->setContentsMargins(12, 6, 12, 6);
        lay->setSpacing(8);
        QLabel *lab = new QLabel(title, this);
        lab->setStyleSheet("font-weight:600;");
        m_value = new QLabel("--", this);
        m_value->setStyleSheet("font-size:16px;font-weight:600;");
        m_bar = new QProgressBar(this);
        m_bar->setRange(0, 1000);
        m_bar->setTextVisible(false);
        m_bar->setFixedHeight(8);
        m_bar->setStyleSheet(
            "QProgressBar{background:rgba(127,127,127,0.25);border:none;border-radius:4px;}"
            "QProgressBar::chunk{background:#3c7df2;border-radius:4px;}");
        lay->addWidget(lab);
        lay->addWidget(m_value);
        lay->addWidget(m_bar, 1);
    }
    void setPct(double pct, const QString &txt) {
        m_value->setText(txt);
        m_bar->setValue((int)(pct * 10.0));
    }
private:
    QString m_title;
    QLabel *m_value;
    QProgressBar *m_bar;
};

static void termCb(const char *t);

class TerminalDialog : public QDialog {
public:
    TerminalDialog(QWidget *parent = nullptr) : QDialog(parent) {
        setWindowTitle("WC Terminal");
        resize(720, 420);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_out = new QPlainTextEdit(this);
        m_out->setReadOnly(true);
        QFont mono("Consolas", 10);
        m_out->setFont(mono);
        m_in = new QLineEdit(this);
        m_in->setPlaceholderText("type a command, e.g. settings display, ipconfig, dir C:\\  (exit to close)");
        lay->addWidget(m_out, 1);
        lay->addWidget(m_in);
        connect(m_in, &QLineEdit::returnPressed, this, [this]() {
            QString line = m_in->text();
            m_in->clear();
            m_out->appendPlainText("wc> " + line);
            QByteArray ba = line.toUtf8();
            QcTermExec(ba.constData(), termCb);
            for (int i = 0; i < (int)m_pending.size(); i++)
                m_out->appendPlainText(QString::fromUtf8(m_pending[i].c_str()));
            m_pending.clear();
            if (g_qcTermClose) {
                m_out->appendPlainText("Terminal closed.");
                close();
            }
        });
    }
public:
    void appendC(const char *t) { m_pending.emplace_back(t ? t : ""); }
private:
    QPlainTextEdit *m_out;
    QLineEdit *m_in;
    std::vector<std::string> m_pending;
};

static TerminalDialog *g_termDlg = nullptr;

static void termCb(const char *t) {
    if (g_termDlg) g_termDlg->appendC(t);
}class ProcessManagerDialog : public QDialog {
public:
    ProcessManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Process Manager");
        resize(800, 520);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText("Filter by name...");
        top->addWidget(m_filter);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &ProcessManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *killBtn = new QPushButton("Kill Selected", this);
        killBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(killBtn, &QPushButton::clicked, this, &ProcessManagerDialog::killSelected);
        top->addWidget(killBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"PID", "Name", "Threads", "Memory"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) { load(); });
        load();
    }
    void load() {
        QString filt = m_filter->text().trimmed().toLower();
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap == INVALID_HANDLE_VALUE) return;
        PROCESSENTRY32 pe{};
        pe.dwSize = sizeof(pe);
        std::vector<QStringList> rows;
        if (Process32First(snap, &pe)) do {
            QString name = QString::fromWCharArray(pe.szExeFile);
            DWORD pid = pe.th32ProcessID;
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            PROCESS_MEMORY_COUNTERS pmc{};
            pmc.cb = sizeof(pmc);
            DWORD threads = pe.cntThreads;
            DWORDLONG memKB = 0;
            if (hProc) {
                if (GetProcessMemoryInfo(hProc, &pmc, sizeof(pmc))) memKB = pmc.WorkingSetSize / 1024;
                CloseHandle(hProc);
            }
            QString nameLow = name.toLower();
            if (!filt.isEmpty() && !nameLow.count(filt)) continue;
            rows.push_back({QString::number(pid), name, QString::number(threads),
                           QString("%1 KB").arg(memKB)});
        } while (Process32Next(snap, &pe));
        CloseHandle(snap);
        m_table->setRowCount((int)rows.size());
        for (int i = 0; i < (int)rows.size(); i++)
            for (int j = 0; j < 4; j++)
                m_table->setItem(i, j, new QTableWidgetItem(rows[i][j]));
        m_count->setText(QString("%1 processes").arg(rows.size()));
    }
    void killSelected() {
        auto sel = m_table->selectionModel()->selectedRows();
        for (auto &idx : sel) {
            DWORD pid = m_table->item(idx.row(), 0)->text().toULong();
            if (pid <= 4) continue;
            HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
            if (h) { TerminateProcess(h, 1); CloseHandle(h); }
        }
        load();
    }
private:
    QTableWidget *m_table;
    QLineEdit *m_filter;
    QLabel *m_count;
};class ServicesManagerDialog : public QDialog {
public:
    ServicesManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Services Manager");
        resize(850, 520);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText("Filter by name...");
        top->addWidget(m_filter);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &ServicesManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *startBtn = new QPushButton("Start", this);
        connect(startBtn, &QPushButton::clicked, this, &ServicesManagerDialog::startSvc);
        top->addWidget(startBtn);
        QPushButton *stopBtn = new QPushButton("Stop", this);
        stopBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(stopBtn, &QPushButton::clicked, this, &ServicesManagerDialog::stopSvc);
        top->addWidget(stopBtn);
        QPushButton *restartBtn = new QPushButton("Restart", this);
        connect(restartBtn, &QPushButton::clicked, this, &ServicesManagerDialog::restartSvc);
        top->addWidget(restartBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Name", "Display Name", "Status", "Start Type"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) { load(); });
        load();
    }
    void load() {
        QString filt = m_filter->text().trimmed().toLower();
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_ENUMERATE_SERVICE);
        if (!scm) { m_count->setText("Cannot open SCM (need admin)"); return; }
        DWORD needed = 0, count = 0, resume = 0;
        EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                              nullptr, 0, &needed, &count, &resume, nullptr);
        std::vector<BYTE> buf(needed);
        if (!EnumServicesStatusExW(scm, SC_ENUM_PROCESS_INFO, SERVICE_WIN32, SERVICE_STATE_ALL,
                                   buf.data(), needed, &needed, &count, &resume, nullptr)) {
            CloseServiceHandle(scm);
            m_count->setText("EnumServices failed");
            return;
        }
        auto *svc = (ENUM_SERVICE_STATUS_PROCESSW *)buf.data();
        QString filt2 = filt;
        std::vector<QStringList> rows;
        for (DWORD i = 0; i < count; i++) {
            QString name = QString::fromWCharArray(svc[i].lpServiceName);
            QString disp = QString::fromWCharArray(svc[i].lpDisplayName);
            QString status;
            switch (svc[i].ServiceStatusProcess.dwCurrentState) {
                case SERVICE_RUNNING: status = "Running"; break;
                case SERVICE_STOPPED: status = "Stopped"; break;
                case SERVICE_START_PENDING: status = "Starting"; break;
                case SERVICE_STOP_PENDING: status = "Stopping"; break;
                case SERVICE_PAUSED: status = "Paused"; break;
                default: status = "Other"; break;
            }
            QString startType;
            DWORD dwStartType = svc[i].ServiceStatusProcess.dwServiceType;
            QUERY_SERVICE_CONFIGW qcfg{};
            DWORD qcfgSz = 0;
            SC_HANDLE hSvc = OpenServiceW(scm, svc[i].lpServiceName, SERVICE_QUERY_CONFIG);
            if (hSvc) {
                QueryServiceConfigW(hSvc, nullptr, 0, &qcfgSz);
                std::vector<BYTE> qbuf(qcfgSz);
                if (QueryServiceConfigW(hSvc, (QUERY_SERVICE_CONFIGW *)qbuf.data(), qcfgSz, &qcfgSz)) {
                    auto *cfg = (QUERY_SERVICE_CONFIGW *)qbuf.data();
                    switch (cfg->dwStartType) {
                        case SERVICE_AUTO_START: startType = "Automatic"; break;
                        case SERVICE_DEMAND_START: startType = "Manual"; break;
                        case SERVICE_DISABLED: startType = "Disabled"; break;
                        default: startType = "Other"; break;
                    }
                }
                CloseServiceHandle(hSvc);
            }
            if (!filt2.isEmpty() && !name.toLower().count(filt2) && !disp.toLower().count(filt2)) continue;
            rows.push_back({name, disp, status, startType});
        }
        CloseServiceHandle(scm);
        m_table->setRowCount((int)rows.size());
        for (int i = 0; i < (int)rows.size(); i++)
            for (int j = 0; j < 4; j++)
                m_table->setItem(i, j, new QTableWidgetItem(rows[i][j]));
        m_count->setText(QString("%1 services").arg(rows.size()));
    }
    QString selectedName() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return {};
        return m_table->item(sel.first().row(), 0)->text();
    }
    void controlSvc(DWORD ctrl) {
        QString svcName = selectedName();
        if (svcName.isEmpty()) return;
        SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
        if (!scm) return;
        SC_HANDLE hSvc = OpenServiceW(scm, (LPCWSTR)svcName.utf16(), SERVICE_START | SERVICE_STOP);
        if (hSvc) {
            if (ctrl == SERVICE_CONTROL_STOP) {
                SERVICE_STATUS ss{};
                ControlService(hSvc, SERVICE_CONTROL_STOP, &ss);
            } else {
                StartServiceW(hSvc, 0, nullptr);
            }
            CloseServiceHandle(hSvc);
        }
        CloseServiceHandle(scm);
        Sleep(500);
        load();
    }
    void startSvc() { controlSvc(1); }
    void stopSvc() { controlSvc(2); }
    void restartSvc() { stopSvc(); Sleep(300); startSvc(); }
private:
    QTableWidget *m_table;
    QLineEdit *m_filter;
    QLabel *m_count;
};class NetworkToolsDialog : public QDialog {
public:
    NetworkToolsDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Network Tools");
        resize(720, 480);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_target = new QLineEdit(this);
        m_target->setPlaceholderText("Host / IP (e.g. 8.8.8.8, google.com)");
        top->addWidget(m_target);
        m_type = new QComboBox(this);
        m_type->addItems({"Ping", "Traceroute", "DNS Lookup", "Port Check"});
        top->addWidget(m_type);
        QPushButton *run = new QPushButton("Run", this);
        connect(run, &QPushButton::clicked, this, &NetworkToolsDialog::runTool);
        top->addWidget(run);
        lay->addLayout(top);
        m_output = new QPlainTextEdit(this);
        m_output->setReadOnly(true);
        QFont mono("Consolas", 9);
        m_output->setFont(mono);
        lay->addWidget(m_output, 1);
    }
    void runTool() {
        QString host = m_target->text().trimmed();
        if (host.isEmpty()) return;
        m_output->clear();
        m_output->appendPlainText("Running...\n");
        QApplication::processEvents();
        QProcess proc;
        QString cmd;
        QStringList args;
        switch (m_type->currentIndex()) {
            case 0: cmd = "ping"; args << "-n" << "4" << host; break;
            case 1: cmd = "tracert"; args << "-d" << "-h" << "15" << host; break;
            case 2: cmd = "nslookup"; args << host; break;
            case 3: {
                cmd = "powershell";
                args << "-Command" << QString("Test-NetConnection -ComputerName '%1' -Port 80 -WarningAction SilentlyContinue | Format-List").arg(host);
                break;
            }
        }
        proc.start(cmd, args);
        proc.waitForFinished(30000);
        m_output->setPlainText(QString::fromUtf8(proc.readAll()));
    }
private:
    QLineEdit *m_target;
    QComboBox *m_type;
    QPlainTextEdit *m_output;
};class ClipboardManagerDialog : public QDialog {
public:
    ClipboardManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Clipboard Manager");
        resize(600, 400);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &ClipboardManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *clearBtn = new QPushButton("Clear Clipboard", this);
        connect(clearBtn, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->clear();
            load();
        });
        top->addWidget(clearBtn);
        lay->addLayout(top);
        m_list = new QListWidget(this);
        m_list->setAlternatingRowColors(true);
        lay->addWidget(m_list, 1);
        m_detail = new QTextEdit(this);
        m_detail->setReadOnly(true);
        m_detail->setMaximumHeight(120);
        lay->addWidget(m_detail);
        connect(m_list, &QListWidget::currentRowChanged, this, &ClipboardManagerDialog::showDetail);
        load();
    }
    void load() {
        m_list->clear();
        m_texts.clear();
        QClipboard *clip = QApplication::clipboard();
        QString text = clip->text();
        if (!text.isEmpty()) {
            m_texts.append(text);
            QString preview = text.left(80).replace('\n', ' ');
            m_list->addItem("[Text] " + preview);
        }
        QStringList formats;
        const QMimeData *md = clip->mimeData();
        if (md) {
            for (auto &fmt : md->formats()) {
                formats << fmt;
                if (fmt == "text/plain") continue;
                m_texts.append(QString("[Format: %1] (binary data)").arg(fmt));
                m_list->addItem(QString("[Binary] %1 (%2 bytes)").arg(fmt).arg(md->data(fmt).size()));
            }
        }
        m_list->addItem(QString("--- Clipboard has %1 format(s) ---").arg(formats.size()));
    }
    void showDetail() {
        int row = m_list->currentRow();
        if (row >= 0 && row < m_texts.size())
            m_detail->setPlainText(m_texts[row]);
        else
            m_detail->clear();
    }
private:
    QListWidget *m_list;
    QTextEdit *m_detail;
    QStringList m_texts;
};class ScreenshotDialog : public QDialog {
public:
    ScreenshotDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Screenshot Tool");
        resize(900, 600);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *capture = new QPushButton("Capture Full Screen", this);
        connect(capture, &QPushButton::clicked, this, &ScreenshotDialog::captureScreen);
        top->addWidget(capture);
        QPushButton *captureArea = new QPushButton("Capture Window Area", this);
        connect(captureArea, &QPushButton::clicked, this, &ScreenshotDialog::captureArea);
        top->addWidget(captureArea);
        QPushButton *saveBtn = new QPushButton("Save", this);
        connect(saveBtn, &QPushButton::clicked, this, &ScreenshotDialog::saveImage);
        top->addWidget(saveBtn);
        lay->addLayout(top);
        m_scroll = new QScrollArea(this);
        m_label = new QLabel("Click 'Capture' to take a screenshot", this);
        m_label->setAlignment(Qt::AlignCenter);
        m_scroll->setWidget(m_label);
        m_scroll->setWidgetResizable(true);
        lay->addWidget(m_scroll, 1);
        m_info = new QLabel(this);
        lay->addWidget(m_info);
    }
    void captureScreen() {
        hide();
        QTimer::singleShot(300, this, [this]() {
            QScreen *screen = QGuiApplication::primaryScreen();
            if (!screen) return;
            m_pixmap = screen->grabWindow(0);
            m_label->setPixmap(m_pixmap.scaled(m_scroll->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_info->setText(QString("Captured: %1 x %2 px").arg(m_pixmap.width()).arg(m_pixmap.height()));
            show();
            raise();
        });
    }
    void captureArea() {
        hide();
        QTimer::singleShot(300, this, [this]() {
            QScreen *screen = QGuiApplication::primaryScreen();
            if (!screen) return;
            m_pixmap = screen->grabWindow(0);
            m_label->setPixmap(m_pixmap.scaled(m_scroll->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
            m_info->setText(QString("Captured: %1 x %2 px").arg(m_pixmap.width()).arg(m_pixmap.height()));
            show();
            raise();
        });
    }
    void saveImage() {
        if (m_pixmap.isNull()) return;
        QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) +
                       QString("/screenshot_%1.png").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        m_pixmap.save(path, "PNG");
        m_info->setText("Saved: " + path);
    }
private:
    QScrollArea *m_scroll;
    QLabel *m_label;
    QLabel *m_info;
    QPixmap m_pixmap;
};class SystemReportDialog : public QDialog {
public:
    SystemReportDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("System Report");
        resize(700, 500);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_text = new QPlainTextEdit(this);
        m_text->setReadOnly(true);
        QFont mono("Consolas", 9);
        m_text->setFont(mono);
        lay->addWidget(m_text, 1);
        QHBoxLayout *bot = new QHBoxLayout();
        QPushButton *genBtn = new QPushButton("Generate Report", this);
        connect(genBtn, &QPushButton::clicked, this, &SystemReportDialog::generate);
        bot->addWidget(genBtn);
        QPushButton *saveBtn = new QPushButton("Save to File", this);
        connect(saveBtn, &QPushButton::clicked, this, &SystemReportDialog::saveToFile);
        bot->addWidget(saveBtn);
        lay->addLayout(bot);
        generate();
    }
    void generate() {
        QString rpt;
        rpt += "=== SYSTEM REPORT ===\n";
        rpt += QString("Generated: %1\n\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss"));
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        rpt += QString("Processor Architecture: %1\n").arg(si.wProcessorArchitecture);
        rpt += QString("Processor Count: %1\n").arg(si.dwNumberOfProcessors);
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);
        rpt += QString("Total Physical Memory: %1 MB\n").arg(mem.ullTotalPhys / 1048576);
        rpt += QString("Available Physical Memory: %1 MB\n").arg(mem.ullAvailPhys / 1048576);
        rpt += QString("Memory Usage: %1%%\n\n").arg(mem.dwMemoryLoad);
        WCHAR compName[MAX_COMPUTERNAME_LENGTH + 1];
        DWORD sz = MAX_COMPUTERNAME_LENGTH + 1;
        GetComputerNameW(compName, &sz);
        rpt += QString("Computer Name: %1\n").arg(QString::fromWCharArray(compName));
        WCHAR userName[256];
        sz = 256;
        GetUserNameW(userName, &sz);
        rpt += QString("User Name: %1\n\n").arg(QString::fromWCharArray(userName));
        WCHAR winDir[MAX_PATH];
        GetWindowsDirectoryW(winDir, MAX_PATH);
        rpt += QString("Windows Directory: %1\n").arg(QString::fromWCharArray(winDir));
        DWORD tickCount = GetTickCount64();
        rpt += QString("Uptime: %1 days %2 hours %3 min\n\n").arg(tickCount / 86400000).arg((tickCount % 86400000) / 3600000).arg((tickCount % 3600000) / 60000);
        DWORD dispFreq, flags;
        QueryPerformanceFrequency((LARGE_INTEGER *)&dispFreq);
        rpt += QString("Timer Frequency: %1 Hz\n").arg(dispFreq);
        rpt += QString("\n=== DISK SPACE ===\n");
        for (char d = 'C'; d <= 'Z'; d++) {
            QString drive = QString("%1:\\").arg(d);
            ULARGE_INTEGER freeBytes, totalBytes;
            if (GetDiskFreeSpaceExW((LPCWSTR)drive.utf16(), nullptr, &totalBytes, &freeBytes)) {
                rpt += QString("%1: %2 GB / %3 GB (%4%% free)\n")
                    .arg(drive)
                    .arg(freeBytes.QuadPart / 1073741824.0, 0, 'f', 1)
                    .arg(totalBytes.QuadPart / 1073741824.0, 0, 'f', 1)
                    .arg((double)freeBytes.QuadPart / totalBytes.QuadPart * 100.0, 0, 'f', 1);
            }
        }
        m_text->setPlainText(rpt);
    }
    void saveToFile() {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) +
                       "/system_report.txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(m_text->toPlainText().toUtf8());
            m_text->appendPlainText("\n--- Saved to: " + path + " ---");
        }
    }
private:
    QPlainTextEdit *m_text;
};class FavoritesDialog : public QDialog {
public:
    FavoritesDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Favorite Commands");
        resize(500, 400);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_list = new QListWidget(this);
        m_list->setAlternatingRowColors(true);
        lay->addWidget(m_list, 1);
        QHBoxLayout *bot = new QHBoxLayout();
        QPushButton *runBtn = new QPushButton("Run", this);
        connect(runBtn, &QPushButton::clicked, this, &FavoritesDialog::runFav);
        bot->addWidget(runBtn);
        QPushButton *removeBtn = new QPushButton("Remove", this);
        removeBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(removeBtn, &QPushButton::clicked, this, &FavoritesDialog::removeFav);
        bot->addWidget(removeBtn);
        lay->addLayout(bot);
        load();
    }
    void load() {
        m_favs = loadFavorites();
        m_list->clear();
        for (auto &f : m_favs) m_list->addItem(f);
    }
    void runFav() {
        auto it = m_list->currentItem();
        if (!it) return;
        QStringList parts = it->text().split("|");
        if (parts.size() >= 2) {
            QcRunCommand(parts[0].toInt(), parts[1].toInt(), nullptr);
        }
    }
    void removeFav() {
        int row = m_list->currentRow();
        if (row < 0 || row >= m_favs.size()) return;
        m_favs.removeAt(row);
        saveFavorites(m_favs);
        load();
    }
    QStringList m_favs;
    QListWidget *m_list;
};class FloatWindow : public QWidget {
public:
    FloatWindow(QWidget *mainW) : m_main(mainW), m_drag(false) {
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_ShowWithoutActivating);
        setFixedSize(340, 128);
        QFrame *card = new QFrame(this);
        card->setObjectName("floatCard");
        card->setGeometry(4, 4, width() - 8, height() - 8);
        QVBoxLayout *lay = new QVBoxLayout(card);
        lay->setContentsMargins(12, 8, 12, 8);
        lay->setSpacing(4);
        QHBoxLayout *top = new QHBoxLayout();
        QLabel *lab = new QLabel("Windows Control Center", card);
        lab->setStyleSheet("font-weight:600;font-size:10pt;");
        top->addWidget(lab);
        top->addStretch(1);
        QPushButton *btnOpen = new QPushButton("Open Main", card);
        btnOpen->setCursor(Qt::PointingHandCursor);
        btnOpen->setStyleSheet("padding:2px 10px;font-size:9pt;");
        connect(btnOpen, &QPushButton::clicked, this, [this]() {
            if (m_main) { m_main->showNormal(); m_main->raise(); m_main->activateWindow(); }
        });
        top->addWidget(btnOpen);
        QPushButton *btnClose = new QPushButton("X", card);
        btnClose->setCursor(Qt::PointingHandCursor);
        btnClose->setFixedSize(24, 22);
        btnClose->setStyleSheet("padding:0;font-size:9pt;");
        connect(btnClose, &QPushButton::clicked, this, &QWidget::close);
        top->addWidget(btnClose);
        lay->addLayout(top);
        m_rows.clear();
        m_rows.push_back(Row("CPU", new QLabel(card), new QProgressBar(card)));
        m_rows.push_back(Row("RAM", new QLabel(card), new QProgressBar(card)));
        m_rows.push_back(Row("DISK", new QLabel(card), new QProgressBar(card)));
        m_rows.push_back(Row("NET", new QLabel(card), new QProgressBar(card)));
        for (auto &r : m_rows) {
            QHBoxLayout *hl = new QHBoxLayout();
            QLabel *nl = new QLabel(r.name, card);
            nl->setFixedWidth(40);
            nl->setStyleSheet("font-size:9pt;font-weight:600;");
            r.value->setStyleSheet("font-size:9pt;");
            r.value->setMinimumWidth(110);
            r.bar->setRange(0, 1000);
            r.bar->setTextVisible(false);
            r.bar->setFixedHeight(6);
            r.bar->setStyleSheet(
                "QProgressBar{background:rgba(127,127,127,0.25);border:none;border-radius:3px;}"
                "QProgressBar::chunk{background:#3c7df2;border-radius:3px;}");
            hl->addWidget(nl);
            hl->addWidget(r.value);
            hl->addWidget(r.bar, 1);
            lay->addLayout(hl);
        }
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &FloatWindow::refresh);
        m_timer->start(1000);
        refresh();
        move(40, 40);
    }
protected:
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) { m_drag = true; m_dragOff = e->globalPosition().toPoint() - frameGeometry().topLeft(); }
    }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (m_drag) move(e->globalPosition().toPoint() - m_dragOff);
    }
    void mouseReleaseEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton) m_drag = false;
    }
private:
    struct Row {
        QString name;
        QLabel *value;
        QProgressBar *bar;
        Row(const QString &n, QLabel *v, QProgressBar *b) : name(n), value(v), bar(b) {}
    };
    void refresh() {
        QcSample();
        setRow(0, QString("%1%").arg(QcCpuPct(), 0, 'f', 1), QcCpuPct());
        setRow(1, QString("%1%").arg(QcRamPct(), 0, 'f', 1), QcRamPct());
        setRow(2, QString("%1%").arg(QcDiskPct(), 0, 'f', 1), QcDiskPct());
        setRow(3, QString("v%1 / ^%2 MB/s").arg(QcNetDown() / 1048576.0, 0, 'f', 2)
                                            .arg(QcNetUp() / 1048576.0, 0, 'f', 2), 0.0);
    }
    void setRow(int i, const QString &txt, double pct) {
        m_rows[i].value->setText(txt);
        m_rows[i].bar->setValue((int)(pct * 10.0));
    }
    QWidget *m_main;
    QTimer *m_timer;
    std::vector<Row> m_rows;
    bool m_drag;
    QPoint m_dragOff;
};

static FloatWindow *g_floatWnd = nullptr;

class ProcessManagerDialog;
class ServicesManagerDialog;
class NetworkToolsDialog;
class ClipboardManagerDialog;
class ScreenshotDialog;
class SystemReportDialog;
class FavoritesDialog;
class WindowManagerDialog;
class StartupManagerDialog;
class TaskSchedulerDialog;
class NetworkConnectionsDialog;
class EnvVarsDialog;
class DiskSpaceDialog;
class FileHasherDialog;
class ColorPickerDialog;
class BatteryDialog;
class WindowsUpdateDialog;
class QuickLaunchDialog;
class NotesDialog;
class PasswordGeneratorDialog;
class RegistryEditorDialog;
class DeviceManagerDialog;
class InstalledAppsDialog;
class EventLogDialog;
class SystemInfoDialog;
class FirewallDialog;
class DriverManagerDialog;
class SystemRestoreDialog;
class SharedFoldersDialog;
class DiskManagerDialog;


class StartupManagerDialog : public QDialog {
public:
    StartupManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Startup Manager");
        resize(800, 500);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &StartupManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *delBtn = new QPushButton("Delete Selected", this);
        delBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(delBtn, &QPushButton::clicked, this, &StartupManagerDialog::deleteSelected);
        top->addWidget(delBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Name", "Command", "Location", "User"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QStringList users = {"HKCU", "HKLM"};
        QStringList paths = {
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
        };
        for (auto &user : users) {
            for (auto &path : paths) {
                HKEY root = (user == "HKCU") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
                QString full = user + "\\" + path;
                HKEY hKey;
                if (RegOpenKeyExW(root, (LPCWSTR)full.utf16(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                    DWORD idx = 0;
                    WCHAR name[256], value[1024];
                    DWORD nameLen, valueLen, type;
                    while (true) {
                        nameLen = 256;
                        valueLen = sizeof(value);
                        if (RegEnumValueW(hKey, idx++, name, &nameLen, nullptr, &type, (LPBYTE)value, &valueLen) != ERROR_SUCCESS) break;
                        if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
                        QString n = QString::fromWCharArray(name, nameLen);
                        QString v = QString::fromWCharArray(value, valueLen / 2);
                        m_table->insertRow(row);
                        m_table->setItem(row, 0, new QTableWidgetItem(n));
                        m_table->setItem(row, 1, new QTableWidgetItem(v));
                        m_table->setItem(row, 2, new QTableWidgetItem(full));
                        m_table->setItem(row, 3, new QTableWidgetItem(user));
                        row++;
                    }
                    RegCloseKey(hKey);
                }
            }
        }
        QString startupDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                             + "\\..\\..\\Microsoft\\Windows\\Start Menu\\Programs\\Startup";
        QDirIterator it(startupDir, QDir::Files);
        while (it.hasNext()) {
            QString fp = it.next();
            QString n = QFileInfo(fp).fileName();
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(n));
            m_table->setItem(row, 1, new QTableWidgetItem(fp));
            m_table->setItem(row, 2, new QTableWidgetItem("Startup Folder"));
            m_table->setItem(row, 3, new QTableWidgetItem("All Users"));
            row++;
        }
        m_count->setText(QString("%1 startup entries").arg(row));
    }
    void deleteSelected() {
        auto sel = m_table->selectionModel()->selectedRows();
        for (auto it = sel.rbegin(); it != sel.rend(); ++it) {
            QString loc = m_table->item(it->row(), 2)->text();
            QString name = m_table->item(it->row(), 0)->text();
            if (loc.count("Startup")) {
                QFile::remove(m_table->item(it->row(), 1)->text());
            } else {
                HKEY root = loc.startsWith("HKCU") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
                QString subkey = loc.mid(loc.indexOf("\\") + 1);
                HKEY hKey;
                if (RegOpenKeyExW(root, (LPCWSTR)subkey.utf16(), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                    RegDeleteValueW(hKey, (LPCWSTR)name.utf16());
                    RegCloseKey(hKey);
                }
            }
        }
        load();
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class EnvVarsDialog : public QDialog {
public:
    EnvVarsDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Environment Variables");
        resize(750, 480);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &EnvVarsDialog::load);
        top->addWidget(refresh);
        QPushButton *addBtn = new QPushButton("Add Variable", this);
        connect(addBtn, &QPushButton::clicked, this, &EnvVarsDialog::addVar);
        top->addWidget(addBtn);
        QPushButton *delBtn = new QPushButton("Delete Selected", this);
        delBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(delBtn, &QPushButton::clicked, this, &EnvVarsDialog::delVar);
        top->addWidget(delBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(3);
        m_table->setHorizontalHeaderLabels({"Variable", "Value", "Scope"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        loadFromEnv("HKCU\\Environment", "User", row);
        WCHAR sysPath[MAX_PATH];
        GetSystemDirectoryW(sysPath, MAX_PATH);
        QString sysEnv = QString::fromWCharArray(sysPath) + "\\config\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
        loadFromEnv(sysEnv, "System", row);
        m_count->setText(QString("%1 variables").arg(row));
    }
    void loadFromEnv(const QString &subkey, const QString &scope, int &row) {
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, (LPCWSTR)subkey.utf16(), 0, KEY_READ, &hKey) != ERROR_SUCCESS &&
            RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LPCWSTR)subkey.utf16(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
        DWORD idx = 0;
        WCHAR name[1024], value[32768];
        DWORD nameLen, valueLen, type;
        while (true) {
            nameLen = 1024;
            valueLen = sizeof(value);
            if (RegEnumValueW(hKey, idx++, name, &nameLen, nullptr, &type, (LPBYTE)value, &valueLen) != ERROR_SUCCESS) break;
            if (type != REG_SZ && type != REG_EXPAND_SZ) continue;
            QString n = QString::fromWCharArray(name, nameLen);
            QString v = QString::fromWCharArray(value, valueLen / 2);
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(n));
            m_table->setItem(row, 1, new QTableWidgetItem(v));
            m_table->setItem(row, 2, new QTableWidgetItem(scope));
            row++;
        }
        RegCloseKey(hKey);
    }
    void addVar() {
        bool ok1, ok2;
        QString name = QInputDialog::getText(this, "Add Variable", "Variable name:", QLineEdit::Normal, "", &ok1);
        if (!ok1 || name.isEmpty()) return;
        QString value = QInputDialog::getText(this, "Add Variable", "Value:", QLineEdit::Normal, "", &ok2);
        if (!ok2) return;
        HKEY hKey;
        if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
            QByteArray vba = value.toUtf8();
            RegSetValueExA(hKey, name.toUtf8().constData(), 0, REG_SZ, (BYTE*)vba.constData(), vba.size() + 1);
            RegCloseKey(hKey);
        }
        load();
    }
    void delVar() {
        auto sel = m_table->selectionModel()->selectedRows();
        for (auto &idx : sel) {
            QString name = m_table->item(idx.row(), 0)->text();
            QString scope = m_table->item(idx.row(), 2)->text();
            HKEY root = (scope == "User") ? HKEY_CURRENT_USER : HKEY_LOCAL_MACHINE;
            HKEY hKey;
            QString subkey = (scope == "User") ? "Environment" :
                QString::fromUtf8(qgetenv("SystemRoot")) + "\\System32\\Config\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment";
            if (RegOpenKeyExW(root, (LPCWSTR)subkey.utf16(), 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
                RegDeleteValueW(hKey, (LPCWSTR)name.utf16());
                RegCloseKey(hKey);
            }
        }
        load();
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class PasswordGeneratorDialog : public QDialog {
public:
    PasswordGeneratorDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Password Generator");
        resize(480, 320);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *lenRow = new QHBoxLayout();
        lenRow->addWidget(new QLabel("Length:", this));
        m_len = new QSpinBox(this);
        m_len->setRange(4, 128);
        m_len->setValue(16);
        lenRow->addWidget(m_len);
        m_upper = new QCheckBox("A-Z", this); m_upper->setChecked(true);
        m_lower = new QCheckBox("a-z", this); m_lower->setChecked(true);
        m_digits = new QCheckBox("0-9", this); m_digits->setChecked(true);
        m_symbols = new QCheckBox("!@#$%", this); m_symbols->setChecked(true);
        lenRow->addWidget(m_upper);
        lenRow->addWidget(m_lower);
        lenRow->addWidget(m_digits);
        lenRow->addWidget(m_symbols);
        lay->addLayout(lenRow);
        m_output = new QLineEdit(this);
        m_output->setReadOnly(true);
        QFont mono("Consolas", 14);
        m_output->setFont(mono);
        m_output->setMinimumHeight(40);
        lay->addWidget(m_output);
        QHBoxLayout *btns = new QHBoxLayout();
        QPushButton *gen = new QPushButton("Generate", this);
        connect(gen, &QPushButton::clicked, this, &PasswordGeneratorDialog::generate);
        btns->addWidget(gen);
        QPushButton *copy = new QPushButton("Copy", this);
        connect(copy, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(m_output->text());
        });
        btns->addWidget(copy);
        QPushButton *gen5 = new QPushButton("Generate 5", this);
        connect(gen5, &QPushButton::clicked, this, [this]() {
            QString all;
            for (int i = 0; i < 5; i++) { generate(); all += m_output->text() + "\n"; }
            m_output->setText(all.trimmed());
        });
        btns->addWidget(gen5);
        lay->addLayout(btns);
        m_strength = new QLabel(this);
        lay->addWidget(m_strength);
        lay->addStretch();
        generate();
    }
    void generate() {
        QString chars;
        if (m_upper->isChecked()) chars += "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        if (m_lower->isChecked()) chars += "abcdefghijklmnopqrstuvwxyz";
        if (m_digits->isChecked()) chars += "0123456789";
        if (m_symbols->isChecked()) chars += "!@#$%^&*()_+-=[]{}|;:,.<>?";
        if (chars.isEmpty()) chars = "abcdefghijklmnopqrstuvwxyz";
        int len = m_len->value();
        QString pw;
        for (int i = 0; i < len; i++)
            pw += chars[QRandomGenerator::global()->bounded(chars.size())];
        m_output->setText(pw);
        double entropy = len * log2((double)chars.size());
        QString level = entropy < 40 ? "Weak" : entropy < 60 ? "Fair" : entropy < 80 ? "Strong" : "Very Strong";
        QColor color = entropy < 40 ? Qt::red : entropy < 60 ? QColor(0xff, 0xaa, 0x00) : entropy < 80 ? QColor(0x00, 0xcc, 0x66) : Qt::green;
        m_strength->setText(QString("Strength: %1 (%2 bits of entropy)").arg(level).arg((int)entropy));
        m_strength->setStyleSheet(QString("color:%1;font-weight:bold;").arg(color.name()));
    }
private:
    QSpinBox *m_len;
    QCheckBox *m_upper, *m_lower, *m_digits, *m_symbols;
    QLineEdit *m_output;
    QLabel *m_strength;
};

class NotesDialog : public QDialog {
public:
    NotesDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Notes / Scratchpad");
        resize(650, 450);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *save = new QPushButton("Save", this);
        connect(save, &QPushButton::clicked, this, &NotesDialog::save);
        top->addWidget(save);
        QPushButton *open = new QPushButton("Open", this);
        connect(open, &QPushButton::clicked, this, &NotesDialog::openFile);
        top->addWidget(open);
        QPushButton *clear = new QPushButton("Clear", this);
        connect(clear, &QPushButton::clicked, this, [this]() { m_text->clear(); });
        top->addWidget(clear);
        QLabel *info = new QLabel("Auto-saved to %APPDATA%\\WCNotes.txt", this);
        info->setStyleSheet("color:rgba(127,127,127,0.7);font-size:9pt;");
        top->addWidget(info);
        lay->addLayout(top);
        m_text = new QPlainTextEdit(this);
        QFont mono("Consolas", 10);
        m_text->setFont(mono);
        lay->addWidget(m_text, 1);
        load();
    }
    QString notePath() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/notes.txt"; }
    void load() {
        QFile f(notePath());
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            m_text->setPlainText(QString::fromUtf8(f.readAll()));
    }
    void save() {
        QFile f(notePath());
        QDir().mkpath(QFileInfo(f).absolutePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
            f.write(m_text->toPlainText().toUtf8());
    }
    void openFile() {
        QString fp = QFileDialog::getOpenFileName(this, "Open Text File", "", "Text (*.txt);;All (*)");
        if (fp.isEmpty()) return;
        QFile f(fp);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            m_text->setPlainText(QString::fromUtf8(f.readAll()));
    }
private:
    QPlainTextEdit *m_text;
};

class ColorPickerDialog : public QDialog {
public:
    ColorPickerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Color Picker");
        resize(420, 350);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_preview = new QFrame(this);
        m_preview->setMinimumHeight(80);
        m_preview->setStyleSheet("background:#3c7df2;border-radius:8px;");
        lay->addWidget(m_preview);
        QHBoxLayout *row = new QHBoxLayout();
        row->addWidget(new QLabel("HEX:", this));
        m_hex = new QLineEdit("#3c7df2", this);
        connect(m_hex, &QLineEdit::textChanged, this, &ColorPickerDialog::fromHex);
        row->addWidget(m_hex);
        QPushButton *pick = new QPushButton("Pick from Screen", this);
        connect(pick, &QPushButton::clicked, this, &ColorPickerDialog::pickScreen);
        row->addWidget(pick);
        lay->addLayout(row);
        QHBoxLayout *rgb = new QHBoxLayout();
        m_r = new QSpinBox(this); m_r->setRange(0, 255); m_r->setValue(60);
        m_g = new QSpinBox(this); m_g->setRange(0, 255); m_g->setValue(125);
        m_b = new QSpinBox(this); m_b->setRange(0, 255); m_b->setValue(242);
        rgb->addWidget(new QLabel("R:", this)); rgb->addWidget(m_r);
        rgb->addWidget(new QLabel("G:", this)); rgb->addWidget(m_g);
        rgb->addWidget(new QLabel("B:", this)); rgb->addWidget(m_b);
        QPushButton *copyBtn = new QPushButton("Copy", this);
        connect(copyBtn, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(m_hex->text());
        });
        rgb->addWidget(copyBtn);
        lay->addLayout(rgb);
        connect(m_r, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::fromRgb);
        connect(m_g, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::fromRgb);
        connect(m_b, QOverload<int>::of(&QSpinBox::valueChanged), this, &ColorPickerDialog::fromRgb);
        lay->addStretch();
    }
    void fromHex() {
        QColor c(m_hex->text());
        if (!c.isValid()) return;
        m_r->blockSignals(true); m_g->blockSignals(true); m_b->blockSignals(true);
        m_r->setValue(c.red()); m_g->setValue(c.green()); m_b->setValue(c.blue());
        m_r->blockSignals(false); m_g->blockSignals(false); m_b->blockSignals(false);
        m_preview->setStyleSheet(QString("background:%1;border-radius:8px;").arg(c.name()));
    }
    void fromRgb() {
        QColor c(m_r->value(), m_g->value(), m_b->value());
        m_hex->blockSignals(true);
        m_hex->setText(c.name());
        m_hex->blockSignals(false);
        m_preview->setStyleSheet(QString("background:%1;border-radius:8px;").arg(c.name()));
    }
    void pickScreen() {
        hide();
        QTimer::singleShot(300, this, [this]() {
            QScreen *screen = QGuiApplication::primaryScreen();
            if (!screen) { show(); return; }
            QPixmap pixmap = screen->grabWindow(0);
            QPoint center = screen->geometry().center();
            QColor c = pixmap.toImage().pixelColor(center);
            m_r->setValue(c.red()); m_g->setValue(c.green()); m_b->setValue(c.blue());
            m_hex->setText(c.name());
            show(); raise();
        });
    }
private:
    QFrame *m_preview;
    QLineEdit *m_hex;
    QSpinBox *m_r, *m_g, *m_b;
};

class FileHasherDialog : public QDialog {
public:
    FileHasherDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("File Hasher");
        resize(600, 380);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_file = new QLineEdit(this);
        m_file->setPlaceholderText("Select a file to hash...");
        top->addWidget(m_file);
        QPushButton *browse = new QPushButton("Browse", this);
        connect(browse, &QPushButton::clicked, this, [this]() {
            QString fp = QFileDialog::getOpenFileName(this, "Select File");
            if (!fp.isEmpty()) m_file->setText(fp);
        });
        top->addWidget(browse);
        QPushButton *hashBtn = new QPushButton("Calculate", this);
        connect(hashBtn, &QPushButton::clicked, this, &FileHasherDialog::calculate);
        top->addWidget(hashBtn);
        lay->addLayout(top);
        m_result = new QPlainTextEdit(this);
        m_result->setReadOnly(true);
        QFont mono("Consolas", 10);
        m_result->setFont(mono);
        lay->addWidget(m_result, 1);
        QPushButton *copyAll = new QPushButton("Copy All", this);
        connect(copyAll, &QPushButton::clicked, this, [this]() {
            QApplication::clipboard()->setText(m_result->toPlainText());
        });
        lay->addWidget(copyAll);
    }
    void calculate() {
        QString fp = m_file->text();
        QFile f(fp);
        if (!f.open(QIODevice::ReadOnly)) { m_result->setPlainText("Cannot open file"); return; }
        QCryptographicHash md5(QCryptographicHash::Md5);
        QCryptographicHash sha1(QCryptographicHash::Sha1);
        QCryptographicHash sha256(QCryptographicHash::Sha256);
        qint64 sz = f.size();
        QByteArray data = f.readAll();
        md5.addData(data); sha1.addData(data); sha256.addData(data);
        QString out = QString("File: %1\nSize: %2 bytes (%3 MB)\n\nMD5:    %4\nSHA-1:  %5\nSHA-256: %6")
            .arg(fp).arg(sz).arg(sz / 1048576.0, 0, 'f', 2)
            .arg(md5.result().toHex())
            .arg(sha1.result().toHex())
            .arg(sha256.result().toHex());
        m_result->setPlainText(out);
    }
private:
    QLineEdit *m_file;
    QPlainTextEdit *m_result;
};

class DiskSpaceDialog : public QDialog {
public:
    DiskSpaceDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Disk Space Analyzer");
        resize(600, 400);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({"Drive", "Total", "Free", "Used", "Usage"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_chart = new QFrame(this);
        m_chart->setMinimumHeight(60);
        lay->addWidget(m_chart);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &DiskSpaceDialog::load);
        lay->addWidget(refresh);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        for (char d = 'A'; d <= 'Z'; d++) {
            QString drive = QString("%1:\\").arg(d);
            ULARGE_INTEGER freeBytes, totalBytes, availBytes;
            if (GetDiskFreeSpaceExW((LPCWSTR)drive.utf16(), &availBytes, &totalBytes, &freeBytes)) {
                double totalGB = totalBytes.QuadPart / 1073741824.0;
                double freeGB = freeBytes.QuadPart / 1073741824.0;
                double usedGB = totalGB - freeGB;
                double pct = totalGB > 0 ? usedGB / totalGB * 100.0 : 0;
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(drive));
                m_table->setItem(row, 1, new QTableWidgetItem(QString("%1 GB").arg(totalGB, 0, 'f', 1)));
                m_table->setItem(row, 2, new QTableWidgetItem(QString("%1 GB").arg(freeGB, 0, 'f', 1)));
                m_table->setItem(row, 3, new QTableWidgetItem(QString("%1 GB").arg(usedGB, 0, 'f', 1)));
                QTableWidgetItem *pctItem = new QTableWidgetItem(QString("%1%").arg(pct, 0, 'f', 1));
                if (pct > 90) pctItem->setForeground(Qt::red);
                else if (pct > 70) pctItem->setForeground(QColor(0xff, 0xaa, 0x00));
                else pctItem->setForeground(QColor(0x00, 0xcc, 0x66));
                m_table->setItem(row, 4, pctItem);
                row++;
            }
        }
    }
private:
    QTableWidget *m_table;
    QFrame *m_chart;
};

class NetworkConnectionsDialog : public QDialog {
public:
    NetworkConnectionsDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Network Connections");
        resize(750, 480);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &NetworkConnectionsDialog::load);
        top->addWidget(refresh);
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText("Filter...");
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) { load(); });
        top->addWidget(m_filter);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({"Protocol", "Local", "Remote", "State", "PID"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QString filt = m_filter->text().trimmed().toLower();
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        std::map<DWORD, QString> pidNames;
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe{}; pe.dwSize = sizeof(pe);
            if (Process32First(snap, &pe)) do {
                pidNames[pe.th32ProcessID] = QString::fromWCharArray(pe.szExeFile);
            } while (Process32Next(snap, &pe));
            CloseHandle(snap);
        }
        PMIB_TCPTABLE_OWNER_MODULE tcpTable = nullptr;
        DWORD size = 0;
        GetExtendedTcpTable(nullptr, &size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);
        std::vector<BYTE> tcpBuf(size);
        if (GetExtendedTcpTable(tcpBuf.data(), &size, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR) {
            auto *table = (PMIB_TCPTABLE_OWNER_MODULE)tcpBuf.data();
            for (DWORD i = 0; i < table->dwNumEntries; i++) {
                auto &e = table->table[i];
                QString local = fmtAddr(e.dwLocalAddr, ntohs(e.dwLocalPort));
                QString remote = fmtAddr(e.dwRemoteAddr, ntohs(e.dwRemotePort));
                QString state;
                switch (e.dwState) {
                    case MIB_TCP_STATE_LISTEN: state = "Listen"; break;
                    case MIB_TCP_STATE_ESTABLISHED: state = "Established"; break;
                    case MIB_TCP_STATE_TIME_WAIT: state = "Time Wait"; break;
                    case MIB_TCP_STATE_CLOSE_WAIT: state = "Close Wait"; break;
                    default: state = "Other"; break;
                }
                QString proc = pidNames.count(e.dwOwningPid) ? pidNames[e.dwOwningPid] : QString::number(e.dwOwningPid);
                if (!filt.isEmpty() && !local.toLower().count(filt) && !remote.toLower().count(filt) &&
                    !state.toLower().count(filt) && !proc.toLower().count(filt)) continue;
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem("TCP"));
                m_table->setItem(row, 1, new QTableWidgetItem(local));
                m_table->setItem(row, 2, new QTableWidgetItem(remote));
                m_table->setItem(row, 3, new QTableWidgetItem(state));
                m_table->setItem(row, 4, new QTableWidgetItem(proc));
                row++;
            }
        }
        m_count->setText(QString("%1 connections").arg(row));
    }
    QString fmtAddr(DWORD addr, WORD port) {
        IN_ADDR ia; ia.s_addr = addr;
        char buf[32];
        inet_ntop(AF_INET, &ia, buf, sizeof(buf));
        return QString("%1:%2").arg(QString::fromUtf8(buf)).arg(port);
    }
private:
    QTableWidget *m_table;
    QLineEdit *m_filter;
    QLabel *m_count;
};

class BatteryDialog : public QDialog {
public:
    BatteryDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Battery Info");
        resize(420, 320);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_text = new QPlainTextEdit(this);
        m_text->setReadOnly(true);
        QFont mono("Consolas", 10);
        m_text->setFont(mono);
        lay->addWidget(m_text, 1);
        QHBoxLayout *bot = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &BatteryDialog::load);
        bot->addWidget(refresh);
        QPushButton *full = new QPushButton("Full Report", this);
        connect(full, &QPushButton::clicked, this, &BatteryDialog::fullReport);
        bot->addWidget(full);
        lay->addLayout(bot);
        load();
    }
    void load() {
        QProcess proc;
        proc.start("powercfg", {"/batteryreport", "/output", "NUL"});
        proc.waitForFinished(5000);
        QString out = QString::fromUtf8(proc.readAllStandardOutput());
        QString err = QString::fromUtf8(proc.readAllStandardError());

        QFile bcFile("battery-report.html");
        if (bcFile.exists()) {
            bcFile.remove();
        }

        QProcess proc2;
        proc2.start("WMIC", {"Path", "Win32_Battery", "get", "EstimatedChargeRemaining,BatteryStatus,Status"});
        proc2.waitForFinished(5000);
        QString wmOut = QString::fromUtf8(proc2.readAll());
        if (wmOut.trimmed().isEmpty() || wmOut.contains("No Instance")) {
            m_text->setPlainText("No battery detected.\n(This device may be a desktop or VM.)");
        } else {
            m_text->setPlainText("Battery Information:\n\n" + wmOut.trimmed());
        }
    }
    void fullReport() {
        QProcess proc;
        proc.start("powercfg", {"/batteryreport", "/output", "battery_report.html"});
        proc.waitForFinished(5000);
        QString path = QDir::currentPath() + "/battery_report.html";
        if (QFile::exists(path)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(path));
            m_text->appendPlainText("\nBattery report saved to: " + path);
        } else {
            m_text->appendPlainText("\nFailed to generate battery report.");
        }
    }
private:
    QPlainTextEdit *m_text;
};

class WindowsUpdateDialog : public QDialog {
public:
    WindowsUpdateDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Windows Update");
        resize(600, 400);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_text = new QPlainTextEdit(this);
        m_text->setReadOnly(true);
        QFont mono("Consolas", 10);
        m_text->setFont(mono);
        lay->addWidget(m_text, 1);
        QHBoxLayout *bot = new QHBoxLayout();
        QPushButton *check = new QPushButton("Check for Updates", this);
        connect(check, &QPushButton::clicked, this, &WindowsUpdateDialog::checkUpdates);
        bot->addWidget(check);
        QPushButton *history = new QPushButton("Update History", this);
        connect(history, &QPushButton::clicked, this, &WindowsUpdateDialog::showHistory);
        bot->addWidget(history);
        lay->addLayout(bot);
        checkUpdates();
    }
    void checkUpdates() {
        m_text->setPlainText("Checking for updates...\nThis may take a minute...");
        QApplication::processEvents();
        QProcess proc;
        proc.start("powershell", {"-Command", "Get-WindowsUpdate -ErrorAction SilentlyContinue | Format-List Title,KB,Size,Severity"});
        proc.waitForFinished(60000);
        QString out = QString::fromUtf8(proc.readAllStandardOutput());
        QString err = QString::fromUtf8(proc.readAllStandardError());
        if (out.trimmed().isEmpty()) {
            m_text->setPlainText("No pending updates found.\n\nAlternative check via USOClient:\n");
            QProcess uso;
            uso.start("UsoClient", {"StartScan"});
            uso.waitForFinished(30000);
            m_text->appendPlainText("Update scan triggered. Check Settings > Windows Update for details.");
        } else {
            m_text->setPlainText(out);
        }
    }
    void showHistory() {
        QProcess proc;
        proc.start("powershell", {"-Command", "Get-HotFix | Sort-Object InstalledOn -Descending | Select-Object -First 20 HotFixID,Description,InstalledOn | Format-Table -AutoSize"});
        proc.waitForFinished(30000);
        m_text->setPlainText(QString::fromUtf8(proc.readAll()));
    }
private:
    QPlainTextEdit *m_text;
};

class TaskSchedulerDialog : public QDialog {
public:
    TaskSchedulerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Task Scheduler");
        resize(750, 480);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText("Filter tasks...");
        top->addWidget(m_filter);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &TaskSchedulerDialog::load);
        top->addWidget(refresh);
        QPushButton *runBtn = new QPushButton("Run Selected", this);
        connect(runBtn, &QPushButton::clicked, this, &TaskSchedulerDialog::runTask);
        top->addWidget(runBtn);
        QPushButton *disableBtn = new QPushButton("Disable", this);
        connect(disableBtn, &QPushButton::clicked, this, &TaskSchedulerDialog::disableTask);
        top->addWidget(disableBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Task Name", "Status", "Next Run", "Last Run"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) { load(); });
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QString filt = m_filter->text().trimmed().toLower();
        QProcess proc;
        proc.start("schtasks", {"/query", "/fo", "csv", "/nh"});
        proc.waitForFinished(15000);
        QStringList lines = QString::fromUtf8(proc.readAll()).split('\n', Qt::SkipEmptyParts);
        for (auto &line : lines) {
            QStringList fields = line.split("\",\"");
            if (fields.size() < 4) continue;
            for (auto &f : fields) f.remove('"');
            QString name = fields[0].trimmed();
            QString status = fields.size() > 1 ? fields[1].trimmed() : "";
            QString nextRun = fields.size() > 2 ? fields[2].trimmed() : "";
            QString lastRun = fields.size() > 3 ? fields[3].trimmed() : "";
            if (!filt.isEmpty() && !name.toLower().count(filt)) continue;
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(name));
            m_table->setItem(row, 1, new QTableWidgetItem(status));
            m_table->setItem(row, 2, new QTableWidgetItem(nextRun));
            m_table->setItem(row, 3, new QTableWidgetItem(lastRun));
            row++;
        }
        m_count->setText(QString("%1 tasks").arg(row));
    }
    QString selectedTask() {
        auto sel = m_table->selectionModel()->selectedRows();
        return sel.isEmpty() ? "" : m_table->item(sel.first().row(), 0)->text();
    }
    void runTask() {
        QString name = selectedTask();
        if (name.isEmpty()) return;
        QProcess::execute("schtasks", {"/run", "/tn", "\"" + name + "\""});
        load();
    }
    void disableTask() {
        QString name = selectedTask();
        if (name.isEmpty()) return;
        QProcess::execute("schtasks", {"/change", "/tn", "\"" + name + "\"", "/disable"});
        load();
    }
private:
    QTableWidget *m_table;
    QLineEdit *m_filter;
    QLabel *m_count;
};

class QuickLaunchDialog : public QDialog {
public:
    QuickLaunchDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Quick Launch");
        resize(500, 400);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_list = new QListWidget(this);
        m_list->setAlternatingRowColors(true);
        lay->addWidget(m_list, 1);
        QHBoxLayout *bot = new QHBoxLayout();
        QPushButton *addBtn = new QPushButton("Add App", this);
        connect(addBtn, &QPushButton::clicked, this, &QuickLaunchDialog::addApp);
        bot->addWidget(addBtn);
        QPushButton *runBtn = new QPushButton("Run", this);
        connect(runBtn, &QPushButton::clicked, this, &QuickLaunchDialog::runApp);
        bot->addWidget(runBtn);
        QPushButton *delBtn = new QPushButton("Remove", this);
        delBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(delBtn, &QPushButton::clicked, this, &QuickLaunchDialog::removeApp);
        bot->addWidget(delBtn);
        lay->addLayout(bot);
        load();
    }
    QString qlPath() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/quicklaunch.json"; }
    void load() {
        QFile f(qlPath());
        m_list->clear();
        m_apps.clear();
        if (f.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
            for (auto v : doc.array()) {
                QJsonObject obj = v.toObject();
                QString name = obj["name"].toString();
                QString path = obj["path"].toString();
                m_apps.push_back({name, path});
                m_list->addItem(QString("%1  [%2]").arg(name, path));
            }
        }
    }
    void save() {
        QJsonArray arr;
        for (auto &a : m_apps) {
            QJsonObject obj;
            obj["name"] = a.first;
            obj["path"] = a.second;
            arr.append(obj);
        }
        QFile f(qlPath());
        QDir().mkpath(QFileInfo(f).absolutePath());
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(arr).toJson());
    }
    void addApp() {
        QString fp = QFileDialog::getOpenFileName(this, "Select Executable", "", "Exe (*.exe);;All (*)");
        if (fp.isEmpty()) return;
        QString name = QFileInfo(fp).baseName();
        bool ok;
        name = QInputDialog::getText(this, "App Name", "Display name:", QLineEdit::Normal, name, &ok);
        if (!ok || name.isEmpty()) return;
        m_apps.push_back({name, fp});
        save();
        load();
    }
    void runApp() {
        int row = m_list->currentRow();
        if (row < 0 || row >= (int)m_apps.size()) return;
        QDesktopServices::openUrl(QUrl::fromLocalFile(m_apps[row].second));
    }
    void removeApp() {
        int row = m_list->currentRow();
        if (row < 0 || row >= (int)m_apps.size()) return;
        m_apps.erase(m_apps.begin() + row);
        save();
        load();
    }
private:
    QListWidget *m_list;
    std::vector<QPair<QString,QString>> m_apps;
};

class WindowManagerDialog : public QDialog {
public:
    WindowManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Window Manager");
        resize(700, 450);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &WindowManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *closeBtn = new QPushButton("Close", this);
        closeBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(closeBtn, &QPushButton::clicked, this, &WindowManagerDialog::closeWindow);
        top->addWidget(closeBtn);
        QPushButton *minBtn = new QPushButton("Minimize", this);
        connect(minBtn, &QPushButton::clicked, this, &WindowManagerDialog::minimizeWindow);
        top->addWidget(minBtn);
        QPushButton *maxBtn = new QPushButton("Maximize", this);
        connect(maxBtn, &QPushButton::clicked, this, &WindowManagerDialog::maximizeWindow);
        top->addWidget(maxBtn);
        QPushButton *activateBtn = new QPushButton("Activate", this);
        connect(activateBtn, &QPushButton::clicked, this, &WindowManagerDialog::activateWindow);
        top->addWidget(activateBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(3);
        m_table->setHorizontalHeaderLabels({"Title", "PID", "Handle"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        m_handles.clear();
        struct EnumData { WindowManagerDialog *self; int row; };
        EnumData data{this, 0};
        EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
            if (!IsWindowVisible(hwnd)) return TRUE;
            WCHAR title[512];
            int len = GetWindowTextW(hwnd, title, 512);
            if (len == 0) return TRUE;
            DWORD pid;
            GetWindowThreadProcessId(hwnd, &pid);
            auto *d = reinterpret_cast<EnumData*>(lParam);
            d->self->m_table->insertRow(d->row);
            d->self->m_table->setItem(d->row, 0, new QTableWidgetItem(QString::fromWCharArray(title, len)));
            d->self->m_table->setItem(d->row, 1, new QTableWidgetItem(QString::number(pid)));
            d->self->m_table->setItem(d->row, 2, new QTableWidgetItem(QString::number((quintptr)hwnd)));
            d->self->m_handles.push_back(hwnd);
            d->row++;
            return TRUE;
        }, reinterpret_cast<LPARAM>(&data));
        m_count->setText(QString("%1 windows").arg(m_table->rowCount()));
    }
    HWND selectedHwnd() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return nullptr;
        int row = sel.first().row();
        return row < (int)m_handles.size() ? m_handles[row] : nullptr;
    }
    void closeWindow() { HWND h = selectedHwnd(); if (h) PostMessage(h, WM_CLOSE, 0, 0); }
    void minimizeWindow() { HWND h = selectedHwnd(); if (h) ShowWindow(h, SW_MINIMIZE); }
    void maximizeWindow() { HWND h = selectedHwnd(); if (h) ShowWindow(h, SW_MAXIMIZE); }
    void activateWindow() { HWND h = selectedHwnd(); if (h) { SetForegroundWindow(h); ShowWindow(h, SW_RESTORE); } }
private:
    QTableWidget *m_table;
    QLabel *m_count;
    std::vector<HWND> m_handles;
};



class RegistryEditorDialog : public QDialog {
public:
    RegistryEditorDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Registry Editor");
        resize(800, 520);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_path = new QLineEdit(this);
        m_path->setPlaceholderText("Registry path (e.g. HKCU\\Software\\Microsoft\\Windows)");
        top->addWidget(m_path);
        QPushButton *go = new QPushButton("Go", this);
        connect(go, &QPushButton::clicked, this, &RegistryEditorDialog::navigate);
        top->addWidget(go);
        QPushButton *newKey = new QPushButton("New Key", this);
        connect(newKey, &QPushButton::clicked, this, &RegistryEditorDialog::createKey);
        top->addWidget(newKey);
        QPushButton *delKey = new QPushButton("Delete", this);
        delKey->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(delKey, &QPushButton::clicked, this, &RegistryEditorDialog::deleteKey);
        top->addWidget(delKey);
        lay->addLayout(top);
        QSplitter *split = new QSplitter(Qt::Horizontal, this);
        m_tree = new QTreeWidget(this);
        m_tree->setHeaderLabel("Keys");
        m_tree->setMinimumWidth(280);
        connect(m_tree, &QTreeWidget::currentItemChanged, this, &RegistryEditorDialog::onKeySelected);
        split->addWidget(m_tree);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(3);
        m_table->setHorizontalHeaderLabels({"Name", "Type", "Value"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        split->addWidget(m_table);
        split->setStretchFactor(0, 1);
        split->setStretchFactor(1, 2);
        lay->addWidget(split, 1);
        loadRootKeys();
    }
    void loadRootKeys() {
        m_tree->clear();
        QStringList roots = {"HKEY_LOCAL_MACHINE", "HKEY_CURRENT_USER", "HKEY_CLASSES_ROOT", "HKEY_USERS", "HKEY_CURRENT_CONFIG"};
        for (auto &r : roots) {
            QTreeWidgetItem *item = new QTreeWidgetItem(m_tree);
            item->setText(0, r);
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            loadSubKeys(item, r);
        }
    }
    void loadSubKeys(QTreeWidgetItem *parent, const QString &path) {
        HKEY root = getRoot(path);
        QString subkey = path.mid(path.indexOf("\\") + 1);
        HKEY hKey;
        if (RegOpenKeyExW(root, (LPCWSTR)subkey.utf16(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD idx = 0;
            WCHAR name[256];
            DWORD nameLen;
            while (idx < 100) {
                nameLen = 256;
                if (RegEnumKeyExW(hKey, idx++, name, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
                QTreeWidgetItem *item = new QTreeWidgetItem(parent);
                item->setText(0, QString::fromWCharArray(name, nameLen));
                item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            }
            RegCloseKey(hKey);
        }
    }
    HKEY getRoot(const QString &path) {
        if (path.startsWith("HKEY_LOCAL_MACHINE")) return HKEY_LOCAL_MACHINE;
        if (path.startsWith("HKEY_CURRENT_USER")) return HKEY_CURRENT_USER;
        if (path.startsWith("HKEY_CLASSES_ROOT")) return HKEY_CLASSES_ROOT;
        if (path.startsWith("HKEY_USERS")) return HKEY_USERS;
        if (path.startsWith("HKEY_CURRENT_CONFIG")) return HKEY_CURRENT_CONFIG;
        return HKEY_CURRENT_USER;
    }
    QString fullPath(QTreeWidgetItem *item) {
        QStringList parts;
        while (item) { parts.prepend(item->text(0)); item = item->parent(); }
        return parts.join("\\");
    }
    void onKeySelected(QTreeWidgetItem *item, QTreeWidgetItem *) {
        if (!item) return;
        QString path = fullPath(item);
        m_path->setText(path);
        HKEY root = getRoot(path);
        QString subkey = path.mid(path.indexOf("\\") + 1);
        HKEY hKey;
        m_table->setRowCount(0);
        if (RegOpenKeyExW(root, (LPCWSTR)subkey.utf16(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            DWORD idx = 0;
            WCHAR name[256];
            DWORD nameLen, valueLen, type;
            BYTE value[4096];
            while (true) {
                nameLen = 256;
                valueLen = sizeof(value);
                if (RegEnumValueW(hKey, idx++, name, &nameLen, nullptr, &type, value, &valueLen) != ERROR_SUCCESS) break;
                QString n = QString::fromWCharArray(name, nameLen);
                QString typeName;
                QString v;
                switch (type) {
                    case REG_SZ: typeName = "REG_SZ"; v = QString::fromWCharArray((LPCWSTR)value); break;
                    case REG_EXPAND_SZ: typeName = "REG_EXPAND_SZ"; v = QString::fromWCharArray((LPCWSTR)value); break;
                    case REG_DWORD: typeName = "REG_DWORD"; v = QString::number(*(DWORD*)value); break;
                    case REG_QWORD: typeName = "REG_QWORD"; v = QString::number(*(ULONGLONG*)value); break;
                    case REG_BINARY: typeName = "REG_BINARY"; v = QString("%1 bytes").arg(valueLen); break;
                    default: typeName = QString("Type %1").arg(type); v = "binary"; break;
                }
                int row = m_table->rowCount();
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(n));
                m_table->setItem(row, 1, new QTableWidgetItem(typeName));
                m_table->setItem(row, 2, new QTableWidgetItem(v));
            }
            RegCloseKey(hKey);
        }
    }
    void navigate() {
        QString path = m_path->text().trimmed();
        if (path.isEmpty()) return;
        QStringList parts = path.split("\\");
        QTreeWidgetItem *item = m_tree->topLevelItem(0);
        for (int i = 1; i < parts.size() && item; i++) {
            bool found = false;
            for (int j = 0; j < item->childCount(); j++) {
                if (item->child(j)->text(0) == parts[i]) {
                    item = item->child(j);
                    found = true;
                    break;
                }
            }
            if (!found) break;
        }
        if (item) {
            m_tree->setCurrentItem(item);
            m_tree->scrollToItem(item);
        }
    }
    void createKey() {
        QString path = m_path->text().trimmed();
        bool ok;
        QString name = QInputDialog::getText(this, "New Key", "Key name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.isEmpty()) return;
        HKEY root = getRoot(path);
        QString subkey = path.mid(path.indexOf("\\") + 1);
        HKEY hKey;
        if (RegCreateKeyExW(root, (LPCWSTR)(subkey + "\\" + name).utf16(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            loadRootKeys();
        }
    }
    void deleteKey() {
        QTreeWidgetItem *item = m_tree->currentItem();
        if (!item) return;
        QString path = fullPath(item);
        HKEY root = getRoot(path);
        QString parent = path.mid(0, path.lastIndexOf("\\"));
        QString subkey = parent.mid(parent.indexOf("\\") + 1);
        QString delName = path.mid(path.lastIndexOf("\\") + 1);
        HKEY hKey;
        if (RegOpenKeyExW(root, (LPCWSTR)subkey.utf16(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
            RegDeleteKeyW(hKey, (LPCWSTR)delName.utf16());
            RegCloseKey(hKey);
            loadRootKeys();
        }
    }
private:
    QLineEdit *m_path;
    QTreeWidget *m_tree;
    QTableWidget *m_table;
};

class DeviceManagerDialog : public QDialog {
public:
    DeviceManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Device Manager");
        resize(750, 500);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &DeviceManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *disableBtn = new QPushButton("Disable", this);
        connect(disableBtn, &QPushButton::clicked, this, &DeviceManagerDialog::disableDevice);
        top->addWidget(disableBtn);
        QPushButton *enableBtn = new QPushButton("Enable", this);
        connect(enableBtn, &QPushButton::clicked, this, &DeviceManagerDialog::enableDevice);
        top->addWidget(enableBtn);
        QPushButton *uninstallBtn = new QPushButton("Uninstall", this);
        uninstallBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(uninstallBtn, &QPushButton::clicked, this, &DeviceManagerDialog::uninstallDevice);
        top->addWidget(uninstallBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Device", "Class", "Status", "Instance ID"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        QProcess proc;
        proc.start("powershell", {"-Command", "Get-PnpDevice | Select-Object FriendlyName,Class,Status,InstanceId | ConvertTo-Json"});
        proc.waitForFinished(15000);
        QByteArray out = proc.readAllStandardOutput();
        QJsonDocument doc = QJsonDocument::fromJson(out);
        int row = 0;
        if (doc.isArray()) {
            for (auto v : doc.array()) {
                QJsonObject obj = v.toObject();
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(obj["FriendlyName"].toString()));
                m_table->setItem(row, 1, new QTableWidgetItem(obj["Class"].toString()));
                m_table->setItem(row, 2, new QTableWidgetItem(obj["Status"].toString()));
                m_table->setItem(row, 3, new QTableWidgetItem(obj["InstanceId"].toString()));
                row++;
            }
        }
        m_count->setText(QString("%1 devices").arg(row));
    }
    QString selectedInstance() {
        auto sel = m_table->selectionModel()->selectedRows();
        return sel.isEmpty() ? "" : m_table->item(sel.first().row(), 3)->text();
    }
    void disableDevice() {
        QString id = selectedInstance();
        if (id.isEmpty()) return;
        QProcess::execute("powershell", {"-Command", QString("Disable-PnpDevice -InstanceId '%1' -Confirm:$false").arg(id)});
        load();
    }
    void enableDevice() {
        QString id = selectedInstance();
        if (id.isEmpty()) return;
        QProcess::execute("powershell", {"-Command", QString("Enable-PnpDevice -InstanceId '%1' -Confirm:$false").arg(id)});
        load();
    }
    void uninstallDevice() {
        QString id = selectedInstance();
        if (id.isEmpty()) return;
        if (QMessageBox::question(this, "Confirm", "Uninstall this device?") != QMessageBox::Yes) return;
        QProcess::execute("powershell", {"-Command", QString("Remove-PnpDevice -InstanceId '%1' -Confirm:$false").arg(id)});
        load();
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class InstalledAppsDialog : public QDialog {
public:
    InstalledAppsDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Installed Programs");
        resize(800, 520);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_filter = new QLineEdit(this);
        m_filter->setPlaceholderText("Filter programs...");
        top->addWidget(m_filter);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &InstalledAppsDialog::load);
        top->addWidget(refresh);
        QPushButton *uninstallBtn = new QPushButton("Uninstall", this);
        uninstallBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(uninstallBtn, &QPushButton::clicked, this, &InstalledAppsDialog::uninstallApp);
        top->addWidget(uninstallBtn);
        QPushButton *exportBtn = new QPushButton("Export List", this);
        connect(exportBtn, &QPushButton::clicked, this, &InstalledAppsDialog::exportList);
        top->addWidget(exportBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Name", "Version", "Publisher", "Uninstall String"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        connect(m_filter, &QLineEdit::textChanged, this, [this](const QString &) { load(); });
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QString filt = m_filter->text().trimmed().toLower();
        QStringList regPaths = {
            "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall",
            "SOFTWARE\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall"
        };
        for (auto &rp : regPaths) {
            HKEY hKey;
            if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, (LPCWSTR)rp.utf16(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                loadApps(hKey, filt, row);
                RegCloseKey(hKey);
            }
            if (RegOpenKeyExW(HKEY_CURRENT_USER, (LPCWSTR)rp.utf16(), 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
                loadApps(hKey, filt, row);
                RegCloseKey(hKey);
            }
        }
        m_count->setText(QString("%1 programs").arg(row));
    }
    void loadApps(HKEY hKey, const QString &filt, int &row) {
        DWORD idx = 0;
        WCHAR name[256];
        DWORD nameLen;
        while (idx < 1000) {
            nameLen = 256;
            if (RegEnumKeyExW(hKey, idx++, name, &nameLen, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            HKEY sub;
            if (RegOpenKeyExW(hKey, name, 0, KEY_READ, &sub) == ERROR_SUCCESS) {
                WCHAR displayName[512], version[128], publisher[256], uninstall[1024];
                DWORD sz;
                QString dn, ver, pub, uninst;
                sz = sizeof(displayName);
                if (RegQueryValueExW(sub, L"DisplayName", nullptr, nullptr, (LPBYTE)displayName, &sz) == ERROR_SUCCESS)
                    dn = QString::fromWCharArray(displayName);
                sz = sizeof(version);
                if (RegQueryValueExW(sub, L"DisplayVersion", nullptr, nullptr, (LPBYTE)version, &sz) == ERROR_SUCCESS)
                    ver = QString::fromWCharArray(version);
                sz = sizeof(publisher);
                if (RegQueryValueExW(sub, L"Publisher", nullptr, nullptr, (LPBYTE)publisher, &sz) == ERROR_SUCCESS)
                    pub = QString::fromWCharArray(publisher);
                sz = sizeof(uninstall);
                if (RegQueryValueExW(sub, L"UninstallString", nullptr, nullptr, (LPBYTE)uninstall, &sz) == ERROR_SUCCESS)
                    uninst = QString::fromWCharArray(uninstall);
                RegCloseKey(sub);
                if (dn.isEmpty()) continue;
                if (!filt.isEmpty() && !dn.toLower().contains(filt) && !pub.toLower().contains(filt)) continue;
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(dn));
                m_table->setItem(row, 1, new QTableWidgetItem(ver));
                m_table->setItem(row, 2, new QTableWidgetItem(pub));
                m_table->setItem(row, 3, new QTableWidgetItem(uninst));
                row++;
            }
        }
    }
    void uninstallApp() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return;
        QString uninst = m_table->item(sel.first().row(), 3)->text();
        if (uninst.isEmpty()) return;
        if (QMessageBox::question(this, "Confirm", "Uninstall " + m_table->item(sel.first().row(), 0)->text() + "?") != QMessageBox::Yes) return;
        QProcess::startDetached("cmd.exe", {"/c", uninst});
    }
    void exportList() {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/installed_programs.txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            for (int i = 0; i < m_table->rowCount(); i++) {
                f.write(QString("%1\t%2\t%3\n")
                    .arg(m_table->item(i, 0)->text())
                    .arg(m_table->item(i, 1)->text())
                    .arg(m_table->item(i, 2)->text()).toUtf8());
            }
            QMessageBox::information(this, "Exported", "Saved to: " + path);
        }
    }
private:
    QLineEdit *m_filter;
    QTableWidget *m_table;
    QLabel *m_count;
};

class EventLogDialog : public QDialog {
public:
    EventLogDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Event Log Viewer");
        resize(850, 520);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        m_logName = new QComboBox(this);
        m_logName->addItems({"Application", "System", "Security"});
        top->addWidget(m_logName);
        QPushButton *refresh = new QPushButton("Load", this);
        connect(refresh, &QPushButton::clicked, this, &EventLogDialog::load);
        top->addWidget(refresh);
        QPushButton *clearBtn = new QPushButton("Clear Log", this);
        connect(clearBtn, &QPushButton::clicked, this, &EventLogDialog::clearLog);
        top->addWidget(clearBtn);
        QPushButton *exportBtn = new QPushButton("Export", this);
        connect(exportBtn, &QPushButton::clicked, this, &EventLogDialog::exportLog);
        top->addWidget(exportBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({"Time", "Source", "Level", "Event ID", "Message"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QString logName = m_logName->currentText();
        QProcess proc;
        proc.start("wevtutil", {"qe", logName, "/c:200", "/f:text", "/rd:true"});
        proc.waitForFinished(30000);
        QString out = QString::fromUtf8(proc.readAll());
        QStringList entries = out.split("\n\n", Qt::SkipEmptyParts);
        for (auto &entry : entries) {
            if (entry.trimmed().isEmpty()) continue;
            QStringList lines = entry.split("\n", Qt::SkipEmptyParts);
            QString time, source, level, eventId, message;
            for (auto &line : lines) {
                if (line.startsWith("Date:")) time = line.mid(5).trimmed();
                else if (line.startsWith("Source:")) source = line.mid(7).trimmed();
                else if (line.startsWith("Level:")) level = line.mid(6).trimmed();
                else if (line.startsWith("Event ID:")) eventId = line.mid(9).trimmed();
                else if (line.startsWith("Description:")) message = line.mid(12).trimmed();
                else if (!message.isEmpty()) message += " " + line.trimmed();
            }
            if (row >= 200) break;
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(time));
            m_table->setItem(row, 1, new QTableWidgetItem(source));
            m_table->setItem(row, 2, new QTableWidgetItem(level));
            m_table->setItem(row, 3, new QTableWidgetItem(eventId));
            m_table->setItem(row, 4, new QTableWidgetItem(message.left(200)));
            row++;
        }
        m_count->setText(QString("%1 events").arg(row));
    }
    void clearLog() {
        if (QMessageBox::question(this, "Confirm", "Clear " + m_logName->currentText() + " log?") != QMessageBox::Yes) return;
        QProcess::execute("wevtutil", {"cl", m_logName->currentText()});
        load();
    }
    void exportLog() {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/" + m_logName->currentText().toLower() + "_log.txt";
        QProcess proc;
        proc.start("wevtutil", {"qe", m_logName->currentText(), "/c:500", "/f:text", "/rd:true"});
        proc.waitForFinished(30000);
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(proc.readAll());
        QMessageBox::information(this, "Exported", "Saved to: " + path);
    }
private:
    QComboBox *m_logName;
    QTableWidget *m_table;
    QLabel *m_count;
};

class SystemInfoDialog : public QDialog {
public:
    SystemInfoDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("System Information");
        resize(700, 520);
        QVBoxLayout *lay = new QVBoxLayout(this);
        m_tabs = new QTabWidget(this);
        m_cpuTab = new QPlainTextEdit(this);
        m_cpuTab->setReadOnly(true);
        m_memTab = new QPlainTextEdit(this);
        m_memTab->setReadOnly(true);
        m_gpuTab = new QPlainTextEdit(this);
        m_gpuTab->setReadOnly(true);
        m_osTab = new QPlainTextEdit(this);
        m_osTab->setReadOnly(true);
        QFont mono("Consolas", 9);
        m_cpuTab->setFont(mono);
        m_memTab->setFont(mono);
        m_gpuTab->setFont(mono);
        m_osTab->setFont(mono);
        m_tabs->addTab(m_cpuTab, "CPU");
        m_tabs->addTab(m_memTab, "Memory");
        m_tabs->addTab(m_gpuTab, "GPU");
        m_tabs->addTab(m_osTab, "OS");
        lay->addWidget(m_tabs, 1);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &SystemInfoDialog::load);
        lay->addWidget(refresh);
        load();
    }
    void load() {
        QProcess proc;
        proc.start("systeminfo");
        proc.waitForFinished(15000);
        QString info = QString::fromUtf8(proc.readAll());
        m_osTab->setPlainText(info);
        proc.start("wmic", {"cpu", "get", "Name,NumberOfCores,NumberOfLogicalProcessors,MaxClockSpeed,CurrentClockSpeed", "/format:list"});
        proc.waitForFinished(10000);
        m_cpuTab->setPlainText(QString::fromUtf8(proc.readAll()));
        proc.start("wmic", {"memorychip", "get", "Capacity,Speed,Manufacturer,PartNumber", "/format:list"});
        proc.waitForFinished(10000);
        QString memInfo = "Physical Memory:\n\n";
        MEMORYSTATUSEX mem{};
        mem.dwLength = sizeof(mem);
        GlobalMemoryStatusEx(&mem);
        memInfo += QString("Total Physical: %1 MB\n").arg(mem.ullTotalPhys / 1048576);
        memInfo += QString("Available Physical: %1 MB\n").arg(mem.ullAvailPhys / 1048576);
        memInfo += QString("Memory Usage: %1%%\n\n").arg(mem.dwMemoryLoad);
        memInfo += "Memory Modules:\n" + QString::fromUtf8(proc.readAll());
        m_memTab->setPlainText(memInfo);
        proc.start("wmic", {"path", "win32_videocontroller", "get", "Name,AdapterRAM,DriverVersion", "/format:list"});
        proc.waitForFinished(10000);
        m_gpuTab->setPlainText(QString::fromUtf8(proc.readAll()));
    }
private:
    QTabWidget *m_tabs;
    QPlainTextEdit *m_cpuTab, *m_memTab, *m_gpuTab, *m_osTab;
};

class FirewallDialog : public QDialog {
public:
    FirewallDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Firewall Manager");
        resize(750, 500);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh Rules", this);
        connect(refresh, &QPushButton::clicked, this, &FirewallDialog::load);
        top->addWidget(refresh);
        QPushButton *addRule = new QPushButton("Add Rule", this);
        connect(addRule, &QPushButton::clicked, this, &FirewallDialog::addRule);
        top->addWidget(addRule);
        QPushButton *delRule = new QPushButton("Delete Rule", this);
        delRule->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(delRule, &QPushButton::clicked, this, &FirewallDialog::deleteRule);
        top->addWidget(delRule);
        QPushButton *enableBtn = new QPushButton("Enable Firewall", this);
        connect(enableBtn, &QPushButton::clicked, this, [this]() {
            QProcess::execute("netsh", {"advfirewall", "set", "allprofiles", "state", "on"});
            load();
        });
        top->addWidget(enableBtn);
        QPushButton *disableBtn = new QPushButton("Disable Firewall", this);
        disableBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(disableBtn, &QPushButton::clicked, this, [this]() {
            QProcess::execute("netsh", {"advfirewall", "set", "allprofiles", "state", "off"});
            load();
        });
        top->addWidget(disableBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({"Name", "Action", "Direction", "Protocol", "Local Port"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QProcess proc;
        proc.start("netsh", {"advfirewall", "firewall", "show", "rule", "name=all", "dir=in"});
        proc.waitForFinished(30000);
        QString out = QString::fromUtf8(proc.readAll());
        QStringList rules = out.split("\nRule Name", Qt::SkipEmptyParts);
        for (auto &rule : rules) {
            if (rule.trimmed().isEmpty()) continue;
            QString name, action, dir, proto, port;
            QStringList lines = rule.split("\n");
            for (auto &line : lines) {
                QString l = line.trimmed();
                if (l.startsWith("Rule Name:")) name = l.mid(10).trimmed();
                else if (l.startsWith("Action:")) action = l.mid(7).trimmed();
                else if (l.startsWith("Direction:")) dir = l.mid(10).trimmed();
                else if (l.startsWith("Protocol:")) proto = l.mid(9).trimmed();
                else if (l.startsWith("LocalPort:")) port = l.mid(10).trimmed();
            }
            if (row >= 100) break;
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(name));
            m_table->setItem(row, 1, new QTableWidgetItem(action));
            m_table->setItem(row, 2, new QTableWidgetItem(dir));
            m_table->setItem(row, 3, new QTableWidgetItem(proto));
            m_table->setItem(row, 4, new QTableWidgetItem(port));
            row++;
        }
        m_count->setText(QString("%1 rules").arg(row));
    }
    void addRule() {
        bool ok;
        QString name = QInputDialog::getText(this, "Rule Name", "Name:", QLineEdit::Normal, "", &ok);
        if (!ok || name.isEmpty()) return;
        QString port = QInputDialog::getText(this, "Port", "Port number:", QLineEdit::Normal, "80", &ok);
        if (!ok || port.isEmpty()) return;
        QProcess::execute("netsh", {"advfirewall", "firewall", "add", "rule", "name=" + name, "dir=in", "action=allow", "protocol=TCP", "localport=" + port});
        load();
    }
    void deleteRule() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return;
        QString name = m_table->item(sel.first().row(), 0)->text();
        if (QMessageBox::question(this, "Confirm", "Delete rule: " + name + "?") != QMessageBox::Yes) return;
        QProcess::execute("netsh", {"advfirewall", "firewall", "delete", "rule", "name=" + name});
        load();
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class DriverManagerDialog : public QDialog {
public:
    DriverManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Driver Manager");
        resize(800, 500);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &DriverManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *exportBtn = new QPushButton("Export List", this);
        connect(exportBtn, &QPushButton::clicked, this, &DriverManagerDialog::exportList);
        top->addWidget(exportBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Driver", "Version", "Date", "Provider"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QProcess proc;
        proc.start("driverquery", {"/v", "/fo", "csv"});
        proc.waitForFinished(15000);
        QString out = QString::fromUtf8(proc.readAll());
        QStringList lines = out.split("\n", Qt::SkipEmptyParts);
        for (int i = 1; i < lines.size() && row < 200; i++) {
            QStringList fields = lines[i].split("\",\"");
            if (fields.size() < 5) continue;
            for (auto &f : fields) f.remove('"');
            m_table->insertRow(row);
            m_table->setItem(row, 0, new QTableWidgetItem(fields[0]));
            m_table->setItem(row, 1, new QTableWidgetItem(fields[3]));
            m_table->setItem(row, 2, new QTableWidgetItem(fields[4]));
            m_table->setItem(row, 3, new QTableWidgetItem(fields[1]));
            row++;
        }
        m_count->setText(QString("%1 drivers").arg(row));
    }
    void exportList() {
        QString path = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/drivers.txt";
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            for (int i = 0; i < m_table->rowCount(); i++) {
                f.write(QString("%1\t%2\t%3\t%4\n")
                    .arg(m_table->item(i, 0)->text())
                    .arg(m_table->item(i, 1)->text())
                    .arg(m_table->item(i, 2)->text())
                    .arg(m_table->item(i, 3)->text()).toUtf8());
            }
            QMessageBox::information(this, "Exported", "Saved to: " + path);
        }
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class SystemRestoreDialog : public QDialog {
public:
    SystemRestoreDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("System Restore");
        resize(650, 450);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *createBtn = new QPushButton("Create Restore Point", this);
        connect(createBtn, &QPushButton::clicked, this, &SystemRestoreDialog::createPoint);
        top->addWidget(createBtn);
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &SystemRestoreDialog::load);
        top->addWidget(refresh);
        QPushButton *restoreBtn = new QPushButton("Restore", this);
        restoreBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(restoreBtn, &QPushButton::clicked, this, &SystemRestoreDialog::restorePoint);
        top->addWidget(restoreBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(3);
        m_table->setHorizontalHeaderLabels({"Date", "Description", "ID"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QProcess proc;
        proc.start("powershell", {"-Command", "Get-ComputerRestorePoint | Select-Object SequenceNumber,Description,CreationTime | ConvertTo-Json"});
        proc.waitForFinished(15000);
        QJsonDocument doc = QJsonDocument::fromJson(proc.readAll());
        if (doc.isArray()) {
            for (auto v : doc.array()) {
                QJsonObject obj = v.toObject();
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(obj["CreationTime"].toString()));
                m_table->setItem(row, 1, new QTableWidgetItem(obj["Description"].toString()));
                m_table->setItem(row, 2, new QTableWidgetItem(QString::number(obj["SequenceNumber"].toInt())));
                row++;
            }
        }
        m_count->setText(QString("%1 restore points").arg(row));
    }
    void createPoint() {
        bool ok;
        QString desc = QInputDialog::getText(this, "Restore Point", "Description:", QLineEdit::Normal, "Manual Restore Point", &ok);
        if (!ok || desc.isEmpty()) return;
        QProcess::execute("powershell", {"-Command", QString("Checkpoint-Computer -Description '%1'").arg(desc)});
        QMessageBox::information(this, "Done", "Restore point created.");
        load();
    }
    void restorePoint() {
        QMessageBox::warning(this, "Warning", "System restore requires restart and cannot be done from here.\nUse 'rstrui.exe' from the command line.");
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class SharedFoldersDialog : public QDialog {
public:
    SharedFoldersDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Shared Folders");
        resize(700, 450);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &SharedFoldersDialog::load);
        top->addWidget(refresh);
        QPushButton *addBtn = new QPushButton("Create Share", this);
        connect(addBtn, &QPushButton::clicked, this, &SharedFoldersDialog::createShare);
        top->addWidget(addBtn);
        QPushButton *delBtn = new QPushButton("Delete Share", this);
        delBtn->setStyleSheet("background:rgba(200,50,50,0.35);");
        connect(delBtn, &QPushButton::clicked, this, &SharedFoldersDialog::deleteShare);
        top->addWidget(delBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(4);
        m_table->setHorizontalHeaderLabels({"Share Name", "Path", "Users", "Status"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        QProcess proc;
        proc.start("net", {"share"});
        proc.waitForFinished(10000);
        QString out = QString::fromUtf8(proc.readAll());
        QStringList lines = out.split("\n", Qt::SkipEmptyParts);
        bool inTable = false;
        for (auto &line : lines) {
            if (line.contains("---")) { inTable = true; continue; }
            if (!inTable || line.trimmed().isEmpty()) continue;
            QStringList fields = line.split(QRegularExpression("\\s{2,}"));
            if (fields.size() >= 2) {
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(fields[0].trimmed()));
                m_table->setItem(row, 1, new QTableWidgetItem(fields.size() > 2 ? fields[2].trimmed() : ""));
                m_table->setItem(row, 2, new QTableWidgetItem(fields.size() > 3 ? fields[3].trimmed() : ""));
                m_table->setItem(row, 3, new QTableWidgetItem("Shared"));
                row++;
            }
        }
        m_count->setText(QString("%1 shares").arg(row));
    }
    void createShare() {
        bool ok;
        QString path = QFileDialog::getExistingDirectory(this, "Select Folder to Share");
        if (path.isEmpty()) return;
        QString name = QInputDialog::getText(this, "Share Name", "Name:", QLineEdit::Normal, QFileInfo(path).fileName(), &ok);
        if (!ok || name.isEmpty()) return;
        QProcess::execute("net", {"share", name + "=" + path});
        load();
    }
    void deleteShare() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return;
        QString name = m_table->item(sel.first().row(), 0)->text();
        if (QMessageBox::question(this, "Confirm", "Delete share: " + name + "?") != QMessageBox::Yes) return;
        QProcess::execute("net", {"share", name, "/delete"});
        load();
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};

class DiskManagerDialog : public QDialog {
public:
    DiskManagerDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle("Disk Manager");
        resize(750, 480);
        QVBoxLayout *lay = new QVBoxLayout(this);
        QHBoxLayout *top = new QHBoxLayout();
        QPushButton *refresh = new QPushButton("Refresh", this);
        connect(refresh, &QPushButton::clicked, this, &DiskManagerDialog::load);
        top->addWidget(refresh);
        QPushButton *chkdskBtn = new QPushButton("Check Disk", this);
        connect(chkdskBtn, &QPushButton::clicked, this, &DiskManagerDialog::checkDisk);
        top->addWidget(chkdskBtn);
        QPushButton *defragBtn = new QPushButton("Defragment", this);
        connect(defragBtn, &QPushButton::clicked, this, &DiskManagerDialog::defrag);
        top->addWidget(defragBtn);
        lay->addLayout(top);
        m_table = new QTableWidget(this);
        m_table->setColumnCount(5);
        m_table->setHorizontalHeaderLabels({"Drive", "Type", "File System", "Total", "Free"});
        m_table->horizontalHeader()->setStretchLastSection(true);
        m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_table->verticalHeader()->setVisible(false);
        m_table->setAlternatingRowColors(true);
        lay->addWidget(m_table, 1);
        m_count = new QLabel(this);
        lay->addWidget(m_count);
        load();
    }
    void load() {
        m_table->setRowCount(0);
        int row = 0;
        for (char d = 'A'; d <= 'Z'; d++) {
            QString drive = QString("%1:\\").arg(d);
            ULARGE_INTEGER freeBytes, totalBytes, availBytes;
            if (GetDiskFreeSpaceExW((LPCWSTR)drive.utf16(), &availBytes, &totalBytes, &freeBytes)) {
                WCHAR volName[256];
                DWORD serial, maxComp, fileSysFlags;
                WCHAR fileSysName[128];
                bool ok = GetVolumeInformationW((LPCWSTR)drive.utf16(), volName, 256, &serial, &maxComp, &fileSysFlags, fileSysName, 128);
                double totalGB = totalBytes.QuadPart / 1073741824.0;
                double freeGB = freeBytes.QuadPart / 1073741824.0;
                m_table->insertRow(row);
                m_table->setItem(row, 0, new QTableWidgetItem(drive));
                m_table->setItem(row, 1, new QTableWidgetItem("Fixed"));
                m_table->setItem(row, 2, new QTableWidgetItem(ok ? QString::fromWCharArray(fileSysName) : "Unknown"));
                m_table->setItem(row, 3, new QTableWidgetItem(QString("%1 GB").arg(totalGB, 0, 'f', 1)));
                m_table->setItem(row, 4, new QTableWidgetItem(QString("%1 GB").arg(freeGB, 0, 'f', 1)));
                row++;
            }
        }
        m_count->setText(QString("%1 drives").arg(row));
    }
    void checkDisk() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return;
        QString drive = m_table->item(sel.first().row(), 0)->text();
        if (QMessageBox::question(this, "Confirm", "Check disk " + drive + " for errors?") != QMessageBox::Yes) return;
        QProcess::startDetached("chkdsk", {drive, "/f"});
    }
    void defrag() {
        auto sel = m_table->selectionModel()->selectedRows();
        if (sel.isEmpty()) return;
        QString drive = m_table->item(sel.first().row(), 0)->text();
        if (QMessageBox::question(this, "Confirm", "Defragment " + drive + "?") != QMessageBox::Yes) return;
        QProcess::startDetached("defrag", {drive});
    }
private:
    QTableWidget *m_table;
    QLabel *m_count;
};


class MainWindow : public QMainWindow {
public:
    MainWindow() {
        setWindowTitle(APP_TITLE);
        resize(1180, 760);
        setMinimumSize(920, 620);
        QWidget *central = new QWidget(this);
        QVBoxLayout *root = new QVBoxLayout(central);
        root->setContentsMargins(16, 14, 16, 12);
        root->setSpacing(10);
        buildHeader(root);
        buildChips(root);
        buildBody(root);
        setCentralWidget(central);
        reloadCategories();
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &MainWindow::refreshChips);
        m_timer->start(1000);
    }
private:
    void buildHeader(QVBoxLayout *root) {
        QHBoxLayout *h = new QHBoxLayout();
        h->setSpacing(10);
        QLabel *title = new QLabel(QString("Windows Control Center"), this);
        QFont tf = title->font();
        tf.setPointSize(13);
        tf.setBold(true);
        title->setFont(tf);
        h->addWidget(title);
        h->addStretch(1);
        m_search = new QLineEdit(this);
        m_search->setPlaceholderText("Search commands or descriptions...");
        m_search->setClearButtonEnabled(true);
        m_search->setMinimumWidth(300);
        h->addWidget(m_search);
        QPushButton *btnFav = new QPushButton("Favorites", this);
        connect(btnFav, &QPushButton::clicked, this, [this]() {
            FavoritesDialog dlg(this);
            dlg.exec();
            reloadCategories();
        });
        h->addWidget(btnFav);
        m_themeBtn = new QPushButton(g_darkMode ? "Light" : "Dark", this);
        connect(m_themeBtn, &QPushButton::clicked, this, &MainWindow::toggleTheme);
        h->addWidget(m_themeBtn);
        QPushButton *btnTools = new QPushButton("System Tools", this);
        connect(btnTools, &QPushButton::clicked, this, [this]() {
            QMenu menu(this);
            menu.addAction("Process Manager", this, &MainWindow::openProcessManager);
            menu.addAction("Services Manager", this, &MainWindow::openServicesManager);
            menu.addAction("Window Manager", this, &MainWindow::openWindowManager);
            menu.addAction("Device Manager", this, &MainWindow::openDeviceManager);
            menu.addAction("Driver Manager", this, &MainWindow::openDriverManager);
            menu.addAction("Startup Manager", this, &MainWindow::openStartupManager);
            menu.addAction("Task Scheduler", this, &MainWindow::openTaskScheduler);
            menu.addAction("System Info", this, &MainWindow::openSystemInfo);
            menu.addSeparator();
            menu.addAction("Installed Programs", this, &MainWindow::openInstalledApps);
            menu.addAction("Registry Editor", this, &MainWindow::openRegistryEditor);
            menu.addAction("Environment Variables", this, &MainWindow::openEnvVars);
            menu.addAction("Shared Folders", this, &MainWindow::openSharedFolders);
            menu.addAction("System Restore", this, &MainWindow::openSystemRestore);
            menu.addSeparator();
            menu.addAction("Network Connections", this, &MainWindow::openNetworkConnections);
            menu.addAction("Network Tools", this, &MainWindow::openNetworkTools);
            menu.addAction("Firewall Manager", this, &MainWindow::openFirewall);
            menu.addAction("Event Log", this, &MainWindow::openEventLog);
            menu.addSeparator();
            menu.addAction("Disk Manager", this, &MainWindow::openDiskManager);
            menu.addAction("Disk Space", this, &MainWindow::openDiskSpace);
            menu.addAction("File Hasher", this, &MainWindow::openFileHasher);
            menu.addAction("Color Picker", this, &MainWindow::openColorPicker);
            menu.addAction("Battery Info", this, &MainWindow::openBattery);
            menu.addAction("Windows Update", this, &MainWindow::openWindowsUpdate);
            menu.addSeparator();
            menu.addAction("Quick Launch", this, &MainWindow::openQuickLaunch);
            menu.addAction("Notes / Scratchpad", this, &MainWindow::openNotes);
            menu.addAction("Clipboard", this, &MainWindow::openClipboard);
            menu.addAction("Password Generator", this, &MainWindow::openPasswordGenerator);
            menu.addAction("Screenshot", this, &MainWindow::openScreenshot);
            menu.addAction("System Report", this, &MainWindow::openSystemReport);
            menu.addSeparator();
            menu.addAction("Live Monitor (console)", [this]() { QcOpenTool(0); });
            menu.addAction("App Uninstaller (console)", [this]() { QcOpenTool(1); });
            menu.addAction("File Search (console)", [this]() { QcOpenTool(2); });
            menu.addAction("Wi-Fi Manager (console)", [this]() { QcOpenTool(3); });
            menu.addAction("Process List (console)", [this]() { QcOpenTool(4); });
            menu.addAction("Net Speed (console)", [this]() { QcOpenTool(8); });
            menu.exec(QCursor::pos());
        });
        h->addWidget(btnTools);
        QPushButton *btnTerm = new QPushButton("Terminal", this);
        connect(btnTerm, &QPushButton::clicked, this, [this]() {
            g_termDlg = new TerminalDialog(this);
            g_termDlg->setAttribute(Qt::WA_DeleteOnClose);
            connect(g_termDlg, &QDialog::finished, this, []() { g_termDlg = nullptr; });
            g_termDlg->show();
        });
        h->addWidget(btnTerm);
        QPushButton *btnFloat = new QPushButton("Float", this);
        connect(btnFloat, &QPushButton::clicked, this, [this]() {
            if (g_floatWnd) { g_floatWnd->show(); g_floatWnd->raise(); return; }
            g_floatWnd = new FloatWindow(this);
            g_floatWnd->setAttribute(Qt::WA_DeleteOnClose);
            connect(g_floatWnd, &QWidget::destroyed, this, []() { g_floatWnd = nullptr; });
            g_floatWnd->show();
        });
        h->addWidget(btnFloat);
        QPushButton *btnAbout = new QPushButton("About", this);
        connect(btnAbout, &QPushButton::clicked, this, [this]() {
            QMessageBox::about(this, APP_TITLE,
                "Windows Control Center - All-in-one Windows control center.\n\n"
                "Pure C core with a Qt front-end.\n"
                "Follows the Windows system theme (light/dark).");
        });
        h->addWidget(btnAbout);
        root->addLayout(h);
    }
    void buildChips(QVBoxLayout *root) {
        QHBoxLayout *h = new QHBoxLayout();
        h->setSpacing(8);
        m_cpu = new StatsChip("CPU", this);
        m_ram = new StatsChip("RAM", this);
        m_disk = new StatsChip("DISK", this);
        m_net = new StatsChip("NET", this);
        m_uptime = new StatsChip("UPTIME", this);
        h->addWidget(m_cpu, 1);
        h->addWidget(m_ram, 1);
        h->addWidget(m_disk, 1);
        h->addWidget(m_net, 1);
        h->addWidget(m_uptime, 1);
        root->addLayout(h);
    }
    void buildBody(QVBoxLayout *root) {
        QHBoxLayout *h = new QHBoxLayout();
        h->setSpacing(10);
        m_catList = new QListWidget(this);
        m_catList->setObjectName("catList");
        m_catList->setFixedWidth(230);
        connect(m_catList, &QListWidget::currentRowChanged, this, &MainWindow::reloadCommands);
        h->addWidget(m_catList);
        QVBoxLayout *right = new QVBoxLayout();
        right->setSpacing(6);
        m_cmdList = new QListWidget(this);
        m_cmdList->setObjectName("cmdList");
        connect(m_cmdList, &QListWidget::currentRowChanged, this, &MainWindow::showDetail);
        connect(m_cmdList, &QListWidget::itemDoubleClicked, this,
                [this](QListWidgetItem *it) { runCmd(it); });
        right->addWidget(m_cmdList, 1);
        QHBoxLayout *ctl = new QHBoxLayout();
        m_detail = new QTextEdit(this);
        m_detail->setReadOnly(true);
        m_detail->setMaximumHeight(150);
        m_detail->setPlaceholderText("Select a command to see its description.");
        ctl->addWidget(m_detail, 1);
        QVBoxLayout *btnCol = new QVBoxLayout();
        m_runBtn = new QPushButton("Run", this);
        m_runBtn->setMinimumWidth(110);
        m_runBtn->setMinimumHeight(34);
        connect(m_runBtn, &QPushButton::clicked, this, [this]() {
            QListWidgetItem *it = m_cmdList->currentItem();
            if (it) runCmd(it);
        });
        btnCol->addWidget(m_runBtn);
        m_favBtn = new QPushButton("Add to Favorites", this);
        m_favBtn->setStyleSheet("font-size:9pt;");
        connect(m_favBtn, &QPushButton::clicked, this, &MainWindow::addFavorite);
        btnCol->addWidget(m_favBtn);
        ctl->addLayout(btnCol);
        right->addLayout(ctl);
        h->addLayout(right, 1);
        root->addLayout(h, 1);
        QLabel *status = new QLabel(this);
        status->setText(QString("%1 categories, %2 commands").arg(QcCategoryCount()).arg(QcTotalCommands()));
        status->setStyleSheet("color:rgba(127,127,127,0.9);");
        root->addWidget(status);
        connect(m_search, &QLineEdit::textChanged, this, &MainWindow::reloadCategories);
    }
    void toggleTheme() {
        g_darkMode = !g_darkMode;
        qApp->setPalette(makePalette(g_darkMode));
        m_themeBtn->setText(g_darkMode ? "Light" : "Dark");
    }
    void openProcessManager() { ProcessManagerDialog(this).exec(); }
    void openServicesManager() { ServicesManagerDialog(this).exec(); }
    void openNetworkTools() { NetworkToolsDialog(this).exec(); }
    void openClipboard() { ClipboardManagerDialog(this).exec(); }
    void openScreenshot() { ScreenshotDialog(this).exec(); }
    void openSystemReport() { SystemReportDialog(this).exec(); }
    void openWindowManager() { WindowManagerDialog(this).exec(); }
    void openStartupManager() { StartupManagerDialog(this).exec(); }
    void openTaskScheduler() { TaskSchedulerDialog(this).exec(); }
    void openNetworkConnections() { NetworkConnectionsDialog(this).exec(); }
    void openEnvVars() { EnvVarsDialog(this).exec(); }
    void openDiskSpace() { DiskSpaceDialog(this).exec(); }
    void openFileHasher() { FileHasherDialog(this).exec(); }
    void openColorPicker() { ColorPickerDialog(this).exec(); }
    void openBattery() { BatteryDialog(this).exec(); }
    void openWindowsUpdate() { WindowsUpdateDialog(this).exec(); }
    void openQuickLaunch() { QuickLaunchDialog(this).exec(); }
    void openNotes() { NotesDialog(this).exec(); }
    void openPasswordGenerator() { PasswordGeneratorDialog(this).exec(); }
    void openRegistryEditor() { RegistryEditorDialog(this).exec(); }
    void openDeviceManager() { DeviceManagerDialog(this).exec(); }
    void openInstalledApps() { InstalledAppsDialog(this).exec(); }
    void openEventLog() { EventLogDialog(this).exec(); }
    void openSystemInfo() { SystemInfoDialog(this).exec(); }
    void openFirewall() { FirewallDialog(this).exec(); }
    void openDriverManager() { DriverManagerDialog(this).exec(); }
    void openSystemRestore() { SystemRestoreDialog(this).exec(); }
    void openSharedFolders() { SharedFoldersDialog(this).exec(); }
    void openDiskManager() { DiskManagerDialog(this).exec(); }
    void addFavorite() {
        QListWidgetItem *it = m_cmdList->currentItem();
        if (!it) return;
        int row = m_cmdList->row(it);
        if (row < 0 || row >= (int)m_cmds.size()) return;
        const CmdRef &c = m_cmds[row];
        QString key = QString("%1|%2").arg(c.cat).arg(c.idx);
        QStringList favs = loadFavorites();
        if (!favs.count(key)) {
            favs.append(key);
            saveFavorites(favs);
            QMessageBox::information(this, "Favorites", "Added to favorites!");
        } else {
            QMessageBox::information(this, "Favorites", "Already in favorites.");
        }
    }
    void reloadCategories() {
        QString q = m_search->text().trimmed();
        QString cur = m_catList->currentItem() ? m_catList->currentItem()->text() : QString();
        m_catList->blockSignals(true);
        m_catList->clear();
        int n = QcCategoryCount();
        m_catIdx.clear();
        std::vector<std::pair<QString,int>> cats;
        for (int i = 0; i < n; i++) {
            int cnt = QcCategoryCommandCount(i);
            if (cnt == 0) continue;
            if (!q.isEmpty()) {
                bool any = false;
                for (int j = 0; j < cnt; j++) {
                    QString nm = QString::fromUtf8(QcCommandName(i, j));
                    QString ds = QString::fromUtf8(QcCommandDesc(i, j));
                    if (nm.count(q, Qt::CaseInsensitive) || ds.count(q, Qt::CaseInsensitive)) { any = true; break; }
                }
                if (!any) continue;
            }
            cats.emplace_back(QString::fromUtf8(QcCategoryName(i)), i);
        }
        int selRow = -1;
        for (size_t k = 0; k < cats.size(); k++) {
            m_catList->addItem(QString("%1  (%2)").arg(cats[k].first).arg(QcCategoryCommandCount(cats[k].second)));
            m_catIdx.push_back(cats[k].second);
            if (!cur.isEmpty() && cats[k].first == cur) selRow = (int)k;
        }
        m_catList->blockSignals(false);
        if (selRow >= 0) m_catList->setCurrentRow(selRow);
        else if (m_catList->count() > 0) m_catList->setCurrentRow(0);
        reloadCommands();
    }
    void reloadCommands() {
        int row = m_catList->currentRow();
        int cat = (row >= 0 && row < (int)m_catIdx.size()) ? m_catIdx[row] : -1;
        m_cmdList->blockSignals(true);
        m_cmdList->clear();
        m_cmds.clear();
        QString q = m_search->text().trimmed();
        if (cat >= 0) {
            int cnt = QcCategoryCommandCount(cat);
            for (int j = 0; j < cnt; j++) {
                QString nm = QString::fromUtf8(QcCommandName(cat, j));
                QString ds = QString::fromUtf8(QcCommandDesc(cat, j));
                if (!q.isEmpty() && !nm.count(q, Qt::CaseInsensitive) && !ds.count(q, Qt::CaseInsensitive))
                    continue;
                CmdRef c;
                c.cat = cat; c.idx = j;
                c.name = nm; c.desc = ds;
                c.needsArg = QcCommandNeedsArg(cat, j) != 0;
                c.detail = QString::fromUtf8(QcCommandDetail(cat, j));
                m_cmds.push_back(c);
                QString label = nm;
                if (c.needsArg) label += "  [ARG]";
                QStringList favs = loadFavorites();
                QString key = QString("%1|%2").arg(c.cat).arg(c.idx);
                if (favs.count(key)) label = "* " + label;
                QListWidgetItem *it = new QListWidgetItem(label, m_cmdList);
                it->setToolTip(ds);
            }
        }
        m_cmdList->blockSignals(false);
        if (m_cmdList->count() > 0) m_cmdList->setCurrentRow(0);
    }
    void showDetail() {
        QListWidgetItem *it = m_cmdList->currentItem();
        if (!it) { m_detail->clear(); return; }
        int row = m_cmdList->row(it);
        if (row < 0 || row >= (int)m_cmds.size()) return;
        const CmdRef &c = m_cmds[row];
        QString text = QString("<b>%1</b>  (%2)<br><br><i>%3</i>").arg(
            c.name.toHtmlEscaped(), c.needsArg ? "needs argument" : "no argument",
            c.desc.toHtmlEscaped());
        if (!c.detail.isEmpty())
            text += QString("<br><br>%1").arg(c.detail.toHtmlEscaped().replace('\n', "<br>"));
        m_detail->setHtml(text);
    }
    void runCmd(QListWidgetItem *it) {
        int row = m_cmdList->row(it);
        if (row < 0 || row >= (int)m_cmds.size()) return;
        const CmdRef &c = m_cmds[row];
        if (c.needsArg) {
            bool ok = false;
            QString arg = QInputDialog::getText(this, "Argument for " + c.name,
                                                "Argument (mask|root for find-files):", QLineEdit::Normal, "", &ok);
            if (!ok) return;
            QByteArray ba = arg.toUtf8();
            QcRunCommand(c.cat, c.idx, ba.constData());
        } else {
            QcRunCommand(c.cat, c.idx, nullptr);
        }
    }
    void refreshChips() {
        QcSample();
        m_cpu->setPct(QcCpuPct(), QString("%1%").arg(QcCpuPct(), 0, 'f', 1));
        m_ram->setPct(QcRamPct(), QString("%1%").arg(QcRamPct(), 0, 'f', 1));
        m_disk->setPct(QcDiskPct(), QString("%1%").arg(QcDiskPct(), 0, 'f', 1));
        m_net->setPct(0.0, QString("v%1 / ^%2 MB/s")
            .arg(QcNetDown() / 1048576.0, 0, 'f', 2)
            .arg(QcNetUp() / 1048576.0, 0, 'f', 2));
        m_uptime->setPct(0.0, QString::fromUtf8(QcUptime()));
    }
    QLineEdit *m_search = nullptr;
    QListWidget *m_catList = nullptr;
    QListWidget *m_cmdList = nullptr;
    QTextEdit *m_detail = nullptr;
    QPushButton *m_runBtn = nullptr;
    QPushButton *m_favBtn = nullptr;
    QPushButton *m_themeBtn = nullptr;
    StatsChip *m_cpu = nullptr, *m_ram = nullptr, *m_disk = nullptr, *m_net = nullptr, *m_uptime = nullptr;
    QTimer *m_timer = nullptr;
    std::vector<CmdRef> m_cmds;
    std::vector<int> m_catIdx;
};



int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    app.setApplicationName("WindowsControl");
    app.setOrganizationName("WindowsControl");
    app.setStyle(QStyleFactory::create("Fusion"));
    g_darkMode = systemIsDarkMode();
    QPalette pal = makePalette(g_darkMode);
    app.setPalette(pal);
    app.setStyleSheet(
        "QWidget{font-family:'Segoe UI';font-size:10pt;}"
        "QPushButton{background:rgba(127,127,127,0.18);border:none;border-radius:6px;padding:6px 14px;}"
        "QPushButton:hover{background:rgba(127,127,127,0.30);}"
        "QPushButton:pressed{background:rgba(127,127,127,0.42);}"
        "QLineEdit,QTextEdit{background:rgba(127,127,127,0.12);border:1px solid rgba(127,127,127,0.25);border-radius:6px;padding:6px 8px;selection-background-color:#3c7df2;}"
        "QLineEdit:focus,QTextEdit:focus{border:1px solid #3c7df2;}"
        "QListWidget{background:rgba(127,127,127,0.08);border:1px solid rgba(127,127,127,0.22);border-radius:8px;padding:4px;outline:0;}"
        "QListWidget::item{border-radius:6px;padding:5px 8px;margin:1px;}"
        "QListWidget::item:selected{background:#3c7df2;color:white;}"
        "QListWidget::item:hover:!selected{background:rgba(127,127,127,0.20);}"
        "#chip{background:rgba(127,127,127,0.10);border:1px solid rgba(127,127,127,0.20);border-radius:10px;}"
        "#floatCard{background:rgba(30,30,30,0.96);border:1px solid rgba(127,127,127,0.35);border-radius:12px;}"
        "QMenu{background:rgba(32,32,32,0.97);border:1px solid rgba(127,127,127,0.3);border-radius:8px;padding:6px;}"
        "QMenu::item{padding:6px 24px 6px 10px;border-radius:5px;}"
        "QMenu::item:selected{background:#3c7df2;color:white;}"
        "QInputDialog QLineEdit{min-width:280px;}"
        "QTableWidget{background:rgba(127,127,127,0.08);border:1px solid rgba(127,127,127,0.22);border-radius:6px;gridline-color:rgba(127,127,127,0.15);}"
        "QTableWidget::item{padding:4px 8px;}"
        "QTableWidget::item:selected{background:#3c7df2;color:white;}"
        "QHeaderView::section{background:rgba(127,127,127,0.15);border:none;border-right:1px solid rgba(127,127,127,0.2);padding:6px 8px;font-weight:600;}"
        "QComboBox{background:rgba(127,127,127,0.12);border:1px solid rgba(127,127,127,0.25);border-radius:6px;padding:6px 12px;}"
        "QComboBox::drop-down{border:none;width:24px;}"
        "QComboBox QAbstractItemView{background:rgba(32,32,32,0.97);border:1px solid rgba(127,127,127,0.3);border-radius:6px;selection-background-color:#3c7df2;}"
        "QScrollBar:vertical{width:10px;background:transparent;}"
        "QScrollBar::handle:vertical{background:rgba(127,127,127,0.4);border-radius:5px;min-height:30px;}"
        "QScrollBar::handle:vertical:hover{background:rgba(127,127,127,0.6);}"
        "QScrollBar::add-line:vertical,QScrollBar::sub-line:vertical{height:0;}"
        "QScrollBar::add-page:vertical,QScrollBar::sub-page:vertical{background:transparent;}");
    MainWindow w;
    w.show();
    return app.exec();
}