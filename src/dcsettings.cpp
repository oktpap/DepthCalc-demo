#include "dcsettings.h"

DCSettings::DCSettings(QObject *parent)
    : QObject{parent}
    , settings("GORIZONT", "DepthCalc")
{
    load();
}

DCSettings &DCSettings::instance()
{
    static DCSettings obj;
    return obj;
}

void DCSettings::load()
{
    timeSync = settings.value("timeSync", timeSyncDef()).toBool();
    dnMed = settings.value("dnMed", dnMedDef()).toInt();
    dvMed = settings.value("dvMed", dvMedDef()).toInt();
    dvExp = settings.value("dvExp",dvExpDef()).toDouble();
    przColor = settings.value("przColor", przColorDef()).value<QColor>();
    dnColor = settings.value("dnColor", dnColorDef()).value<QColor>();
    mkColor = settings.value("mkColor", mkColorDef()).value<QColor>();
    pdColor = settings.value("pdColor", pdColorDef()).value<QColor>();
    gl1Color = settings.value("gl1Color", gl1ColorDef()).value<QColor>();
    dv1XColor = settings.value("dv1XColor", dv1XColorDef()).value<QColor>();
    dv1ZColor = settings.value("dv1ZColor", dv1ZColorDef()).value<QColor>();
    dv2XColor = settings.value("dv2XColor", dv2XColorDef()).value<QColor>();
    dv2ZColor = settings.value("dv2ZColor", dv2ZColorDef()).value<QColor>();
    sampStep = settings.value("sampStep",sampStepDef()).toInt();
    minCandleLen = settings.value("minCandleLen", minCandleLenDef()).toDouble();
    snapshotsDir = settings.value("snapshotsDir", snapshotsDirDef()).toString();
}

// подключен к SettingsDialog::settingsApplied
void DCSettings::applyChanges(const SettingsDelta &d)
{
    if(d.dnMed && *d.dnMed != dnMed)
    {
        dnMed = *d.dnMed;
        settings.setValue("dnMed", dnMed);
    }

    if(d.timeSync && *d.timeSync != timeSync)
    {
        timeSync = *d.timeSync;
        settings.setValue("timeSync", timeSync);
    }

    if(d.dvMed && *d.dvMed != dvMed)
    {
        dvMed = *d.dvMed;
        settings.setValue("dvMed", dvMed);
    }

    if(d.dvExp && *d.dvExp != dvExp)
    {
        dvExp = *d.dvExp;
        settings.setValue("dvExp", dvExp);
    }

    if(d.sampStep && *d.sampStep != sampStep)
    {
        sampStep = *d.sampStep;
        settings.setValue("sampStep", sampStep);
    }

    if(d.minCandleLen && *d.minCandleLen != minCandleLen)
    {
        minCandleLen = *d.minCandleLen;
        settings.setValue("minCandleLen", minCandleLen);
    }
}

void DCSettings::resetGraphColors()
{
    przColor = przColorDef();
    dnColor  = dnColorDef();
    mkColor  = mkColorDef();
    pdColor  = pdColorDef();
    gl1Color = gl1ColorDef();
    dv1XColor = dv1XColorDef();
    dv1ZColor = dv1ZColorDef();
    dv2XColor = dv2XColorDef();
    dv2ZColor = dv2ZColorDef();

    emit graphColorsReseted();
}

