#ifndef MIXERDIALOG_H
#define MIXERDIALOG_H

#include <QDialog>
#include "pers.h"

namespace Ui {
    class MixerDialog;
}

class MixerDialog : public QDialog {
    Q_OBJECT
public:
		MixerDialog(QWidget *parent, MixData *mixdata, int stickMode, QString * comment, int modelVersion, int modelType) ;
    ~MixerDialog();

//    QString getComment();

protected:
    void changeEvent(QEvent *e);

private slots:
    void valuesChanged();


private:
    MixData *md;
    Ui::MixerDialog *ui;
    QString * mixCommennt;
		int mType ;
};

#endif // MIXERDIALOG_H
