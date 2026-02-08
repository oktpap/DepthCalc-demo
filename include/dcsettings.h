#ifndef DCSETTINGS_H
#define DCSETTINGS_H
#include <QObject>


struct SettingsDelta
{
    std::optional<bool> timeSync;
    std::optional<int> dnMed;
    std::optional<int> dvMed;
    std::optional<double> dvExp;
    std::optional<double> sampStep;
    std::optional<double> minCandleLen;
};


/*
 * The DCSettings class manages persistent application settings and defaults,
 * including synchronization parameters, sampling options, UI colors, and
 * snapshot storage paths.
 *
 * Responsibilities:
 * - Load and store settings via QSettings
 * - Provide getters/setters for numeric options and color themes
 * - Apply batched updates using SettingsDelta
 * - Reset graph colors to defaults and notify listeners
 */
class DCSettings : public QObject
{
    Q_OBJECT

public:

    explicit DCSettings(QObject *parent = nullptr);

    static DCSettings& instance();

    bool getTimeSync() const {return timeSync;}

    int getDnMed() const {return dnMed;}

    int getDvMed() const {return dvMed;}

    double getDvExp() const {return dvExp;}

    QColor getPrzColor() const {return przColor;}

    QColor getDnColor() const {return dnColor;}

    QColor getMkColor() const {return mkColor;}

    QColor getPdColor() const {return pdColor;}

    QColor getGl1Color() const {return gl1Color;}

    QColor getDv1XColor() const {return dv1XColor;}

    QColor getDv1ZColor() const {return dv1ZColor;}

    QColor getDv2XColor() const {return dv2XColor;}

    QColor getDv2ZColor() const {return dv2ZColor;}

    double getSampStep() const {return sampStep;}

    double getMinCandleLen() const {return minCandleLen;}

    QString getSnapshotsDir() const {return snapshotsDir;}


    void setPrzColor(const QColor &c)  {przColor  = c; settings.setValue("przColor",  c);}

    void setDnColor(const QColor &c)   {dnColor   = c; settings.setValue("dnColor",   c);}

    void setMkColor(const QColor &c)   {mkColor   = c; settings.setValue("mkColor",   c);}

    void setPdColor(const QColor &c)   {pdColor   = c; settings.setValue("pdColor",   c);}

    void setGl1Color(const QColor &c)  {gl1Color  = c; settings.setValue("gl1Color",  c);}

    void setDv1XColor(const QColor &c) {dv1XColor = c; settings.setValue("dv1XColor", c);}

    void setDv1ZColor(const QColor &c) {dv1ZColor = c; settings.setValue("dv1ZColor", c);}

    void setDv2XColor(const QColor &c) {dv2XColor = c; settings.setValue("dv2XColor", c);}

    void setDv2ZColor(const QColor &c) {dv2ZColor = c; settings.setValue("dv2ZColor", c);}

    void setMinCandleLen(const double &val) {minCandleLen = val; settings.setValue("minCandleLen", val);}


public slots:

    void applyChanges(const SettingsDelta &d);

    void resetGraphColors();

signals:

    void graphColorsReseted();

private:

    QSettings settings;

    void load();

    bool timeSync;

    int dnMed;

    int dvMed;

    double dvExp;

    QColor przColor;

    QColor dnColor;

    QColor mkColor;

    QColor pdColor;

    QColor gl1Color;

    QColor dv1XColor;

    QColor dv1ZColor;

    QColor dv2XColor;

    QColor dv2ZColor;

    int sampStep;

    QString snapshotsDir;

    double minCandleLen;


    bool timeSyncDef() const {return true;}

    int dnMedDef() const {return 3;}

    int dvMedDef() const {return 3;}

    double dvExpDef() const {return 0.7;}

    QColor przColorDef() const {return Qt::red;}

    QColor dnColorDef() const {return Qt::blue;}

    QColor mkColorDef() const {return Qt::darkGreen;}

    QColor pdColorDef() const {return Qt::black;}

    QColor gl1ColorDef() const {return Qt::darkGreen;}

    QColor dv1XColorDef() const {return QColor(255,140,0);}

    QColor dv1ZColorDef() const {return Qt::red;}

    QColor dv2XColorDef() const {return Qt::darkGreen;}

    QColor dv2ZColorDef() const {return Qt::blue;}

    int sampStepDef() const {return 8;}

    QString snapshotsDirDef() const 
    {
        return QStandardPaths::writableLocation(
            QStandardPaths::TempLocation)
           + "/DepthCalc snapshots";
    }

    double minCandleLenDef() const {return 50;}
};

#endif // DCSETTINGS_H
