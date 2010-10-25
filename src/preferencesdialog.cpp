#include "preferencesdialog.h"
#include "ui_preferencesdialog.h"
#include <QtGui>

preferencesDialog::preferencesDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::preferencesDialog)
{
    ui->setupUi(this);

    populateLocale();
    initSettings();

    connect(this,SIGNAL(accepted()),this,SLOT(write_values()));
}

preferencesDialog::~preferencesDialog()
{
    delete ui;
}

void preferencesDialog::write_values()
{
    QSettings settings("er9x-eePe", "eePe");
    settings.setValue("locale", ui->locale_QB->itemData(ui->locale_QB->currentIndex()));
    settings.setValue("default_channel_order", ui->channelorderCB->currentIndex());
    settings.setValue("default_mode", ui->stickmodeCB->currentIndex());
}


void preferencesDialog::initSettings()
{
    QSettings settings("er9x-eePe", "eePe");
    int i=ui->locale_QB->findData(settings.value("locale",QLocale::system().name()).toString());
    if(i<0) i=0;
    ui->locale_QB->setCurrentIndex(i);

    ui->channelorderCB->setCurrentIndex(settings.value("default_channel_order", 0).toInt());
    ui->stickmodeCB->setCurrentIndex(settings.value("default_mode", 1).toInt());
}

void preferencesDialog::populateLocale()
{
    ui->locale_QB->clear();
    ui->locale_QB->addItem("English (default)", "");


    QStringList strl = QApplication::arguments();
    if(!strl.count()) return;

    QString path = ".";
    if(strl.count()) path = QFileInfo(strl[0]).canonicalPath() + "/lang";

    QDir directory = QDir(path);
    QStringList files = directory.entryList(QStringList("eepe_*.qm"), QDir::Files | QDir::NoSymLinks);

    foreach(QString file, files)
    {
        QLocale loc(file.mid(5,2));
        ui->locale_QB->addItem(QLocale::languageToString(loc.language()), loc.name());
    }


    //ui->locale_QB->addItems(files);


}

