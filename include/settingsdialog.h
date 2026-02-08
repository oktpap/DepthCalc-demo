#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include "dcsettings.h"

namespace Ui {
class SettingsDialog;
}

/*
 * The SettingsDialog class provides a UI for editing application settings
 * and emits batched updates to DCSettings.
 *
 * Responsibilities:
 * - Load current settings into UI controls
 * - Track parameter changes and enable apply/reset actions
 * - Apply changes via SettingsDelta and reset graph colors
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:

    void on_listWidget_clicked(const QModelIndex &index);

    void on_okButton_clicked();

    void on_cancelButton_clicked();

    void on_syncCheckBox_toggled(bool checked);

    void on_dnMedSpinBox_valueChanged(int arg1);

    void on_applyButton_clicked();

    void on_dvMedSpinBox_valueChanged(int arg1);

    void on_dvExpDoubleSpinBox_valueChanged(double arg1);

    void on_resetColorsPushButton_clicked();

    void on_minCandleLenSpinBox_valueChanged(int arg1);

private:

    Ui::SettingsDialog *ui;

    SettingsDelta delta;

    void loadSettings();

    void paramsChanged(bool state);

signals:

    void settingsApplied(const SettingsDelta &delta);

    void goResetGraphColors();
};

#endif // SETTINGSDIALOG_H
