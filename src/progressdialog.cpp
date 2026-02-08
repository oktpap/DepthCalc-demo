#include "progressdialog.h"
#include "ui_progressdialog.h"

ProgressDialog::ProgressDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::ProgressDialog)
{
    ui->setupUi(this);

    this->setWindowTitle("Loading files");
    this->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint
                         | Qt::WindowTitleHint);

    mainLayout = new QVBoxLayout(this);
    setLayout(mainLayout);

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

ProgressDialog::~ProgressDialog()
{
    delete ui;
}

void ProgressDialog::updateProgress(const QString &name, int percent)
{
    int index = -1;
    for(int i = 0; i < labels.size(); i++)
    {
        if(labels[i]->text() == name + ":")
        {
            index = i;
            break;
        }
    }

    if(index < 0)
    {
        QLabel *label = new QLabel(this);
        QProgressBar *bar = new QProgressBar(this);

        label->setText(name + ":");
        bar->setRange(0, 100);
        bar->setValue(percent);

        bar->setMaximumWidth(250);
        bar->setMinimumWidth(250);

        labels.append(label);
        progressBars.append(bar);

        QWidget *row = new QWidget(this);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->addWidget(label);
        rowLayout->addWidget(bar, 1);
        mainLayout->addWidget(row);

        this->adjustSize();
    }

    else progressBars[index]->setValue(percent);
}

void ProgressDialog::clear()
{
    labels.clear();
    progressBars.clear();

    QLayoutItem *item;
    while ((item = mainLayout->takeAt(0)) != nullptr)
    {
        if (QWidget *w = item->widget())
            w->deleteLater();
        delete item;
    }
}


void ProgressDialog::synchronization(bool state)
{
    if (state) 
    {
        if (!syncLabel) {
            syncLabel = new QLabel("Synchronization", this);
            mainLayout->addWidget(syncLabel);
            
            dotTimer = new QTimer(this);
            connect(dotTimer, &QTimer::timeout, this, [this]() {
                dotCount = (dotCount + 1) % 4;
                QString dots(dotCount, '.');
                syncLabel->setText("Synchronization" + dots);
            });
            dotTimer->start(500); // update every 500 ms
        }
    } 
    
    else {
        if (syncLabel) {
            if (dotTimer) {
                dotTimer->stop();
                dotTimer->deleteLater();
                dotTimer = nullptr;
            }
            syncLabel->deleteLater();
            syncLabel = nullptr;
            dotCount = 0;
        }
    }
    
    this->adjustSize();
}
