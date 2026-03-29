#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QtCharts/QChartView>
#include <QLabel>
#include <QJsonDocument>
#include <QTableWidget>
#include <QScrollBar>
#include "resilience_indicators.h"
class ToggleSwitch;

class QComboBox;
class QSpinBox;
class QDoubleSpinBox;
class QGraphicsView;
class QGraphicsScene;
class QTabWidget;
class QTabBar;
class QTableWidget;
class QMenu;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onMenuItemClicked();
    void onConvertEPW();
    void onConvertIDF();
    void onConvertSHP();
    void onConvertGeoJSON();
    void onConvertScheduleCompact(); // Nuovo slot per convertire Schedule:Compact
    void onLoadWeatherFile();
    void onRadiationComboChanged();  // Slot per cambio selezione radiazione
    void onXRangeChanged();  // Slot per cambio range X
    void onLoadGeoJSONFile();  // Slot per caricare file GeoJSON
    void onLoadComponentsFile();   // Slot per caricare file componenti JSON
    void onSaveComponentsFile();   // Slot per salvare file componenti
    void onAddComponent();         // Slot per aggiungere componente (sezione corrente)
    void onLoadBuildingTypesFile();  // Slot per caricare file tipologie edilizie CSV
    void onAddRow();  // Slot per aggiungere riga alla tabella
    void onAddColumn();  // Slot per aggiungere colonna alla tabella
    void onDeleteRow();  // Slot per eliminare riga selezionata
    void onDeleteColumn();  // Slot per eliminare colonna selezionata
    void onSaveBuildingTypesFile();  // Slot per salvare file CSV
    void onLoadBuildingsFile();  // Slot per caricare file edifici JSON
    void onBuildingSelectionChanged(int index);  // Slot per cambio edificio selezionato
    void onAddBuildingSurface();  // Slot per aggiungere superficie edificio
    void onAddZone();  // Slot per aggiungere zona
    void onSaveBuildingsFile();  // Slot per salvare file edifici
    void onLoadSchedulesFile();  // Slot per caricare file schedules
    void onSaveSchedulesFile();  // Slot per salvare file schedules CSV
    void onBrowseEnergyPlusDir();  // Slot per sfogliare directory EnergyPlus
    void onEnergyPlusToggleChanged(Qt::CheckState state);  // Slot per cambio stato toggle EnergyPlus
    void onCrea();  // Slot per pulsante Crea nella pagina BuildingTypes
    void onSaveSchedulesIdf();  // Slot per salvare solo il file IDF dalla pagina Schedules
    void onSelectSimulationClimateFile();
    void onSelectSimulationComponentsFile();
    void onSelectSimulationSchedulesFile();
    void onSelectSimulationBuildingsFile();
    void onLaunchSimulation();

    // Simulation Results & Resilience Indicators
    void onLoadSimResultsFile();
    void onLoadSimResultsRefFile();
    void onLoadSimResultsOccupancyFile();
    void onComputeResilienceIndicators();
    void onSimResultsZoneChanged(int index);
    void onSimResultsRangeChanged();

private:
    void setupUI();
    void createPages();
    void createHomePage();
    void createFileConversionPage();
    void createWeatherVisualizationPage();
    void createGeoJSONVisualizationPage();
    void createComponentsEditorPage();
    void createBuildingTypesEditorPage();
    void createBuildingsEditorPage();
    void createSchedulesEditorPage();
    void createSimulationPage();
    void createSimulationResultsPage();
    void updateSimResultsCharts();
    static QStringList srExtractZoneNames(const QString &filePath);
    void createSettingsPage();
    void createInfoPage();
    void createContactPage();
    void applyStyling();
    void updateRadiationChart();  // Aggiorna grafico radiazione
    void updateAllChartsRange(int xMin, int xMax);  // Aggiorna range X di tutti i grafici
    QString formatJsonProperties(const QJsonObject &properties);  // Formatta proprietà JSON con numeri a 1 decimale
    void populateComponentsTabs();  // Popola i tab con i componenti dal JSON
    void populateBuildingSurfacesTable(const QJsonObject &building);  // Popola tabella superfici edificio
    void populateZonesTable(const QJsonObject &building);  // Popola tabella zone edificio
    QString getBinaryPath(const QString &binaryName);
    void setSimulationFileSelection(QString &targetPath, QLabel *targetLabel, const QString &dialogTitle, const QString &fileFilter);
    void updateSelectedFileLabel(QLabel *label, const QString &filePath);
    
    QListWidget *sidebar;
    QStackedWidget *contentArea;
    QTextEdit *outputConsole;
    
    // Weather visualization widgets
    QTextEdit *fileHeaderText;  // Per mostrare le prime 8 righe del file WTST
    QVBoxLayout *chartsLayout;  // Layout per i grafici
    QChartView *temperatureChartView;  // Grafico temperatura
    QChartView *humidityChartView;  // Grafico umidità
    QChartView *radiationChartView;  // Grafico radiazione solare
    
    // Menu a tendina per selezione colonne radiazione
    QComboBox *radiationCombo1;
    QComboBox *radiationCombo2;
    
    // Controlli per range asse X
    QSpinBox *xMinSpinBox;
    QSpinBox *xMaxSpinBox;
    
    // Dati caricati
    QStringList loadedHeaders;  // Intestazioni colonne
    QVector<QVector<QPointF>> loadedData;  // Tutti i dati caricati
    int totalDataPoints;  // Numero totale di punti dati
    
    // GeoJSON visualization widgets
    QLabel *geoTypeLabel;
    QLabel *geoNameLabel;
    QGraphicsView *geoGraphicsView;
    QGraphicsScene *geoGraphicsScene;
    QTextEdit *geoPropertiesText;
    QJsonDocument geoJsonDoc;
    
    // Components JSON widgets
    class QTabWidget *componentsTabs;
    class QTabBar *compTabBar1 = nullptr;
    class QTabBar *compTabBar2 = nullptr;
    class QPushButton *saveComponentsBtn;
    bool _updatingTable = false;
    bool _syncingTabBars = false;
    QJsonDocument componentsJsonDoc;
    QString currentComponentsFilePath;
    bool is_convepjson=false; // se convertire con energyplus
    // Context-menu helpers for components editor
    void showComponentsContextMenu(QTableWidget *table, const QPoint &pos);
    void onComponentsCellChanged(QTableWidget *table, int row, int col);
    void addComponentToSection(const QString &sectionKey);
    void deleteComponentFromSection(const QString &sectionKey, const QString &componentName);
    void addFieldToSection(const QString &sectionKey);
    void deleteFieldFromSection(const QString &sectionKey, const QString &fieldName);
    
    // Building Types CSV widgets
    QTableWidget *buildingTypesTable;
    QScrollBar *topScrollBar = nullptr;
    QPushButton *addRowBtn;
    QPushButton *addColumnBtn;
    QPushButton *deleteRowBtn;
    QPushButton *deleteColumnBtn;
    QPushButton *saveBuildingTypesBtn;
    QString currentBuildingTypesFilePath;
    
    // Buildings JSON widgets
    class QComboBox *buildingsComboBox;
    QLabel *buildingsCountLabel;
    QTextEdit *siteLocationText;
    QTableWidget *buildingSurfacesTable;
    QTableWidget *zonesTable;
    class QPushButton *addSurfaceBtn;
    class QPushButton *addZoneBtn;
    class QPushButton *saveBuildingsBtn;
    QJsonDocument buildingsJsonDoc;
    QString currentBuildingsFilePath;
    int currentBuildingIndex;

    // Schedules widgets
    QTextEdit *schedulesTextEdit;
    QPushButton *loadSchedulesBtn;
    QPushButton *saveSchedulesBtn;
    QPushButton *saveSchedulesIdfBtn = nullptr;
    QString currentSchedulesFilePath;
    bool schedulesFileSaved = false;
    QStringList savedScheduleNames;  // nomi schedule estratti dall'IDF salvato

    // Simulation launcher widgets
    QLabel *simulationClimateFilePathLabel = nullptr;
    QLabel *simulationComponentsFilePathLabel = nullptr;
    QLabel *simulationSchedulesFilePathLabel = nullptr;
    QLabel *simulationBuildingsFilePathLabel = nullptr;
    QTextEdit *simulationOutputConsole = nullptr;
    QComboBox *simulationModeCombo = nullptr;
    QPushButton *launchSimulationBtn = nullptr;
    QString simulationClimateFilePath;
    QString simulationComponentsFilePath;
    QString simulationSchedulesFilePath;
    QString simulationBuildingsFilePath;
    class QProcess *simulationProcess = nullptr;

    // "Crea" button (buildingTypes page) — attivo solo quando tutti i prerequisiti sono soddisfatti
    QPushButton *createBtn = nullptr;
    void updateCreateButton();  // aggiorna enabled/disabled del pulsante Crea

    // Settings page widgets
    ToggleSwitch *energyPlusToggle;
    QLabel *energyPlusDirLabel;
    QString energyPlusDirPath;
    QComboBox *firstDayCombo = nullptr; // Giorno della settimana del 1 gennaio (0=Lun, 6=Dom)

    // ── Simulation Results & Resilience Indicators page ──────────────────────
    // File paths & labels
    QString    srMainFilePath;
    QString    srRefFilePath;
    QString    srOccupancyFilePath;
    QLabel    *srMainFileLabel      = nullptr;
    QLabel    *srRefFileLabel       = nullptr;
    QLabel    *srOccupancyFileLabel = nullptr;
    QLabel    *srStatusLabel        = nullptr;
    QLabel    *srDataInfoLabel      = nullptr;

    // Parameters – comfort / thresholds
    QDoubleSpinBox *srTComfortSpin    = nullptr;
    QDoubleSpinBox *srTSetAlertSpin   = nullptr;
    QDoubleSpinBox *srTColdSafeSpin   = nullptr;
    QDoubleSpinBox *srTBaseSpin       = nullptr;
    QDoubleSpinBox *srFloorAreaSpin   = nullptr;
    QDoubleSpinBox *srDtHSpin         = nullptr;

    // Parameters – HVAC & energy
    QComboBox      *srHeatSystemCombo = nullptr;
    QDoubleSpinBox *srEtaHSpin        = nullptr;
    QDoubleSpinBox *srCopCSpin        = nullptr;
    QDoubleSpinBox *srEfGasSpin       = nullptr;
    QDoubleSpinBox *srEfElecSpin      = nullptr;
    QDoubleSpinBox *srElecConvSpin    = nullptr;
    QDoubleSpinBox *srGasConvSpin     = nullptr;

    // Parameters – SET occupant model
    QDoubleSpinBox *srMetSpin  = nullptr;
    QDoubleSpinBox *srCloSpin  = nullptr;
    QDoubleSpinBox *srVAirSpin = nullptr;

    // Indicator result labels – single dataset
    QLabel *srResHumidexBand[6]      = {};
    QLabel *srResHumidexUnmet        = nullptr;
    QLabel *srResIOD                 = nullptr;
    QLabel *srResRecoveryTime        = nullptr;
    QLabel *srResAbsorptivityTime    = nullptr;
    QLabel *srResAWD                 = nullptr;
    QLabel *srResPrimaryEnergy       = nullptr;
    QLabel *srResSBOI                = nullptr;

    // Indicator result labels – two datasets
    QLabel *srResCO2Reduction        = nullptr;
    QLabel *srResSurfTempReduction   = nullptr;
    QLabel *srResRCI                 = nullptr;
    QLabel *srResTRI                 = nullptr;
    QLabel *srResOEF                 = nullptr;

    // Chart controls
    QComboBox  *srZoneCombo    = nullptr;
    QSpinBox   *srXMinSpin     = nullptr;
    QSpinBox   *srXMaxSpin     = nullptr;
    QTabWidget *srChartsTab    = nullptr;
    QChartView *srTempChart    = nullptr;
    QChartView *srHumChart     = nullptr;
    QChartView *srEnergyChart  = nullptr;
    QChartView *srCO2Chart     = nullptr;
    QChartView *srComfortChart = nullptr;

    // Loaded data (populated by loadCSV / computeDerivedFields)
    std::vector<HourlyData> srMainData;
    std::vector<HourlyData> srRefData;
};

#endif // MAINWINDOW_H
