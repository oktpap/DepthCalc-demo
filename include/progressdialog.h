#ifndef PROGRESSDIALOG_H
#define PROGRESSDIALOG_H

#include <QDialog>

namespace Ui {
class ProgressDialog;
}

/*
 * The ProgressDialog class shows file-loading progress bars and an optional
 * synchronization indicator with animated dots.
 *
 * Responsibilities:
 * - Create and update per-file progress rows
 * - Show/hide synchronization status
 * - Clear all progress rows when loading completes
 */
class ProgressDialog : public QDialog
{
    Q_OBJECT

public:
    ProgressDialog(QWidget *parent = nullptr);

    ~ProgressDialog();

    void updateProgress(const QString &name, int percent);

    void synchronization(bool state);

    void clear();

private:

    Ui::ProgressDialog *ui;

    QVector<QLabel*> labels;

    QVector<QProgressBar*> progressBars;

    QVBoxLayout *mainLayout;

    QLabel* syncLabel = nullptr;

    QTimer* dotTimer = nullptr;

    int dotCount {0};
};

#endif // PROGRESSDIALOG_H
