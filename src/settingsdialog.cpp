#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "dcsettings.h"

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::SettingsDialog)
{
    blockSignals(true);
    ui->setupUi(this);
    this->setWindowTitle("Settings");
    loadSettings();
    blockSignals(false);

    paramsChanged(false);

    ui->resetColorsPushButton->setStyleSheet("QPushButton { color: blue; text-decoration: underline; border: none; }");

    connect(this, &SettingsDialog::settingsApplied, &DCSettings::instance(), &DCSettings::applyChanges);
    connect(this, &SettingsDialog::goResetGraphColors, &DCSettings::instance(), &DCSettings::resetGraphColors);
}

SettingsDialog::~SettingsDialog()
{
    delete ui;
}

void SettingsDialog::on_listWidget_clicked(const QModelIndex &index)
{
    ui->stackedWidget->setCurrentIndex(index.row());
}


void SettingsDialog::on_okButton_clicked()
{
    emit settingsApplied(delta);
    delta = {};
    paramsChanged(false);

    this->hide();
}

void SettingsDialog::loadSettings()
{
    ui->dnMedSpinBox->setValue(DCSettings::instance().getDnMed());
    ui->syncCheckBox->setChecked(DCSettings::instance().getTimeSync());
    ui->dvMedSpinBox->setValue(DCSettings::instance().getDvMed());
    ui->dvExpDoubleSpinBox->setValue(DCSettings::instance().getDvExp());
    ui->minCandleLenSpinBox->setValue(static_cast<int>(DCSettings::instance().getMinCandleLen()));
}

void SettingsDialog::paramsChanged(bool state)
{
    ui->applyButton->setEnabled(state);
    ui->cancelButton->setEnabled(state);
}

void SettingsDialog::on_cancelButton_clicked()
{
    loadSettings();
    delta = {};
    paramsChanged(false);
}


void SettingsDialog::on_syncCheckBox_toggled(bool checked)
{
    delta.timeSync = checked;
    paramsChanged(true);
}


void SettingsDialog::on_dnMedSpinBox_valueChanged(int arg1)
{
    delta.dnMed = arg1;
    paramsChanged(true);
}


void SettingsDialog::on_applyButton_clicked()
{
    emit settingsApplied(delta);
    delta = {};
    ui->cancelButton->setEnabled(false);
}


void SettingsDialog::on_dvMedSpinBox_valueChanged(int arg1)
{
    delta.dnMed = arg1;
    paramsChanged(true);
}


void SettingsDialog::on_dvExpDoubleSpinBox_valueChanged(double arg1)
{
    delta.dvExp = arg1;
    paramsChanged(true);
}


void SettingsDialog::on_resetColorsPushButton_clicked()
{
    emit goResetGraphColors();
}


void SettingsDialog::on_minCandleLenSpinBox_valueChanged(int arg1)
{
    delta.minCandleLen = static_cast<double>(arg1);
    paramsChanged(true);
}

