#include "mainwindow.h"
#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QFont>
#include <QScreen>
#include <QPushButton>
#include <QFileDialog>
#include <QProcess>
#include <QMessageBox>
#include <QDir>
#include <QCoreApplication>
#include <QGroupBox>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include <QGuiApplication>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QScrollArea>
#include <QScrollBar>
#include <QPainter>
#include <QComboBox>
#include <QSpinBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsPolygonItem>
#include <QGraphicsTextItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QInputDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QTabBar>
#include <QMenu>
#include <QAction>
#include <QAbstractButton>
#include <QPropertyAnimation>
#include <QPainter>
#include <QStyledItemDelegate>
#include <QRegularExpression>
#include <QTextCursor>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QFrame>
#include <QScrollArea>
#include <functional>

// ─── ComponentsDelegate ──────────────────────────────────────────────────────
// Delegate per la tabella componenti (trasposta).
// Righe > 0 corrispondono a campi; se il campo ha suggerimenti noti mostra
// un QComboBox editabile, altrimenti usa l'editor di default.
class ComponentsDelegate : public QStyledItemDelegate {
public:
    QStringList fieldNames;   // allFields; fieldNames[i] corrisponde alla riga i+1
    std::function<QStringList(const QString &)> getDynamicSuggestions;

    using QStyledItemDelegate::QStyledItemDelegate;

    static const QMap<QString, QStringList> &staticSuggestions() {
        static const QMap<QString, QStringList> s = {
            {"gas_type",        {"Air", "Argon", "Krypton", "Xenon"}},
            {"roughness",       {"VeryRough", "Rough", "MediumRough",
                                 "MediumSmooth", "Smooth", "VerySmooth"}},
            {"thermostat_type", {"DualSetpoint", "SingleHeatingSetpoint",
                                 "SingleCoolingSetpoint",
                                 "SingleHeatingOrCoolingSetpoint"}},
        };
        return s;
    }

    QWidget *createEditor(QWidget *parent,
                          const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override
    {
        // Riga 0 = nome componente → editor di default
        if (index.row() == 0)
            return QStyledItemDelegate::createEditor(parent, option, index);

        int fi = index.row() - 1;
        if (fi < 0 || fi >= fieldNames.size())
            return QStyledItemDelegate::createEditor(parent, option, index);

        const QString &fieldName = fieldNames[fi];
        QStringList suggestions;
        if (staticSuggestions().contains(fieldName))
            suggestions = staticSuggestions()[fieldName];
        else if (getDynamicSuggestions)
            suggestions = getDynamicSuggestions(fieldName);

        if (!suggestions.isEmpty()) {
            QComboBox *cb = new QComboBox(parent);
            cb->setEditable(true);
            cb->addItems(suggestions);
            return cb;
        }
        return QStyledItemDelegate::createEditor(parent, option, index);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const override {
        if (QComboBox *cb = qobject_cast<QComboBox *>(editor)) {
            QString val = index.data(Qt::EditRole).toString();
            int idx = cb->findText(val);
            if (idx >= 0) cb->setCurrentIndex(idx);
            else          cb->setEditText(val);
        } else {
            QStyledItemDelegate::setEditorData(editor, index);
        }
    }

    void setModelData(QWidget *editor,
                      QAbstractItemModel *model,
                      const QModelIndex &index) const override
    {
        if (QComboBox *cb = qobject_cast<QComboBox *>(editor))
            model->setData(index, cb->currentText(), Qt::EditRole);
        else
            QStyledItemDelegate::setModelData(editor, model, index);
    }
};

// ─── ToggleSwitch ────────────────────────────────────────────────────────────
// Custom animated iOS-style toggle switch.
// API is identical to QCheckBox (isChecked, setChecked, toggled signal).
class ToggleSwitch : public QAbstractButton {
    Q_OBJECT
    Q_PROPERTY(qreal offset READ offset WRITE setOffset)

    static constexpr int  kTrackW  = 46;
    static constexpr int  kTrackH  = 24;
    static constexpr int  kMargin  = 2;
    static constexpr int  kKnobD   = kTrackH - 2 * kMargin;
    static constexpr qreal kTravel = kTrackW - kKnobD - 2 * kMargin;

public:
    explicit ToggleSwitch(const QString &text = {}, QWidget *parent = nullptr)
        : QAbstractButton(parent), m_offset(0.0)
    {
        setCheckable(true);
        setText(text);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        m_anim = new QPropertyAnimation(this, "offset", this);
        m_anim->setDuration(150);
        m_anim->setEasingCurve(QEasingCurve::InOutQuad);

        connect(this, &QAbstractButton::toggled, this, [this](bool on) {
            m_anim->stop();
            m_anim->setStartValue(m_offset);
            m_anim->setEndValue(on ? 1.0 : 0.0);
            m_anim->start();
        });
    }

    QSize sizeHint() const override {
        int textW = 0;
        if (!text().isEmpty())
            textW = fontMetrics().horizontalAdvance(text()) + 8;
        return { kTrackW + textW, kTrackH };
    }

    qreal offset() const { return m_offset; }
    void  setOffset(qreal o) { m_offset = o; update(); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        // Track
        QColor trackCol = isEnabled()
            ? (isChecked() ? QColor("#27ae60") : QColor("#bdc3c7"))
            : QColor("#ecf0f1");
        p.setBrush(trackCol);
        p.setPen(Qt::NoPen);
        p.drawRoundedRect(QRectF(0, 0, kTrackW, kTrackH), kTrackH / 2.0, kTrackH / 2.0);

        // Knob
        qreal kx = kMargin + m_offset * kTravel;
        p.setBrush(isEnabled() ? Qt::white : QColor("#d0d0d0"));
        p.drawEllipse(QRectF(kx, kMargin, kKnobD, kKnobD));

        // Label
        if (!text().isEmpty()) {
            p.setPen(isEnabled() ? QColor("#2c3e50") : QColor("#bdc3c7"));
            p.setFont(font());
            p.drawText(QRect(kTrackW + 8, 0, width() - kTrackW - 8, height()),
                       Qt::AlignVCenter | Qt::AlignLeft, text());
        }
    }

private:
    qreal               m_offset;
    QPropertyAnimation *m_anim;
};
// ─────────────────────────────────────────────────────────────────────────────

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), totalDataPoints(0) {
    setWindowTitle("Urban District Energy Analysis");
    setWindowIcon(QIcon());
    
    resize(1400, 800);
    setMinimumSize(900, 600);
    move(QGuiApplication::primaryScreen()->geometry().center() - frameGeometry().center());
    
    setupUI();
    applyStyling();
    createPages();
}

MainWindow::~MainWindow() {
}

void MainWindow::setupUI() {
    // Widget centrale
    QWidget *centralWidget = new QWidget(this);
    centralWidget->setStyleSheet("background-color: #f5f5f5;");
    setCentralWidget(centralWidget);
    
    // Layout principale orizzontale
    QHBoxLayout *mainLayout = new QHBoxLayout(centralWidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // --- SIDEBAR (sinistra) ---
    sidebar = new QListWidget();
    sidebar->setMaximumWidth(250);
    sidebar->setMinimumWidth(200);
    sidebar->setUniformItemSizes(true);
    sidebar->setFont(QFont(".AppleSystemUIFont", 11));
    sidebar->setFocusPolicy(Qt::NoFocus);
    
    // Aggiungi elementi al menu con icone emoji
    QStringList menuItems = {
        "🏠 Home",
        "🔄 Conversione File",
        "📊 Visualizzazione Clima",
        "🗺️ Visualizzazione GeoJSON",
        "🏗️ Editor Componenti",
        "📅 Gestione Schedules",
        "📋 Tabella Tipologie",
        "🏢 Visualizzazione Edifici",
        "▶️ Avvia Simulazione",
        "📈 Risultati Simulazione",
        "⚙️ Impostazioni",
        "ℹ️ Informazioni",
        "📞 Contatti"
    };
    
    for (const QString &item : menuItems) {
        QListWidgetItem *listItem = new QListWidgetItem(item);
        listItem->setSizeHint(QSize(200, 50));
        sidebar->addItem(listItem);
    }
    
    // Seleziona il primo elemento
    sidebar->setCurrentRow(0);
    
    // --- AREA DI CONTENUTO (destra) ---
    contentArea = new QStackedWidget();
    contentArea->setStyleSheet("background-color: #ffffff;");
    
    // Wrappa contentArea in uno scroll area
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidget(contentArea);
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { background-color: #ffffff; border: none; }");
    // Abilita scroll orizzontale
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Connetti il segnale di cambio selezione
    connect(sidebar, &QListWidget::itemSelectionChanged,
            this, &MainWindow::onMenuItemClicked);
    
    // Aggiungi i widget al layout principale
    mainLayout->addWidget(sidebar, 0);
    mainLayout->addWidget(scrollArea, 1);
}

void MainWindow::applyStyling() {
    // Stile CSS per la sidebar
    sidebar->setStyleSheet(
        "QListWidget {"
        "    background-color: #2c3e50;"
        "    border-right: 2px solid #34495e;"
        "    outline: none;"
        "}"
        "QListWidget::item {"
        "    padding: 12px 15px;"
        "    color: #ecf0f1;"
        "    border-bottom: 1px solid #34495e;"
        "}"
        "QListWidget::item:hover {"
        "    background-color: #34495e;"
        "}"
        "QListWidget::item:selected {"
        "    background-color: #3498db;"
        "    color: white;"
        "    font-weight: bold;"
        "}"
    );
}

void MainWindow::createPages() {
    createHomePage();
    createFileConversionPage();
    createWeatherVisualizationPage();
    createGeoJSONVisualizationPage();
    createComponentsEditorPage();
    createSchedulesEditorPage();
    createBuildingTypesEditorPage();
    createBuildingsEditorPage();
    createSimulationPage();
    createSimulationResultsPage();
    createSettingsPage();
    createInfoPage();
    createContactPage();

    // Impedisce che QStackedWidget propaghi il minimum size massimo di tutte le
    // pagine (alcune hanno chart/table con minimum height elevati). In questo modo
    // la scroll area esterna non mostra scrollbar inutili quando si è su pagine leggere.
    for (int i = 0; i < contentArea->count(); ++i)
        contentArea->widget(i)->setMinimumSize(0, 0);
}

void MainWindow::createHomePage() {
    QWidget *homePage = new QWidget();
    homePage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *homeLayout = new QVBoxLayout(homePage);
    homeLayout->setContentsMargins(40, 40, 40, 40);
    homeLayout->setSpacing(20);
    
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>Benvenuto!</h1>");
    QLabel *contentLabel = new QLabel(
        "<p style='color: #555; font-size: 14px; line-height: 1.6;'>"
        "Questa è la pagina Home dell'applicazione.<br><br>"
        "Usa il menu sulla sinistra per navigare tra le diverse sezioni.<br><br>"
        "L'applicazione è stata sviluppata con Qt6 e C++20 per offrire "
        "un'esperienza moderna e reattiva."
        "</p>"
    );
    contentLabel->setWordWrap(true);
    
    homeLayout->addWidget(titleLabel);
    homeLayout->addWidget(contentLabel);
    homeLayout->addStretch();
    contentArea->addWidget(homePage);
}

void MainWindow::createFileConversionPage() {
    QWidget *fileConversionPage = new QWidget();
    fileConversionPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *fileConvLayout = new QVBoxLayout(fileConversionPage);
    fileConvLayout->setContentsMargins(40, 40, 40, 40);
    fileConvLayout->setSpacing(20);
    
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>Conversione File</h1>");
    QLabel *descLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "Converti file tra diversi formati utilizzando i seguenti strumenti.<br>"
        "Seleziona il file di input e scegli dove salvare il file convertito."
        "</p>"
    );
    descLabel->setWordWrap(true);
    
    // Layout orizzontale principale per i pulsanti
    QHBoxLayout *buttonsHLayout = new QHBoxLayout();
    buttonsHLayout->setSpacing(30);
    
    // --- Convertitore EPW → CSV ---
    QVBoxLayout *epwVLayout = new QVBoxLayout();
    epwVLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *epwLabel = new QLabel("<b style='color: #2c3e50;'>EPW → wtst</b>");
    epwLabel->setAlignment(Qt::AlignCenter);
    epwLabel->setFixedHeight(30);
    QPushButton *epwButton = new QPushButton("Seleziona");
    epwButton->setFixedHeight(45);
    epwButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #21618c;"
        "}"
    );
    epwVLayout->addWidget(epwLabel);
    epwVLayout->addWidget(epwButton);
    epwVLayout->setSpacing(10);
    
    // --- Convertitore IDF → JSON ---
    QVBoxLayout *idfVLayout = new QVBoxLayout();
    idfVLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *idfLabel = new QLabel("<b style='color: #2c3e50;'>IDF → JSON/EPJSON</b>");
    idfLabel->setAlignment(Qt::AlignCenter);
    idfLabel->setFixedHeight(30);
    QPushButton *idfButton = new QPushButton("Seleziona");
    idfButton->setFixedHeight(45);
    idfButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #229954;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #1e8449;"
        "}"
    );
    idfVLayout->addWidget(idfLabel);
    idfVLayout->addWidget(idfButton);
    idfVLayout->setSpacing(10);
    
    // --- Convertitore SHP → GeoJSON ---
    QVBoxLayout *shpVLayout = new QVBoxLayout();
    shpVLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *shpLabel = new QLabel("<b style='color: #2c3e50;'>SHP → GeoJSON</b>");
    shpLabel->setAlignment(Qt::AlignCenter);
    shpLabel->setFixedHeight(30);
    QPushButton *shpButton = new QPushButton("Seleziona");
    shpButton->setFixedHeight(45);
    shpButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #e67e22;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #d35400;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #ba4a00;"
        "}"
    );
    shpVLayout->addWidget(shpLabel);
    shpVLayout->addWidget(shpButton);
    shpVLayout->setSpacing(10);
    
    // --- Convertitore GeoJSON + CSV → JSON ---
    QVBoxLayout *geoJsonVLayout = new QVBoxLayout();
    geoJsonVLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *geoJsonLabel = new QLabel("<b style='color: #2c3e50;'>GeoJSON+CSV → JSON</b>");
    geoJsonLabel->setAlignment(Qt::AlignCenter);
    geoJsonLabel->setFixedHeight(30);
    QPushButton *geoJsonButton = new QPushButton("Seleziona");
    geoJsonButton->setFixedHeight(45);
    geoJsonButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #9b59b6;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #8e44ad;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #7d3c98;"
        "}"
    );
    geoJsonVLayout->addWidget(geoJsonLabel);
    geoJsonVLayout->addWidget(geoJsonButton);
    geoJsonVLayout->setSpacing(10);

    // --- Convertitore Schedule:Compact → 8760 ---
    QVBoxLayout *schedVLayout = new QVBoxLayout();
    schedVLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *schedLabel = new QLabel("<b style='color: #2c3e50;'>Schedule:Compact → 8760</b>");
    schedLabel->setAlignment(Qt::AlignCenter);
    schedLabel->setFixedHeight(30);
    QPushButton *schedButton = new QPushButton("Seleziona");
    schedButton->setFixedHeight(45);
    schedButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #f1c40f;"
        "    color: #2c3e50;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "    font-weight: bold;"
        "}"
        "QPushButton:hover {"
        "    background-color: #f39c12;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #e67e22;"
        "}"
    );
    schedVLayout->addWidget(schedLabel);
    schedVLayout->addWidget(schedButton);
    schedVLayout->setSpacing(10);
    
    // Aggiungi i layout verticali al layout orizzontale principale
    buttonsHLayout->addLayout(epwVLayout);
    buttonsHLayout->addLayout(idfVLayout);
    buttonsHLayout->addLayout(shpVLayout);
    buttonsHLayout->addLayout(geoJsonVLayout);
    buttonsHLayout->addLayout(schedVLayout);
    buttonsHLayout->addStretch();
    
    // --- Casella di testo per l'output della console ---
    QLabel *outputLabel = new QLabel("<b style='color: #2c3e50;'>Output Console:</b>");
    outputConsole = new QTextEdit();
    outputConsole->setReadOnly(true);
    outputConsole->setStyleSheet(
        "QTextEdit {"
        "    background-color: #2c3e50;"
        "    color: #ecf0f1;"
        "    border: 1px solid #34495e;"
        "    border-radius: 5px;"
        "    padding: 10px;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 11px;"
        "}"
    );
    outputConsole->setMinimumHeight(150);
    
    // Aggiungi tutti i widget al layout principale
    fileConvLayout->addWidget(titleLabel);
    fileConvLayout->addWidget(descLabel);
    fileConvLayout->addSpacing(20);
    fileConvLayout->addLayout(buttonsHLayout);
    fileConvLayout->addSpacing(20);
    fileConvLayout->addWidget(outputLabel);
    fileConvLayout->addWidget(outputConsole);
    
    // Connetti i pulsanti agli slot
    connect(epwButton, &QPushButton::clicked, this, &MainWindow::onConvertEPW);
    connect(idfButton, &QPushButton::clicked, this, &MainWindow::onConvertIDF);
    connect(shpButton, &QPushButton::clicked, this, &MainWindow::onConvertSHP);
    connect(geoJsonButton, &QPushButton::clicked, this, &MainWindow::onConvertGeoJSON);
    connect(schedButton, &QPushButton::clicked, this, &MainWindow::onConvertScheduleCompact);
    
    contentArea->addWidget(fileConversionPage);
}

void MainWindow::createWeatherVisualizationPage() {
    QWidget *weatherPage = new QWidget();
    weatherPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *weatherLayout = new QVBoxLayout(weatherPage);
    weatherLayout->setContentsMargins(20, 20, 20, 20);
    weatherLayout->setSpacing(15);
    
    // 1. TITOLO
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>Visualizzazione Dati Climatici</h1>");
    weatherLayout->addWidget(titleLabel);
    
    // 2. DESCRIZIONE
    QLabel *descLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "Carica un file per visualizzare i dati... o prima convertilo da epw a wtst e poi dovrai caricalo"
        "</p>"
    );
    descLabel->setWordWrap(true);
    weatherLayout->addWidget(descLabel);
    
    // 3. PULSANTE CARICAMENTO
    QHBoxLayout *loadLayout = new QHBoxLayout();
    QLabel *loadLabel = new QLabel("<b style='color: #2c3e50;'>Carica file WTST:</b>");
    QPushButton *loadButton = new QPushButton("Seleziona File");
    loadButton->setFixedHeight(40);
    loadButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #16a085;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 150px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #138d75;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #117a65;"
        "}"
    );
    QPushButton *epwConvertBtn = new QPushButton("🔄 EPW → wtst");
    epwConvertBtn->setFixedHeight(40);
    epwConvertBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e67e22; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 130px;"
        "}"
        "QPushButton:hover { background-color: #ca6f1e; }"
        "QPushButton:pressed { background-color: #a04000; }"
    );
    loadLayout->addWidget(loadLabel);
    loadLayout->addWidget(loadButton);
    loadLayout->addWidget(epwConvertBtn);
    loadLayout->addStretch();
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadWeatherFile);
    connect(epwConvertBtn, &QPushButton::clicked, this, &MainWindow::onConvertEPW);
    weatherLayout->addLayout(loadLayout);
    weatherLayout->addSpacing(10);
    
    // 4. CASELLA INTESTAZIONE FILE (Prime 8 righe)
    QGroupBox *fileHeaderBox = new QGroupBox("📄 Intestazione File WTST (Prime 8 righe)");
    fileHeaderBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #9b59b6;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *headerLayout = new QVBoxLayout(fileHeaderBox);
    fileHeaderText = new QTextEdit();
    fileHeaderText->setReadOnly(true);
    fileHeaderText->setMaximumHeight(150);
    fileHeaderText->setStyleSheet(
        "QTextEdit {"
        "    background-color: #ecf0f1;"
        "    border: 1px solid #bdc3c7;"
        "    border-radius: 3px;"
        "    padding: 5px;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 10px;"
        "}"
    );
    fileHeaderText->setPlainText("In attesa di caricamento file...");
    headerLayout->addWidget(fileHeaderText);
    weatherLayout->addWidget(fileHeaderBox);
    
    // 5. AREA SCROLLABILE PER I GRAFICI
    QScrollArea *scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setStyleSheet("QScrollArea { border: none; }");
    
    QWidget *chartsWidget = new QWidget();
    chartsLayout = new QVBoxLayout(chartsWidget);
    chartsLayout->setSpacing(15);
    chartsLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *chartsTitle = new QLabel("<h2 style='color: #2c3e50;'>📈 Grafici Dati Climatici</h2>");
    chartsLayout->addWidget(chartsTitle);
    
    // Controlli per range asse X
    QGroupBox *rangeBox = new QGroupBox("📊 Intervallo Visualizzazione Asse X");
    rangeBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #2ecc71;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QHBoxLayout *rangeLayout = new QHBoxLayout(rangeBox);
    
    QLabel *xMinLabel = new QLabel("Valore iniziale:");
    xMinSpinBox = new QSpinBox();
    xMinSpinBox->setMinimum(0);
    xMinSpinBox->setMaximum(0);
    xMinSpinBox->setValue(0);
    xMinSpinBox->setEnabled(false);
    xMinSpinBox->setMinimumWidth(100);
    xMinSpinBox->setStyleSheet("QSpinBox { padding: 5px; font-size: 12px; }");
    
    QLabel *xMaxLabel = new QLabel("Valore finale:");
    xMaxSpinBox = new QSpinBox();
    xMaxSpinBox->setMinimum(10);
    xMaxSpinBox->setMaximum(10);
    xMaxSpinBox->setValue(10);
    xMaxSpinBox->setEnabled(false);
    xMaxSpinBox->setMinimumWidth(100);
    xMaxSpinBox->setStyleSheet("QSpinBox { padding: 5px; font-size: 12px; }");
    
    rangeLayout->addWidget(xMinLabel);
    rangeLayout->addWidget(xMinSpinBox);
    rangeLayout->addSpacing(20);
    rangeLayout->addWidget(xMaxLabel);
    rangeLayout->addWidget(xMaxSpinBox);
    rangeLayout->addStretch();
    
    QLabel *infoLabel = new QLabel("<i style='color: #7f8c8d;'>Nota: Il valore finale deve essere almeno 10 maggiore del valore iniziale</i>");
    rangeLayout->addWidget(infoLabel);
    
    // Connetti i segnali degli spinbox
    connect(xMinSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onXRangeChanged);
    connect(xMaxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onXRangeChanged);
    
    chartsLayout->addWidget(rangeBox);
    chartsLayout->addSpacing(10);
    
    // Crea grafico temperatura (inizialmente vuoto)
    QChart *tempChart = new QChart();
    tempChart->setTitle("🌡️ Temperatura (Colonna 5)");
    tempChart->setAnimationOptions(QChart::SeriesAnimations);
    temperatureChartView = new QChartView(tempChart);
    temperatureChartView->setRenderHint(QPainter::Antialiasing);
    temperatureChartView->setMinimumHeight(300);
    temperatureChartView->setStyleSheet("QChartView { border: 2px solid #e74c3c; border-radius: 5px; background-color: white; }");
    chartsLayout->addWidget(temperatureChartView);
    
    // Crea grafico umidità (inizialmente vuoto)
    QChart *humChart = new QChart();
    humChart->setTitle("💧 Umidità Relativa (Colonna 6)");
    humChart->setAnimationOptions(QChart::SeriesAnimations);
    humidityChartView = new QChartView(humChart);
    humidityChartView->setRenderHint(QPainter::Antialiasing);
    humidityChartView->setMinimumHeight(300);
    humidityChartView->setStyleSheet("QChartView { border: 2px solid #3498db; border-radius: 5px; background-color: white; }");
    chartsLayout->addWidget(humidityChartView);
    
    // Crea grafico radiazione solare (inizialmente vuoto)
    QChart *radChart = new QChart();
    radChart->setTitle("☀️ Radiazione Solare");
    radChart->setAnimationOptions(QChart::SeriesAnimations);
    radiationChartView = new QChartView(radChart);
    radiationChartView->setRenderHint(QPainter::Antialiasing);
    radiationChartView->setMinimumHeight(350);
    radiationChartView->setStyleSheet("QChartView { border: 2px solid #f39c12; border-radius: 5px; background-color: white; }");
    chartsLayout->addWidget(radiationChartView);
    
    // Menu a tendina per selezione colonne aggiuntive
    QGroupBox *radiationControlBox = new QGroupBox("🔧 Seleziona colonne aggiuntive da visualizzare");
    radiationControlBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #e67e22;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QHBoxLayout *comboLayout = new QHBoxLayout(radiationControlBox);
    
    QLabel *combo1Label = new QLabel("Colonna extra 1:");
    radiationCombo1 = new QComboBox();
    radiationCombo1->setMinimumWidth(200);
    radiationCombo1->setEnabled(false);  // Disabilitato finché non si carica un file
    
    QLabel *combo2Label = new QLabel("Colonna extra 2:");
    radiationCombo2 = new QComboBox();
    radiationCombo2->setMinimumWidth(200);
    radiationCombo2->setEnabled(false);  // Disabilitato finché non si carica un file
    
    comboLayout->addWidget(combo1Label);
    comboLayout->addWidget(radiationCombo1);
    comboLayout->addSpacing(20);
    comboLayout->addWidget(combo2Label);
    comboLayout->addWidget(radiationCombo2);
    comboLayout->addStretch();
    
    // Connetti i segnali dei combo box
    connect(radiationCombo1, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onRadiationComboChanged);
    connect(radiationCombo2, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &MainWindow::onRadiationComboChanged);
    
    chartsLayout->addWidget(radiationControlBox);
    
    chartsLayout->addStretch();
    
    scrollArea->setWidget(chartsWidget);
    weatherLayout->addWidget(scrollArea);
    
    // Spacer per riempire lo spazio rimanente
    weatherLayout->addStretch();
    
    contentArea->addWidget(weatherPage);
}

void MainWindow::createGeoJSONVisualizationPage() {
    QWidget *geoPage = new QWidget();
    geoPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *geoLayout = new QVBoxLayout(geoPage);
    geoLayout->setContentsMargins(20, 20, 20, 20);
    geoLayout->setSpacing(15);
    
    // 1. TITOLO
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>🗺️ Visualizzazione File GeoJSON</h1>");
    geoLayout->addWidget(titleLabel);
    
    // 2. DESCRIZIONE
    QLabel *descLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "Carica un file GeoJSON per visualizzare i poligoni e le loro proprietà..."
        "</p>"
    );
    descLabel->setWordWrap(true);
    geoLayout->addWidget(descLabel);
    
    // 3. PULSANTE CARICAMENTO
    QHBoxLayout *loadLayout = new QHBoxLayout();
    QLabel *loadLabel = new QLabel("<b style='color: #2c3e50;'>Carica file GeoJSON:</b>");
    QPushButton *loadButton = new QPushButton("Seleziona File");
    loadButton->setFixedHeight(40);
    loadButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 150px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #2980b9;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #21618c;"
        "}"
    );
    QPushButton *shpConvertBtn = new QPushButton("🔄 SHP → GeoJSON");
    shpConvertBtn->setFixedHeight(40);
    shpConvertBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e67e22; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 150px;"
        "}"
        "QPushButton:hover { background-color: #ca6f1e; }"
        "QPushButton:pressed { background-color: #a04000; }"
    );
    loadLayout->addWidget(loadLabel);
    loadLayout->addWidget(loadButton);
    loadLayout->addWidget(shpConvertBtn);
    loadLayout->addStretch();
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadGeoJSONFile);
    connect(shpConvertBtn, &QPushButton::clicked, this, &MainWindow::onConvertSHP);
    geoLayout->addLayout(loadLayout);
    geoLayout->addSpacing(10);
    
    // 4. LABELS PER TYPE E NAME (affiancate)
    QHBoxLayout *infoLayout = new QHBoxLayout();
    
    QGroupBox *typeBox = new QGroupBox("📋 Type");
    typeBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #9b59b6;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *typeLayout = new QVBoxLayout(typeBox);
    geoTypeLabel = new QLabel("In attesa di caricamento...");
    geoTypeLabel->setStyleSheet("QLabel { padding: 5px; font-size: 13px; color: #555; }");
    typeLayout->addWidget(geoTypeLabel);
    
    QGroupBox *nameBox = new QGroupBox("🏷️ Name");
    nameBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #e67e22;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *nameLayout = new QVBoxLayout(nameBox);
    geoNameLabel = new QLabel("In attesa di caricamento...");
    geoNameLabel->setStyleSheet("QLabel { padding: 5px; font-size: 13px; color: #555; }");
    nameLayout->addWidget(geoNameLabel);
    
    infoLayout->addWidget(typeBox);
    infoLayout->addWidget(nameBox);
    geoLayout->addLayout(infoLayout);
    geoLayout->addSpacing(10);
    
    // 5. AREA GRAFICA E PROPRIETÀ
    QHBoxLayout *mainContentLayout = new QHBoxLayout();
    
    // Area grafica per poligoni
    QGroupBox *graphicsBox = new QGroupBox("🗺️ Visualizzazione Poligoni (Gauss-Boaga)");
    graphicsBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #16a085;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *graphicsLayout = new QVBoxLayout(graphicsBox);
    
    // Barra controlli zoom
    QHBoxLayout *zoomControlsLayout = new QHBoxLayout();
    QPushButton *zoomInBtn = new QPushButton("🔍+ Zoom In");
    QPushButton *zoomOutBtn = new QPushButton("🔍- Zoom Out");
    QPushButton *zoomFitBtn = new QPushButton("⛶ Adatta Vista");
    
    zoomInBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 15px;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #229954; }"
        "QPushButton:pressed { background-color: #1e8449; }"
    );
    
    zoomOutBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e74c3c;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 15px;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #c0392b; }"
        "QPushButton:pressed { background-color: #a93226; }"
    );
    
    zoomFitBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 15px;"
        "    border-radius: 4px;"
        "    font-size: 12px;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }"
    );
    
    zoomControlsLayout->addWidget(zoomInBtn);
    zoomControlsLayout->addWidget(zoomOutBtn);
    zoomControlsLayout->addWidget(zoomFitBtn);
    zoomControlsLayout->addStretch();
    
    QLabel *zoomHintLabel = new QLabel("<i style='color: #7f8c8d;'>Usa anche la rotella del mouse per lo zoom</i>");
    zoomControlsLayout->addWidget(zoomHintLabel);
    
    graphicsLayout->addLayout(zoomControlsLayout);
    
    geoGraphicsScene = new QGraphicsScene();
    geoGraphicsView = new QGraphicsView(geoGraphicsScene);
    geoGraphicsView->setRenderHint(QPainter::Antialiasing);
    geoGraphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    geoGraphicsView->setStyleSheet("QGraphicsView { background-color: #ecf0f1; border: 1px solid #bdc3c7; }");
    geoGraphicsView->setMinimumHeight(400);
    geoGraphicsView->setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    geoGraphicsView->setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    graphicsLayout->addWidget(geoGraphicsView);
    
    // Connetti i pulsanti zoom
    connect(zoomInBtn, &QPushButton::clicked, [this]() {
        geoGraphicsView->scale(1.25, 1.25);
    });
    
    connect(zoomOutBtn, &QPushButton::clicked, [this]() {
        geoGraphicsView->scale(0.8, 0.8);
    });
    
    connect(zoomFitBtn, &QPushButton::clicked, [this]() {
        if (geoGraphicsScene && !geoGraphicsScene->items().isEmpty()) {
            geoGraphicsView->fitInView(geoGraphicsScene->sceneRect(), Qt::KeepAspectRatio);
        }
    });
    
    // Area proprietà selezionate
    QGroupBox *propsBox = new QGroupBox("📊 Proprietà Poligono Selezionato");
    propsBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #e74c3c;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *propsLayout = new QVBoxLayout(propsBox);
    geoPropertiesText = new QTextEdit();
    geoPropertiesText->setReadOnly(true);
    geoPropertiesText->setMinimumWidth(300);
    geoPropertiesText->setStyleSheet(
        "QTextEdit {"
        "    background-color: #ecf0f1;"
        "    border: 1px solid #bdc3c7;"
        "    border-radius: 3px;"
        "    padding: 5px;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 11px;"
        "}"
    );
    geoPropertiesText->setPlainText("Clicca su un poligono per visualizzare le sue proprietà...");
    propsLayout->addWidget(geoPropertiesText);
    
    mainContentLayout->addWidget(graphicsBox, 2);
    mainContentLayout->addWidget(propsBox, 1);
    geoLayout->addLayout(mainContentLayout);
    
    contentArea->addWidget(geoPage);
}

void MainWindow::createComponentsEditorPage() {
    QWidget *componentsPage = new QWidget();
    componentsPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *componentsLayout = new QVBoxLayout(componentsPage);
    componentsLayout->setContentsMargins(20, 10, 20, 10);
    componentsLayout->setSpacing(8);

    // 1. TITOLO
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>🏗️ Editor Componenti Edifici</h1>");
    componentsLayout->addWidget(titleLabel);

    // 2. DESCRIZIONE
    QLabel *descLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "Carica un file JSON dei componenti. "
        "<b>Colonne</b> = componenti, <b>Righe</b> = campi. "
        "<b>Doppio clic</b> su una cella per modificarne il valore (riga «Nome» per rinominare). "
        "<b>Tasto destro</b> sulla tabella per aggiungere/eliminare componenti (colonne) e campi (righe)."
        "</p>"
    );
    descLabel->setWordWrap(true);
    componentsLayout->addWidget(descLabel);

    // 3. BARRA PULSANTI (solo Carica e Salva)
    QHBoxLayout *buttonsLayout = new QHBoxLayout();

    QPushButton *loadButton = new QPushButton("📁 Carica File JSON");
    loadButton->setFixedHeight(40);
    loadButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 150px;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }"
    );

    saveComponentsBtn = new QPushButton("💾 Salva File");
    saveComponentsBtn->setFixedHeight(40);
    saveComponentsBtn->setEnabled(false);
    saveComponentsBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 150px;"
        "}"
        "QPushButton:hover { background-color: #229954; }"
        "QPushButton:pressed { background-color: #1e8449; }"
        "QPushButton:disabled { background-color: #95a5a6; color: #ecf0f1; }"
    );

    QPushButton *idfConvertBtn = new QPushButton("🔄 IDF → JSON");
    idfConvertBtn->setFixedHeight(40);
    idfConvertBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e67e22; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 130px;"
        "}"
        "QPushButton:hover { background-color: #ca6f1e; }"
        "QPushButton:pressed { background-color: #a04000; }"
    );

    buttonsLayout->addWidget(loadButton);
    buttonsLayout->addWidget(saveComponentsBtn);
    buttonsLayout->addWidget(idfConvertBtn);
    buttonsLayout->addStretch();

    componentsLayout->addLayout(buttonsLayout);
    // no extra spacing needed

    connect(loadButton,        &QPushButton::clicked, this, &MainWindow::onLoadComponentsFile);
    connect(saveComponentsBtn, &QPushButton::clicked, this, &MainWindow::onSaveComponentsFile);
    connect(idfConvertBtn,     &QPushButton::clicked, this, &MainWindow::onConvertIDF);

    // 4. TAB WIDGET — due righe di tab (una sopra l'altra) + contenuto
    const QString tabBarStyle =
        "QTabBar { background: transparent; }"
        "QTabBar::tab {"
        "    background-color: #ecf0f1; color: #2c3e50;"
        "    padding: 7px 12px; font-weight: bold;"
        "    border: 1px solid #bdc3c7; border-bottom: none;"
        "    border-radius: 4px 4px 0 0; margin-right: 2px;"
        "}"
        "QTabBar::tab:selected { background-color: #16a085; color: white; }"
        "QTabBar::tab:hover:!selected { background-color: #d5f5e3; }"
        "QTabBar[inactive=\"true\"]::tab:selected {"
        "    background-color: #ecf0f1; color: #2c3e50;"
        "}";

    compTabBar1 = new QTabBar();
    compTabBar1->setExpanding(false);
    compTabBar1->setDrawBase(false);
    compTabBar1->setUsesScrollButtons(false);
    compTabBar1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    compTabBar1->setStyleSheet(tabBarStyle);

    compTabBar2 = new QTabBar();
    compTabBar2->setExpanding(false);
    compTabBar2->setDrawBase(false);
    compTabBar2->setUsesScrollButtons(false);
    compTabBar2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    compTabBar2->setStyleSheet(tabBarStyle);

    componentsTabs = new QTabWidget();
    componentsTabs->tabBar()->hide();
    componentsTabs->setStyleSheet(
        "QTabWidget::pane {"
        "    border: 2px solid #16a085;"
        "    border-radius: 0 5px 5px 5px;"
        "}"
    );
    componentsTabs->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(compTabBar1, &QTabBar::currentChanged, this, [this](int idx) {
        if (idx < 0 || _syncingTabBars) return;
        _syncingTabBars = true;
        componentsTabs->setCurrentIndex(idx);
        compTabBar2->setProperty("inactive", true);
        compTabBar1->setProperty("inactive", false);
        compTabBar2->style()->unpolish(compTabBar2);
        compTabBar2->style()->polish(compTabBar2);
        compTabBar1->style()->unpolish(compTabBar1);
        compTabBar1->style()->polish(compTabBar1);
        _syncingTabBars = false;
    });

    connect(compTabBar2, &QTabBar::currentChanged, this, [this](int idx) {
        if (idx < 0 || _syncingTabBars) return;
        _syncingTabBars = true;
        componentsTabs->setCurrentIndex(compTabBar1->count() + idx);
        compTabBar1->setProperty("inactive", true);
        compTabBar2->setProperty("inactive", false);
        compTabBar1->style()->unpolish(compTabBar1);
        compTabBar1->style()->polish(compTabBar1);
        compTabBar2->style()->unpolish(compTabBar2);
        compTabBar2->style()->polish(compTabBar2);
        _syncingTabBars = false;
    });

    QWidget *tabsSection = new QWidget();
    QVBoxLayout *tabsSectionLayout = new QVBoxLayout(tabsSection);
    tabsSectionLayout->setContentsMargins(0, 0, 0, 0);
    tabsSectionLayout->setSpacing(0);
    tabsSectionLayout->addWidget(compTabBar1);
    tabsSectionLayout->addWidget(compTabBar2);
    tabsSectionLayout->addWidget(componentsTabs);
    componentsLayout->addWidget(tabsSection, 1);
    componentsLayout->setAlignment(Qt::AlignTop);

    contentArea->addWidget(componentsPage);
}

void MainWindow::createBuildingTypesEditorPage() {
    QWidget *buildingTypesPage = new QWidget();
    buildingTypesPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *mainLayout = new QVBoxLayout(buildingTypesPage);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // 1. TITOLO
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>📋 Editor Tabella Tipologie Edilizie</h1>");
    mainLayout->addWidget(titleLabel);
    
    // 2. DESCRIZIONE
    QLabel *descLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "Carica un file CSV delle tipologie edilizie per visualizzare e modificare direttamente i dati come in un foglio di calcolo..."
        "</p>"
    );
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);
    
    // 3. BARRA PULSANTI
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    
    QPushButton *loadButton = new QPushButton("📁 Carica CSV");
    loadButton->setFixedHeight(40);
    loadButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }"
    );
    
    addRowBtn = new QPushButton("➕ Aggiungi Riga");
    addRowBtn->setFixedHeight(40);
    addRowBtn->setEnabled(false);
    addRowBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #229954; }"
        "QPushButton:pressed { background-color: #1e8449; }"
        "QPushButton:disabled {"
        "    background-color: #95a5a6;"
        "    color: #ecf0f1;"
        "}"
    );
    
    addColumnBtn = new QPushButton("➕ Aggiungi Colonna");
    addColumnBtn->setFixedHeight(40);
    addColumnBtn->setEnabled(false);
    addColumnBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #16a085;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #138d75; }"
        "QPushButton:pressed { background-color: #117a65; }"
        "QPushButton:disabled {"
        "    background-color: #95a5a6;"
        "    color: #ecf0f1;"
        "}"
    );
    
    deleteRowBtn = new QPushButton("🗑️ Elimina Riga");
    deleteRowBtn->setFixedHeight(40);
    deleteRowBtn->setEnabled(false);
    deleteRowBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e67e22;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #d35400; }"
        "QPushButton:pressed { background-color: #ba4a00; }"
        "QPushButton:disabled {"
        "    background-color: #95a5a6;"
        "    color: #ecf0f1;"
        "}"
    );
    
    deleteColumnBtn = new QPushButton("🗑️ Elimina Colonna");
    deleteColumnBtn->setFixedHeight(40);
    deleteColumnBtn->setEnabled(false);
    deleteColumnBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #c0392b;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #a93226; }"
        "QPushButton:pressed { background-color: #922b21; }"
        "QPushButton:disabled {"
        "    background-color: #95a5a6;"
        "    color: #ecf0f1;"
        "}"
    );
    
    saveBuildingTypesBtn = new QPushButton("💾 Salva CSV");
    saveBuildingTypesBtn->setFixedHeight(40);
    saveBuildingTypesBtn->setEnabled(false);
    saveBuildingTypesBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #8e44ad;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #7d3c98; }"
        "QPushButton:pressed { background-color: #6c3483; }"
        "QPushButton:disabled {"
        "    background-color: #95a5a6;"
        "    color: #ecf0f1;"
        "}"
    );
    
    buttonsLayout->addWidget(loadButton);
    buttonsLayout->addWidget(addRowBtn);
    buttonsLayout->addWidget(addColumnBtn);
    buttonsLayout->addWidget(deleteRowBtn);
    buttonsLayout->addWidget(deleteColumnBtn);
    buttonsLayout->addWidget(saveBuildingTypesBtn);

    createBtn = new QPushButton("🚀 Crea");
    createBtn->setFixedHeight(40);
    createBtn->setEnabled(false);
    createBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #2c3e50; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 120px;"
        "}"
        "QPushButton:hover { background-color: #1a252f; }"
        "QPushButton:pressed { background-color: #17202a; }"
        "QPushButton:disabled { background-color: #95a5a6; color: #ecf0f1; }"
    );
    buttonsLayout->addWidget(createBtn);
    buttonsLayout->addStretch();
    
    mainLayout->addLayout(buttonsLayout);
    mainLayout->addSpacing(10);
    
    // Connetti i segnali
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadBuildingTypesFile);
    connect(addRowBtn, &QPushButton::clicked, this, &MainWindow::onAddRow);
    connect(addColumnBtn, &QPushButton::clicked, this, &MainWindow::onAddColumn);
    connect(deleteRowBtn, &QPushButton::clicked, this, &MainWindow::onDeleteRow);
    connect(deleteColumnBtn, &QPushButton::clicked, this, &MainWindow::onDeleteColumn);
    connect(saveBuildingTypesBtn, &QPushButton::clicked, this, &MainWindow::onSaveBuildingTypesFile);
    connect(createBtn, &QPushButton::clicked, this, &MainWindow::onCrea);
    
    // 4. TABELLA EDITABILE (stile spreadsheet)
    QGroupBox *tableBox = new QGroupBox("📊 Dati Tipologie Edilizie");
    tableBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #2ecc71;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *tableLayout = new QVBoxLayout(tableBox);
    tableLayout->setSpacing(0);
    tableLayout->setContentsMargins(6, 6, 6, 6);

    // Scrollbar orizzontale in cima, sincronizzata con la tabella
    topScrollBar = new QScrollBar(Qt::Horizontal);
    topScrollBar->setStyleSheet(
        "QScrollBar:horizontal { height: 14px; background: #ecf0f1; border-radius: 4px; }"
        "QScrollBar::handle:horizontal { background: #95a5a6; border-radius: 4px; min-width: 30px; }"
        "QScrollBar::handle:horizontal:hover { background: #7f8c8d; }"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }"
    );
    tableLayout->addWidget(topScrollBar);

    buildingTypesTable = new QTableWidget();
    buildingTypesTable->setAlternatingRowColors(true);
    buildingTypesTable->setSelectionBehavior(QAbstractItemView::SelectItems);
    buildingTypesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    buildingTypesTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    buildingTypesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    buildingTypesTable->horizontalHeader()->setStretchLastSection(false);
    buildingTypesTable->horizontalHeader()->setMinimumSectionSize(80);
    buildingTypesTable->setStyleSheet(
        "QTableWidget {"
        "    background-color: white;"
        "    gridline-color: #bdc3c7;"
        "    border: 1px solid #bdc3c7;"
        "    alternate-background-color: #ecf0f1;"
        "}"
        "QTableWidget::item {"
        "    padding: 5px;"
        "}"
        "QTableWidget::item:selected {"
        "    background-color: #3498db;"
        "    color: white;"
        "}"
        "QTableWidget::item:hover {"
        "    background-color: #d6eaf8;"
        "}"
        "QHeaderView::section {"
        "    background-color: #34495e;"
        "    color: white;"
        "    padding: 8px;"
        "    border: 1px solid #2c3e50;"
        "    font-weight: bold;"
        "    font-size: 11px;"
        "}"
    );
    
    // Abilita ordinamento cliccando sulle intestazioni
    buildingTypesTable->setSortingEnabled(false);
    buildingTypesTable->setMinimumHeight(400);
    buildingTypesTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // Sincronizza la scrollbar in cima con quella interna della tabella
    connect(buildingTypesTable->horizontalScrollBar(), &QScrollBar::rangeChanged,
            this, [this](int min, int max) {
                topScrollBar->setRange(min, max);
                topScrollBar->setPageStep(buildingTypesTable->horizontalScrollBar()->pageStep());
                topScrollBar->setSingleStep(buildingTypesTable->horizontalScrollBar()->singleStep());
            });
    connect(buildingTypesTable->horizontalScrollBar(), &QScrollBar::valueChanged,
            topScrollBar, &QScrollBar::setValue);
    connect(topScrollBar, &QScrollBar::valueChanged,
            buildingTypesTable->horizontalScrollBar(), &QScrollBar::setValue);
    
    // Connetti selezione per abilitare pulsanti elimina
    connect(buildingTypesTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        bool hasSelection = buildingTypesTable->selectedItems().count() > 0;
        deleteRowBtn->setEnabled(hasSelection);
        deleteColumnBtn->setEnabled(hasSelection);
    });
    
    tableLayout->addWidget(buildingTypesTable);
    
    // Info label
    QLabel *infoLabel = new QLabel(
        "<i style='color: #7f8c8d; font-size: 11px;'>"
        "💡 Doppio click su una cella per modificarla. Le modifiche sono immediate. "
        "Ricorda di salvare il file quando hai finito!"
        "</i>"
    );
    infoLabel->setWordWrap(true);
    tableLayout->addWidget(infoLabel);
    
    mainLayout->addWidget(tableBox);
    
    contentArea->addWidget(buildingTypesPage);
}

void MainWindow::createBuildingsEditorPage() {
    QWidget *buildingsPage = new QWidget();
    buildingsPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *mainLayout = new QVBoxLayout(buildingsPage);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(15);
    
    // 1. TITOLO
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>🏢 Visualizzazione Edifici</h1>");
    mainLayout->addWidget(titleLabel);
    
    // 2. DESCRIZIONE
    QLabel *descLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "Carica un file JSON degli edifici per visualizzare e modificare SITE:LOCATION, BuildingSurface e Zone..."
        "</p>"
    );
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);
    
    // 3. BARRA SUPERIORE - CARICA E CONTA EDIFICI
    QHBoxLayout *topLayout = new QHBoxLayout();
    
    QPushButton *loadButton = new QPushButton("📁 Carica File Edifici");
    loadButton->setFixedHeight(40);
    loadButton->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 150px;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }"
    );

    saveBuildingsBtn = new QPushButton("💾 Salva Edifici");
    saveBuildingsBtn->setFixedHeight(40);
    saveBuildingsBtn->setEnabled(false);
    saveBuildingsBtn->setMinimumWidth(150);
    saveBuildingsBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "}"
        "QPushButton:hover { background-color: #229954; }"
        "QPushButton:pressed { background-color: #1e8449; }"
        "QPushButton:disabled { background-color: #95a5a6; }"
    );

    buildingsCountLabel = new QLabel("📊 Numero edifici: 0");
    buildingsCountLabel->setStyleSheet("font-size: 13px; color: #2c3e50; font-weight: bold;");

    QPushButton *geoJsonConvertBtn = new QPushButton("🔄 GeoJSON+CSV → JSON");
    geoJsonConvertBtn->setFixedHeight(40);
    geoJsonConvertBtn->setMinimumWidth(170);
    geoJsonConvertBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e67e22; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px;"
        "}"
        "QPushButton:hover { background-color: #ca6f1e; }"
        "QPushButton:pressed { background-color: #a04000; }"
    );

    topLayout->addWidget(loadButton);
    topLayout->addWidget(saveBuildingsBtn);
    topLayout->addWidget(geoJsonConvertBtn);
    topLayout->addWidget(buildingsCountLabel);
    topLayout->addStretch();
    
    connect(loadButton, &QPushButton::clicked, this, &MainWindow::onLoadBuildingsFile);
    connect(geoJsonConvertBtn, &QPushButton::clicked, this, &MainWindow::onConvertGeoJSON);
    
    mainLayout->addLayout(topLayout);
    
    // 4. SELEZIONE EDIFICIO
    QGroupBox *selectionBox = new QGroupBox("🔍 Seleziona Edificio");
    selectionBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #3498db;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *selectionLayout = new QVBoxLayout(selectionBox);
    
    buildingsComboBox = new QComboBox();
    buildingsComboBox->setEnabled(false);
    buildingsComboBox->setMinimumHeight(35);
    buildingsComboBox->setStyleSheet(
        "QComboBox {"
        "    background-color: white;"
        "    border: 1px solid #bdc3c7;"
        "    padding: 5px;"
        "    border-radius: 3px;"
        "    font-size: 12px;"
        "}"
    );
    
    selectionLayout->addWidget(buildingsComboBox);
    mainLayout->addWidget(selectionBox);
    
    connect(buildingsComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onBuildingSelectionChanged);
    
    // 5. SITE:LOCATION INFO
    QGroupBox *locationBox = new QGroupBox("📍 Informazioni Ubicazione (SITE:LOCATION)");
    locationBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #2ecc71;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *locationLayout = new QVBoxLayout(locationBox);
    
    siteLocationText = new QTextEdit();
    siteLocationText->setReadOnly(true);
    siteLocationText->setMaximumHeight(150);
    siteLocationText->setStyleSheet(
        "QTextEdit {"
        "    background-color: #ecf0f1;"
        "    border: 1px solid #bdc3c7;"
        "    border-radius: 3px;"
        "    padding: 5px;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 11px;"
        "}"
    );
    siteLocationText->setPlainText("In attesa di caricamento edificio...");
    locationLayout->addWidget(siteLocationText);
    mainLayout->addWidget(locationBox);
    
    // 6. TABELLA BUILDING SURFACES
    QGroupBox *surfacesBox = new QGroupBox("🔶 BuildingSurface - Superfici");
    surfacesBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #e74c3c;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *surfacesLayout = new QVBoxLayout(surfacesBox);
    
    QHBoxLayout *surfaceButtonsLayout = new QHBoxLayout();
    addSurfaceBtn = new QPushButton("➕ Aggiungi Superficie");
    addSurfaceBtn->setFixedHeight(35);
    addSurfaceBtn->setEnabled(false);
    addSurfaceBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #e74c3c;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 15px;"
        "    border-radius: 4px;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #c0392b; }"
        "QPushButton:disabled { background-color: #95a5a6; }"
    );
    surfaceButtonsLayout->addWidget(addSurfaceBtn);
    surfaceButtonsLayout->addStretch();
    surfacesLayout->addLayout(surfaceButtonsLayout);
    
    buildingSurfacesTable = new QTableWidget();
    buildingSurfacesTable->setAlternatingRowColors(true);
    buildingSurfacesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    buildingSurfacesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    buildingSurfacesTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    buildingSurfacesTable->horizontalHeader()->setStretchLastSection(true);
    buildingSurfacesTable->setMinimumHeight(200);
    buildingSurfacesTable->setMaximumHeight(250);
    buildingSurfacesTable->setStyleSheet(
        "QTableWidget {"
        "    background-color: white;"
        "    gridline-color: #bdc3c7;"
        "    alternate-background-color: #ecf0f1;"
        "}"
        "QHeaderView::section {"
        "    background-color: #34495e;"
        "    color: white;"
        "    padding: 5px;"
        "    font-weight: bold;"
        "}"
    );
    surfacesLayout->addWidget(buildingSurfacesTable);
    mainLayout->addWidget(surfacesBox);
    
    // 7. TABELLA ZONES
    QGroupBox *zonesBox = new QGroupBox("🔷 Zone");
    zonesBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #9b59b6;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *zonesLayout = new QVBoxLayout(zonesBox);
    
    QHBoxLayout *zoneButtonsLayout = new QHBoxLayout();
    addZoneBtn = new QPushButton("➕ Aggiungi Zona");
    addZoneBtn->setFixedHeight(35);
    addZoneBtn->setEnabled(false);
    addZoneBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #9b59b6;"
        "    color: white;"
        "    border: none;"
        "    padding: 8px 15px;"
        "    border-radius: 4px;"
        "    font-size: 11px;"
        "}"
        "QPushButton:hover { background-color: #8e44ad; }"
        "QPushButton:disabled { background-color: #95a5a6; }"
    );
    zoneButtonsLayout->addWidget(addZoneBtn);
    zoneButtonsLayout->addStretch();
    zonesLayout->addLayout(zoneButtonsLayout);
    
    zonesTable = new QTableWidget();
    zonesTable->setAlternatingRowColors(true);
    zonesTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    zonesTable->setSelectionMode(QAbstractItemView::SingleSelection);
    zonesTable->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    zonesTable->horizontalHeader()->setStretchLastSection(true);
    zonesTable->setMinimumHeight(200);
    zonesTable->setMaximumHeight(250);
    zonesTable->setStyleSheet(
        "QTableWidget {"
        "    background-color: white;"
        "    gridline-color: #bdc3c7;"
        "    alternate-background-color: #ecf0f1;"
        "}"
        "QHeaderView::section {"
        "    background-color: #34495e;"
        "    color: white;"
        "    padding: 5px;"
        "    font-weight: bold;"
        "}"
    );
    zonesLayout->addWidget(zonesTable);
    mainLayout->addWidget(zonesBox);

    connect(addSurfaceBtn, &QPushButton::clicked, this, &MainWindow::onAddBuildingSurface);
    connect(addZoneBtn, &QPushButton::clicked, this, &MainWindow::onAddZone);
    connect(saveBuildingsBtn, &QPushButton::clicked, this, &MainWindow::onSaveBuildingsFile);

    contentArea->addWidget(buildingsPage);
}

void MainWindow::createSimulationPage() {
    QWidget *simulationPage = new QWidget();
    simulationPage->setStyleSheet("background-color: #ffffff;");

    QHBoxLayout *outerLayout = new QHBoxLayout(simulationPage);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QWidget *innerWidget = new QWidget();
    innerWidget->setMaximumWidth(1200);
    QVBoxLayout *simulationLayout = new QVBoxLayout(innerWidget);
    simulationLayout->setContentsMargins(40, 40, 40, 40);
    simulationLayout->setSpacing(18);

    outerLayout->addWidget(innerWidget);

    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>▶️ Avvio Simulazione</h1>");
    QLabel *descriptionLabel = new QLabel(
        "Seleziona i file di input richiesti da BldSimu anche se si trovano in cartelle differenti, "
        "scegli la modalita' finale e avvia la simulazione dal binario presente in `bin`."
    );
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet("color: #7f8c8d; font-size: 13px;");

    QGroupBox *inputsGroup = new QGroupBox("Input simulazione");
    inputsGroup->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #2c3e50; "
        "border: 1px solid #bdc3c7; border-radius: 6px; margin-top: 10px; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    QVBoxLayout *inputsLayout = new QVBoxLayout(inputsGroup);
    inputsLayout->setSpacing(12);

    auto createSelectorRow = [this, inputsLayout](const QString &labelText,
                                                  QLabel *&pathLabel,
                                                  const QString &buttonText,
                                                  auto slot) {
        QHBoxLayout *rowLayout = new QHBoxLayout();
        rowLayout->setSpacing(12);

        QLabel *rowLabel = new QLabel(labelText);
        rowLabel->setMinimumWidth(240);
        rowLabel->setStyleSheet("color: #2c3e50; font-size: 13px; font-weight: 600;");

        pathLabel = new QLabel("Nessun file selezionato");
        pathLabel->setMinimumWidth(340);
        pathLabel->setWordWrap(true);
        pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pathLabel->setStyleSheet(
            "background-color: #f8f9fa; border: 1px solid #dcdde1; border-radius: 4px; "
            "padding: 8px; color: #7f8c8d; font-size: 12px; font-style: italic;");

        QPushButton *browseButton = new QPushButton(buttonText);
        browseButton->setFixedHeight(38);
        browseButton->setMinimumWidth(180);
        browseButton->setStyleSheet(
            "QPushButton { background-color: #3498db; color: white; border: none; "
            "border-radius: 4px; padding: 0 16px; font-size: 13px; }"
            "QPushButton:hover { background-color: #2980b9; }"
            "QPushButton:pressed { background-color: #21618c; }");
        connect(browseButton, &QPushButton::clicked, this, slot);

        rowLayout->addWidget(rowLabel);
        rowLayout->addWidget(pathLabel, 1);
        rowLayout->addWidget(browseButton);
        inputsLayout->addLayout(rowLayout);
    };

    createSelectorRow("Selezione file wtst climatico",
                      simulationClimateFilePathLabel,
                      "📂 Seleziona file",
                      &MainWindow::onSelectSimulationClimateFile);
    createSelectorRow("Seleziona file json componenti",
                      simulationComponentsFilePathLabel,
                      "📂 Seleziona file",
                      &MainWindow::onSelectSimulationComponentsFile);
    createSelectorRow("Seleziona file csv schedule",
                      simulationSchedulesFilePathLabel,
                      "📂 Seleziona file",
                      &MainWindow::onSelectSimulationSchedulesFile);
    createSelectorRow("Seleziona file json edifici",
                      simulationBuildingsFilePathLabel,
                      "📂 Seleziona file",
                      &MainWindow::onSelectSimulationBuildingsFile);

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(12);

    QLabel *modeLabel = new QLabel("Modalita' simulazione");
    modeLabel->setMinimumWidth(240);
    modeLabel->setStyleSheet("color: #2c3e50; font-size: 13px; font-weight: 600;");

    simulationModeCombo = new QComboBox();
    simulationModeCombo->addItems({"iso", "noise"});
    simulationModeCombo->setFixedHeight(36);
    simulationModeCombo->setMinimumWidth(160);
    simulationModeCombo->setStyleSheet(
        "QComboBox { font-size: 13px; color: #2c3e50; border: 1px solid #bdc3c7; "
        "border-radius: 4px; padding: 0 10px; background: #ffffff; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { color: #2c3e50; background: #ffffff; "
        "selection-background-color: #2980b9; selection-color: #ffffff; }");

    QLabel *modeHintLabel = new QLabel("Seleziona il valore finale da passare a BldSimu.");
    modeHintLabel->setWordWrap(true);
    modeHintLabel->setStyleSheet("color: #7f8c8d; font-size: 12px;");

    QVBoxLayout *modeDetailsLayout = new QVBoxLayout();
    modeDetailsLayout->setSpacing(4);
    modeDetailsLayout->addWidget(simulationModeCombo, 0, Qt::AlignLeft);
    modeDetailsLayout->addWidget(modeHintLabel);

    modeLayout->addWidget(modeLabel);
    modeLayout->addLayout(modeDetailsLayout, 1);
    modeLayout->addStretch();
    inputsLayout->addLayout(modeLayout);

    launchSimulationBtn = new QPushButton("▶️ Avvia simulazione");
    launchSimulationBtn->setFixedHeight(42);
    launchSimulationBtn->setMinimumWidth(220);
    launchSimulationBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: 5px; padding: 0 20px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover { background-color: #219150; }"
        "QPushButton:pressed { background-color: #1e8449; }"
        "QPushButton:disabled { background-color: #95a5a6; color: #ecf0f1; }");
    connect(launchSimulationBtn, &QPushButton::clicked, this, &MainWindow::onLaunchSimulation);

    QLabel *commandHintLabel = new QLabel(
        "Comando eseguito: BldSimu componenti.json edifici.json climatico.wtst schedule.csv iso|noise"
    );
    commandHintLabel->setWordWrap(true);
    commandHintLabel->setStyleSheet("color: #7f8c8d; font-size: 12px;");

    QLabel *simulationOutputLabel = new QLabel("<b style='color: #2c3e50;'>Output simulazione:</b>");
    simulationOutputConsole = new QTextEdit();
    simulationOutputConsole->setReadOnly(true);
    simulationOutputConsole->setMinimumHeight(220);
    simulationOutputConsole->setStyleSheet(
        "QTextEdit {"
        "    background-color: #2c3e50;"
        "    color: #ecf0f1;"
        "    border: 1px solid #34495e;"
        "    border-radius: 5px;"
        "    padding: 10px;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 11px;"
        "}"
    );
    simulationOutputConsole->setPlaceholderText("L'output di BldSimu comparira' qui...");

    simulationLayout->addWidget(titleLabel);
    simulationLayout->addWidget(descriptionLabel);
    simulationLayout->addWidget(inputsGroup);
    simulationLayout->addWidget(commandHintLabel);
    simulationLayout->addWidget(launchSimulationBtn, 0, Qt::AlignLeft);
    simulationLayout->addWidget(simulationOutputLabel);
    simulationLayout->addWidget(simulationOutputConsole);
    simulationLayout->addStretch();

    contentArea->addWidget(simulationPage);
}

void MainWindow::createSettingsPage() {
    QWidget *settingsPage = new QWidget();
    settingsPage->setStyleSheet("background-color: #ffffff;");

    // Layout allineato a sinistra con larghezza massima
    QHBoxLayout *outerLayout = new QHBoxLayout(settingsPage);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QWidget *innerWidget = new QWidget();
    innerWidget->setMaximumWidth(700);
    QVBoxLayout *settingsLayout = new QVBoxLayout(innerWidget);
    settingsLayout->setContentsMargins(40, 40, 40, 40);
    settingsLayout->setSpacing(15);

    outerLayout->addWidget(innerWidget);

    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>Impostazioni</h1>");
    settingsLayout->addWidget(titleLabel);

    // --- Sezione EnergyPlus ---
    QGroupBox *epGroupBox = new QGroupBox("EnergyPlus");
    epGroupBox->setStyleSheet(
        "QGroupBox { font-size: 14px; font-weight: bold; color: #2c3e50; "
        "border: 1px solid #bdc3c7; border-radius: 6px; margin-top: 10px; padding: 10px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }");
    QVBoxLayout *epLayout = new QVBoxLayout(epGroupBox);
    epLayout->setSpacing(12);

    // Riga con pulsante sfoglia + toggle
    QHBoxLayout *epTopRow = new QHBoxLayout();

    QPushButton *browseEpBtn = new QPushButton("📂  Seleziona directory EnergyPlus");
    browseEpBtn->setFixedHeight(36);
    browseEpBtn->setStyleSheet(
        "QPushButton { background-color: #2980b9; color: white; border: none; "
        "border-radius: 4px; padding: 0 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: #3498db; }"
        "QPushButton:pressed { background-color: #1f618d; }");
    connect(browseEpBtn, &QPushButton::clicked, this, &MainWindow::onBrowseEnergyPlusDir);

    // Toggle switch animato
    energyPlusToggle = new ToggleSwitch("Abilita EnergyPlus");
    energyPlusToggle->setEnabled(false);
    energyPlusToggle->setFont(QFont(QString(), 13));
    connect(energyPlusToggle, &ToggleSwitch::toggled, this,
            [this](bool checked) {
                onEnergyPlusToggleChanged(checked ? Qt::Checked : Qt::Unchecked);
            });

    epTopRow->addWidget(browseEpBtn);
    epTopRow->addStretch();
    epTopRow->addWidget(energyPlusToggle);

    // Label percorso
    energyPlusDirLabel = new QLabel("Nessuna directory selezionata");
    energyPlusDirLabel->setStyleSheet(
        "color: #7f8c8d; font-size: 12px; font-style: italic;");
    energyPlusDirLabel->setWordWrap(true);

    epLayout->addLayout(epTopRow);
    epLayout->addWidget(energyPlusDirLabel);

    settingsLayout->addWidget(epGroupBox);
    settingsLayout->addStretch();
    contentArea->addWidget(settingsPage);
}

void MainWindow::onBrowseEnergyPlusDir() {
    QString dir = QFileDialog::getOpenFileName(
        this,
        "Seleziona l'eseguibile energyplus",
        energyPlusDirPath.isEmpty() ? QDir::homePath() : energyPlusDirPath,
        "EnergyPlus (energyplus energyplus.exe);;All Files (*)");

    if (dir.isEmpty())
        return;

    energyPlusDirPath = dir;
    energyPlusDirLabel->setText(dir);
    energyPlusDirLabel->setStyleSheet("color: #2c3e50; font-size: 12px; font-style: normal;");

    // Abilita il toggle ora che abbiamo un percorso valido
    energyPlusToggle->setEnabled(true);
}

void MainWindow::onEnergyPlusToggleChanged(Qt::CheckState state) {
    is_convepjson = (state == Qt::Checked);
}

void MainWindow::createSchedulesEditorPage() {
    QWidget *schedulesPage = new QWidget();
    schedulesPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *mainLayout = new QVBoxLayout(schedulesPage);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(15);
    
    // 1. TITOLO
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>📅 Gestione Schedules Orari (8760h)</h1>");
    mainLayout->addWidget(titleLabel);
    
    // 2. DESCRIZIONE
    QLabel *descriptionLabel = new QLabel(
        "Carica gli schedule da un file IDF (Schedule:Compact) o da un file CSV esistente. "
        "Puoi modificare i valori direttamente nella tabella e salvare il risultato in formato CSV."
    );
    descriptionLabel->setWordWrap(true);
    descriptionLabel->setStyleSheet("color: #7f8c8d; font-size: 13px; margin-bottom: 10px;");
    mainLayout->addWidget(descriptionLabel);
    
    // 3. BARRA PULSANTI
    QHBoxLayout *buttonsLayout = new QHBoxLayout();
    
    loadSchedulesBtn = new QPushButton("📂 Carica Schedules (IDF/CSV)");
    loadSchedulesBtn->setFixedHeight(40);
    loadSchedulesBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #3498db;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 180px;"
        "}"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }"
    );
    
    saveSchedulesBtn = new QPushButton("💾 Salva CSV");
    saveSchedulesBtn->setFixedHeight(40);
    saveSchedulesBtn->setEnabled(false);
    saveSchedulesBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #27ae60;"
        "    color: white;"
        "    border: none;"
        "    padding: 10px 20px;"
        "    font-size: 13px;"
        "    border-radius: 5px;"
        "    min-width: 150px;"
        "}"
        "QPushButton:hover { background-color: #219150; }"
        "QPushButton:pressed { background-color: #1e8449; }"
        "QPushButton:disabled { background-color: #95a5a6; color: #ecf0f1; }"
    );
    
    saveSchedulesIdfBtn = new QPushButton("📄 Salva IDF");
    saveSchedulesIdfBtn->setFixedHeight(40);
    saveSchedulesIdfBtn->setEnabled(false);
    saveSchedulesIdfBtn->setStyleSheet(
        "QPushButton {"
        "    background-color: #8e44ad; color: white; border: none;"
        "    padding: 10px 20px; font-size: 13px; border-radius: 5px; min-width: 150px;"
        "}"
        "QPushButton:hover { background-color: #7d3c98; }"
        "QPushButton:pressed { background-color: #6c3483; }"
        "QPushButton:disabled { background-color: #95a5a6; color: #ecf0f1; }"
    );

    buttonsLayout->addWidget(loadSchedulesBtn);
    buttonsLayout->addWidget(saveSchedulesBtn);
    buttonsLayout->addWidget(saveSchedulesIdfBtn);
    buttonsLayout->addStretch();
    
    mainLayout->addLayout(buttonsLayout);

    // Selezione giorno della settimana del 1° Gennaio
    QHBoxLayout *dayRow = new QHBoxLayout();
    QLabel *dayLabel = new QLabel("Giorno della settimana del 1° Gennaio:");
    dayLabel->setStyleSheet("color: #2c3e50; font-size: 13px;");
    firstDayCombo = new QComboBox();
    firstDayCombo->addItems({"lunedì", "martedì", "mercoledì", "giovedì",
                             "venerdì", "sabato", "domenica"});
    firstDayCombo->setFixedHeight(32);
    firstDayCombo->setMinimumWidth(160);
    firstDayCombo->setStyleSheet(
        "QComboBox { font-size: 13px; color: #2c3e50; border: 1px solid #bdc3c7; "
        "border-radius: 4px; padding: 0 10px; background: #ffffff; }"
        "QComboBox::drop-down { border: none; width: 24px; }"
        "QComboBox QAbstractItemView { color: #2c3e50; background: #ffffff; "
        "selection-background-color: #2980b9; selection-color: #ffffff; }");
    dayRow->addWidget(dayLabel);
    dayRow->addWidget(firstDayCombo);
    dayRow->addStretch();
    mainLayout->addLayout(dayRow);
    
    // 4. CASELLA DI TESTO (QTextEdit)
    QGroupBox *textBox = new QGroupBox("📝 Comandi Schedule:Compact (IDF)");
    textBox->setStyleSheet(
        "QGroupBox {"
        "    border: 2px solid #3498db;"
        "    border-radius: 5px;"
        "    margin-top: 10px;"
        "    font-weight: bold;"
        "    color: #2c3e50;"
        "}"
        "QGroupBox::title {"
        "    subcontrol-origin: margin;"
        "    left: 10px;"
        "    padding: 0 5px 0 5px;"
        "}"
    );
    QVBoxLayout *textLayout = new QVBoxLayout(textBox);
    
    schedulesTextEdit = new QTextEdit();
    schedulesTextEdit->setAcceptRichText(false);
    schedulesTextEdit->setPlaceholderText("Incolla qui i comandi Schedule:Compact del file IDF o carica un file...");
    schedulesTextEdit->setStyleSheet(
        "QTextEdit {"
        "    background-color: white;"
        "    border: 1px solid #bdc3c7;"
        "    font-family: 'Courier New', monospace;"
        "    font-size: 13px;"
        "}"
    );
    
    textLayout->addWidget(schedulesTextEdit);
    mainLayout->addWidget(textBox);
    
    // Connessioni
    connect(loadSchedulesBtn,    &QPushButton::clicked, this, &MainWindow::onLoadSchedulesFile);
    connect(saveSchedulesBtn,    &QPushButton::clicked, this, &MainWindow::onSaveSchedulesFile);
    connect(saveSchedulesIdfBtn, &QPushButton::clicked, this, &MainWindow::onSaveSchedulesIdf);
    connect(schedulesTextEdit, &QTextEdit::textChanged, [this]() {
        bool hasText = !schedulesTextEdit->toPlainText().trimmed().isEmpty();
        saveSchedulesBtn->setEnabled(hasText);
        saveSchedulesIdfBtn->setEnabled(hasText);
    });
    
    contentArea->addWidget(schedulesPage);
}

void MainWindow::onLoadSchedulesFile() {
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file IDF",
        QDir::homePath(),
        "File IDF (*.idf);;Tutti i file (*)"
    );
    
    if (filePath.isEmpty()) return;
    
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file.");
        return;
    }
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // Estrazione blocchi Schedule:Compact
    QString extractedSchedules;
    QRegularExpression scheduleRegex("Schedule:Compact\\s*,[^;]*;", QRegularExpression::CaseInsensitiveOption);
    auto it = scheduleRegex.globalMatch(content);
    while (it.hasNext()) {
        extractedSchedules += it.next().captured(0) + "\n\n";
    }

    if (extractedSchedules.isEmpty()) {
        QMessageBox::warning(this, "Attenzione", "Nessun Schedule:Compact trovato nel file.");
        return;
    }

    schedulesTextEdit->setPlainText(extractedSchedules);
    currentSchedulesFilePath = filePath;
    saveSchedulesBtn->setEnabled(true);
    outputConsole->append("✓ Estratti schedules da: " + filePath);
}

void MainWindow::onSaveSchedulesFile() {
    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Salva Schedules CSV",
        QDir::homePath() + "/schedules.csv",
        "File CSV (*.csv)"
    );
    if (savePath.isEmpty()) return;

    QString rawContent = schedulesTextEdit->toPlainText();

    // 1. Rimuovi commenti (da '!' a fine riga) e converti tutto in minuscolo
    QString content;
    for (const QString &line : rawContent.split('\n')) {
        int bangPos = line.indexOf('!');
        content += (bangPos >= 0 ? line.left(bangPos) : line) + '\n';
    }
    content = content.toLower();

    int daysInMonth[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

    // Giorno della settimana del 1 gennaio: 0=Lun, ..., 6=Dom (dalla impostazione)
    int jan1Dow = (firstDayCombo ? firstDayCombo->currentIndex() : 0);

    // Restituisce il giorno della settimana (0=Lun,...,6=Dom) per il giorno dell'anno
    auto dayOfWeek = [jan1Dow](int dayOfYear) -> int {
        return (dayOfYear - 1 + jan1Dow) % 7;
    };

    QMap<QString, QVector<double>> allSchedules;

    // 2. Trova i blocchi Schedule:Compact (da "schedule:compact" al ";" di chiusura)
    int searchPos = 0;
    while (true) {
        int scPos = content.indexOf("schedule:compact", searchPos);
        if (scPos == -1) break;
        int endPos = content.indexOf(';', scPos);
        if (endPos == -1) break;
        searchPos = endPos + 1;

        QString block = content.mid(scPos, endPos - scPos + 1);

        // 3. Dividi per ',' e ';' per ottenere i singoli campi IDF
        QStringList tokens;
        for (const QString &t : block.split(QRegularExpression("[,;]"), Qt::SkipEmptyParts)) {
            QString tok = t.trimmed();
            if (!tok.isEmpty()) tokens << tok;
        }
        // tokens[0] = "schedule:compact", tokens[1] = nome, tokens[2] = tipo (skip)
        if (tokens.size() < 3) continue;
        QString scheduleName = tokens[1].trimmed();

        QVector<double> hourlyValues(8760, 0.0);

        struct Period {
            int endDay = 0;
            // chiave: tipo giorno (es. "weekdays", "weekends", "alldays", ...)
            // valore: lista di {ora_finale (1-24), valore}
            QMap<QString, QList<QPair<int, double>>> dayProfiles;
        };
        QList<Period> periods;
        Period *currentPeriod = nullptr;
        QStringList currentDayTypes;

        QRegularExpression throughRx(R"(through:\s*(\d+)/(\d+))");
        QRegularExpression forRx(R"(for:\s*(.+))");
        QRegularExpression untilRx(R"(until:\s*(\d+):(\d+))");

        // Partiamo da tokens[3] (saltiamo schedule:compact, nome, tipo)
        for (int i = 3; i < tokens.size(); ++i) {
            const QString &tok = tokens[i];

            // Through: MM/DD
            auto tM = throughRx.match(tok);
            if (tM.hasMatch()) {
                int month = tM.captured(1).toInt();
                int day   = tM.captured(2).toInt();
                int endDay = 0;
                for (int m = 1; m < month; ++m) endDay += daysInMonth[m];
                endDay += day;
                Period p; p.endDay = endDay;
                periods.append(p);
                currentPeriod = &periods.last();
                currentDayTypes.clear();
                continue;
            }

            // For: <tipi giorno separati da spazio>
            auto fM = forRx.match(tok);
            if (fM.hasMatch() && currentPeriod) {
                currentDayTypes.clear();
                for (const QString &dt : fM.captured(1).split(QRegularExpression("\\s+"), Qt::SkipEmptyParts)) {
                    // Normalizza i keyword EnergyPlus al calendario 8760h
                    if (dt == "alldays" || dt == "weekdays" || dt == "weekends" ||
                        dt == "allotherdays" ||
                        dt == "monday" || dt == "tuesday" || dt == "wednesday" ||
                        dt == "thursday" || dt == "friday" || dt == "saturday" || dt == "sunday") {
                        currentDayTypes << dt;
                    }
                    // summerdesignday, winterdesignday, holiday, customday* → non sono
                    // giorni del calendario 8760h, vengono ignorati
                }
                if (currentDayTypes.isEmpty()) currentDayTypes << "alldays";
                continue;
            }

            // Until: HH:MM  → il valore è nel token successivo
            auto uM = untilRx.match(tok);
            if (uM.hasMatch() && currentPeriod && !currentDayTypes.isEmpty()) {
                int h = uM.captured(1).toInt(); // ora finale (1-24)
                if (i + 1 < tokens.size()) {
                    double v = tokens[++i].trimmed().toDouble();
                    for (const QString &dt : currentDayTypes) {
                        currentPeriod->dayProfiles[dt].append({h, v});
                    }
                }
                continue;
            }
        }

        // 4. Espandi i periodi in 8760 valori orari
        int currentDay = 1;
        for (const auto &period : periods) {
            while (currentDay <= period.endDay && currentDay <= 365) {
                int dow = dayOfWeek(currentDay);
                const QString dayNames[] = {"monday","tuesday","wednesday","thursday",
                                            "friday","saturday","sunday"};

                // Ordine di precedenza: nome specifico > weekdays/weekends > allotherdays > alldays
                QStringList searchOrder;
                searchOrder << dayNames[dow];
                searchOrder << (dow < 5 ? "weekdays" : "weekends");
                searchOrder << "allotherdays" << "alldays";

                QList<QPair<int, double>> profile;
                for (const QString &st : searchOrder) {
                    if (period.dayProfiles.contains(st)) {
                        profile = period.dayProfiles[st];
                        break;
                    }
                }

                if (!profile.isEmpty()) {
                    // Ogni voce {untilHour, value} copre le ore da quella precedente fino a untilHour
                    int prevH = 0;
                    for (const auto &uv : profile) {
                        for (int h = prevH; h < uv.first && h < 24; ++h) {
                            int globalHour = (currentDay - 1) * 24 + h;
                            if (globalHour < 8760)
                                hourlyValues[globalHour] = uv.second;
                        }
                        prevH = uv.first;
                    }
                }
                currentDay++;
            }
        }
        allSchedules.insert(scheduleName, hourlyValues);
    }

    if (allSchedules.isEmpty()) {
        QMessageBox::warning(this, "Errore", "Impossibile elaborare gli schedule. Verifica il formato.");
        return;
    }

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile salvare il file.");
        return;
    }

    QTextStream out(&file);
    QStringList names = allSchedules.keys();
    out << "Ora," << names.join(",") << "\n";
    for (int h = 0; h < 8760; ++h) {
        out << (h + 1);
        for (const QString &name : names)
            out << "," << QString::number(allSchedules[name][h], 'f', 4);
        out << "\n";
    }
    file.close();

    // Salva i nomi degli schedule per uso nella tabella tipologie
    savedScheduleNames = names;
    savedScheduleNames.sort();
    QMessageBox::information(this, "Successo",
        QString("File CSV generato correttamente (%1 schedule, 8760 ore).").arg(allSchedules.size()));
    outputConsole->append("✓ CSV generato in: " + savePath);

    // Salva anche il file .idf con il testo grezzo dello schedulesTextEdit
    QString idfPath = QFileInfo(savePath).absolutePath() + "/" + QFileInfo(savePath).completeBaseName() + ".idf";
    QFile idfFile(idfPath);
    if (idfFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream idfOut(&idfFile);
        idfOut << schedulesTextEdit->toPlainText();
        idfFile.close();
        outputConsole->append("✓ IDF generato in: " + idfPath);
    } else {
        QMessageBox::warning(this, "Attenzione", "Impossibile salvare il file IDF: " + idfPath);
    }

    schedulesFileSaved = true;
    updateCreateButton();
}

void MainWindow::onSaveSchedulesIdf() {
    if (schedulesTextEdit->toPlainText().trimmed().isEmpty()) return;

    QString savePath = QFileDialog::getSaveFileName(
        this,
        "Salva file IDF",
        QDir::homePath() + "/schedules.idf",
        "File IDF (*.idf);;Tutti i file (*)"
    );
    if (savePath.isEmpty()) return;

    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile salvare il file: " + savePath);
        return;
    }
    QTextStream out(&file);
    out << schedulesTextEdit->toPlainText();
    file.close();

    // Estrai i nomi degli schedule dal testo grezzo
    QString content = schedulesTextEdit->toPlainText().toLower();
    QRegularExpression scRx("schedule:compact\\s*,\\s*([^,;]+)");
    QRegularExpressionMatchIterator it = scRx.globalMatch(content);
    QStringList names;
    while (it.hasNext()) {
        QString name = it.next().captured(1).trimmed();
        if (!name.isEmpty() && !names.contains(name))
            names.append(name);
    }
    names.sort();
    savedScheduleNames = names;

    schedulesFileSaved = true;
    updateCreateButton();

    QMessageBox::information(this, "Successo", "File IDF salvato in:\n" + savePath);
    outputConsole->append("✓ IDF salvato in: " + savePath);
}

void MainWindow::updateCreateButton() {
    if (!createBtn) return;
    bool geoLoaded        = !geoJsonDoc.isNull() && geoJsonDoc.isObject();
    bool componentsLoaded = !currentComponentsFilePath.isEmpty();
    bool schedulesSaved   = schedulesFileSaved;
    createBtn->setEnabled(geoLoaded && componentsLoaded && schedulesSaved);
}

void MainWindow::onCrea() {
    if (!geoJsonDoc.isObject() || !componentsJsonDoc.isObject()) return;

    // ── 1. Estrai bldtype unici dal GeoJSON ─────────────────────────────────
    QJsonArray features = geoJsonDoc.object().value("features").toArray();
    QStringList bldTypes;
    for (const QJsonValue &fv : features) {
        QString bt = fv.toObject().value("properties").toObject().value("bldtype").toString().trimmed();
        if (!bt.isEmpty() && !bldTypes.contains(bt))
            bldTypes.append(bt);
    }
    bldTypes.sort();
    if (bldTypes.isEmpty()) {
        QMessageBox::warning(this, "Attenzione", "Nessun campo 'bldtype' trovato nel GeoJSON.");
        return;
    }

    // ── 2. Recupera liste dai componenti ────────────────────────────────────
    QJsonObject rootComp = componentsJsonDoc.object();
    auto keysOf = [&](const QString &section) -> QStringList {
        QStringList list;
        if (rootComp.contains(section))
            list = rootComp.value(section).toObject().keys();
        list.sort();
        return list;
    };

    QStringList constructionNames   = keysOf("CONSTRUCTION");
    QStringList internalMassNames   = keysOf("INTERNALMASS");
    QStringList windowFrameNames    = keysOf("WINDOWFRAME");   // può essere vuoto
    QStringList hvacNames           = keysOf("HVAC");
    QStringList hvacManagerNames    = keysOf("HVAC_MANAGER");
    QStringList peopleNames         = keysOf("PEOPLE");
    QStringList lightsNames         = keysOf("LIGHTS");
    QStringList electricEquipNames  = keysOf("ELECTRICEQUIPMENT");
    QStringList latentEquipNames    = keysOf("LATENTEQUIPMENT");

    // ── 3. Definisci colonne ─────────────────────────────────────────────────
    struct ColDef {
        QString header;
        enum Type { Plain, Double, Combo } type;
        QStringList choices;  // per Combo
    };

    QStringList schedChoices = savedScheduleNames;

    QVector<ColDef> cols = {
        { "bldtype",                            ColDef::Plain,  {} },
        { "WWR (WinArea/WallArea)",             ColDef::Double, {} },
        { "h_interfloor(m)",                    ColDef::Double, {} },
        { "h_groundfloor(m)",                   ColDef::Double, {} },
        { "h_lastfloor(m)",                     ColDef::Double, {} },
        { "external wall construction name",    ColDef::Combo,  constructionNames },
        { "basement floor construction name",   ColDef::Combo,  constructionNames },
        { "roof construction name",             ColDef::Combo,  constructionNames },
        { "inter-storey construction name",     ColDef::Combo,  constructionNames },
        { "ceiling construction name",          ColDef::Combo,  constructionNames },
        { "window construction name",           ColDef::Combo,  constructionNames },
        { "window-frame name",                  ColDef::Combo,  windowFrameNames },
        { "internal heat capacity (J/m2K)",   ColDef::Double, {"10000"} },
        { "convective fraction internal heat gain",   ColDef::Double, {"0.40"} },
        { "convective fraction solar",   ColDef::Double, {"0.10"} },
        //{ "internal mass",                      ColDef::Combo,  internalMassNames },
        { "hvac",                               ColDef::Combo,  hvacNames },
        { "hvac manager",                       ColDef::Combo,  hvacManagerNames },
        { "hvac schedule",                      ColDef::Combo,  schedChoices },
        { "hvac heating schedule",              ColDef::Combo,  schedChoices },
        { "hvac cooling schedule",              ColDef::Combo,  schedChoices },
        { "zone infiltration (m3/s m2)",        ColDef::Double,  {} },
        { "zone infiltration schedule",         ColDef::Combo,  schedChoices },
        { "people (people/m2)",                 ColDef::Double, {} },
        { "people schedule",                    ColDef::Combo,  schedChoices },
        { "people activity schedule",           ColDef::Combo,  schedChoices },
        { "lights (W/m2)",                      ColDef::Double, {} },
        { "lights schedule",                    ColDef::Combo,  schedChoices },
        { "electric equipment (W/m2)",          ColDef::Double,  {} },
        { "electric equipment schedule",        ColDef::Combo,  schedChoices },
        { "latent equipment (kg/s)",            ColDef::Double,  {} },
        { "latent equipment schedule",          ColDef::Combo,  schedChoices },
    };

    // ── 4. Imposta la tabella ────────────────────────────────────────────────
    buildingTypesTable->clear();
    buildingTypesTable->setRowCount(bldTypes.size());
    buildingTypesTable->setColumnCount(cols.size());

    QStringList headers;
    for (const ColDef &c : cols) headers << c.header;
    buildingTypesTable->setHorizontalHeaderLabels(headers);

    for (int r = 0; r < bldTypes.size(); ++r) {
        for (int c = 0; c < cols.size(); ++c) {
            const ColDef &col = cols[c];
            if (col.type == ColDef::Plain) {
                // bldtype — read-only
                QTableWidgetItem *item = new QTableWidgetItem(bldTypes[r]);
                item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
                item->setBackground(QBrush(QColor("#dfe6e9")));
                buildingTypesTable->setItem(r, c, item);
            } else if (col.type == ColDef::Double) {
                const QString defaultValue = col.choices.isEmpty() ? QStringLiteral("0.0")
                                                                   : col.choices.first();
                QTableWidgetItem *item = new QTableWidgetItem(defaultValue);
                item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
                buildingTypesTable->setItem(r, c, item);
            } else {
                // Combo
                QComboBox *combo = new QComboBox();
                combo->addItem("");  // opzione vuota
                combo->addItems(col.choices);
                combo->setStyleSheet(
                    "QComboBox { font-size: 12px; color: #2c3e50; border: none; "
                    "background: transparent; }"
                    "QComboBox QAbstractItemView { color: #2c3e50; background: white; "
                    "selection-background-color: #3498db; selection-color: white; }");
                buildingTypesTable->setCellWidget(r, c, combo);
            }
        }
    }

    buildingTypesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    buildingTypesTable->horizontalHeader()->setStretchLastSection(false);
    buildingTypesTable->resizeColumnsToContents();

    // Abilita pulsanti di modifica tabella
    addRowBtn->setEnabled(true);
    addColumnBtn->setEnabled(true);
    saveBuildingTypesBtn->setEnabled(true);

    QMessageBox::information(this, "Tabella generata",
        QString("Tabella creata con %1 tipologie di edificio.").arg(bldTypes.size()));
}

void MainWindow::createInfoPage() {
    QWidget *infoPage = new QWidget();
    infoPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *infoLayout = new QVBoxLayout(infoPage);
    infoLayout->setContentsMargins(40, 40, 40, 40);
    infoLayout->setSpacing(15);
    
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>Informazioni</h1>");
    QLabel *infoLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "<b style='color: #2c3e50;'>Versione:</b> 1.0.0<br><br>"
        "<b style='color: #2c3e50;'>Piattaforma:</b> Qt6 Framework<br><br>"
        "<b style='color: #2c3e50;'>Linguaggio:</b> C++20<br><br>"
        "<b style='color: #2c3e50;'>Licenza:</b> MIT<br><br>"
        "<b style='color: #2c3e50;'>Sviluppatore:</b> Gianluca"
        "</p>"
    );
    infoLabel->setWordWrap(true);
    
    infoLayout->addWidget(titleLabel);
    infoLayout->addWidget(infoLabel);
    infoLayout->addStretch();
    contentArea->addWidget(infoPage);
}

void MainWindow::createContactPage() {
    QWidget *contactPage = new QWidget();
    contactPage->setStyleSheet("background-color: #ffffff;");
    QVBoxLayout *contactLayout = new QVBoxLayout(contactPage);
    contactLayout->setContentsMargins(40, 40, 40, 40);
    contactLayout->setSpacing(15);
    
    QLabel *titleLabel = new QLabel("<h1 style='color: #2c3e50;'>Contatti</h1>");
    QLabel *contactLabel = new QLabel(
        "<p style='color: #555; font-size: 14px;'>"
        "<b style='color: #2c3e50;'>Email:</b> gianluca.scaccianoce@unipa.it<br><br>"
        "<b style='color: #2c3e50;'>Telefono:</b> +39 23861934<br><br>"
        "<b style='color: #2c3e50;'>Sito web:</b> www.unipa.it<br><br>"
        "<b style='color: #2c3e50;'>Indirizzo:</b> Viale delle scienze ed.9, Palermo, Italia"
        "</p>"
    );
    contactLabel->setWordWrap(true);
    
    contactLayout->addWidget(titleLabel);
    contactLayout->addWidget(contactLabel);
    contactLayout->addStretch();
    contentArea->addWidget(contactPage);
}

void MainWindow::updateSelectedFileLabel(QLabel *label, const QString &filePath) {
    if (!label) {
        return;
    }

    if (filePath.isEmpty()) {
        label->setText("Nessun file selezionato");
        label->setStyleSheet(
            "background-color: #f8f9fa; border: 1px solid #dcdde1; border-radius: 4px; "
            "padding: 8px; color: #7f8c8d; font-size: 12px; font-style: italic;");
        return;
    }

    label->setText(filePath);
    label->setStyleSheet(
        "background-color: #f8f9fa; border: 1px solid #dcdde1; border-radius: 4px; "
        "padding: 8px; color: #2c3e50; font-size: 12px; font-style: normal;");
}

void MainWindow::setSimulationFileSelection(QString &targetPath,
                                            QLabel *targetLabel,
                                            const QString &dialogTitle,
                                            const QString &fileFilter) {
    QString startDirectory = targetPath.isEmpty()
                                 ? QDir::homePath()
                                 : QFileInfo(targetPath).absolutePath();

    QString filePath = QFileDialog::getOpenFileName(
        this,
        dialogTitle,
        startDirectory,
        fileFilter
    );

    if (filePath.isEmpty()) {
        return;
    }

    targetPath = filePath;
    updateSelectedFileLabel(targetLabel, targetPath);
}

void MainWindow::onSelectSimulationClimateFile() {
    setSimulationFileSelection(
        simulationClimateFilePath,
        simulationClimateFilePathLabel,
        "Seleziona file climatico",
        "File climatici (*.wtst *.csv);;File WTST (*.wtst);;File CSV (*.csv);;Tutti i file (*)"
    );
}

void MainWindow::onSelectSimulationComponentsFile() {
    setSimulationFileSelection(
        simulationComponentsFilePath,
        simulationComponentsFilePathLabel,
        "Seleziona file JSON componenti",
        "File JSON (*.json);;Tutti i file (*)"
    );
}

void MainWindow::onSelectSimulationSchedulesFile() {
    setSimulationFileSelection(
        simulationSchedulesFilePath,
        simulationSchedulesFilePathLabel,
        "Seleziona file CSV schedules",
        "File CSV (*.csv);;Tutti i file (*)"
    );
}

void MainWindow::onSelectSimulationBuildingsFile() {
    setSimulationFileSelection(
        simulationBuildingsFilePath,
        simulationBuildingsFilePathLabel,
        "Seleziona file JSON edifici",
        "File JSON (*.json);;Tutti i file (*)"
    );
}

void MainWindow::onLaunchSimulation() {
    QTextEdit *targetConsole = simulationOutputConsole ? simulationOutputConsole : outputConsole;

    QStringList missingInputs;
    if (simulationComponentsFilePath.isEmpty()) {
        missingInputs << "file json componenti";
    }
    if (simulationBuildingsFilePath.isEmpty()) {
        missingInputs << "file json edifici";
    }
    if (simulationClimateFilePath.isEmpty()) {
        missingInputs << "file climatico";
    }
    if (simulationSchedulesFilePath.isEmpty()) {
        missingInputs << "file csv schedule";
    }

    if (!missingInputs.isEmpty()) {
        QMessageBox::warning(
            this,
            "Input mancanti",
            "Seleziona tutti gli input richiesti prima di avviare la simulazione:\n- " +
                missingInputs.join("\n- ")
        );
        return;
    }

    if (simulationProcess && simulationProcess->state() != QProcess::NotRunning) {
        QMessageBox::information(
            this,
            "Simulazione in corso",
            "Una simulazione e' gia' in esecuzione."
        );
        return;
    }

    const QString mode = simulationModeCombo ? simulationModeCombo->currentText().trimmed() : QStringLiteral("iso");
    const QString programPath = getBinaryPath("BldSimu");
    QFileInfo programInfo(programPath);

    if (!programInfo.exists() || !programInfo.isExecutable()) {
        QMessageBox::critical(
            this,
            "Errore",
            "Impossibile trovare o eseguire il binario BldSimu:\n" + programPath
        );
        return;
    }

    QStringList arguments;
    arguments << simulationComponentsFilePath
              << simulationBuildingsFilePath
              << simulationClimateFilePath
              << simulationSchedulesFilePath
              << mode;

    if (targetConsole) {
        targetConsole->clear();
        targetConsole->append("[Avvio simulazione BldSimu]");
        targetConsole->append("─────────────────────────────────");
        targetConsole->append("Comando: " + programPath + " " + arguments.join(" "));
        targetConsole->append("Working directory: " + programInfo.absolutePath());
        targetConsole->append("");
    }

    if (launchSimulationBtn) {
        launchSimulationBtn->setEnabled(false);
    }

    simulationProcess = new QProcess(this);
    QProcess *process = simulationProcess;
    process->setWorkingDirectory(programInfo.absolutePath());

    connect(process, &QProcess::readyReadStandardOutput, this, [this, process]() {
        if (simulationProcess != process || !simulationOutputConsole) {
            return;
        }

        const QString chunk = QString::fromLocal8Bit(process->readAllStandardOutput());
        if (chunk.isEmpty()) {
            return;
        }

        simulationOutputConsole->moveCursor(QTextCursor::End);
        simulationOutputConsole->insertPlainText(chunk);
        simulationOutputConsole->ensureCursorVisible();
    });

    connect(process, &QProcess::readyReadStandardError, this, [this, process]() {
        if (simulationProcess != process || !simulationOutputConsole) {
            return;
        }

        const QString chunk = QString::fromLocal8Bit(process->readAllStandardError());
        if (chunk.isEmpty()) {
            return;
        }

        simulationOutputConsole->moveCursor(QTextCursor::End);
        simulationOutputConsole->insertPlainText(chunk);
        simulationOutputConsole->ensureCursorVisible();
    });

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this,
            [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
                const QString remainingStdout = QString::fromLocal8Bit(process->readAllStandardOutput());
                const QString remainingStderr = QString::fromLocal8Bit(process->readAllStandardError());

                if (simulationOutputConsole) {
                    if (!remainingStdout.isEmpty()) {
                        simulationOutputConsole->moveCursor(QTextCursor::End);
                        simulationOutputConsole->insertPlainText(remainingStdout);
                    }
                    if (!remainingStderr.isEmpty()) {
                        simulationOutputConsole->moveCursor(QTextCursor::End);
                        simulationOutputConsole->insertPlainText(remainingStderr);
                    }

                    simulationOutputConsole->append("");
                    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                        simulationOutputConsole->append("✓ Simulazione completata con successo!");
                    } else {
                        simulationOutputConsole->append("✗ Simulazione terminata con errore (codice: " + QString::number(exitCode) + ")");
                    }
                }

                if (launchSimulationBtn) {
                    launchSimulationBtn->setEnabled(true);
                }

                if (exitStatus == QProcess::NormalExit && exitCode == 0) {
                    QMessageBox::information(this, "Successo", "Simulazione completata con successo.");
                } else {
                    QMessageBox::warning(this, "Errore simulazione",
                                         "BldSimu non ha completato correttamente la simulazione.");
                }

                if (simulationProcess == process) {
                    simulationProcess = nullptr;
                }
                process->deleteLater();
            });

    process->start(programPath, arguments);

    if (!process->waitForStarted(5000)) {
        const QString errorMessage = process->errorString();
        if (targetConsole) {
            targetConsole->append("✗ Avvio fallito: " + errorMessage);
        }
        if (launchSimulationBtn) {
            launchSimulationBtn->setEnabled(true);
        }

        simulationProcess = nullptr;
        process->deleteLater();

        QMessageBox::critical(
            this,
            "Errore",
            "Impossibile avviare BldSimu.\n\n" + errorMessage
        );
        return;
    }

    if (targetConsole) {
        targetConsole->append("✓ Simulazione avviata...");
        targetConsole->append("");
    }

    QMessageBox::information(
        this,
        "Simulazione avviata",
        "La simulazione e' stata avviata. L'output viene scritto nella console della pagina Avvia Simulazione."
    );
}

void MainWindow::onMenuItemClicked() {
    contentArea->setCurrentIndex(sidebar->currentRow());
}

void MainWindow::onConvertEPW() {
    // 1. Apri dialog per selezionare il file EPW
    QString epwFilePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file EPW",
        QDir::homePath(),
        "File EPW (*.epw);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato la selezione
    if (epwFilePath.isEmpty()) {
        return;
    }
    
    // 2. Apri dialog per salvare il file wtst
    QString csvFilePath = QFileDialog::getSaveFileName(
        this,
        "Salva file wtst",
        QDir::homePath() + "/weather.wtst",
        "File wtst (*.wtst);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato il salvataggio
    if (csvFilePath.isEmpty()) {
        return;
    }
    
    // 3. Costruisci il percorso del comando
    QString programPath = getBinaryPath("epw2wtst");
    
    // 4. Prepara gli argomenti del comando
    QStringList arguments;
    arguments << epwFilePath << csvFilePath << "0.2";
    
    // 5. Esegui il comando
    QProcess process;
    process.start(programPath, arguments);
    
    // Attendi che il processo termini (max 30 secondi)
    if (!process.waitForFinished(30000)) {
        QMessageBox::critical(
            this,
            "Errore",
            "Il processo di conversione ha impiegato troppo tempo o è fallito.\n\n" +
            process.errorString()
        );
        return;
    }
    
    // 6. Verifica il risultato
    int exitCode = process.exitCode();
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    
    // Scrivi l'output nella casella di testo
    outputConsole->clear();
    outputConsole->append("[EPW → wtst Conversione]");
    outputConsole->append("─────────────────────────────────");
    outputConsole->append("Comando: " + programPath + " " + arguments.join(" "));
    outputConsole->append("");
    
    if (exitCode == 0) {
        outputConsole->append("✓ Conversione completata con successo!");
        outputConsole->append("");
        outputConsole->append("File di output: " + csvFilePath);
        if (!output.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        QMessageBox::information(
            this,
            "Conversione completata",
            QString("Il file EPW è stato convertito con successo in wtst!\n\nFile salvato in: ") + csvFilePath
        );
    } else {
        outputConsole->append("✗ Errore nella conversione (Codice: " + QString::number(exitCode) + ")");
        outputConsole->append("");
        if (!output.isEmpty()) {
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        if (!errorOutput.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Errori:");
            outputConsole->append(errorOutput);
        }
        QMessageBox::warning(
            this,
            "Errore nella conversione",
            "La conversione è fallita con codice di uscita: " + QString::number(exitCode) +
            "\n\nOutput:\n" + output +
            "\n\nErrori:\n" + errorOutput
        );
    }
}

void MainWindow::onConvertIDF() {
    // 1. Apri dialog per selezionare il file IDF
    QString idfFilePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file IDF",
        QDir::homePath(),
        "File IDF (*.idf);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato la selezione
    if (idfFilePath.isEmpty()) {
        return;
    }
    
    // 2. Apri dialog per salvare il file JSON
    QString jsonFilePath = QFileDialog::getSaveFileName(
        this,
        "Salva file JSON",
        QDir::homePath() + "/output.json",
        "File JSON (*.json);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato il salvataggio
    if (jsonFilePath.isEmpty()) {
        return;
    }

    QString jsonDirPath = QFileInfo(jsonFilePath).absolutePath();

    QString programPath="";
    QStringList arguments;
    if (is_convepjson) {
        programPath = energyPlusDirPath;
        arguments << "--convert-only" << "-d" << jsonDirPath << idfFilePath;
    }else {
        programPath = getBinaryPath("idf2json"); // 3. Costruisci il percorso del comando
        arguments << idfFilePath << "-o" << jsonFilePath << "--no-merge"; // 4. Prepara gli argomenti del comando: idf2json input.idf -o output.json --no-merge
    }

    QProcess process; // 5. Esegui il comando
    process.start(programPath, arguments);
    // Attendi che il processo termini (max 60 secondi)
    if (!process.waitForFinished(60000)) {
        QMessageBox::critical(
            this,
            "Errore",
            "Il processo di conversione ha impiegato troppo tempo o è fallito.\n\n" +
            process.errorString()
        );
        return;
    }
    if (is_convepjson) {
        QString filein  = QDir(jsonDirPath).filePath(QFileInfo(idfFilePath).baseName() + ".epJSON");
        QString fileout = jsonFilePath;
        if (!QFile::rename(filein, fileout)) {
            outputConsole->append("⚠ Rinomina fallita: " + filein + " → " + fileout);
        }
    }
    // 6. Verifica il risultato
    int exitCode = process.exitCode();
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();

    // Scrivi l'output nella casella di testo
    outputConsole->clear();
    outputConsole->append("[IDF → JSON Conversione]");
    outputConsole->append("─────────────────────────────────");
    outputConsole->append("Comando: " + programPath + " " + arguments.join(" "));
    outputConsole->append("");
    
    if (exitCode == 0) {
        outputConsole->append("✓ Conversione completata con successo!");
        outputConsole->append("");
        outputConsole->append("File di output: " + jsonFilePath);
        if (!output.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        QMessageBox::information(
            this,
            "Conversione completata",
            QString("Il file IDF è stato convertito con successo in JSON!\n\nFile salvato in: ") + jsonFilePath
        );
    } else {
        outputConsole->append("✗ Errore nella conversione (Codice: " + QString::number(exitCode) + ")");
        outputConsole->append("");
        if (!output.isEmpty()) {
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        if (!errorOutput.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Errori:");
            outputConsole->append(errorOutput);
        }
        QMessageBox::warning(
            this,
            "Errore nella conversione",
            "La conversione è fallita con codice di uscita: " + QString::number(exitCode) +
            "\n\nOutput:\n" + output +
            "\n\nErrori:\n" + errorOutput
        );
    }
}

void MainWindow::onConvertScheduleCompact() {
    // 1. Selezione file IDF
    QString idfFilePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file IDF",
        QDir::homePath(),
        "File IDF (*.idf);;Tutti i file (*)"
    );
    if (idfFilePath.isEmpty()) return;

    QFile file(idfFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file.");
        return;
    }

    QTextStream in(&file);
    QString content = in.readAll();
    file.close();

    // 2. Trova tutti gli oggetti Schedule:Compact
    QMap<QString, QVector<double>> allSchedules;
    int daysInMonth[] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; // 2024 è bisestile (2/29) nel file di esempio
    
    QRegularExpression scheduleRegex("Schedule:Compact\\s*,\\s*([^,;!]+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression throughRegex("Through:\\s*(\\d+)/(\\d+)", QRegularExpression::CaseInsensitiveOption);
    QRegularExpression untilRegex("Until:\\s*(\\d+):(\\d+)\\s*,\\s*([\\d\\.\\-]+)", QRegularExpression::CaseInsensitiveOption);

    auto it = scheduleRegex.globalMatch(content);
    while (it.hasNext()) {
        auto match = it.next();
        QString scheduleName = match.captured(1).trimmed();
        int startPos = match.capturedStart();
        int endPos = content.indexOf(';', startPos);
        if (endPos == -1) continue;

        QString scheduleBlock = content.mid(startPos, endPos - startPos + 1);
        QVector<double> hourlyValues(8760, 0.0);
        QStringList lines = scheduleBlock.split('\n');

        int lastEndHour = 0;
        double lastValue = 0.0;

        for (int i = 0; i < lines.size(); ++i) {
            QString line = lines[i].trimmed();
            auto tMatch = throughRegex.match(line);
            if (tMatch.hasMatch()) {
                int month = tMatch.captured(1).toInt();
                int day = tMatch.captured(2).toInt();
                
                int daysUntilMonth = 0;
                for(int m=1; m < month; ++m) daysUntilMonth += daysInMonth[m];
                int endHour = (daysUntilMonth + day) * 24;

                // Cerca i valori Until in questo blocco Through
                // Per semplicità, prendiamo l'ultimo Until: 24:00 (copre l'intera giornata AllDays)
                for (int j = i + 1; j < lines.size(); ++j) {
                    QString nextLine = lines[j].trimmed();
                    if (nextLine.contains("Through:", Qt::CaseInsensitive)) break;
                    
                    auto uMatch = untilRegex.match(nextLine);
                    if (uMatch.hasMatch()) {
                        lastValue = uMatch.captured(3).toDouble();
                        // Se è 24:00, abbiamo il valore finale per il periodo
                        if (uMatch.captured(1) == "24" && uMatch.captured(2) == "00") break;
                    }
                }

                for (int h = lastEndHour; h < endHour && h < 8760; ++h) {
                    hourlyValues[h] = lastValue;
                }
                lastEndHour = endHour;
            }
        }
        allSchedules.insert(scheduleName, hourlyValues);
    }

    if (allSchedules.isEmpty()) {
        QMessageBox::warning(this, "Attenzione", "Nessun oggetto Schedule:Compact trovato nel file.");
        return;
    }

    // 3. Salvataggio su file CSV con colonne
    QString savePath = QFileDialog::getSaveFileName(this, "Salva CSV Orario", QDir::homePath() + "/schedules_8760.csv", "CSV Files (*.csv)");
    if (!savePath.isEmpty()) {
        QFile outFile(savePath);
        if (outFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&outFile);
            
            // Intestazione
            QStringList headers;
            headers << "Ora";
            QStringList names = allSchedules.keys();
            for (const QString &name : names) headers << "\"" + name + "\"";
            out << headers.join(",") << "\n";

            // Dati (8760 righe)
            for (int h = 0; h < 8760; ++h) {
                QStringList row;
                row << QString::number(h + 1);
                for (const QString &name : names) {
                    row << QString::number(allSchedules[name][h], 'f', 4);
                }
                out << row.join(",") << "\n";
            }
            outFile.close();
            
            outputConsole->clear();
            outputConsole->append("✓ Conversione completata per " + QString::number(allSchedules.size()) + " schedule!");
            outputConsole->append("File salvato: " + savePath);
            outputConsole->append("\nSchedules trovati:");
            for (const QString &name : names) outputConsole->append(" - " + name);

            QMessageBox::information(this, "Successo", "File CSV con tutti gli schedule generato con successo!");
        }
    }
}

void MainWindow::onConvertSHP() {
    // 1. Apri dialog per selezionare il file SHP
    QString shpFilePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file SHP",
        QDir::homePath(),
        "File SHP (*.shp);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato la selezione
    if (shpFilePath.isEmpty()) {
        return;
    }
    
    // 2. Apri directory dialog per la directory contenente il file SHP
    QString shpDirectory = QFileDialog::getExistingDirectory(
        this,
        "Seleziona la directory contenente il file SHP",
        QFileInfo(shpFilePath).dir().path(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    // Se l'utente ha annullato la selezione della directory
    if (shpDirectory.isEmpty()) {
        return;
    }
    
    // 3. Apri dialog per salvare il file GeoJSON
    QString geoJsonFilePath = QFileDialog::getSaveFileName(
        this,
        "Salva file GeoJSON",
        QDir::homePath() + "/output.geojson",
        "File GeoJSON (*.geojson);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato il salvataggio
    if (geoJsonFilePath.isEmpty()) {
        return;
    }
    
    // 4. Costruisci il percorso del comando
    QString programPath = getBinaryPath("shaperead");
    
    // 5. Prepara gli argomenti del comando: shaperead input.shp -o output.geojson --strict-closed
    QStringList arguments;
    arguments << shpFilePath << "-o" << geoJsonFilePath << "--strict-closed";
    
    // 6. Esegui il comando
    QProcess process;
    // Imposta la working directory sulla directory del file SHP
    process.setWorkingDirectory(shpDirectory);
    process.start(programPath, arguments);
    
    // Attendi che il processo termini (max 30 secondi)
    if (!process.waitForFinished(30000)) {
        QMessageBox::critical(
            this,
            "Errore",
            "Il processo di conversione ha impiegato troppo tempo o è fallito.\n\n" +
            process.errorString()
        );
        return;
    }
    
    // 7. Verifica il risultato
    int exitCode = process.exitCode();
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    
    // Scrivi l'output nella casella di testo
    outputConsole->clear();
    outputConsole->append("[SHP → GeoJSON Conversione]");
    outputConsole->append("─────────────────────────────────");
    outputConsole->append("Comando: " + programPath + " " + arguments.join(" "));
    outputConsole->append("Working directory: " + shpDirectory);
    outputConsole->append("");
    
    if (exitCode == 0) {
        outputConsole->append("✓ Conversione completata con successo!");
        outputConsole->append("");
        outputConsole->append("File di output: " + geoJsonFilePath);
        if (!output.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        QMessageBox::information(
            this,
            "Conversione completata",
            QString("Il file SHP è stato convertito con successo in GeoJSON!\n\nFile salvato in: ") + geoJsonFilePath
        );
    } else {
        outputConsole->append("✗ Errore nella conversione (Codice: " + QString::number(exitCode) + ")");
        outputConsole->append("");
        if (!output.isEmpty()) {
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        if (!errorOutput.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Errori:");
            outputConsole->append(errorOutput);
        }
        QMessageBox::warning(
            this,
            "Errore nella conversione",
            "La conversione è fallita con codice di uscita: " + QString::number(exitCode) +
            "\n\nOutput:\n" + output +
            "\n\nErrori:\n" + errorOutput
        );
    }
}

void MainWindow::onConvertGeoJSON() {
    // 1. Apri dialog per selezionare il primo file GeoJSON
    QString geoJsonFilePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file GeoJSON (poligoni degli edifici)",
        QDir::homePath(),
        "File GeoJSON (*.geojson);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato la selezione
    if (geoJsonFilePath.isEmpty()) {
        return;
    }
    
    // 2. Apri dialog per selezionare il secondo file CSV (dati tipologie edilizie)
    QString csvFilePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file CSV (dati tipologie edilizie)",
        QFileInfo(geoJsonFilePath).dir().path(),
        "File CSV (*.csv);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato la selezione
    if (csvFilePath.isEmpty()) {
        return;
    }
    
    // 3. Apri dialog per salvare il file JSON di output
    QString outputJsonFilePath = QFileDialog::getSaveFileName(
        this,
        "Salva file JSON di output",
        QFileInfo(geoJsonFilePath).dir().path() + "/output.json",
        "File JSON (*.json);;Tutti i file (*)"
    );
    
    // Se l'utente ha annullato il salvataggio
    if (outputJsonFilePath.isEmpty()) {
        return;
    }
    
    // 4. Costruisci il percorso del comando
    QString programPath = getBinaryPath("pol_geoelem");
    
    // 5. Prepara gli argomenti del comando: pol_geoelem input.geojson input2.csv output.json
    QStringList arguments;
    arguments << geoJsonFilePath << csvFilePath << outputJsonFilePath;
    
    // 6. Esegui il comando
    QProcess process;
    process.start(programPath, arguments);
    
    // Attendi che il processo termini (max 60 secondi)
    if (!process.waitForFinished(60000)) {
        QMessageBox::critical(
            this,
            "Errore",
            "Il processo di conversione ha impiegato troppo tempo o è fallito.\n\n" +
            process.errorString()
        );
        return;
    }
    
    // 7. Verifica il risultato
    int exitCode = process.exitCode();
    QString output = process.readAllStandardOutput();
    QString errorOutput = process.readAllStandardError();
    
    // Scrivi l'output nella casella di testo
    outputConsole->clear();
    outputConsole->append("[GeoJSON+CSV → JSON Conversione]");
    outputConsole->append("─────────────────────────────────");
    outputConsole->append("Comando: " + programPath + " " + arguments.join(" "));
    outputConsole->append("");
    
    if (exitCode == 0) {
        outputConsole->append("✓ Conversione completata con successo!");
        outputConsole->append("");
        outputConsole->append("File GeoJSON: " + geoJsonFilePath);
        outputConsole->append("File CSV: " + csvFilePath);
        outputConsole->append("File di output: " + outputJsonFilePath);
        if (!output.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        QMessageBox::information(
            this,
            "Conversione completata",
            QString("Il file GeoJSON è stato convertito con successo in JSON!\n\nFile salvato in: ") + outputJsonFilePath
        );
    } else {
        outputConsole->append("✗ Errore nella conversione (Codice: " + QString::number(exitCode) + ")");
        outputConsole->append("");
        if (!output.isEmpty()) {
            outputConsole->append("Output standard:");
            outputConsole->append(output);
        }
        if (!errorOutput.isEmpty()) {
            outputConsole->append("");
            outputConsole->append("Errori:");
            outputConsole->append(errorOutput);
        }
        QMessageBox::warning(
            this,
            "Errore nella conversione",
            "La conversione è fallita con codice di uscita: " + QString::number(exitCode) +
            "\n\nOutput:\n" + output +
            "\n\nErrori:\n" + errorOutput
        );
    }
}

void MainWindow::onLoadWeatherFile() {
    // Seleziona il file WTST
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file WTST",
        QDir::homePath(),
        "File WTST (*.wtst *.csv);;Tutti i file (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Apri e leggi il file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file: " + filePath);
        return;
    }
    
    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();
    
    if (lines.size() < 10) {
        QMessageBox::warning(this, "Errore", "Il file deve contenere almeno 10 righe (8 di intestazione + riga con nomi colonne + dati).");
        return;
    }
    
    // Mostra le prime 8 righe nel widget fileHeaderText
    if (fileHeaderText) {
        QString header8Lines;
        for (int i = 0; i < 8 && i < lines.size(); i++) {
            header8Lines += QString("Riga %1: %2\n").arg(i + 1).arg(lines[i]);
        }
        fileHeaderText->setPlainText(header8Lines);
    }
    
    // Leggi le intestazioni delle colonne dalla riga 9 (indice 8)
    QStringList columnHeaders = lines[8].split(',');
    
    // Pulisci gli header rimuovendo spazi bianchi
    for (int i = 0; i < columnHeaders.size(); i++) {
        columnHeaders[i] = columnHeaders[i].trimmed();
    }
    
    // Salva gli header
    loadedHeaders = columnHeaders;
    
    // Verifica che ci siano almeno 11 colonne
    if (columnHeaders.size() < 11) {
        QMessageBox::warning(this, "Errore", 
            QString("Il file deve avere almeno 11 colonne. Trovate: %1").arg(columnHeaders.size()));
        return;
    }
    
    // Prepara i vettori per tutti i dati
    loadedData.clear();
    loadedData.resize(columnHeaders.size());
    
    // Leggi i dati dalle righe successive (dalla riga 10 in poi, indice 9+)
    for (int i = 9; i < lines.size(); i++) {
        QString line = lines[i].trimmed();
        if (line.isEmpty()) continue;
        
        QStringList values = line.split(',');
        if (values.size() < columnHeaders.size()) continue;
        
        // Indice del dato
        double index = i - 9;
        
        // Leggi tutti i valori delle colonne
        for (int col = 0; col < columnHeaders.size(); col++) {
            bool ok;
            double value = values[col].trimmed().toDouble(&ok);
            if (ok) {
                loadedData[col].append(QPointF(index, value));
            }
        }
    }
    
    // Popola il grafico della temperatura (colonna 5, indice 4)
    if (loadedData.size() > 4 && !loadedData[4].isEmpty() && temperatureChartView) {
        QChart *chart = temperatureChartView->chart();
        chart->removeAllSeries();
        
        QLineSeries *series = new QLineSeries();
        series->setName(QString("Temperatura - %1").arg(columnHeaders[4]));
        series->setColor(QColor("#e74c3c"));
        series->append(loadedData[4]);
        
        chart->addSeries(series);
        
        QValueAxis *axisX = new QValueAxis();
        QValueAxis *axisY = new QValueAxis();
        
        axisX->setTitleText("Indice dato");
        axisY->setTitleText("Temperatura (°C)");
        axisX->setLabelFormat("%i");
        axisY->setLabelFormat("%.1f");
        
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisX);
        series->attachAxis(axisY);
        
        // Calcola range automatico
        double minTemp = loadedData[4][0].y();
        double maxTemp = loadedData[4][0].y();
        for (const QPointF &p : loadedData[4]) {
            if (p.y() < minTemp) minTemp = p.y();
            if (p.y() > maxTemp) maxTemp = p.y();
        }
        
        axisX->setRange(0, loadedData[4].size());
        axisY->setRange(minTemp - 2, maxTemp + 2);
    }
    
    // Popola il grafico dell'umidità (colonna 6, indice 5)
    if (loadedData.size() > 5 && !loadedData[5].isEmpty() && humidityChartView) {
        QChart *chart = humidityChartView->chart();
        chart->removeAllSeries();
        
        QLineSeries *series = new QLineSeries();
        series->setName(QString("Umidità - %1").arg(columnHeaders[5]));
        series->setColor(QColor("#3498db"));
        series->append(loadedData[5]);
        
        chart->addSeries(series);
        
        QValueAxis *axisX = new QValueAxis();
        QValueAxis *axisY = new QValueAxis();
        
        axisX->setTitleText("Indice dato");
        axisY->setTitleText("Umidità (%)");
        axisX->setLabelFormat("%i");
        axisY->setLabelFormat("%.1f");
        
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        series->attachAxis(axisX);
        series->attachAxis(axisY);
        
        // Calcola range automatico
        double minHum = loadedData[5][0].y();
        double maxHum = loadedData[5][0].y();
        for (const QPointF &p : loadedData[5]) {
            if (p.y() < minHum) minHum = p.y();
            if (p.y() > maxHum) maxHum = p.y();
        }
        
        axisX->setRange(0, loadedData[5].size());
        axisY->setRange(minHum - 5, maxHum + 5);
    }
    
    // Popola i menu a tendina con le colonne disponibili (escluse le prime 11)
    radiationCombo1->clear();
    radiationCombo2->clear();
    radiationCombo1->addItem("-- Nessuna --", -1);
    radiationCombo2->addItem("-- Nessuna --", -1);
    
    for (int i = 11; i < columnHeaders.size(); i++) {
        radiationCombo1->addItem(QString("Col %1: %2").arg(i + 1).arg(columnHeaders[i]), i);
        radiationCombo2->addItem(QString("Col %1: %2").arg(i + 1).arg(columnHeaders[i]), i);
    }
    
    radiationCombo1->setEnabled(true);
    radiationCombo2->setEnabled(true);
    
    // Salva il numero totale di punti dati
    totalDataPoints = loadedData[0].size();
    
    // Inizializza i controlli range X
    if (xMinSpinBox && xMaxSpinBox) {
        // Disconnetti temporaneamente i segnali per evitare chiamate multiple
        disconnect(xMinSpinBox, nullptr, this, nullptr);
        disconnect(xMaxSpinBox, nullptr, this, nullptr);
        
        xMinSpinBox->setMinimum(0);
        xMinSpinBox->setMaximum(totalDataPoints - 10);
        xMinSpinBox->setValue(0);
        xMinSpinBox->setEnabled(true);
        
        xMaxSpinBox->setMinimum(10);
        xMaxSpinBox->setMaximum(totalDataPoints);
        xMaxSpinBox->setValue(totalDataPoints);
        xMaxSpinBox->setEnabled(true);
        
        // Riconnetti i segnali
        connect(xMinSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
                this, &MainWindow::onXRangeChanged);
        connect(xMaxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
                this, &MainWindow::onXRangeChanged);
    }
    
    // Popola il grafico radiazione con colonne 10 e 11 (indici 9 e 10)
    updateRadiationChart();
    
    // Messaggio di conferma
    QMessageBox::information(this, "Successo", 
        QString("File caricato con successo!\n\n"
                "Righe totali: %1\n"
                "Colonne: %2\n"
                "Dati caricati per tutte le colonne")
        .arg(lines.size())
        .arg(columnHeaders.size()));
}

void MainWindow::onLoadGeoJSONFile() {
    // Seleziona il file GeoJSON
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file GeoJSON",
        QDir::homePath(),
        "File GeoJSON (*.geojson *.json);;Tutti i file (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Apri e leggi il file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file: " + filePath);
        return;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    // Parsa il JSON
    QJsonParseError parseError;
    geoJsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, "Errore JSON", 
            QString("Errore nel parsing del JSON: %1").arg(parseError.errorString()));
        return;
    }
    
    if (!geoJsonDoc.isObject()) {
        QMessageBox::warning(this, "Errore", "Il file JSON non contiene un oggetto valido.");
        return;
    }
    
    QJsonObject rootObj = geoJsonDoc.object();
    
    // Leggi type e name
    QString type = rootObj.value("type").toString("N/A");
    QString name = rootObj.value("name").toString("N/A");
    
    geoTypeLabel->setText(type);
    geoNameLabel->setText(name);
    
    // Pulisci la scena
    geoGraphicsScene->clear();
    
    // Leggi features
    QJsonArray features = rootObj.value("features").toArray();
    
    if (features.isEmpty()) {
        QMessageBox::warning(this, "Attenzione", "Nessun feature trovato nel file GeoJSON.");
        return;
    }
    
    // Variabili per calcolare i bounds
    double minX = 1e10, maxX = -1e10;
    double minY = 1e10, maxY = -1e10;
    
    // Lista di colori per i poligoni
    QVector<QColor> colors = {
        QColor(231, 76, 60, 180),   // Rosso
        QColor(52, 152, 219, 180),  // Blu
        QColor(46, 204, 113, 180),  // Verde
        QColor(241, 196, 15, 180),  // Giallo
        QColor(155, 89, 182, 180),  // Viola
        QColor(230, 126, 34, 180),  // Arancione
        QColor(26, 188, 156, 180),  // Turchese
        QColor(149, 165, 166, 180)  // Grigio
    };
    
    int colorIndex = 0;
    int polygonCount = 0;
    
    // Processa ogni feature
    for (const QJsonValue &featureVal : features) {
        QJsonObject feature = featureVal.toObject();
        QJsonObject geometry = feature.value("geometry").toObject();
        QJsonObject properties = feature.value("properties").toObject();
        
        QString geomType = geometry.value("type").toString();
        
        if (geomType == "Polygon" || geomType == "MultiPolygon") {
            QJsonArray coordinates = geometry.value("coordinates").toArray();
            
            // Per Polygon, coordinates è un array di anelli (il primo è l'esterno)
            // Per MultiPolygon, è un array di poligoni
            
            // Gestione semplificata: prendi il primo anello del primo poligono
            QJsonArray rings;
            if (geomType == "Polygon") {
                rings = coordinates;
            } else { // MultiPolygon
                if (!coordinates.isEmpty()) {
                    rings = coordinates[0].toArray();
                }
            }
            
            if (!rings.isEmpty()) {
                QJsonArray exteriorRing = rings[0].toArray();
                
                QPolygonF polygon;
                
                for (const QJsonValue &coordVal : exteriorRing) {
                    QJsonArray coord = coordVal.toArray();
                    if (coord.size() >= 2) {
                        double x = coord[0].toDouble();
                        double y = coord[1].toDouble();
                        
                        // Inverti Y per coordinate Gauss-Boaga (sistema cartesiano standard)
                        polygon << QPointF(x, -y);
                        
                        // Aggiorna bounds
                        if (x < minX) minX = x;
                        if (x > maxX) maxX = x;
                        if (y < minY) minY = y;
                        if (y > maxY) maxY = y;
                    }
                }
                
                if (!polygon.isEmpty()) {
                    // Crea il poligono grafico
                    QGraphicsPolygonItem *polyItem = new QGraphicsPolygonItem(polygon);
                    
                    QColor fillColor = colors[colorIndex % colors.size()];
                    polyItem->setBrush(QBrush(fillColor));
                    polyItem->setPen(QPen(Qt::black, 2));
                    polyItem->setFlag(QGraphicsItem::ItemIsSelectable);
                    
                    // Memorizza le proprietà come QVariantMap (convertito da QJsonObject)
                    polyItem->setData(0, QVariant::fromValue(properties.toVariantMap()));
                    
                    geoGraphicsScene->addItem(polyItem);
                    
                    colorIndex++;
                    polygonCount++;
                }
            }
        }
    }
    
    // Adatta la vista ai bounds
    if (polygonCount > 0) {
        geoGraphicsScene->setSceneRect(minX - 100, -maxY - 100, 
                                       (maxX - minX) + 200, (maxY - minY) + 200);
        geoGraphicsView->fitInView(geoGraphicsScene->sceneRect(), Qt::KeepAspectRatio);
        
        // Connetti il segnale di selezione
        connect(geoGraphicsScene, &QGraphicsScene::selectionChanged, this, [this]() {
            QList<QGraphicsItem*> selectedItems = geoGraphicsScene->selectedItems();
            if (!selectedItems.isEmpty()) {
                QGraphicsPolygonItem *poly = dynamic_cast<QGraphicsPolygonItem*>(selectedItems.first());
                if (poly) {
                    // Recupera le proprietà come QVariantMap e converti in QJsonObject
                    QVariantMap propsMap = poly->data(0).toMap();
                    QJsonObject propsObj = QJsonObject::fromVariantMap(propsMap);
                    
                    // Formatta le proprietà con numeri a 1 decimale
                    QString formattedProps = formatJsonProperties(propsObj);
                    geoPropertiesText->setPlainText(formattedProps);
                }
            } else {
                geoPropertiesText->setPlainText("Clicca su un poligono per visualizzare le sue proprietà...");
            }
        });
    }
    
    QMessageBox::information(this, "Successo", 
        QString("File GeoJSON caricato con successo!\n\n"
                "Type: %1\n"
                "Name: %2\n"
                "Poligoni caricati: %3")
        .arg(type)
        .arg(name)
        .arg(polygonCount));
    updateCreateButton();
}

void MainWindow::onLoadComponentsFile() {
    // Seleziona il file JSON componenti
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file JSON componenti",
        QDir::homePath(),
        "File JSON (*.json);;Tutti i file (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Apri e leggi il file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file: " + filePath);
        return;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    // Parsa il JSON
    QJsonParseError parseError;
    componentsJsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, "Errore JSON", 
            QString("Errore nel parsing del JSON: %1").arg(parseError.errorString()));
        return;
    }
    
    if (!componentsJsonDoc.isObject()) {
        QMessageBox::warning(this, "Errore", "Il file JSON non contiene un oggetto valido.");
        return;
    }

    // Normalizza le chiavi di primo livello in MAIUSCOLO
    {
        QJsonObject srcObj = componentsJsonDoc.object();
        QJsonObject normObj;
        for (const QString &key : srcObj.keys())
            normObj.insert(key.toUpper(), srcObj.value(key));
        componentsJsonDoc.setObject(normObj);
    }

    currentComponentsFilePath = filePath;

    // Popola il tab widget
    populateComponentsTabs();

    // Abilita il pulsante salva
    saveComponentsBtn->setEnabled(true);
    
    int totalItems = 0;
    QJsonObject rootObj = componentsJsonDoc.object();
    for (const QString &key : rootObj.keys()) {
        if (rootObj.value(key).isObject()) {
            totalItems += rootObj.value(key).toObject().keys().size();
        }
    }
    
    QMessageBox::information(this, "Successo", 
        QString("File componenti caricato con successo!\n\n"
                "Sezioni: %1\n"
                "Componenti totali: %2")
        .arg(rootObj.keys().size())
        .arg(totalItems));
    updateCreateButton();
}

void MainWindow::populateComponentsTabs() {
    if (!componentsJsonDoc.isObject() || !componentsTabs) return;

    _updatingTable = true;

    // Ricorda il tab corrente
    QString currentTabName;
    if (componentsTabs->currentIndex() >= 0)
        currentTabName = componentsTabs->tabText(componentsTabs->currentIndex());

    // Rimuovi tutti i tab esistenti
    while (componentsTabs->count() > 0)
        componentsTabs->removeTab(0);

    const QString tableStyle =
        "QTableWidget {"
        "    background-color: white; border: 1px solid #bdc3c7;"
        "    alternate-background-color: #f9f9f9; gridline-color: #ddd;"
        "}"
        "QTableWidget::item:selected { background-color: #3498db; color: white; }"
        "QHeaderView::section {"
        "    background-color: #2c3e50; color: white;"
        "    padding: 6px; font-weight: bold; border: 1px solid #1a252f;"
        "}";

    QJsonObject rootObj = componentsJsonDoc.object();

    const QStringList sectionOrder = {
        "MATERIAL",
        "WINDOWMATERIAL:GLAZING",
        "WINDOWMATERIAL:GAS",
        "WINDOWFRAME",
        "CONSTRUCTION",
        "INTERNALMASS",
        "ZONEINFILTRATION:DESIGNFLOWRATE",
        "PEOPLE",
        "LIGHTS",
        "ELECTRICEQUIPMENT",
        "LATENTEQUIPMENT",
        "HVAC",
        "HVAC_MANAGER"
    };

    for (const QString &sectionKey : sectionOrder) {
        if (!rootObj.contains(sectionKey)) continue;
        QJsonValue sectionValue = rootObj.value(sectionKey);
        if (!sectionValue.isObject()) continue;
        QJsonObject sectionObj = sectionValue.toObject();

        // Unione di tutti i campi presenti nella sezione
        QStringList allFields;
        for (const QString &compKey : sectionObj.keys()) {
            if (sectionObj.value(compKey).isObject()) {
                for (const QString &f : sectionObj.value(compKey).toObject().keys())
                    if (!allFields.contains(f)) allFields.append(f);
            }
        }
        allFields.sort();

        // Per CONSTRUCTION: OUTSIDE_LAYER prima, poi LAYER_2, LAYER_3, ... in ordine numerico
        if (sectionKey == "CONSTRUCTION") {
            QStringList layerFields;
            QStringList otherFields;
            for (const QString &f : allFields) {
                if (f == "OUTSIDE_LAYER" || QRegularExpression("^LAYER_\\d+$").match(f).hasMatch())
                    layerFields.append(f);
                else
                    otherFields.append(f);
            }
            // Ordina LAYER_N numericamente
            std::sort(layerFields.begin(), layerFields.end(), [](const QString &a, const QString &b) -> bool {
                if (a == "OUTSIDE_LAYER") return true;
                if (b == "OUTSIDE_LAYER") return false;
                int na = a.mid(6).toInt();  // "LAYER_" = 6 chars
                int nb = b.mid(6).toInt();
                return na < nb;
            });
            allFields = layerFields + otherFields;
        }

        QStringList compKeys = sectionObj.keys();

        // ── Tabella trasposta: colonne = componenti, righe = campi ──────────
        // Riga 0  → "Nome Componente" (editabile per rinominare)
        // Riga r>0 → campo allFields[r-1]
        QTableWidget *table = new QTableWidget();
        table->setProperty("section", sectionKey);
        table->setColumnCount(compKeys.size());
        table->setRowCount(1 + allFields.size());
        table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

        // Intestazioni verticali: "Nome" + nomi dei campi
        QStringList vHeaders;
        vHeaders << "Nome Componente";
        vHeaders << allFields;
        table->setVerticalHeaderLabels(vHeaders);

        // Intestazioni orizzontali: nomi dei componenti (solo per riferimento e resize colonne)
        table->setHorizontalHeaderLabels(compKeys);

        for (int c = 0; c < compKeys.size(); ++c) {
            const QString &compKey = compKeys[c];
            QJsonObject compObj = sectionObj.value(compKey).toObject();

            // Riga 0 — nome componente (rinomina tramite doppio clic)
            QTableWidgetItem *nameItem = new QTableWidgetItem(compKey);
            nameItem->setData(Qt::UserRole, compKey);
            nameItem->setFont(QFont(".AppleSystemUIFont", 10, QFont::Bold));
            nameItem->setForeground(QBrush(QColor("#16a085")));
            nameItem->setBackground(QBrush(QColor("#e8f8f5")));
            table->setItem(0, c, nameItem);

            // Righe 1..N — valori dei campi
            for (int r = 0; r < allFields.size(); ++r) {
                const QString &field = allFields[r];
                QTableWidgetItem *cellItem;
                if (compObj.contains(field)) {
                    QJsonValue val = compObj.value(field);
                    QString valStr;
                    if (val.isDouble())      valStr = QString::number(val.toDouble(), 'f', 6);
                    else if (val.isString()) valStr = val.toString();
                    else if (val.isBool())   valStr = val.toBool() ? "true" : "false";
                    else if (val.isArray())  valStr = QString("[%1 elementi]").arg(val.toArray().size());
                    else                     valStr = "null";
                    cellItem = new QTableWidgetItem(valStr);
                } else {
                    cellItem = new QTableWidgetItem("");
                    cellItem->setBackground(QBrush(QColor("#eeeeee")));
                }
                cellItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled | Qt::ItemIsEditable);
                table->setItem(1 + r, c, cellItem);
            }
        }

        table->setAlternatingRowColors(true);
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        table->horizontalHeader()->setStretchLastSection(false);
        table->horizontalHeader()->setMinimumSectionSize(80);
        table->verticalHeader()->setMinimumWidth(180);
        table->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        table->verticalHeader()->setDefaultSectionSize(26);
        table->resizeColumnsToContents();
        table->setSelectionBehavior(QAbstractItemView::SelectItems);
        table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::AnyKeyPressed);
        table->setContextMenuPolicy(Qt::CustomContextMenu);
        table->setStyleSheet(tableStyle);

        // Delegate con suggerimenti per certi campi
        ComponentsDelegate *delegate = new ComponentsDelegate(table);
        delegate->fieldNames = allFields;
        // Per CONSTRUCTION: suggerisce i nomi dei MATERIAL presenti nel JSON
        if (sectionKey == "CONSTRUCTION") {
            delegate->getDynamicSuggestions = [this](const QString &fieldName) -> QStringList {
                static const QRegularExpression layerRe("^(OUTSIDE_LAYER|LAYER_\\d+)$");
                if (!layerRe.match(fieldName).hasMatch()) return {};
                if (!componentsJsonDoc.isObject()) return {};
                QJsonObject mat = componentsJsonDoc.object().value("MATERIAL").toObject();
                QStringList names = mat.keys();
                names.sort();
                return names;
            };
        }
        table->setItemDelegate(delegate);

        connect(table, &QTableWidget::customContextMenuRequested, this,
            [this, table](const QPoint &pos) {
                showComponentsContextMenu(table, pos);
            });

        connect(table, &QTableWidget::cellChanged, this,
            [this, table](int row, int col) {
                onComponentsCellChanged(table, row, col);
            });

        // Avvolgi tabella in un widget con scrollbar orizzontale in cima
        QWidget *tabContainer = new QWidget();
        QVBoxLayout *tabLayout = new QVBoxLayout(tabContainer);
        tabLayout->setContentsMargins(0, 0, 0, 0);
        tabLayout->setSpacing(0);

        QScrollBar *hTopBar = new QScrollBar(Qt::Horizontal);
        hTopBar->setStyleSheet(
            "QScrollBar:horizontal { height: 14px; background: #ecf0f1; border-radius: 4px; }"
            "QScrollBar::handle:horizontal { background: #95a5a6; border-radius: 4px; min-width: 30px; }"
            "QScrollBar::handle:horizontal:hover { background: #7f8c8d; }"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }");
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        connect(table->horizontalScrollBar(), &QScrollBar::rangeChanged,
                hTopBar, [hTopBar, table](int min, int max) {
                    hTopBar->setRange(min, max);
                    hTopBar->setPageStep(table->horizontalScrollBar()->pageStep());
                    hTopBar->setSingleStep(table->horizontalScrollBar()->singleStep());
                });
        connect(table->horizontalScrollBar(), &QScrollBar::valueChanged, hTopBar, &QScrollBar::setValue);
        connect(hTopBar, &QScrollBar::valueChanged, table->horizontalScrollBar(), &QScrollBar::setValue);

        tabLayout->addWidget(hTopBar);
        tabLayout->addWidget(table);
        componentsTabs->addTab(tabContainer, sectionKey);
    }

    // Sync visual tab bars
    if (compTabBar1 && compTabBar2) {
        _syncingTabBars = true;
        while (compTabBar1->count() > 0) compTabBar1->removeTab(0);
        while (compTabBar2->count() > 0) compTabBar2->removeTab(0);
        int total = componentsTabs->count();
        int split = qMin(6, total);
        for (int i = 0; i < split; ++i)
            compTabBar1->addTab(componentsTabs->tabText(i));
        for (int i = split; i < total; ++i)
            compTabBar2->addTab(componentsTabs->tabText(i));
        _syncingTabBars = false;
    }

    // Ripristina il tab selezionato
    int restoredIdx = 0;
    for (int i = 0; i < componentsTabs->count(); ++i) {
        if (componentsTabs->tabText(i) == currentTabName) {
            componentsTabs->setCurrentIndex(i);
            restoredIdx = i;
            break;
        }
    }

    // Sync visual tab bar selection
    if (compTabBar1 && compTabBar2 && componentsTabs->count() > 0) {
        int split = compTabBar1->count();
        _syncingTabBars = true;
        if (restoredIdx < split) {
            compTabBar1->setCurrentIndex(restoredIdx);
            compTabBar1->setProperty("inactive", false);
            compTabBar2->setProperty("inactive", true);
        } else {
            compTabBar2->setCurrentIndex(restoredIdx - split);
            compTabBar2->setProperty("inactive", false);
            compTabBar1->setProperty("inactive", true);
        }
        compTabBar1->style()->unpolish(compTabBar1);
        compTabBar1->style()->polish(compTabBar1);
        compTabBar2->style()->unpolish(compTabBar2);
        compTabBar2->style()->polish(compTabBar2);
        _syncingTabBars = false;
    }

    _updatingTable = false;
}

void MainWindow::onAddComponent() {
    // Aggiunge un componente alla sezione del tab corrente
    if (!componentsJsonDoc.isObject()) {
        QMessageBox::warning(this, "Attenzione", "Carica prima un file JSON.");
        return;
    }
    int idx = componentsTabs->currentIndex();
    if (idx < 0) return;
    addComponentToSection(componentsTabs->tabText(idx));
}

// ── Context menu ──────────────────────────────────────────────────────────────

void MainWindow::showComponentsContextMenu(QTableWidget *table, const QPoint &pos) {
    QString sectionKey = table->property("section").toString();
    QTableWidgetItem *item = table->itemAt(pos);
    int row = item ? item->row() : -1;
    int col = item ? item->column() : -1;

    QMenu menu(this);

    // ── Componenti (colonne) ──────────────────────────────────────────────────
    QAction *addComp = menu.addAction("➕ Aggiungi Componente");
    connect(addComp, &QAction::triggered, this, [this, sectionKey]() {
        addComponentToSection(sectionKey);
    });

    if (col >= 0) {
        QTableWidgetItem *nameItem = table->item(0, col);
        QString compName = nameItem ? nameItem->text() : QString();
        if (!compName.isEmpty()) {
            QAction *delComp = menu.addAction(QString("🗑️ Elimina Componente \"%1\"").arg(compName));
            connect(delComp, &QAction::triggered, this, [this, sectionKey, compName]() {
                deleteComponentFromSection(sectionKey, compName);
            });
        }
    }

    menu.addSeparator();

    // ── Campi (righe) ─────────────────────────────────────────────────────────
    QAction *addField = menu.addAction("🔧 Aggiungi Campo (riga)");
    connect(addField, &QAction::triggered, this, [this, sectionKey]() {
        addFieldToSection(sectionKey);
    });

    if (row > 0) {   // row 0 = nomi componenti → non eliminabile
        QTableWidgetItem *vHdr = table->verticalHeaderItem(row);
        QString fieldName = vHdr ? vHdr->text() : QString();
        if (!fieldName.isEmpty()) {
            QAction *delField = menu.addAction(QString("🗑️ Elimina Campo \"%1\"").arg(fieldName));
            connect(delField, &QAction::triggered, this, [this, sectionKey, fieldName]() {
                deleteFieldFromSection(sectionKey, fieldName);
            });
        }
    }

    menu.exec(table->viewport()->mapToGlobal(pos));
}

void MainWindow::onComponentsCellChanged(QTableWidget *table, int row, int col) {
    if (_updatingTable) return;

    QString sectionKey = table->property("section").toString();

    QJsonObject rootObj    = componentsJsonDoc.object();
    QJsonObject sectionObj = rootObj.value(sectionKey).toObject();

    if (row == 0) {
        // Riga 0 → rinomina componente (col = indice componente)
        QTableWidgetItem *nameItem = table->item(0, col);
        if (!nameItem) return;
        QString newName = nameItem->text().trimmed();
        QString oldName = nameItem->data(Qt::UserRole).toString();
        if (newName == oldName || newName.isEmpty()) return;

        if (sectionObj.contains(newName)) {
            QMessageBox::warning(this, "Attenzione",
                QString("Il componente \"%1\" esiste già in questa sezione.").arg(newName));
            _updatingTable = true;
            nameItem->setText(oldName);
            _updatingTable = false;
            return;
        }

        QJsonObject compObj = sectionObj.value(oldName).toObject();
        sectionObj.remove(oldName);
        sectionObj[newName] = compObj;
        rootObj[sectionKey] = sectionObj;
        componentsJsonDoc.setObject(rootObj);
        nameItem->setData(Qt::UserRole, newName);
        // Aggiorna anche l'intestazione orizzontale
        _updatingTable = true;
        if (QTableWidgetItem *hdr = table->horizontalHeaderItem(col))
            hdr->setText(newName);
        _updatingTable = false;
    } else {
        // Riga > 0 → modifica valore campo (col = componente, row-1 = campo)
        QTableWidgetItem *nameItem = table->item(0, col);
        if (!nameItem) return;
        QString compName = nameItem->text();

        QTableWidgetItem *vHdr = table->verticalHeaderItem(row);
        if (!vHdr) return;
        QString fieldName = vHdr->text();

        QTableWidgetItem *cellItem = table->item(row, col);
        if (!cellItem) return;
        QString newValue = cellItem->text();

        QJsonObject compObj = sectionObj.value(compName).toObject();
        QJsonValue  oldVal  = compObj.value(fieldName);

        if (oldVal.isDouble())    compObj[fieldName] = newValue.toDouble();
        else if (oldVal.isBool()) compObj[fieldName] = (newValue.toLower() == "true" || newValue == "1");
        else                      compObj[fieldName] = newValue;

        sectionObj[compName] = compObj;
        rootObj[sectionKey]  = sectionObj;
        componentsJsonDoc.setObject(rootObj);
    }
}

// ── CRUD helpers ──────────────────────────────────────────────────────────────

void MainWindow::addComponentToSection(const QString &sectionKey) {
    if (!componentsJsonDoc.isObject()) return;

    bool ok;
    QString componentName = QInputDialog::getText(this, "Nuovo Componente",
        QString("Nome del nuovo componente in \"%1\":").arg(sectionKey),
        QLineEdit::Normal, "", &ok);
    if (!ok || componentName.trimmed().isEmpty()) return;
    componentName = componentName.trimmed();

    QJsonObject rootObj    = componentsJsonDoc.object();
    QJsonObject sectionObj = rootObj.value(sectionKey).toObject();

    if (sectionObj.contains(componentName)) {
        QMessageBox::warning(this, "Attenzione",
            QString("Il componente \"%1\" esiste già.").arg(componentName));
        return;
    }

    // Template dal primo componente esistente, oppure campi di default
    QJsonObject newComp;
    if (!sectionObj.isEmpty()) {
        QJsonObject tmpl = sectionObj.value(sectionObj.keys().first()).toObject();
        for (const QString &key : tmpl.keys()) {
            QJsonValue v = tmpl.value(key);
            if (v.isDouble())      newComp[key] = 0.0;
            else if (v.isString()) newComp[key] = QString("");
            else if (v.isBool())   newComp[key] = false;
            else if (v.isArray())  newComp[key] = QJsonArray();
            else                   newComp[key] = QJsonValue::Null;
        }
    } else {
        QString sl = sectionKey.toLower();
        if (sl.contains("material") && !sl.contains("window")) {
            newComp["thickness"] = 0.0; newComp["conductivity"] = 0.0;
            newComp["resistance"] = 0.0; newComp["density"] = 0.0; newComp["specific_heat"] = 0.0;
        } else if (sl.contains("glazing")) {
            newComp["thickness"] = 0.0; newComp["conductivity"] = 0.0;
        } else if (sl.contains("gas")) {
            newComp["gas_type"] = QString(""); newComp["thickness"] = 0.0;
        } else if (sl.contains("construction")) {
            newComp["OUTSIDE_LAYER"] = QString(""); newComp["layer_2"] = QString("");
        } else if (sl.contains("hvac") && sl.contains("manager")) {
            newComp["thermostat_type"] = QString("");
            newComp["heating_setpoint"] = 0.0; newComp["cooling_setpoint"] = 0.0;
        } else if (sl.contains("hvac")) {
            newComp["type"] = QString(""); newComp["capacity"] = 0.0; newComp["efficiency"] = 0.0;
        } else {
            newComp["value"] = 0.0;
        }
    }

    sectionObj[componentName] = newComp;
    rootObj[sectionKey] = sectionObj;
    componentsJsonDoc.setObject(rootObj);
    populateComponentsTabs();
}

void MainWindow::deleteComponentFromSection(const QString &sectionKey, const QString &componentName) {
    auto reply = QMessageBox::question(this, "Conferma Eliminazione",
        QString("Eliminare il componente \"%1\" dalla sezione \"%2\"?").arg(componentName, sectionKey),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QJsonObject rootObj    = componentsJsonDoc.object();
    QJsonObject sectionObj = rootObj.value(sectionKey).toObject();
    sectionObj.remove(componentName);
    rootObj[sectionKey] = sectionObj;
    componentsJsonDoc.setObject(rootObj);
    populateComponentsTabs();
}

void MainWindow::addFieldToSection(const QString &sectionKey) {
    bool ok;
    QString fieldName = QInputDialog::getText(this, "Nuovo Campo",
        QString("Nome del campo da aggiungere a tutti i componenti in \"%1\":").arg(sectionKey),
        QLineEdit::Normal, "", &ok);
    if (!ok || fieldName.trimmed().isEmpty()) return;
    fieldName = fieldName.trimmed();

    QStringList types = {"Stringa", "Numero (Double)", "Booleano", "Array"};
    QString fieldType = QInputDialog::getItem(this, "Tipo Campo", "Seleziona il tipo:", types, 0, false, &ok);
    if (!ok) return;

    QJsonObject rootObj    = componentsJsonDoc.object();
    QJsonObject sectionObj = rootObj.value(sectionKey).toObject();

    for (const QString &compKey : sectionObj.keys()) {
        QJsonObject compObj = sectionObj.value(compKey).toObject();
        if (!compObj.contains(fieldName)) {
            if (fieldType == "Stringa")              compObj[fieldName] = QString("");
            else if (fieldType == "Numero (Double)") compObj[fieldName] = 0.0;
            else if (fieldType == "Booleano")        compObj[fieldName] = false;
            else if (fieldType == "Array")           compObj[fieldName] = QJsonArray();
        }
        sectionObj[compKey] = compObj;
    }

    rootObj[sectionKey] = sectionObj;
    componentsJsonDoc.setObject(rootObj);
    populateComponentsTabs();
}

void MainWindow::deleteFieldFromSection(const QString &sectionKey, const QString &fieldName) {
    auto reply = QMessageBox::question(this, "Conferma Eliminazione",
        QString("Eliminare il campo \"%1\" da tutti i componenti in \"%2\"?").arg(fieldName, sectionKey),
        QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) return;

    QJsonObject rootObj    = componentsJsonDoc.object();
    QJsonObject sectionObj = rootObj.value(sectionKey).toObject();

    for (const QString &compKey : sectionObj.keys()) {
        QJsonObject compObj = sectionObj.value(compKey).toObject();
        compObj.remove(fieldName);
        sectionObj[compKey] = compObj;
    }

    rootObj[sectionKey] = sectionObj;
    componentsJsonDoc.setObject(rootObj);
    populateComponentsTabs();
}

void MainWindow::onSaveComponentsFile() {
    if (!componentsJsonDoc.isObject() || currentComponentsFilePath.isEmpty()) {
        QMessageBox::warning(this, "Attenzione", "Nessun file da salvare.");
        return;
    }
    
    // Chiedi conferma
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Conferma Salvataggio", 
        QString("Vuoi sovrascrivere il file:\n%1\n\ncon le modifiche apportate?").arg(currentComponentsFilePath),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Salva il file
    QFile file(currentComponentsFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile scrivere il file: " + currentComponentsFilePath);
        return;
    }
    
    QJsonDocument doc = componentsJsonDoc;
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    QMessageBox::information(this, "Successo", "File salvato con successo!");
}

void MainWindow::onLoadBuildingTypesFile() {
    // Seleziona il file CSV
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file CSV tipologie edilizie",
        QDir::homePath(),
        "File CSV (*.csv);;Tutti i file (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Apri e leggi il file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file: " + filePath);
        return;
    }
    
    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        lines.append(in.readLine());
    }
    file.close();
    
    if (lines.isEmpty()) {
        QMessageBox::warning(this, "Errore", "Il file CSV è vuoto.");
        return;
    }
    
    currentBuildingTypesFilePath = filePath;
    
    // Prima riga = intestazioni
    QStringList headers = lines[0].split(',');
    
    // Pulisci intestazioni
    for (int i = 0; i < headers.size(); i++) {
        headers[i] = headers[i].trimmed();
    }
    
    // Configura la tabella
    buildingTypesTable->clear();
    buildingTypesTable->setRowCount(lines.size() - 1);
    buildingTypesTable->setColumnCount(headers.size());
    buildingTypesTable->setHorizontalHeaderLabels(headers);
    
    // Popola la tabella con i dati
    for (int row = 1; row < lines.size(); row++) {
        QStringList values = lines[row].split(',');
        
        for (int col = 0; col < values.size() && col < headers.size(); col++) {
            QTableWidgetItem *item = new QTableWidgetItem(values[col].trimmed());
            item->setTextAlignment(Qt::AlignCenter);
            buildingTypesTable->setItem(row - 1, col, item);
        }
        
        // Riempi celle mancanti se la riga è più corta
        for (int col = values.size(); col < headers.size(); col++) {
            QTableWidgetItem *item = new QTableWidgetItem("");
            item->setTextAlignment(Qt::AlignCenter);
            buildingTypesTable->setItem(row - 1, col, item);
        }
    }
    
    // Adatta larghezza colonne al contenuto
    buildingTypesTable->resizeColumnsToContents();
    
    // Imposta larghezza minima per colonne strette
    for (int col = 0; col < buildingTypesTable->columnCount(); col++) {
        if (buildingTypesTable->columnWidth(col) < 100) {
            buildingTypesTable->setColumnWidth(col, 100);
        }
    }
    
    // Abilita i pulsanti
    addRowBtn->setEnabled(true);
    addColumnBtn->setEnabled(true);
    saveBuildingTypesBtn->setEnabled(true);
    
    QMessageBox::information(this, "Successo", 
        QString("File CSV caricato con successo!\n\n"
                "Righe: %1\n"
                "Colonne: %2")
        .arg(buildingTypesTable->rowCount())
        .arg(buildingTypesTable->columnCount()));
}

void MainWindow::onAddRow() {
    if (!buildingTypesTable) {
        return;
    }
    
    // Chiedi dove inserire la riga
    QStringList options;
    options << "Alla fine" << "Prima della riga selezionata";
    
    bool ok;
    QString choice = QInputDialog::getItem(this, "Aggiungi Riga", 
        "Dove vuoi inserire la nuova riga?", 
        options, 0, false, &ok);
    
    if (!ok) {
        return;
    }
    
    int insertRow = buildingTypesTable->rowCount();
    
    if (choice == "Prima della riga selezionata" && buildingTypesTable->currentRow() >= 0) {
        insertRow = buildingTypesTable->currentRow();
    }
    
    // Inserisci la riga
    buildingTypesTable->insertRow(insertRow);
    
    // Riempi con celle vuote
    for (int col = 0; col < buildingTypesTable->columnCount(); col++) {
        QTableWidgetItem *item = new QTableWidgetItem("");
        buildingTypesTable->setItem(insertRow, col, item);
    }
    
    // Seleziona la prima cella della nuova riga
    buildingTypesTable->setCurrentCell(insertRow, 0);
    
    QMessageBox::information(this, "Successo", 
        QString("Riga %1 aggiunta. Ricorda di salvare!").arg(insertRow + 1));
}

void MainWindow::onAddColumn() {
    if (!buildingTypesTable) {
        return;
    }
    
    // Chiedi il nome della colonna
    bool ok;
    QString columnName = QInputDialog::getText(this, "Aggiungi Colonna", 
        "Nome della nuova colonna:", 
        QLineEdit::Normal, "", &ok);
    
    if (!ok || columnName.isEmpty()) {
        return;
    }
    
    // Chiedi dove inserire la colonna
    QStringList options;
    options << "Alla fine" << "Prima della colonna selezionata";
    
    QString choice = QInputDialog::getItem(this, "Aggiungi Colonna", 
        "Dove vuoi inserire la nuova colonna?", 
        options, 0, false, &ok);
    
    if (!ok) {
        return;
    }
    
    int insertCol = buildingTypesTable->columnCount();
    
    if (choice == "Prima della colonna selezionata" && buildingTypesTable->currentColumn() >= 0) {
        insertCol = buildingTypesTable->currentColumn();
    }
    
    // Inserisci la colonna
    buildingTypesTable->insertColumn(insertCol);
    
    // Imposta l'intestazione
    QTableWidgetItem *headerItem = new QTableWidgetItem(columnName);
    buildingTypesTable->setHorizontalHeaderItem(insertCol, headerItem);
    
    // Riempi con celle vuote
    for (int row = 0; row < buildingTypesTable->rowCount(); row++) {
        QTableWidgetItem *item = new QTableWidgetItem("");
        buildingTypesTable->setItem(row, insertCol, item);
    }
    
    // Adatta larghezza
    buildingTypesTable->resizeColumnToContents(insertCol);
    if (buildingTypesTable->columnWidth(insertCol) < 100) {
        buildingTypesTable->setColumnWidth(insertCol, 100);
    }
    
    QMessageBox::information(this, "Successo", 
        QString("Colonna '%1' aggiunta. Ricorda di salvare!").arg(columnName));
}

void MainWindow::onDeleteRow() {
    if (!buildingTypesTable || buildingTypesTable->currentRow() < 0) {
        QMessageBox::warning(this, "Attenzione", "Seleziona una riga da eliminare.");
        return;
    }
    
    int row = buildingTypesTable->currentRow();
    
    // Chiedi conferma
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Conferma Eliminazione", 
        QString("Vuoi eliminare la riga %1?").arg(row + 1),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    buildingTypesTable->removeRow(row);
    
    QMessageBox::information(this, "Successo", 
        QString("Riga %1 eliminata. Ricorda di salvare!").arg(row + 1));
}

void MainWindow::onDeleteColumn() {
    if (!buildingTypesTable || buildingTypesTable->currentColumn() < 0) {
        QMessageBox::warning(this, "Attenzione", "Seleziona una colonna da eliminare.");
        return;
    }
    
    int col = buildingTypesTable->currentColumn();
    QString columnName = buildingTypesTable->horizontalHeaderItem(col) ? 
        buildingTypesTable->horizontalHeaderItem(col)->text() : QString("Colonna %1").arg(col + 1);
    
    // Chiedi conferma
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Conferma Eliminazione", 
        QString("Vuoi eliminare la colonna '%1'?").arg(columnName),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    buildingTypesTable->removeColumn(col);
    
    QMessageBox::information(this, "Successo", 
        QString("Colonna '%1' eliminata. Ricorda di salvare!").arg(columnName));
}

void MainWindow::onSaveBuildingTypesFile() {
    if (!buildingTypesTable || buildingTypesTable->rowCount() == 0) {
        QMessageBox::warning(this, "Attenzione", "Nessuna tabella da salvare.");
        return;
    }

    QString savePath = currentBuildingTypesFilePath;

    if (savePath.isEmpty()) {
        // Nessun file caricato: chiedi dove salvare
        savePath = QFileDialog::getSaveFileName(
            this,
            "Salva file CSV tipologie edilizie",
            QDir::homePath() + "/building_types.csv",
            "File CSV (*.csv);;Tutti i file (*)"
        );
        if (savePath.isEmpty()) return;
        currentBuildingTypesFilePath = savePath;
    } else {
        // Chiedi conferma sovrascrittura
        QMessageBox::StandardButton reply = QMessageBox::question(this, "Conferma Salvataggio",
            QString("Vuoi sovrascrivere il file:\n%1\n\ncon le modifiche apportate?").arg(savePath),
            QMessageBox::Yes | QMessageBox::No);
        if (reply != QMessageBox::Yes) return;
    }

    // Salva il file
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile scrivere il file: " + currentBuildingTypesFilePath);
        return;
    }
    
    QTextStream out(&file);
    
    // Scrivi intestazioni
    QStringList headers;
    for (int col = 0; col < buildingTypesTable->columnCount(); col++) {
        QString header = buildingTypesTable->horizontalHeaderItem(col) ? 
            buildingTypesTable->horizontalHeaderItem(col)->text() : QString("Column%1").arg(col + 1);
        headers.append(header);
    }
    out << headers.join(",") << "\n";
    
    // Scrivi dati
    for (int row = 0; row < buildingTypesTable->rowCount(); row++) {
        QStringList rowData;
        for (int col = 0; col < buildingTypesTable->columnCount(); col++) {
            QString value;
            QWidget *w = buildingTypesTable->cellWidget(row, col);
            if (auto *combo = qobject_cast<QComboBox*>(w)) {
                value = combo->currentText();
            } else {
                QTableWidgetItem *item = buildingTypesTable->item(row, col);
                value = item ? item->text() : "-";
            }
            if (value.isEmpty()) value = "-";
            rowData.append(value);
        }
        out << rowData.join(",") << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "Successo", "File CSV salvato con successo!");
}

void MainWindow::onLoadBuildingsFile() {
    // Seleziona il file JSON edifici
    QString filePath = QFileDialog::getOpenFileName(
        this,
        "Seleziona file JSON edifici",
        QDir::homePath(),
        "File JSON (*.json);;Tutti i file (*)"
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    // Apri e leggi il file
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile aprire il file: " + filePath);
        return;
    }
    
    QByteArray jsonData = file.readAll();
    file.close();
    
    // Parsa il JSON
    QJsonParseError parseError;
    buildingsJsonDoc = QJsonDocument::fromJson(jsonData, &parseError);
    
    if (parseError.error != QJsonParseError::NoError) {
        QMessageBox::critical(this, "Errore JSON", 
            QString("Errore nel parsing del JSON: %1").arg(parseError.errorString()));
        return;
    }
    
    if (!buildingsJsonDoc.isObject()) {
        QMessageBox::warning(this, "Errore", "Il file JSON non contiene un oggetto valido.");
        return;
    }
    
    currentBuildingsFilePath = filePath;
    
    // Estrai edifici - "Buildings" è un OGGETTO che contiene edifici come proprietà
    QJsonObject rootObj = buildingsJsonDoc.object();
    QJsonObject buildingsObj = rootObj.value("Buildings").toObject();
    
    if (buildingsObj.isEmpty()) {
        QMessageBox::warning(this, "Errore", "Nessun edificio trovato. Verificare che ci sia l'oggetto 'Buildings'.");
        return;
    }
    
    // Estrai le chiavi degli edifici (b0, b1, b2, etc.)
    QStringList buildingKeys = buildingsObj.keys();
    
    if (buildingKeys.isEmpty()) {
        QMessageBox::warning(this, "Errore", "L'oggetto 'Buildings' è vuoto.");
        return;
    }
    
    // Popola il combo box
    buildingsComboBox->clear();
    
    int index = 0;
    for (const QString &buildingKey : buildingKeys) {
        QJsonObject building = buildingsObj.value(buildingKey).toObject();
        QJsonObject siteLocation = building.value("SITE:LOCATION").toObject();
        
        // SITE:LOCATION contiene un oggetto con il nome edificio (es: "BuildingLocationName")
        // Estrai il primo (e unico) oggetto
        QJsonObject locationData;
        QString locationKey;
        if (!siteLocation.isEmpty()) {
            locationKey = siteLocation.keys().first();
            locationData = siteLocation.value(locationKey).toObject();
        }
        
        QString buildingCode = locationData.value("BuildingCode").toString(buildingKey);
        double latitude = locationData.value("Latitude").toDouble(0.0);
        double longitude = locationData.value("Longitude").toDouble(0.0);
        
        QString displayText = QString("%1 - Lat: %2, Lon: %3")
            .arg(buildingCode)
            .arg(latitude, 0, 'f', 6)
            .arg(longitude, 0, 'f', 6);
        
        buildingsComboBox->addItem(displayText, index);
        index++;
    }
    
    // Aggiorna label
    buildingsCountLabel->setText(QString("📊 Numero edifici: %1").arg(buildingKeys.size()));
    
    // Salva i dati globali per use successivo
    currentBuildingIndex = 0;
    
    // Abilita combo box
    buildingsComboBox->setEnabled(true);
    
    // Carica il primo edificio
    buildingsComboBox->setCurrentIndex(0);
    
    // Abilita pulsanti
    addSurfaceBtn->setEnabled(true);
    addZoneBtn->setEnabled(true);
    saveBuildingsBtn->setEnabled(true);
    
    QMessageBox::information(this, "Successo", 
        QString("File edifici caricato con successo!\n\nEdifici totali: %1").arg(buildingKeys.size()));
}

void MainWindow::onBuildingSelectionChanged(int index) {
    if (index < 0 || buildingsJsonDoc.isEmpty()) {
        return;
    }
    
    QJsonObject rootObj = buildingsJsonDoc.object();
    QJsonObject buildingsObj = rootObj.value("Buildings").toObject();
    
    if (buildingsObj.isEmpty()) {
        return;
    }
    
    // Ottieni l'edificio usando l'indice dalla lista di chiavi
    QStringList buildingKeys = buildingsObj.keys();
    if (index >= buildingKeys.size()) {
        return;
    }
    
    currentBuildingIndex = index;
    QString buildingKey = buildingKeys[index];
    QJsonObject building = buildingsObj.value(buildingKey).toObject();
    
    // Carica SITE:LOCATION - è un oggetto che contiene un singolo oggetto
    QJsonObject siteLocation = building.value("SITE:LOCATION").toObject();
    QJsonObject location;
    
    if (!siteLocation.isEmpty()) {
        QString locationKey = siteLocation.keys().first();
        location = siteLocation.value(locationKey).toObject();
    }
    
    QString locationInfo;
    locationInfo += "SITE:LOCATION\n";
    locationInfo += "════════════════════════════════\n\n";
    
    for (const QString &key : location.keys()) {
        QJsonValue val = location.value(key);
        QString valueStr;
        
        if (val.isDouble()) {
            valueStr = QString::number(val.toDouble(), 'f', 6);
        } else if (val.isString()) {
            valueStr = val.toString();
        } else if (val.isBool()) {
            valueStr = val.toBool() ? "true" : "false";
        } else if (val.isNull()) {
            valueStr = "null";
        } else {
            // Per altri tipi, usa rappresentazione JSON
            QJsonDocument doc;
            if (val.isArray()) {
                doc = QJsonDocument(val.toArray());
            } else if (val.isObject()) {
                doc = QJsonDocument(val.toObject());
            }
            valueStr = doc.toJson(QJsonDocument::Compact);
        }
        
        locationInfo += key + ": " + valueStr + "\n";
    }
    
    siteLocationText->setPlainText(locationInfo);
    
    // Carica BuildingSurface
    populateBuildingSurfacesTable(building);
    
    // Carica Zones
    populateZonesTable(building);
}

void MainWindow::populateBuildingSurfacesTable(const QJsonObject &building) {
    // BuildingSurface è un OGGETTO che contiene oggetti, non un array
    QJsonObject surfacesObj = building.value("BuildingSurface").toObject();
    
    buildingSurfacesTable->clear();
    buildingSurfacesTable->setRowCount(0);
    
    if (surfacesObj.isEmpty()) {
        buildingSurfacesTable->setColumnCount(1);
        buildingSurfacesTable->setHorizontalHeaderLabels(QStringList() << "Nome Superficie");
        return;
    }
    
    // Estrai tutte le chiavi e proprietà possibili
    QSet<QString> allKeys;
    allKeys.insert("Nome");  // Aggiungi sempre il nome
    
    QStringList surfaceNames = surfacesObj.keys();
    for (const QString &surfName : surfaceNames) {
        QJsonValue surfVal = surfacesObj.value(surfName);
        if (surfVal.isObject()) {
            QJsonObject surf = surfVal.toObject();
            for (const QString &key : surf.keys()) {
                allKeys.insert(key);
            }
        }
    }
    
    QStringList headers = allKeys.values();
    headers.sort();
    headers.removeAll("Nome");
    headers.prepend("Nome");  // Nome sempre primo
    
    buildingSurfacesTable->setColumnCount(headers.size());
    buildingSurfacesTable->setHorizontalHeaderLabels(headers);
    
    // Popola righe
    int row = 0;
    for (const QString &surfName : surfaceNames) {
        buildingSurfacesTable->insertRow(row);
        
        QJsonObject surface = surfacesObj.value(surfName).toObject();
        
        // Nome superficie (prima colonna)
        QTableWidgetItem *nameItem = new QTableWidgetItem(surfName);
        buildingSurfacesTable->setItem(row, 0, nameItem);
        
        // Altre proprietà
        for (int col = 1; col < headers.size(); col++) {
            QJsonValue val = surface.value(headers[col]);
            
            QString valueStr;
            if (val.isDouble()) {
                valueStr = QString::number(val.toDouble(), 'f', 2);
            } else if (val.isString()) {
                valueStr = val.toString();
            } else if (val.isBool()) {
                valueStr = val.toBool() ? "true" : "false";
            } else {
                valueStr = "";
            }
            
            QTableWidgetItem *item = new QTableWidgetItem(valueStr);
            buildingSurfacesTable->setItem(row, col, item);
        }
        
        row++;
    }
    
    buildingSurfacesTable->resizeColumnsToContents();
}

void MainWindow::populateZonesTable(const QJsonObject &building) {
    // Zone è un OGGETTO che contiene oggetti, non un array
    QJsonObject zonesObj = building.value("Zone").toObject();
    
    zonesTable->clear();
    zonesTable->setRowCount(0);
    
    if (zonesObj.isEmpty()) {
        zonesTable->setColumnCount(1);
        zonesTable->setHorizontalHeaderLabels(QStringList() << "Nome Zona");
        return;
    }
    
    // Estrai tutte le chiavi e proprietà possibili
    QSet<QString> allKeys;
    allKeys.insert("Nome");  // Aggiungi sempre il nome
    
    QStringList zoneNames = zonesObj.keys();
    for (const QString &zoneName : zoneNames) {
        QJsonValue zoneVal = zonesObj.value(zoneName);
        if (zoneVal.isObject()) {
            QJsonObject zone = zoneVal.toObject();
            for (const QString &key : zone.keys()) {
                // Salta le proprietà complesse
                QJsonValue val = zone.value(key);
                if (!val.isArray() && !val.isObject()) {
                    allKeys.insert(key);
                }
            }
        }
    }
    
    QStringList headers = allKeys.values();
    headers.sort();
    headers.removeAll("Nome");
    headers.prepend("Nome");  // Nome sempre primo
    
    zonesTable->setColumnCount(headers.size());
    zonesTable->setHorizontalHeaderLabels(headers);
    
    // Popola righe
    int row = 0;
    for (const QString &zoneName : zoneNames) {
        zonesTable->insertRow(row);
        
        QJsonObject zone = zonesObj.value(zoneName).toObject();
        
        // Nome zona (prima colonna)
        QTableWidgetItem *nameItem = new QTableWidgetItem(zoneName);
        zonesTable->setItem(row, 0, nameItem);
        
        // Altre proprietà
        for (int col = 1; col < headers.size(); col++) {
            QJsonValue val = zone.value(headers[col]);
            
            QString valueStr;
            if (val.isDouble()) {
                valueStr = QString::number(val.toDouble(), 'f', 2);
            } else if (val.isString()) {
                valueStr = val.toString();
            } else if (val.isBool()) {
                valueStr = val.toBool() ? "true" : "false";
            } else {
                valueStr = "";
            }
            
            QTableWidgetItem *item = new QTableWidgetItem(valueStr);
            zonesTable->setItem(row, col, item);
        }
        
        row++;
    }
    
    zonesTable->resizeColumnsToContents();
}

void MainWindow::onAddBuildingSurface() {
    if (buildingSurfacesTable->columnCount() == 0) {
        QMessageBox::warning(this, "Attenzione", "Carica prima un edificio.");
        return;
    }
    
    // Chiedi il nome della nuova superficie
    bool ok;
    QString surfaceName = QInputDialog::getText(this, "Nuova Superficie", 
        "Inserisci il nome della superficie:", 
        QLineEdit::Normal, "", &ok);
    
    if (!ok || surfaceName.isEmpty()) {
        return;
    }
    
    // Aggiungi una nuova riga
    int newRow = buildingSurfacesTable->rowCount();
    buildingSurfacesTable->insertRow(newRow);
    
    // Riempi con celle vuote
    for (int col = 0; col < buildingSurfacesTable->columnCount(); col++) {
        QTableWidgetItem *item = new QTableWidgetItem("");
        buildingSurfacesTable->setItem(newRow, col, item);
    }
    
    // Imposta il nome nella prima colonna
    if (buildingSurfacesTable->columnCount() > 0) {
        buildingSurfacesTable->item(newRow, 0)->setText(surfaceName);
    }
    
    QMessageBox::information(this, "Successo", 
        QString("Superficie '%1' aggiunta. Ricorda di salvare!").arg(surfaceName));
}

void MainWindow::onAddZone() {
    if (zonesTable->columnCount() == 0) {
        QMessageBox::warning(this, "Attenzione", "Carica prima un edificio.");
        return;
    }
    
    // Chiedi il nome della nuova zona
    bool ok;
    QString zoneName = QInputDialog::getText(this, "Nuova Zona", 
        "Inserisci il nome della zona:", 
        QLineEdit::Normal, "", &ok);
    
    if (!ok || zoneName.isEmpty()) {
        return;
    }
    
    // Aggiungi una nuova riga
    int newRow = zonesTable->rowCount();
    zonesTable->insertRow(newRow);
    
    // Riempi con celle vuote
    for (int col = 0; col < zonesTable->columnCount(); col++) {
        QTableWidgetItem *item = new QTableWidgetItem("");
        zonesTable->setItem(newRow, col, item);
    }
    
    // Imposta il nome nella prima colonna
    if (zonesTable->columnCount() > 0) {
        zonesTable->item(newRow, 0)->setText(zoneName);
    }
    
    QMessageBox::information(this, "Successo", 
        QString("Zona '%1' aggiunta. Ricorda di salvare!").arg(zoneName));
}

void MainWindow::onSaveBuildingsFile() {
    if (buildingsJsonDoc.isEmpty() || currentBuildingsFilePath.isEmpty() || currentBuildingIndex < 0) {
        QMessageBox::warning(this, "Attenzione", "Nessun edificio da salvare.");
        return;
    }
    
    // Chiedi conferma
    QMessageBox::StandardButton reply = QMessageBox::question(this, "Conferma Salvataggio", 
        QString("Vuoi sovrascrivere il file:\n%1\n\ncon le modifiche apportate?").arg(currentBuildingsFilePath),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) {
        return;
    }
    
    // Aggiorna i dati nel JSON
    QJsonObject rootObj = buildingsJsonDoc.object();
    QJsonObject buildingsObj = rootObj.value("Buildings").toObject();
    
    if (buildingsObj.isEmpty()) {
        QMessageBox::warning(this, "Errore", "Edifici non trovati nel JSON.");
        return;
    }
    
    QStringList buildingKeys = buildingsObj.keys();
    if (currentBuildingIndex >= buildingKeys.size()) {
        QMessageBox::warning(this, "Errore", "Indice edificio non valido.");
        return;
    }
    
    QString buildingKey = buildingKeys[currentBuildingIndex];
    QJsonObject building = buildingsObj.value(buildingKey).toObject();
    
    // Aggiorna BuildingSurface - è un OGGETTO non un array
    QJsonObject surfacesObj;
    for (int row = 0; row < buildingSurfacesTable->rowCount(); row++) {
        QString surfaceName = buildingSurfacesTable->item(row, 0)->text();
        QJsonObject surface;
        
        for (int col = 0; col < buildingSurfacesTable->columnCount(); col++) {
            QString header = buildingSurfacesTable->horizontalHeaderItem(col)->text();
            QString value = buildingSurfacesTable->item(row, col)->text();
            
            if (header == "Nome") continue;  // Salta il nome
            
            // Prova a convertire in numero se possibile
            bool isNumber;
            double numValue = value.toDouble(&isNumber);
            
            if (isNumber) {
                surface[header] = numValue;
            } else {
                surface[header] = value;
            }
        }
        surfacesObj[surfaceName] = surface;
    }
    building["BuildingSurface"] = surfacesObj;
    
    // Aggiorna Zone - è un OGGETTO non un array
    QJsonObject zonesObj;
    for (int row = 0; row < zonesTable->rowCount(); row++) {
        QString zoneName = zonesTable->item(row, 0)->text();
        QJsonObject zone;
        
        for (int col = 0; col < zonesTable->columnCount(); col++) {
            QString header = zonesTable->horizontalHeaderItem(col)->text();
            QString value = zonesTable->item(row, col)->text();
            
            if (header == "Nome") continue;  // Salta il nome
            
            // Prova a convertire in numero se possibile
            bool isNumber;
            double numValue = value.toDouble(&isNumber);
            
            if (isNumber) {
                zone[header] = numValue;
            } else {
                zone[header] = value;
            }
        }
        zonesObj[zoneName] = zone;
    }
    building["Zone"] = zonesObj;
        
        // Aggiorna l'edificio nell'oggetto
        buildingsObj[buildingKey] = building;
        rootObj["Buildings"] = buildingsObj;
        buildingsJsonDoc.setObject(rootObj);
    
    // Salva il file
    QFile file(currentBuildingsFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "Errore", "Impossibile scrivere il file: " + currentBuildingsFilePath);
        return;
    }
    
    file.write(buildingsJsonDoc.toJson(QJsonDocument::Indented));
    file.close();
    
    QMessageBox::information(this, "Successo", "File edifici salvato con successo!");
}

QString MainWindow::getBinaryPath(const QString &binaryName) {
    // Percorso della directory bin relativa al progetto
    QString binPath = QCoreApplication::applicationDirPath() + "/../bin/" + binaryName;
    QFileInfo fileInfo(binPath);
    
    if (fileInfo.exists() && fileInfo.isExecutable()) {
        return fileInfo.absoluteFilePath();
    }
    
    // Fallback: cerca nella directory del progetto
    binPath = QCoreApplication::applicationDirPath() + "/" + binaryName;
    fileInfo.setFile(binPath);
    if (fileInfo.exists() && fileInfo.isExecutable()) {
        return fileInfo.absoluteFilePath();
    }
    
    // Se non trovato, ritorna il percorso di default
    return binaryName;
}

void MainWindow::onXRangeChanged() {
    if (!xMinSpinBox || !xMaxSpinBox) {
        return;
    }
    
    int xMin = xMinSpinBox->value();
    int xMax = xMaxSpinBox->value();
    
    // Valida che xMax sia almeno 10 maggiore di xMin
    if (xMax < xMin + 10) {
        // Disconnetti temporaneamente per evitare loop
        disconnect(xMaxSpinBox, nullptr, this, nullptr);
        xMaxSpinBox->setValue(xMin + 10);
        connect(xMaxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
                this, &MainWindow::onXRangeChanged);
        xMax = xMin + 10;
    }
    
    // Aggiorna il massimo di xMinSpinBox
    disconnect(xMinSpinBox, nullptr, this, nullptr);
    xMinSpinBox->setMaximum(xMax - 10);
    connect(xMinSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onXRangeChanged);
    
    // Aggiorna il minimo di xMaxSpinBox
    disconnect(xMaxSpinBox, nullptr, this, nullptr);
    xMaxSpinBox->setMinimum(xMin + 10);
    connect(xMaxSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &MainWindow::onXRangeChanged);
    
    // Aggiorna tutti i grafici con il nuovo range
    updateAllChartsRange(xMin, xMax);
}

void MainWindow::updateAllChartsRange(int xMin, int xMax) {
    // Aggiorna grafico temperatura
    if (temperatureChartView && temperatureChartView->chart()->series().size() > 0) {
        QChart *chart = temperatureChartView->chart();
        QAbstractAxis *axisX = chart->axes(Qt::Horizontal).first();
        if (axisX) {
            axisX->setRange(xMin, xMax);
        }
    }
    
    // Aggiorna grafico umidità
    if (humidityChartView && humidityChartView->chart()->series().size() > 0) {
        QChart *chart = humidityChartView->chart();
        QAbstractAxis *axisX = chart->axes(Qt::Horizontal).first();
        if (axisX) {
            axisX->setRange(xMin, xMax);
        }
    }
    
    // Aggiorna grafico radiazione
    if (radiationChartView && radiationChartView->chart()->series().size() > 0) {
        QChart *chart = radiationChartView->chart();
        QAbstractAxis *axisX = chart->axes(Qt::Horizontal).first();
        if (axisX) {
            axisX->setRange(xMin, xMax);
        }
    }
}

QString MainWindow::formatJsonProperties(const QJsonObject &properties) {
    QString formatted = "PROPRIETÀ POLIGONO\n";
    formatted += "══════════════════════════════════════════\n\n";
    
    QStringList keys = properties.keys();
    keys.sort();
    
    for (const QString &key : keys) {
        QJsonValue value = properties.value(key);
        
        formatted += key + ": ";
        
        if (value.isDouble()) {
            // Formatta numeri con 1 decimale
            formatted += QString::number(value.toDouble(), 'f', 1);
        } else if (value.isString()) {
            formatted += value.toString();
        } else if (value.isBool()) {
            formatted += value.toBool() ? "true" : "false";
        } else if (value.isNull()) {
            formatted += "null";
        } else {
            // Per altri tipi, usa rappresentazione JSON
            QJsonDocument doc;
            if (value.isArray()) {
                doc = QJsonDocument(value.toArray());
            } else if (value.isObject()) {
                doc = QJsonDocument(value.toObject());
            }
            formatted += doc.toJson(QJsonDocument::Compact);
        }
        
        formatted += "\n";
    }
    
    return formatted;
}

void MainWindow::updateRadiationChart() {
    if (!radiationChartView || loadedData.isEmpty() || loadedHeaders.isEmpty()) {
        return;
    }
    
    QChart *chart = radiationChartView->chart();
    chart->removeAllSeries();
    
    // Rimuovi tutti gli assi esistenti
    for (auto axis : chart->axes()) {
        chart->removeAxis(axis);
    }
    
    // Colori per le diverse serie
    QVector<QColor> colors = {
        QColor("#e67e22"),  // Arancione per colonna 10
        QColor("#f39c12"),  // Giallo per colonna 11
        QColor("#d35400"),  // Arancione scuro per extra 1
        QColor("#c0392b")   // Rosso per extra 2
    };
    
    double minVal = 1e10;
    double maxVal = -1e10;
    int dataSize = 0;
    QList<QLineSeries*> allSeries;
    
    // Aggiungi sempre colonne 10 e 11 (indici 9 e 10)
    if (loadedData.size() > 9 && !loadedData[9].isEmpty()) {
        QLineSeries *series = new QLineSeries();
        series->setName(QString("Col 10: %1").arg(loadedHeaders[9]));
        series->setColor(colors[0]);
        series->append(loadedData[9]);
        allSeries.append(series);
        
        dataSize = loadedData[9].size();
        
        // Calcola min/max
        for (const QPointF &p : loadedData[9]) {
            if (p.y() < minVal) minVal = p.y();
            if (p.y() > maxVal) maxVal = p.y();
        }
    }
    
    if (loadedData.size() > 10 && !loadedData[10].isEmpty()) {
        QLineSeries *series = new QLineSeries();
        series->setName(QString("Col 11: %1").arg(loadedHeaders[10]));
        series->setColor(colors[1]);
        series->append(loadedData[10]);
        allSeries.append(series);
        
        // Calcola min/max
        for (const QPointF &p : loadedData[10]) {
            if (p.y() < minVal) minVal = p.y();
            if (p.y() > maxVal) maxVal = p.y();
        }
    }
    
    // Aggiungi colonne extra dai menu a tendina
    if (radiationCombo1 && radiationCombo1->currentIndex() > 0) {
        int colIndex = radiationCombo1->currentData().toInt();
        if (colIndex >= 0 && colIndex < loadedData.size() && !loadedData[colIndex].isEmpty()) {
            QLineSeries *series = new QLineSeries();
            series->setName(QString("Col %1: %2").arg(colIndex + 1).arg(loadedHeaders[colIndex]));
            series->setColor(colors[2]);
            series->append(loadedData[colIndex]);
            allSeries.append(series);
            
            // Calcola min/max
            for (const QPointF &p : loadedData[colIndex]) {
                if (p.y() < minVal) minVal = p.y();
                if (p.y() > maxVal) maxVal = p.y();
            }
        }
    }
    
    if (radiationCombo2 && radiationCombo2->currentIndex() > 0) {
        int colIndex = radiationCombo2->currentData().toInt();
        if (colIndex >= 0 && colIndex < loadedData.size() && !loadedData[colIndex].isEmpty()) {
            QLineSeries *series = new QLineSeries();
            series->setName(QString("Col %1: %2").arg(colIndex + 1).arg(loadedHeaders[colIndex]));
            series->setColor(colors[3]);
            series->append(loadedData[colIndex]);
            allSeries.append(series);
            
            // Calcola min/max
            for (const QPointF &p : loadedData[colIndex]) {
                if (p.y() < minVal) minVal = p.y();
                if (p.y() > maxVal) maxVal = p.y();
            }
        }
    }
    
    // Se ci sono serie da visualizzare
    if (!allSeries.isEmpty()) {
        // Crea gli assi UNA SOLA VOLTA
        QValueAxis *axisX = new QValueAxis();
        QValueAxis *axisY = new QValueAxis();
        
        axisX->setTitleText("Indice dato");
        axisY->setTitleText("Radiazione (W/m²)");
        axisX->setLabelFormat("%i");
        axisY->setLabelFormat("%.0f");
        
        // Aggiungi gli assi al grafico
        chart->addAxis(axisX, Qt::AlignBottom);
        chart->addAxis(axisY, Qt::AlignLeft);
        
        // Aggiungi tutte le serie e attacca gli assi
        for (QLineSeries *series : allSeries) {
            chart->addSeries(series);
            series->attachAxis(axisX);
            series->attachAxis(axisY);
        }
        
        // Imposta il range degli assi
        int xMin = (xMinSpinBox && xMinSpinBox->isEnabled()) ? xMinSpinBox->value() : dataSize;
        int xMax = (xMaxSpinBox && xMaxSpinBox->isEnabled()) ? xMaxSpinBox->value() : dataSize;
        
        axisX->setRange(xMin, xMax);
        
        // Aggiungi un margine del 10% per il range Y
        double margin = (maxVal - minVal) * 0.1;
        if (margin < 1) margin = 10;
        axisY->setRange(minVal - margin, maxVal + margin);
    }
}

// Required for Q_OBJECT classes defined in .cpp files (CMake AUTOMOC)
#include "mainwindow.moc"

void MainWindow::onRadiationComboChanged() {
    updateRadiationChart();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Simulation Results & Resilience Indicators – page creation
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::createSimulationResultsPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background-color: #ffffff;");

    QHBoxLayout *outerLayout = new QHBoxLayout(page);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Inner scrollable widget for all content
    QScrollArea *innerScroll = new QScrollArea();
    innerScroll->setWidgetResizable(true);
    innerScroll->setFrameShape(QFrame::NoFrame);
    innerScroll->setStyleSheet("QScrollArea { background: #ffffff; border: none; }");

    QWidget *innerWidget = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(innerWidget);
    layout->setContentsMargins(40, 40, 40, 40);
    layout->setSpacing(18);
    innerScroll->setWidget(innerWidget);
    outerLayout->addWidget(innerScroll);

    // ── Shared styles ─────────────────────────────────────────────────────────
    const QString groupStyle =
        "QGroupBox { font-size: 14px; font-weight: bold; color: #2c3e50; "
        "border: 1px solid #bdc3c7; border-radius: 6px; margin-top: 10px; padding: 12px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }";
    const QString subGroupStyle =
        "QGroupBox { font-size: 12px; font-weight: bold; color: #2c3e50; "
        "border: 1px solid #dcdde1; border-radius: 4px; margin-top: 8px; padding: 8px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 8px; padding: 0 4px; }";
    const QString btnPrimary =
        "QPushButton { background-color: #3498db; color: white; border: none; "
        "border-radius: 4px; padding: 0 16px; font-size: 13px; }"
        "QPushButton:hover { background-color: #2980b9; }"
        "QPushButton:pressed { background-color: #21618c; }";
    const QString fileLabelStyle =
        "background-color: #f8f9fa; border: 1px solid #dcdde1; border-radius: 4px; "
        "padding: 8px; color: #7f8c8d; font-size: 12px; font-style: italic;";
    const QString resultValueStyle =
        "background-color: #ecf0f1; border: 1px solid #bdc3c7; border-radius: 4px; "
        "padding: 4px 10px; color: #2c3e50; font-size: 13px; font-weight: bold; "
        "min-width: 130px;";

    // Helper: create a standard QDoubleSpinBox
    auto makeDSpin = [](double val, double lo, double hi, double step, int dec) {
        QDoubleSpinBox *s = new QDoubleSpinBox();
        s->setRange(lo, hi);
        s->setSingleStep(step);
        s->setDecimals(dec);
        s->setValue(val);
        s->setFixedHeight(30);
        s->setMinimumWidth(90);
        return s;
    };

    // ── Title ─────────────────────────────────────────────────────────────────
    QLabel *title = new QLabel(
        "<h1 style='color: #2c3e50;'>📈 Risultati Simulazione &amp; Indicatori di Resilienza</h1>");
    QLabel *desc  = new QLabel(
        "Carica il file CSV di output della simulazione per visualizzare le serie temporali e "
        "calcolare gli indicatori di resilienza termica NEST WP5 (rif. Peri et al., "
        "Energy and Buildings, 2025). Il file deve contenere le colonne: "
        "<b>timestamp, T_out, x_ear</b>, e per ogni zona termica "
        "<b>&lt;zona&gt;_T_op, _T_air, _x_iar, _Phi_C, _Phi_H, _T_surf, _T_surf_dev</b>.");
    desc->setWordWrap(true);
    desc->setStyleSheet("color: #7f8c8d; font-size: 13px;");
    layout->addWidget(title);
    layout->addWidget(desc);

    // ── Group 1: File loading ──────────────────────────────────────────────────
    QGroupBox *fileGroup = new QGroupBox("📂 Caricamento File CSV");
    fileGroup->setStyleSheet(groupStyle);
    QVBoxLayout *fileLayout = new QVBoxLayout(fileGroup);
    fileLayout->setSpacing(10);

    auto makeFileRow = [&](const QString &labelText, QLabel *&pathLabel,
                           const QString &btnText, auto slot) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(12);
        QLabel *lbl = new QLabel(labelText);
        lbl->setMinimumWidth(260);
        lbl->setStyleSheet("color: #2c3e50; font-size: 13px; font-weight: 600;");
        pathLabel = new QLabel("Nessun file selezionato");
        pathLabel->setMinimumWidth(280);
        pathLabel->setWordWrap(true);
        pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pathLabel->setStyleSheet(fileLabelStyle);
        QPushButton *btn = new QPushButton(btnText);
        btn->setFixedHeight(36);
        btn->setMinimumWidth(220);
        btn->setStyleSheet(btnPrimary);
        connect(btn, &QPushButton::clicked, this, slot);
        row->addWidget(lbl);
        row->addWidget(pathLabel, 1);
        row->addWidget(btn);
        fileLayout->addLayout(row);
    };

    makeFileRow("File simulazione principale:",
                srMainFileLabel,      "📂 Carica CSV principale",
                &MainWindow::onLoadSimResultsFile);
    makeFileRow("File riferimento (opzionale):",
                srRefFileLabel,       "📂 Carica CSV riferimento",
                &MainWindow::onLoadSimResultsRefFile);
    makeFileRow("File occupazione (opzionale):",
                srOccupancyFileLabel, "📂 Carica CSV occupazione",
                &MainWindow::onLoadSimResultsOccupancyFile);

    srDataInfoLabel = new QLabel("Nessun dato caricato.");
    srDataInfoLabel->setStyleSheet(
        "color: #7f8c8d; font-size: 12px; font-style: italic; padding: 4px;");
    fileLayout->addWidget(srDataInfoLabel);
    layout->addWidget(fileGroup);

    // ── Group 2: Parameters ───────────────────────────────────────────────────
    QGroupBox *paramGroup = new QGroupBox("⚙️ Parametri di Calcolo");
    paramGroup->setStyleSheet(groupStyle);
    QVBoxLayout *paramOuterLayout = new QVBoxLayout(paramGroup);

    QHBoxLayout *paramRow = new QHBoxLayout();
    paramRow->setSpacing(20);
    paramRow->setAlignment(Qt::AlignTop);

    // Sub-group: Soglie di comfort
    QGroupBox *comfortSub = new QGroupBox("Soglie di comfort");
    comfortSub->setStyleSheet(subGroupStyle);
    QFormLayout *comfortForm = new QFormLayout(comfortSub);
    comfortForm->setSpacing(6);
    srTComfortSpin  = makeDSpin(26.0,   10.0, 40.0,   0.5, 1);
    srTSetAlertSpin = makeDSpin(28.0,   10.0, 50.0,   0.5, 1);
    srTColdSafeSpin = makeDSpin(17.78, -10.0, 30.0,   0.1, 2);
    srTBaseSpin     = makeDSpin(26.0,   10.0, 40.0,   0.5, 1);
    srFloorAreaSpin = makeDSpin(150.0,   1.0, 1e6,   10.0, 1);
    srDtHSpin       = makeDSpin(1.0,     0.1, 24.0,   0.5, 2);
    comfortForm->addRow("T comfort – IOD/SBOI [°C]:", srTComfortSpin);
    comfortForm->addRow("T allerta SET [°C]:",         srTSetAlertSpin);
    comfortForm->addRow("T sicurezza freddo [°C]:",    srTColdSafeSpin);
    comfortForm->addRow("T base – AWD/SBOI [°C]:",    srTBaseSpin);
    comfortForm->addRow("Area condizionata [m²]:",     srFloorAreaSpin);
    comfortForm->addRow("Timestep [h]:",               srDtHSpin);
    paramRow->addWidget(comfortSub);

    // Sub-group: Impianti HVAC & fattori energetici
    QGroupBox *hvacSub = new QGroupBox("Impianti HVAC & Conversione Energetica");
    hvacSub->setStyleSheet(subGroupStyle);
    QFormLayout *hvacForm = new QFormLayout(hvacSub);
    hvacForm->setSpacing(6);
    srHeatSystemCombo = new QComboBox();
    srHeatSystemCombo->addItem("Caldaia a gas",                0);
    srHeatSystemCombo->addItem("Pompa di calore elettrica",    1);
    srHeatSystemCombo->setFixedHeight(30);
    srEtaHSpin     = makeDSpin(0.85, 0.1,  10.0, 0.05, 2);
    srCopCSpin     = makeDSpin(3.0,  0.5,  20.0, 0.1,  1);
    srEfGasSpin    = makeDSpin(0.202, 0.0,  5.0, 0.001, 3);
    srEfElecSpin   = makeDSpin(0.233, 0.0,  5.0, 0.001, 3);
    srElecConvSpin = makeDSpin(2.36,  0.1, 10.0, 0.01,  2);
    srGasConvSpin  = makeDSpin(1.0,   0.1, 10.0, 0.01,  2);
    hvacForm->addRow("Sistema di riscaldamento:", srHeatSystemCombo);
    hvacForm->addRow("η riscald. / COP_H [-]:",  srEtaHSpin);
    hvacForm->addRow("COP raffrescamento [-]:",  srCopCSpin);
    hvacForm->addRow("Emiss. gas [kg_CO₂/kWh]:", srEfGasSpin);
    hvacForm->addRow("Emiss. elett. [kg_CO₂/kWh]:", srEfElecSpin);
    hvacForm->addRow("Conv. en. prim. elett. [-]:", srElecConvSpin);
    hvacForm->addRow("Conv. en. prim. gas [-]:",    srGasConvSpin);
    paramRow->addWidget(hvacSub);

    // Sub-group: Parametri occupante SET
    QGroupBox *setSub = new QGroupBox("Occupante (SET)");
    setSub->setStyleSheet(subGroupStyle);
    QFormLayout *setForm = new QFormLayout(setSub);
    setForm->setSpacing(6);
    srMetSpin  = makeDSpin(1.2, 0.5, 5.0, 0.1,  1);
    srCloSpin  = makeDSpin(0.5, 0.0, 5.0, 0.1,  1);
    srVAirSpin = makeDSpin(0.1, 0.0, 5.0, 0.05, 2);
    setForm->addRow("Metabolismo [met]:",    srMetSpin);
    setForm->addRow("Abbigliamento [clo]:",  srCloSpin);
    setForm->addRow("Vel. aria interna [m/s]:", srVAirSpin);
    paramRow->addWidget(setSub);

    paramOuterLayout->addLayout(paramRow);
    layout->addWidget(paramGroup);

    // ── Compute button ────────────────────────────────────────────────────────
    QPushButton *computeBtn = new QPushButton("🧮  Calcola Indicatori di Resilienza");
    computeBtn->setFixedHeight(44);
    computeBtn->setStyleSheet(
        "QPushButton { background-color: #27ae60; color: white; border: none; "
        "border-radius: 5px; font-size: 14px; font-weight: bold; }"
        "QPushButton:hover  { background-color: #219a52; }"
        "QPushButton:pressed { background-color: #1a7a42; }"
        "QPushButton:disabled { background-color: #bdc3c7; color: #7f8c8d; }");
    connect(computeBtn, &QPushButton::clicked,
            this, &MainWindow::onComputeResilienceIndicators);
    layout->addWidget(computeBtn);

    srStatusLabel = new QLabel("");
    srStatusLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; font-style: italic;");
    layout->addWidget(srStatusLabel);

    // ── Group 3: Indicator results ────────────────────────────────────────────
    QGroupBox *resultsGroup = new QGroupBox("📊 Indicatori di Resilienza");
    resultsGroup->setStyleSheet(groupStyle);
    QHBoxLayout *resultsHLayout = new QHBoxLayout(resultsGroup);
    resultsHLayout->setSpacing(20);
    resultsHLayout->setAlignment(Qt::AlignTop);

    auto makeResLabel = [&]() {
        QLabel *lbl = new QLabel("—");
        lbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        lbl->setStyleSheet(resultValueStyle);
        return lbl;
    };

    auto addResultRow = [&](QFormLayout *form,
                            const QString &code, const QString &name,
                            QLabel *&valLabel) {
        valLabel = makeResLabel();
        QLabel *row = new QLabel(
            QString("<b style='color:#2c3e50;'>%1</b><br>"
                    "<small style='color:#7f8c8d;'>%2</small>")
            .arg(code, name));
        row->setWordWrap(true);
        row->setTextFormat(Qt::RichText);
        form->addRow(row, valLabel);
    };

    // Left: single-dataset indicators
    QWidget *leftColWidget = new QWidget();
    QVBoxLayout *leftColLayout = new QVBoxLayout(leftColWidget);
    leftColLayout->setContentsMargins(0, 0, 0, 0);
    leftColLayout->setSpacing(8);

    QGroupBox *singleSub = new QGroupBox("Dataset singolo");
    singleSub->setStyleSheet(subGroupStyle);
    QFormLayout *singleForm = new QFormLayout(singleSub);
    singleForm->setSpacing(8);
    addResultRow(singleForm, "In_I-B-2_03",
                 "Indoor Overheating Degree (IOD) [°C·h]", srResIOD);
    addResultRow(singleForm, "In_O-B-1_01",
                 "Ambient Warmness Degree (AWD) [°C·h]", srResAWD);
    addResultRow(singleForm, "In_I-A-1_01",
                 "Seasonal Overheating Index (SBOI) [%]", srResSBOI);
    addResultRow(singleForm, "In_O-B-2_03",
                 "Primary Energy [kWh/(m²·anno)]", srResPrimaryEnergy);
    addResultRow(singleForm, "In_I-B-2_11",
                 "Recovery time [h]", srResRecoveryTime);
    addResultRow(singleForm, "In_I-B-2_13",
                 "Absorptivity time [h]", srResAbsorptivityTime);
    leftColLayout->addWidget(singleSub);

    // Humidex sub-section
    QGroupBox *humSub = new QGroupBox("In_I-B-1_07 – Humidex Unmet Hours");
    humSub->setStyleSheet(subGroupStyle);
    QFormLayout *humForm = new QFormLayout(humSub);
    humForm->setSpacing(4);
    static const char *bandLabels[6] = {
        "Banda 0 – Nessun disagio (H < 27) [%]",
        "Banda 1 – Disagio percepibile (27–34) [%]",
        "Banda 2 – Disagio evidente (35–39) [%]",
        "Banda 3 – Disagio intenso (40–44) [%]",
        "Banda 4 – Disagio pericoloso (45–54) [%]",
        "Banda 5 – Colpo di calore (H ≥ 54) [%]"
    };
    for (int b = 0; b < 6; ++b) {
        srResHumidexBand[b] = makeResLabel();
        humForm->addRow(bandLabels[b], srResHumidexBand[b]);
    }
    srResHumidexUnmet = makeResLabel();
    humForm->addRow("Ore occ. con disagio (H ≥ 27) [%]:", srResHumidexUnmet);
    leftColLayout->addWidget(humSub);
    leftColLayout->addStretch();
    resultsHLayout->addWidget(leftColWidget, 1);

    // Right: two-dataset indicators
    QWidget *rightColWidget = new QWidget();
    QVBoxLayout *rightColLayout = new QVBoxLayout(rightColWidget);
    rightColLayout->setContentsMargins(0, 0, 0, 0);
    rightColLayout->setSpacing(8);

    QGroupBox *twoSub = new QGroupBox("Due dataset (principale + riferimento)");
    twoSub->setStyleSheet(subGroupStyle);
    QFormLayout *twoForm = new QFormLayout(twoSub);
    twoForm->setSpacing(8);
    addResultRow(twoForm, "In_O-B-2_02",
                 "Riduzione emissioni CO₂ [kg/anno]", srResCO2Reduction);
    addResultRow(twoForm, "In_O-B-3_07",
                 "Riduzione T sup. esterna [°C]", srResSurfTempReduction);
    addResultRow(twoForm, "In_I-A-2_01",
                 "Resilience Class Index (RCI) [-]", srResRCI);
    addResultRow(twoForm, "In_I-A-2_02",
                 "Thermal Resilience Index (TRI) [-]", srResTRI);
    addResultRow(twoForm, "In_I-B-2_08",
                 "Overheating Escalation Factor (aIOD) [-]", srResOEF);
    rightColLayout->addWidget(twoSub);

    QLabel *twoNote = new QLabel(
        "ℹ️  Richiede il caricamento del file CSV di riferimento.\n"
        "Per aIOD (OEF) i valori IOD/AWD di più scenari devono essere\n"
        "inseriti manualmente (v. documentazione).");
    twoNote->setWordWrap(true);
    twoNote->setStyleSheet(
        "color: #7f8c8d; font-size: 11px; font-style: italic; padding: 4px;");
    rightColLayout->addWidget(twoNote);
    rightColLayout->addStretch();
    resultsHLayout->addWidget(rightColWidget, 1);

    layout->addWidget(resultsGroup);

    // ── Group 4: Charts ───────────────────────────────────────────────────────
    QGroupBox *chartsGroup = new QGroupBox("📉 Grafici");
    chartsGroup->setStyleSheet(groupStyle);
    QVBoxLayout *chartsVLayout = new QVBoxLayout(chartsGroup);
    chartsVLayout->setSpacing(10);

    // Controls row
    QHBoxLayout *ctrlRow = new QHBoxLayout();
    ctrlRow->setSpacing(16);

    QLabel *zoneLbl = new QLabel("Zona termica:");
    zoneLbl->setStyleSheet("color: #2c3e50; font-size: 13px;");
    srZoneCombo = new QComboBox();
    srZoneCombo->setMinimumWidth(180);
    srZoneCombo->setFixedHeight(30);
    srZoneCombo->addItem("Nessun dato");
    srZoneCombo->setEnabled(false);

    QLabel *xMinLbl = new QLabel("Da ora:");
    xMinLbl->setStyleSheet("color: #2c3e50; font-size: 13px;");
    srXMinSpin = new QSpinBox();
    srXMinSpin->setRange(0, 8760);
    srXMinSpin->setValue(0);
    srXMinSpin->setFixedHeight(30);
    srXMinSpin->setEnabled(false);

    QLabel *xMaxLbl = new QLabel("A ora:");
    xMaxLbl->setStyleSheet("color: #2c3e50; font-size: 13px;");
    srXMaxSpin = new QSpinBox();
    srXMaxSpin->setRange(1, 8760);
    srXMaxSpin->setValue(8760);
    srXMaxSpin->setFixedHeight(30);
    srXMaxSpin->setEnabled(false);

    QPushButton *refreshBtn = new QPushButton("🔄 Aggiorna grafici");
    refreshBtn->setFixedHeight(30);
    refreshBtn->setStyleSheet(btnPrimary);
    connect(refreshBtn, &QPushButton::clicked,
            this, &MainWindow::onSimResultsRangeChanged);

    ctrlRow->addWidget(zoneLbl);
    ctrlRow->addWidget(srZoneCombo);
    ctrlRow->addStretch();
    ctrlRow->addWidget(xMinLbl);
    ctrlRow->addWidget(srXMinSpin);
    ctrlRow->addWidget(xMaxLbl);
    ctrlRow->addWidget(srXMaxSpin);
    ctrlRow->addWidget(refreshBtn);
    chartsVLayout->addLayout(ctrlRow);

    connect(srZoneCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &MainWindow::onSimResultsZoneChanged);

    // Chart tab widget
    srChartsTab = new QTabWidget();
    srChartsTab->setMinimumHeight(420);
    srChartsTab->setStyleSheet(
        "QTabWidget::pane  { border: 1px solid #bdc3c7; border-radius: 4px; }"
        "QTabBar::tab      { padding: 6px 14px; font-size: 12px; color: #2c3e50; }"
        "QTabBar::tab:selected { background: #3498db; color: white; border-radius: 3px; }");

    auto makeCV = [](const QColor &borderCol) {
        QChart *c = new QChart();
        c->setAnimationOptions(QChart::NoAnimation);
        c->legend()->setVisible(true);
        c->legend()->setAlignment(Qt::AlignBottom);
        QChartView *cv = new QChartView(c);
        cv->setRenderHint(QPainter::Antialiasing);
        cv->setStyleSheet(
            QString("QChartView { border: 2px solid %1; border-radius: 5px; }")
            .arg(borderCol.name()));
        return cv;
    };

    srTempChart    = makeCV(QColor("#e74c3c"));
    srHumChart     = makeCV(QColor("#3498db"));
    srEnergyChart  = makeCV(QColor("#f39c12"));
    srCO2Chart     = makeCV(QColor("#8e44ad"));
    srComfortChart = makeCV(QColor("#27ae60"));

    srChartsTab->addTab(srTempChart,    "🌡️  Temperature");
    srChartsTab->addTab(srHumChart,     "💧  Umidità");
    srChartsTab->addTab(srEnergyChart,  "⚡  Carichi Termici");
    srChartsTab->addTab(srCO2Chart,     "🌫️  CO₂");
    srChartsTab->addTab(srComfortChart, "🏠  Comfort (SET)");

    chartsVLayout->addWidget(srChartsTab);
    layout->addWidget(chartsGroup);
    layout->addStretch();

    contentArea->addWidget(page);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helper: extract zone names from CSV header
// ─────────────────────────────────────────────────────────────────────────────

QStringList MainWindow::srExtractZoneNames(const QString &filePath)
{
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return {};
    QTextStream ts(&f);
    const QString header = ts.readLine();
    const QStringList cols = header.split(',');

    QStringList names;
    for (const QString &col : cols) {
        const QString c = col.trimmed();
        if (c.endsWith("_T_op")) {
            const QString zname = c.left(c.length() - 5);
            if (!names.contains(zname))
                names.append(zname);
        }
    }
    // Legacy single-zone format (no prefix)
    if (names.isEmpty() && cols.contains("T_op"))
        names.append("Default");

    return names;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Chart update
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::updateSimResultsCharts()
{
    if (srMainData.empty()) return;

    const int total   = static_cast<int>(srMainData.size());
    int zIdx  = srZoneCombo->currentIndex();
    const int nZones  = static_cast<int>(srMainData.front().zones.size());
    if (zIdx < 0 || zIdx >= nZones) zIdx = 0;

    int xMin = qBound(0,        srXMinSpin->value(), total - 1);
    int xMax = qBound(xMin + 1, srXMaxSpin->value(), total);

    // Struct to describe one line series
    struct SeriesDef {
        QString name;
        QColor  color;
        std::function<double(int)> fn;
    };

    // Generic chart builder
    auto buildChart = [&](QChartView *cv,
                          const QString &title,
                          const QString &yTitle,
                          QList<SeriesDef> defs)
    {
        QChart *chart = cv->chart();
        chart->removeAllSeries();
        const auto axes = chart->axes();
        for (auto *ax : axes) chart->removeAxis(ax);
        chart->setTitle(title);

        double yMin =  1e15, yMax = -1e15;

        for (const SeriesDef &d : defs) {
            QLineSeries *s = new QLineSeries();
            s->setName(d.name);
            QPen pen(d.color);
            pen.setWidthF(1.5);
            s->setPen(pen);
            for (int i = xMin; i < xMax; ++i) {
                const double v = d.fn(i);
                s->append(i, v);
                yMin = std::min(yMin, v);
                yMax = std::max(yMax, v);
            }
            chart->addSeries(s);
        }

        QValueAxis *axX = new QValueAxis();
        axX->setTitleText("Ora [h]");
        axX->setLabelFormat("%d");
        axX->setRange(xMin, xMax);

        QValueAxis *axY = new QValueAxis();
        axY->setTitleText(yTitle);
        const double margin = std::max((yMax - yMin) * 0.05, 0.01);
        axY->setRange(yMin - margin, yMax + margin);

        chart->addAxis(axX, Qt::AlignBottom);
        chart->addAxis(axY, Qt::AlignLeft);
        for (QAbstractSeries *s : chart->series()) {
            s->attachAxis(axX);
            s->attachAxis(axY);
        }
        chart->legend()->setVisible(true);
        chart->legend()->setAlignment(Qt::AlignBottom);
    };

    // Convenience lambda to safely access per-zone field
    auto zVal = [&](int i, auto member) -> double {
        if (zIdx < static_cast<int>(srMainData[i].zones.size()))
            return srMainData[i].zones[zIdx].*member;
        return 0.0;
    };

    // ── Tab 1: Temperatures (all °C) ─────────────────────────────────────────
    buildChart(srTempChart, "Temperature", "Temperatura [°C]", {
        {"T_out [°C]",        QColor("#e74c3c"),
            [&](int i){ return srMainData[i].T_out; }},
        {"T_op zona [°C]",    QColor("#8e44ad"),
            [&](int i){ return zVal(i, &ZoneData::T_op); }},
        {"T_air zona [°C]",   QColor("#3498db"),
            [&](int i){ return zVal(i, &ZoneData::T_air); }},
        {"T_surf zona [°C]",  QColor("#27ae60"),
            [&](int i){ return zVal(i, &ZoneData::T_surf); }},
        {"T_dew [°C]",        QColor("#f39c12"),
            [&](int i){ return srMainData[i].T_dew; }},
    });

    // ── Tab 2: Humidity (kg/kg) ───────────────────────────────────────────────
    buildChart(srHumChart, "Umidità Specifica", "Umidità specifica [kg_v/kg_as]", {
        {"x_ear (est.) [kg/kg]",   QColor("#3498db"),
            [&](int i){ return srMainData[i].x_ear; }},
        {"x_iar zona [kg/kg]",     QColor("#e67e22"),
            [&](int i){ return zVal(i, &ZoneData::x_iar); }},
    });

    // ── Tab 3: Thermal loads & energy (kWh/h) ────────────────────────────────
    buildChart(srEnergyChart, "Carichi Termici & Consumi", "Energia [kWh/h]", {
        {"Φ_C raffr. zona [kWh/h]", QColor("#3498db"),
            [&](int i){ return zVal(i, &ZoneData::Phi_C); }},
        {"Φ_H risc. zona [kWh/h]",  QColor("#e74c3c"),
            [&](int i){ return zVal(i, &ZoneData::Phi_H); }},
        {"EC_kWh elett. [kWh/h]",   QColor("#f39c12"),
            [&](int i){ return zVal(i, &ZoneData::EC_kWh); }},
        {"NG_kWh gas [kWh/h]",      QColor("#e67e22"),
            [&](int i){ return zVal(i, &ZoneData::NG_kWh); }},
    });

    // ── Tab 4: CO₂ (kg/h) ────────────────────────────────────────────────────
    buildChart(srCO2Chart, "Emissioni CO₂", "CO₂ [kg/h]", {
        {"CO₂ zona [kg/h]",    QColor("#8e44ad"),
            [&](int i){ return zVal(i, &ZoneData::CO2_kg); }},
        {"CO₂ totale [kg/h]",  QColor("#2c3e50"),
            [&](int i){ return srMainData[i].CO2_kg; }},
    });

    // ── Tab 5: Comfort – SET & T_op (°C) ─────────────────────────────────────
    buildChart(srComfortChart, "Comfort – SET e Temperatura Operativa",
               "Temperatura [°C]", {
        {"SET zona [°C]",         QColor("#e74c3c"),
            [&](int i){ return zVal(i, &ZoneData::SET); }},
        {"T_op zona [°C]",        QColor("#8e44ad"),
            [&](int i){ return zVal(i, &ZoneData::T_op); }},
        {"T_out [°C]",            QColor("#3498db"),
            [&](int i){ return srMainData[i].T_out; }},
    });
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slots – file loading
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onLoadSimResultsFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Carica CSV simulazione principale",
        QDir::homePath(),
        "File CSV (*.csv);;Tutti i file (*)");
    if (path.isEmpty()) return;

    try {
        srMainData = loadCSV(path.toStdString());
        if (srMainData.empty()) {
            QMessageBox::warning(this, "File vuoto",
                                 "Il file CSV non contiene righe di dati.");
            return;
        }
        srMainFilePath = path;
        srMainFileLabel->setText(QFileInfo(path).fileName());
        srMainFileLabel->setStyleSheet(
            "background-color: #d5f5e3; border: 1px solid #27ae60; "
            "border-radius: 4px; padding: 8px; color: #1e8449; font-size: 12px;");

        // Populate zone selector
        const QStringList zoneNames = srExtractZoneNames(path);
        srZoneCombo->blockSignals(true);
        srZoneCombo->clear();
        for (const QString &zn : (zoneNames.isEmpty()
                                  ? QStringList{"Zona 1"} : zoneNames))
            srZoneCombo->addItem(zn);
        srZoneCombo->blockSignals(false);
        srZoneCombo->setEnabled(true);

        // Update range spinboxes
        const int total = static_cast<int>(srMainData.size());
        srXMinSpin->setMaximum(total - 1);
        srXMaxSpin->setMaximum(total);
        srXMinSpin->setValue(0);
        srXMaxSpin->setValue(total);
        srXMinSpin->setEnabled(true);
        srXMaxSpin->setEnabled(true);

        srDataInfoLabel->setText(
            QString("✅  %1 | %2 ore | %3 zone termiche")
            .arg(QFileInfo(path).fileName())
            .arg(total)
            .arg(zoneNames.isEmpty() ? 1 : zoneNames.size()));
        srDataInfoLabel->setStyleSheet(
            "color: #27ae60; font-size: 12px; font-weight: bold; padding: 4px;");

        srStatusLabel->setText("File principale caricato – premi «Calcola» per gli indicatori.");
        srStatusLabel->setStyleSheet("color: #7f8c8d; font-size: 12px; font-style: italic;");

        // Show raw time-series (derived fields not yet computed → zeros for SET/CO2)
        updateSimResultsCharts();

    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Errore caricamento CSV",
                              QString("Impossibile caricare il file:\n%1")
                              .arg(QString::fromStdString(e.what())));
    }
}

void MainWindow::onLoadSimResultsRefFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Carica CSV riferimento",
        QDir::homePath(),
        "File CSV (*.csv);;Tutti i file (*)");
    if (path.isEmpty()) return;

    try {
        srRefData = loadCSV(path.toStdString());
        srRefFilePath = path;
        srRefFileLabel->setText(QFileInfo(path).fileName());
        srRefFileLabel->setStyleSheet(
            "background-color: #d5f5e3; border: 1px solid #27ae60; "
            "border-radius: 4px; padding: 8px; color: #1e8449; font-size: 12px;");
        srStatusLabel->setText(
            QString("File riferimento caricato: %1 (%2 ore).")
            .arg(QFileInfo(path).fileName())
            .arg(static_cast<int>(srRefData.size())));
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Errore",
                              QString::fromStdString(e.what()));
    }
}

void MainWindow::onLoadSimResultsOccupancyFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, "Carica CSV occupazione",
        QDir::homePath(),
        "File CSV (*.csv);;Tutti i file (*)");
    if (path.isEmpty()) return;

    try {
        std::vector<bool> occ = loadOccupancy(path.toStdString());
        srOccupancyFilePath = path;
        srOccupancyFileLabel->setText(QFileInfo(path).fileName());
        srOccupancyFileLabel->setStyleSheet(
            "background-color: #d5f5e3; border: 1px solid #27ae60; "
            "border-radius: 4px; padding: 8px; color: #1e8449; font-size: 12px;");

        bool applied = false;
        if (!srMainData.empty()) {
            if (occ.size() == srMainData.size()) {
                applyOccupancy(srMainData, occ);
                applied = true;
            } else {
                QMessageBox::warning(this, "Dimensioni non compatibili",
                    QString("File occupazione: %1 righe; dataset principale: %2 ore.")
                    .arg(static_cast<int>(occ.size()))
                    .arg(static_cast<int>(srMainData.size())));
            }
        }
        if (!srRefData.empty() && occ.size() == srRefData.size())
            applyOccupancy(srRefData, occ);

        srStatusLabel->setText(applied
            ? "✓ Occupazione applicata al dataset principale."
            : "File occupazione caricato (verrà applicato al caricamento dei dati).");
    } catch (const std::exception &e) {
        QMessageBox::critical(this, "Errore", QString::fromStdString(e.what()));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slot – compute all indicators
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onComputeResilienceIndicators()
{
    if (srMainData.empty()) {
        QMessageBox::warning(this, "Nessun dato",
                             "Caricare prima il file CSV di simulazione principale.");
        return;
    }

    // Build ResilienceParams from UI
    ResilienceParams params;
    params.T_comfort   = srTComfortSpin->value();
    params.T_SET_alert = srTSetAlertSpin->value();
    params.T_cold_safe = srTColdSafeSpin->value();
    params.T_base      = srTBaseSpin->value();
    params.floor_area  = srFloorAreaSpin->value();
    params.dt_h        = srDtHSpin->value();
    params.heating_system =
        (srHeatSystemCombo->currentIndex() == 0)
        ? HeatingSystem::GAS_BOILER : HeatingSystem::ELECTRIC_HP;
    params.eta_H     = srEtaHSpin->value();
    params.COP_C     = srCopCSpin->value();
    params.ef_gas    = srEfGasSpin->value();
    params.ef_elec   = srEfElecSpin->value();
    params.elec_conv = srElecConvSpin->value();
    params.gas_conv  = srGasConvSpin->value();
    params.met       = srMetSpin->value();
    params.clo       = srCloSpin->value();
    params.v_air     = srVAirSpin->value();

    try {
        computeDerivedFields(srMainData, params);

        const SingleDatasetResults r = computeAllIndicators(srMainData, params);

        // Helper: format a double result
        auto setVal = [](QLabel *lbl, double v, int dec = 2,
                         const QString &unit = "") {
            if (!std::isfinite(v) || v < 0.0)
                lbl->setText("N/D");
            else
                lbl->setText(QString::number(v, 'f', dec)
                             + (unit.isEmpty() ? "" : "\u202F" + unit));
        };
        auto setValAlways = [](QLabel *lbl, double v, int dec = 2,
                               const QString &unit = "") {
            lbl->setText(QString::number(v, 'f', dec)
                         + (unit.isEmpty() ? "" : "\u202F" + unit));
        };

        // Single-dataset results
        for (int b = 0; b < 6; ++b)
            setValAlways(srResHumidexBand[b], r.humidex.band_pct[b], 1, "%");
        setValAlways(srResHumidexUnmet,    r.humidex.unmet_pct,      1, "%");
        setValAlways(srResIOD,             r.IOD,                    1, "°C·h");
        setValAlways(srResAWD,             r.AWD,                    1, "°C·h");
        setValAlways(srResSBOI,            r.SBOI,                   1, "%");
        setValAlways(srResPrimaryEnergy,   r.primary_energy,         2, "kWh/(m²·a)");
        setVal(srResRecoveryTime,          r.recovery_time_h,        0, "h");
        setVal(srResAbsorptivityTime,      r.absorptivity_time_h,    0, "h");

        // Two-dataset results
        if (!srRefData.empty()) {
            computeDerivedFields(srRefData, params);
            const TwoDatasetResults r2 =
                computeTwoDatasetIndicators(srRefData, srMainData, params, {}, {});
            setValAlways(srResCO2Reduction,      r2.CO2_reduction,      1, "kg/anno");
            setValAlways(srResSurfTempReduction, r2.surf_temp_reduction, 2, "°C");
            setValAlways(srResRCI,               r2.RCI,                3);
            setValAlways(srResTRI,               r2.TRI,                3);
            if (r2.OEF != 0.0)
                setValAlways(srResOEF, r2.OEF, 3);
            else
                srResOEF->setText("N/D (serve più scenari)");
        } else {
            for (QLabel *lbl : {srResCO2Reduction, srResSurfTempReduction,
                                srResRCI, srResTRI, srResOEF})
                lbl->setText("— (caricare file rif.)");
        }

        srStatusLabel->setText("✅  Calcolo completato con successo.");
        srStatusLabel->setStyleSheet(
            "color: #27ae60; font-size: 12px; font-weight: bold;");

        // Refresh charts (now derived fields are available)
        updateSimResultsCharts();

    } catch (const std::exception &e) {
        const QString msg = QString::fromStdString(e.what());
        QMessageBox::critical(this, "Errore nel calcolo", msg);
        srStatusLabel->setText("❌  Errore: " + msg);
        srStatusLabel->setStyleSheet("color: #e74c3c; font-size: 12px;");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Slots – chart controls
// ─────────────────────────────────────────────────────────────────────────────

void MainWindow::onSimResultsZoneChanged(int) {
    updateSimResultsCharts();
}

void MainWindow::onSimResultsRangeChanged() {
    updateSimResultsCharts();
}

