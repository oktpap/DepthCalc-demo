#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "dataloader.h"
#include "progressdialog.h"
#include "settingsdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

/*
 * The MainWindow class implements the main application UI and coordinates
 * user interaction, file workflows, and plot/table updates.
 *
 * Responsibilities:
 * - Manage loading dialogs, settings dialogs, and progress updates
 * - Drive plot initialization and updates for PRZ/PD/GL1 and related curves
 * - Handle file tree management, interval tables, and measurement views
 * - Orchestrate user input for selecting points, intervals, and corrections
 * - Provide accessors for calibration parameters and UI state
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:

    MainWindow(QWidget *parent = nullptr);

    int ADNWindow(); // get median filter window width

    void initMainPlot(const QVector<const DataLoader*> &loaders);

    void setLoaders(const QVector<const DataLoader*> &loaders);

    void updateProgress(const QString &fileId, int percent);

    double getMkFactor();

    void getCalPoints(QString &first, QString &second);

    QString getLoadType();

    void addIntervalRow(  const QString &type,
                          const int &number,
                          const double &lenght = -1,
                          const double &mes = -1,
                          const double &err = -1,
                          const double &depth = -1,
                          const double &speed = -1);

    void clearTable(const QString &type);

    void fillMeasure(const QVector<double> &data, const double &totalLen);

    void getCandleCorWin(double &window);

    void getPdCandleCorWin(double &window);

    void getCandleLength(const QString &type, QVector<double> &mas);

    void getMeasure(const QString &type, QVector<double> &mas);

    void updateGl1();

    void updatePD();

    void getRefTotalLen(int &len);

    void getPdRefTotalLen(int &len);

    bool hasMeasure(const QString &type);

    bool activeSync();

    void syncIsDone(const double &MK,
                    const double &ADN);

    void clear();

    void updateStage();

    ~MainWindow();

private:

    Ui::MainWindow *ui;

    ProgressDialog progressDialog;

    SettingsDialog settingsDialog;

    QDialog loadDialog;

    QVector<QString> files;

    QProgressBar *loadProgressBar = nullptr;

    QPushButton *redoButton = nullptr;

    QPushButton *undoButton = nullptr;

    void processFiles(const QStringList &filePaths);

    QVector<QString> datFiles;

    QVector<const DataLoader*> loaders;

    void setupProjectTree();

    QStandardItemModel *model = nullptr;

    QStandardItem *przItem = nullptr;

    QStandardItem *ADNItem = nullptr;

    QStandardItem *MKItem = nullptr;

    QStandardItem *dvlItem = nullptr;

    bool choosingFisrtPoint = false;

    bool choosingSecondPoint = false;

    bool choosingLoad = false;

    bool choosingRefTime = false;

    bool choosingFirstPDLogPoint = false;

    bool choosingSecondPDLogPoint = false;

    bool choosingFirstPDEditPoint = false;

    bool choosingSecondPDEditPoint = false;

    bool choosingFirstDvlPoint = false;

    bool choosingSecondDvlPoint = false;

    void projectTreeAppend(QString const &name, bool active, QString const &path); // adding file in tree

    void projectTreeClear(); // clearing tree

    void projectTreeUpdate(); // updating tree

    void deleteFile(QString const &name);

    void projectTreeRemove(const QModelIndex &index);

    void updateTreeSection(QStandardItem *item);



    void choosingPointPlotClick(QMouseEvent *event);

    void openStageFiles();

    void setStage(const QStringList &paths);

    QString getFName(const QString &name);

    QString pdFormat = "UNIX";

    void addIntervalsTableRow(QTableWidget *table,
                                int number,
                                double lenght = -1,
                                double mes = -1,
                                double err = -1,
                                double depth = -1,
                                double speed = -1);

    void deleteTableRow(QTableWidget *table, const int &number);

    void setMeasure(QTableWidget *table, const QVector<double> &data);

    void clearMeasure(QTableWidget *table);

    bool isCellFull(const QTableWidgetItem *cell);

    void countMeasureLen(const QString &type);

    void clearEmptyMeasure(const QString &type);

    void initTable(const QString &type);

    void cleanTableBoxes();

    void setStage();

    void showShiftLayout(bool state);

    void showSettings();

    void paintButton(QPushButton* btn, const QColor& c);

    bool hasNumberInRow(QTableWidget *table, int row);

signals:

    void goPrzConvert();

    void goDetLoad();

    void goManLoad(const double &lvl);

    void goPDcreate(double time, double depth, double firstPoint, double secondPoint);

    void cleanLoad();

    void showPalette();

    void goSavePD(const QString path, const QString format);

    void goGl1Create(double time,
                     double depth,
                     const double &firstPoint,
                     const double &secondPoint,
                     const QString &direction,
                     const QString &method);

    void goDeleteInterval(const QString &first, const QString &second);

    void goShiftGraph(const double &num);

    void goCancelShift();

    void goAddInterval(const QString &first, const QString &second);

    void goOpenMeasure(const QString &path);

    void goCleanMeasure();

    void goGl1Correction(const bool &candle, const bool &len, double totalLen, 
        const double &refTime, const double &refDepth);

    void goSaveGl1(const QString &path, int startFrame);

    void goPdCorrection(const bool &candle, const bool &len, double totalLen, 
        const double &refTime, const double &refDepth);

    void goMainPlotInit(const QVector<QString> &paths, bool sync);

    void goCleanAll();

    void sendFirstDvlPoint(const double &time1);

    void sendSecondDvlPoint(const double &time2);

    void goPrzCreate();

    void goUndo();

    void goRedo();

private slots:

    QString lastPath();

    void on_initPlotButton_clicked();

    void on_cleanPlotButton_clicked();

    void on_openProjectButton_clicked();

    void sliderUpdate(int value);

    void saveLastPath(const QString &path);

    void onFileTreeClicked(const QPoint &pos); // handle clicks on the file tree

    void on_applyPaletteButton_clicked();

    void on_cleanPaletteButton_clicked();

    void on_convertPrzButton_clicked();

    void cleanAll();

    void createLoadingDialog(QDialog &loadingDialog);

    void on_applyLoadButton_clicked();

    bool loadLinesChanged(const QString &text);

    void gl1LinesChanged(const int &index);

    void loaderAdded(QString lName);

    void loaderDeleted(QString lName);

    void graphViewChanged(bool checked);

    void setUnixFormat();

    void setDateFormat();

    void on_savePDButton_clicked();

    void on_applyGl1PushButton_clicked();

    void on_pdRadioButton_clicked();

    void on_gl1RadioButton_clicked();

    void on_delPDIntervalButton_clicked();

    void on_cancelPDEditingButton_clicked();

    void editLoadLonesChahged(const QString &text);

    void on_shiftLeftPushButton_clicked();

    void on_shiftRightPushButton_clicked();

    void on_cancelShiftPushButton_clicked();

    void on_addPDIntervalButton_clicked();

    void on_openMeasureButton_clicked();

    void on_cleanMeasureButton_clicked();

    void tableCellChanged(int row, int column);

    void on_candleCorrectionCheckBox_toggled(bool checked);

    void on_lengthCorrectionCheckBox_toggled(bool checked);

    void on_applyGL1CorPushButton_clicked();

    void on_saveGl1PushButton_clicked();

    void on_pdCandleCorrectionCheckBox_toggled(bool checked);

    void on_pdLengthCorrectionCheckBox_toggled(bool checked);

    void on_applyPdCorPushButton_clicked();

    void enablePDCorrection(bool candle, bool len);

    void enableGl1Correction(bool candle, bool len);

    void on_cancelGl1CorPushButton_clicked();

    void on_cancelPdCorPushButton_clicked();

    void tableSectionResized(int index, int /*oldSize*/, int newSize);

    void speedBoxChanged(int index);

    void on_applyDvlPushButton_clicked();

    void on_colorPushButton_clicked();

    void undoButtonClicked();

    void redoButtonClicked();

public slots:

    void setCalGraph(QVector<double> x, QVector<double> y);

    void calFinished();

    void przToPDFinished(const QVector<double> *lenghts);

    // connected to PlotWidget::graphSelected()
    void graphSelected(const bool &state, const QString &type);

    void removeLoader(const DataLoader* loader);

    void addLoader(const DataLoader* loader);

    void setScrollRange(const double &start, const double &finish);

    void filesLoadFinished();

    void initLogDebug(const QString &text);

    void przCreated();

    void manualLoadAdded();

    void synchronizationChanged(bool state);

    void startLoadingProgress(const QString &op);

    void updateLoadingProgress(int percent);

    void finishLoadingProgress();

    void snapshotHistoryChanged();

    void pdCreated();

protected:

    bool eventFilter(QObject *watched, QEvent *event) override;

};
#endif // MAINWINDOW_H
