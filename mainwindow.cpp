#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "personneleditdialog.h"
#include "work.h"
#include <QPainter>
#include <QDesktopWidget>
#include <QKeyEvent>

//#include <windows.h>
//MainWindow mw;
int ScreenWidth;
int ScreenHeight;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ScreenWidth = QApplication::desktop()->screenGeometry(-1).width();
    ScreenHeight = QApplication::desktop()->screenGeometry(-1).height();
    ui->frame->move(ScreenWidth/2 - ui->frame->width()/2, ScreenHeight/2 - ui->frame->height()/2);

//ui->frame->move(0,0);


   ui->background_widget->setGeometry(0,0, ScreenWidth, ScreenHeight);
   QPalette pal;
   pal.setColor(ui->frame->backgroundRole(), Qt::lightGray);
   ui->frame->setPalette(pal);
   ui->frame->setAutoFillBackground(true);
   WorkForm = new Work();

   //input_dialog = new InputDialog();

   // считываю список персонала из файла
   QFile f("personnel.txt");
   if (f.exists())
   {
       if (f.open(QIODevice::ReadOnly))
       {
           f.read((char*)&PersonnelCount, sizeof(int));
           for (int i = 0; i< PersonnelCount; i++)
           {
               wchar_t persona[50];
               f.read((char*)persona, 50 * sizeof(wchar_t));
               Personnel[i] = QString::fromWCharArray(persona);
           }
           f.close();
       }

   }
   else PersonnelCount = 0;
}

MainWindow::~MainWindow()
{
    delete WorkForm;
    //delete input_dialog;
    delete ui;
}

//Work *f2;

void MainWindow::on_pushButton_clicked()
{
    //hide();
    //WorkForm->run();
    //show();
    InputDialog input_dialog;
    input_dialog.move(ScreenWidth/2 - input_dialog.width()/2, ScreenHeight/2 - input_dialog.height()/2);
    connect(&input_dialog, SIGNAL(MainWindowClose()), SLOT(close()));
    input_dialog.exec();

}

void MainWindow::on_pushButton_3_clicked()
{
    this->close();

}

//------------------------------------------------------------------
void  MainWindow::keyPressEvent(QKeyEvent * e)
{
    if (e->key() == Qt::Key_F10) close();
}



void MainWindow::on_pushButton_2_clicked()
{
    PersonnelEditDialog pesonnel_edit;
    pesonnel_edit.exec();
}


