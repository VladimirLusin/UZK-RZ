#include "work.h"
#include "ui_WorkForm.h"

#include "vars.h"

#include "newinspection.h"
#include "Scene3D.h"
#include "scanform.h"
#include "mainwindow.h"
#include "enc600.h"
#include "channelsconfigdialog.h"
#include "modbusthread.h"


#include <algorithm>
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>

#include <QFont>

//#include <Qstring>

Work *WorkForm;


Work::Work(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Work)
{

  /*   TextureImage = new QImage(700, 300, QImage::Format_Indexed8);
    // создаю палитру
    QRgb clr;
    for (int i = 0; i < 128; i++)
    {
        clr = qRgb(255 * (double)i / 127., 0, 255 * double(127-i)/127.);
        TextureImage->setColor(i, clr);
    }
   TextureImage->setColor(255, qRgb(200,200,200));
    TextureImage->fill(255);
    for (int i = 0; i < 35; i++)
        for (int k = 0; k < 300; k++) TextureImage->setPixel(i * 20, k, 120);

    for (int i = 0; i < 700; i++)
        for (int k = 0; k < 15; k++) TextureImage->setPixel(i, k * 20, 120);*/



    ui->setupUi(this);
    //big_signal_widget = new SignalWidget(this, false, true);
    //signal_widget->BlockSignals = false;
    //signal_widget->big_size = true;

    grid_pxm = new QPixmap(AscanLength + 30, 450);
    ascan_pxm = new QPixmap(AscanLength + 30, 450);

#ifndef emulation
    UdThread = new TUdChannelThread();
    connect(UdThread, SIGNAL(ascan_ready()), SLOT(UpdateAScanPixmap()));
    connect(UdThread, SIGNAL(ascan_ready()), SLOT(UpdateTable()));

#endif
    connect(this, SIGNAL(UpdateAScan()), ui->big_signal_widget, SLOT(update()));
//    connect(this, SIGNAL(PrepareGrid()), ui->big_signal_widget, SLOT(prepare_grid()));

    connect(ui->RangePanel,         SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->GainPanel,          SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->DelayPanel,         SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->FreqPanel,          SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->PeriodCountPanel,   SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->aStartPanel,        SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->aThreshPanel,       SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->aWidthPanel,        SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->bStartPanel,        SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->bThreshPanel,       SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->bWidthPanel,        SIGNAL(edit_finish()), this, SLOT(UpdateValues()));

    connect(ui->VelocityPanel,      SIGNAL(edit_finish()), this, SLOT(UpdateValues()));
    connect(ui->PrismDelayPanel,    SIGNAL(edit_finish()), this, SLOT(UpdateValues()));

    connect(ui->GainSlider,             &QSlider::valueChanged,  [=] {ParameterChanget(1);});
    connect(ui->Range_horizontalSlider, &QSlider::valueChanged,  [=] {ParameterChanget(2);});
    connect(ui->Delay_horizontalSlider, &QSlider::valueChanged,  [=] {ParameterChanget(3);});
    connect(ui->FreqSlider,             &QSlider::valueChanged,  [=] {ParameterChanget(4);});
    connect(ui->PeriodCountSlider,      &QSlider::valueChanged,  [=] {ParameterChanget(5);});

  //  connect(ui->Detector_comboBox, &QComboBox::currentIndexChanged, [=] {ParameterChanget(6);});

    connect(ui->Detector_comboBox,      SIGNAL(currentIndexChanged(int)), SLOT(ParameterChanget(int)));
    connect(ui->Filter_comboBox,        SIGNAL(currentIndexChanged(int)), SLOT(ParameterChanget(int)));

    connect(ui->aStartSlider,           &QSlider::valueChanged,  [=] {ParameterChanget(8);});
    connect(ui->aWidthSlider,           &QSlider::valueChanged,  [=] {ParameterChanget(9);});
    connect(ui->aThresholdSlider,       &QSlider::valueChanged,  [=] {ParameterChanget(10);});

    connect(ui->bStartSlider,           &QSlider::valueChanged,  [=] {ParameterChanget(11);});
    connect(ui->bWidthSlider,           &QSlider::valueChanged,  [=] {ParameterChanget(12);});
    connect(ui->bThresholdSlider,       &QSlider::valueChanged,  [=] {ParameterChanget(13);});

    connect(ui->TgcTimeSlider,          &QSlider::valueChanged,  [=] {ParameterChanget(14);});
    connect(ui->TgcGainSlider,          &QSlider::valueChanged,  [=] {ParameterChanget(15);});
    connect(ui->TGCenable_checkBox,     &QCheckBox::stateChanged,  [=] {ParameterChanget(16);});


    connect(ui->Chan1Button, SIGNAL(clicked()), this, SLOT(AScanChannelSelect()));
    connect(ui->Chan2Button, SIGNAL(clicked()), this, SLOT(AScanChannelSelect()));



    connect(ui->BilletLengthPanel, SIGNAL(edit_finish()), this, SLOT(UpdateBillet()));
    connect(ui->TopDiameterPanel, SIGNAL(edit_finish()), this, SLOT(UpdateBillet()));
    connect(ui->BottomDiameterPanel, SIGNAL(edit_finish()), this, SLOT(UpdateBillet()));
    connect(ui->ControlStepPanel, SIGNAL(edit_finish()), this, SLOT(UpdateBillet()));
    connect(ui->AreaLengthPanel, SIGNAL(edit_finish()), this, SLOT(UpdateBillet()));
    connect(ui->StartHeightPanel, SIGNAL(edit_finish()), this, SLOT(UpdateBillet()));

    connect(ui->TgcTimePanel, &QPanel::edit_finish, [=] {on_TGCtableWidget_cellDoubleClicked(ui->TGCtableWidget->currentRow(), 0);});
    connect(ui->TgcGainPanel, &QPanel::edit_finish, [=] {on_TGCtableWidget_cellDoubleClicked(ui->TGCtableWidget->currentRow(), 1);});

    connect(ui->NumberlineEdit, &QLineEdit::textEdited, [=] {ResetAdvancedSettingsWarning();});
    connect(ui->Batch_lineEdit, &QLineEdit::textEdited, [=] {ResetAdvancedSettingsWarning();});
    connect(ui->Melt_lineEdit, &QLineEdit::textEdited, [=] {ResetAdvancedSettingsWarning();});
    connect(ui->certificate_lineEdit, &QLineEdit::textEdited, [=] {ResetAdvancedSettingsWarning();});
    connect(ui->material_lineEdit, &QLineEdit::textEdited, [=] {ResetAdvancedSettingsWarning();});
    connect(ui->accompanying_lineEdit, &QLineEdit::textEdited, [=] {ResetAdvancedSettingsWarning();});

    UpdateVarsFlag = false;

    for (int ch = 0; ch < MaxChannels; ch++)
    {

         FlawChannel[ch].enabled = 1;
         FlawChannel[ch].gain = 20*ch;
         FlawChannel[ch].range = 500;
         FlawChannel[ch].delay = 0;

         FlawChannel[ch].adc_offset = 0;

         FlawChannel[ch].detector = 3;
         FlawChannel[ch].polosa = 0;
         FlawChannel[ch].gzi_width = 3;
         FlawChannel[ch].gzi_count = 1;



         FlawChannel[ch].vrch_points = 0;
         memset(FlawChannel[ch].vrch_times, 0, sizeof(FlawChannel[ch].vrch_times));
         memset(FlawChannel[ch].vrch_gains, 0, sizeof(FlawChannel[ch].vrch_gains));
         FlawChannel[ch].vrch_go = 0;

         FlawChannel[ch].a_start = 50;
         FlawChannel[ch].a_width = 50;
         FlawChannel[ch].a_thresh = 50;

         //FlawChannel.a_time_mode = ;

         FlawChannel[ch].b_start = 75;
         FlawChannel[ch].b_width = 25;
         FlawChannel[ch].b_thresh = 50;
         // b_time_mode;

         // c_time_mode;

         /*FlawChannel[ch].komm_output = ch;
         FlawChannel[ch].komm_input = ch;*/



    }


    ActiveChannel = 0;



    QFile f("umd8_8k.vars");
    if (f.exists())
    {
        if (f.open(QIODevice::ReadOnly))
        {
            f.read((char*)FlawChannel, sizeof(TFlawChannel) * MaxChannels);
            f.read((char*)&Velocity, sizeof(int));
            f.read((char*)PrismDelay, sizeof(PrismDelay));
            f.close();
        }
    }

    CheckVars();

    // настройки коммутатора
    for (int ch = 0; ch < ChannelsCount; ch++)
    {

        FlawChannel[ch].komm_output = 2 * ch + 1;
        FlawChannel[ch].komm_input = 2 * ch;
        FlawChannel[ch].vars_ident = GetTickCount();
        UpdateChannelVars[ch] = 1;
    }

    display_height = 400;

    display_top = 10;
    display_bottom = display_top + display_height; // уровень 0 сигнала на фоне
    display_yscale = display_height / 100.;  // масштаб вывода % высоты экрана
    display_width = AscanLength;             // ширина зоны сигнала
//    if (big_size) display_width *= 2;
    display_left = 15;
    display_right = display_left + display_width;

    PrepareGrid();
#ifndef emulation
    UdThread->WorkFlag = true;
    UdThread->start();
#endif

    ui->TGCtableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->TGCtableWidget->setColumnWidth(0,40);
    ui->TGCtableWidget->setColumnWidth(1,40);
    for (int row = 0; row < ui->TGCtableWidget->rowCount(); row++)
        ui->TGCtableWidget->setRowHeight(row, 20);
    ui->tabWidget->setCurrentIndex(0);

    UpdateVars();




    connect(ui->big_signal_widget, SIGNAL(UpdateVars()), this, SLOT(UpdateVarsAndIdent()));


    // инициализирую карту PISO-ENCODER3
    ENC6_INITIAL();
    EncoderCardReady = ENC6_REGISTRATION(EncoderCardNumber, EncoderCardAddress) == YES;

    if (EncoderCardReady)
    {
       ENC6_INIT_CARD(EncoderCardNumber,
                      ENC_QUADRANT,
                      ENC_QUADRANT,
                      ENC_QUADRANT,
                      ENC_QUADRANT,
                      ENC_QUADRANT,
                      ENC_QUADRANT);

       ENC6_RESET_ENCODER(EncoderCardNumber, X1_axis);
       ENC6_RESET_ENCODER(EncoderCardNumber, X2_axis);
    }
#ifndef emulation2

    // инициализирую структуру адреса и сокет для соединения по Modbus
    peer.sin_family = AF_INET;
    peer.sin_port = htons(502);
    peer.sin_addr.s_addr = inet_addr("192.168.1.5"); //  IP адрес ПЛК


    tv.tv_sec = 0;
    tv.tv_usec = 0;
    ModbusSocket = INVALID_SOCKET;
    Connection = false;
    WSADATA WSAData;
    if (WSAStartup(0x202,(LPWSADATA)&WSAData)!=0)
    {
        QMessageBox::information(this, "Отсутствует соединение", "Ошибка инициализации библиотеки TCP/IP!");
    }
    // создаю поток, который будет осуществлять соединение по Modbus, чтобы не блокировать основной поток
    ModbusThread = new QModbusThread();
    ModbusThread->start();



#endif

    InspectionForm = new Inspection();
    connect(InspectionForm, SIGNAL(show_work_form()), SLOT(ShowAgain()));
    connect(InspectionForm, SIGNAL(SwitchChannel()), SLOT(SwitchChannel()));
    connect(this, SIGNAL(UpdateAScan()), InspectionForm, SLOT(UpdateAScan()));

    alarm_widget = new AlarmWidget(ui->AlarmGroupBox, true);
    alarm_widget->setGeometry(10, 16, 400, 260);
    alarm_widget->setAutoFillBackground(true);
    alarm_widget->setFrameStyle(QFrame::NoFrame);

    connect(ModbusThread, SIGNAL(UpdateAlarms()), alarm_widget, SLOT(UpdateIndicators()));


    UpdateInstallationStatePanels();


    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(UpdateInstallationStatePanels()));
    timer->start(300);

    connect(ui->big_signal_widget, SIGNAL(SelectTgcRow(int)), SLOT(SelectTgcRow(int)));

    //ui->additional_parameters_pushButton->setIcon(QIcon(":/warning.png"));
    AdvancedSettingsAdded = false;
    ui->big_signal_widget->setGeometry(10, 10, 1060, 900);
    //ui->verticalLayout->setGeometry(QRect(1840, 50, 20, 850));
}

Work::~Work()
{
    timer->stop();
    delete timer;
#ifndef emulation
    UdThread->WorkFlag = false;
    Sleep(100);

    UdThread->terminate();
    delete UdThread;
#endif
    QFile f("umd8_8k.vars");
    if(f.open(QIODevice::WriteOnly))
    {
        f.write((char*)FlawChannel, sizeof(TFlawChannel) * MaxChannels);
        // записываю скорость в материале и задержку призмы
        f.write((char*)&Velocity, sizeof(int));
        f.write((char*)PrismDelay, sizeof(PrismDelay));
        f.close();
    }
    //delete signal_widget;
    delete InspectionForm;
#ifndef emulation2
    ModbusThread->Terminated = true;
    Sleep(100);

    // дополнительно проверяю, что ModbusThread удалил сокет
    while (ModbusSocket != INVALID_SOCKET)
    {
         QApplication::processEvents();
    }
    ModbusThread->terminate();
    delete ModbusThread;
#endif

    delete alarm_widget;
    delete grid_pxm;
    delete ascan_pxm;

    //delete TextureImage;

    delete ui;
}


void Work::ParameterChanget(int tag)
{
    if (UpdateVarsFlag) return;
    TFlawChannel &fchan = FlawChannel[ActiveChannel];
    int vp;

    switch (tag)
    {
        case 1: fchan.gain = ui->GainSlider->value(); break;
        case 2: fchan.range = ui->Range_horizontalSlider->value(); break;
        case 3: fchan.delay = ui->Delay_horizontalSlider->value(); break;
        case 4: fchan.gzi_width = 202 - ui->FreqSlider->value(); break;
        case 5: fchan.gzi_count = ui->PeriodCountSlider->value(); break;



        case 8: fchan.a_start = ui->aStartSlider->value(); break;
        case 9: fchan.a_width = ui->aWidthSlider->value(); break;
        case 10: fchan.a_thresh = ui->aThresholdSlider->value(); break;

        case 11: fchan.b_start = ui->bStartSlider->value(); break;
        case 12: fchan.b_width = ui->bWidthSlider->value(); break;
        case 13: fchan.b_thresh = ui->bThresholdSlider->value(); break;

        case 14: vp = ui->TGCtableWidget->currentRow();
                 if ((vp >= 0) && (vp < fchan.vrch_points))
                 {
                    fchan.vrch_times[vp] = (__int16)ui->TgcTimeSlider->value();
                    if (vp > 0)
                    {
                        if (fchan.vrch_times[vp] < fchan.vrch_times[vp - 1]) fchan.vrch_times[vp] = (__int16)(fchan.vrch_times[vp - 1] + 1);
                    }
                    if (vp < fchan.vrch_points - 1)
                    {
                        if (fchan.vrch_times[vp] > fchan.vrch_times[vp + 1]) fchan.vrch_times[vp] = (__int16)(fchan.vrch_times[vp + 1] - 1);
                    }
                 }
                 break;
        case 15: vp = ui->TGCtableWidget->currentRow();
                 if ((vp >= 0) && (vp < fchan.vrch_points))
                 {
                    fchan.vrch_gains[vp] = (__int16)ui->TgcGainSlider->value();
                 }
                 break;

        case 16: fchan.vrch_go = ui->TGCenable_checkBox->isChecked(); break;

    }


    // чекбоксы пришлось программировать отдельно, т.к. не удалось привязать их через лямбда-функцию
    fchan.detector = ui->Detector_comboBox->currentIndex();
    fchan.polosa = ui->Filter_comboBox->currentIndex();

    fchan.vars_ident++;

    CheckVars();
    UpdateVars();

    //UpdateChannelVars[ActiveChannel]++;

}



void Work::AScanChannelSelect()
{
         if(ui->Chan1Button->isChecked())  ActiveChannel = 0;
    else if(ui->Chan2Button->isChecked())  ActiveChannel = 1;


    UpdateVars();
}

void Work::SwitchChannel()
{
    ui->Chan1Button->setChecked(ActiveChannel == 0);
    ui->Chan2Button->setChecked(ActiveChannel == 1);
    UpdateVars();

}

void __fastcall Work::UpdateActiveChannel()
{

}
void Work::UpdateVars()
{
    UpdateVarsFlag = true;

    TFlawChannel &vars = FlawChannel[ActiveChannel];

    ui->GainSlider->setSliderPosition(vars.gain);

    ui->Range_horizontalSlider->setSliderPosition(vars.range);

    ui->Delay_horizontalSlider->setSliderPosition(vars.delay);

    ui->FreqSlider->setSliderPosition(202 - vars.gzi_width);

    ui->PeriodCountSlider->setValue(vars.gzi_count);

    ui->Filter_comboBox->setCurrentIndex(vars.polosa);

    ui->Detector_comboBox->setCurrentIndex(vars.detector);

    ui->aStartSlider->setSliderPosition(vars.a_start);

    ui->aWidthSlider->setSliderPosition(vars.a_width);

    ui->aThresholdSlider->setSliderPosition(vars.a_thresh);

    ui->bStartSlider->setSliderPosition(vars.b_start);

    ui->bWidthSlider->setSliderPosition(vars.b_width);

    ui->bThresholdSlider->setSliderPosition(vars.b_thresh);


    if (vars.detector > 0) ui->aThresholdSlider->setMinimum(0);
    else ui->aThresholdSlider->setMinimum(-100);


    UpdateTGCtableWidget();
    int vp = ui->TGCtableWidget->currentRow();
    if (vp >= 0 && vp < vars.vrch_points)
    {
        ui->TgcTimePanel->setText(QString::number(vars.vrch_times[vp] * 0.5, 'f', 1));
        ui->TgcTimePanel->setEnabled(true);
        ui->TgcTimeSlider->setValue(vars.vrch_times[vp]);
        ui->TgcTimeSlider->setVisible(true);
        ui->TgcGainPanel->setText(QString::number(vars.vrch_gains[vp] * 0.5, 'f', 1));
        ui->TgcGainPanel->setEnabled(true);
        ui->TgcGainSlider->setValue(vars.vrch_gains[vp]);
        ui->TgcGainSlider->setVisible(true);
        ui->DelTGCPointButton->setEnabled(true);
    }
    else
    {
        ui->TgcTimePanel->setText("---");
        ui->TgcTimePanel->setEnabled(false);
        ui->TgcTimeSlider->setVisible(false);
        ui->TgcGainPanel->setText("---");
        ui->TgcGainPanel->setEnabled(false);
        ui->TgcGainSlider->setVisible(false);
        ui->DelTGCPointButton->setEnabled(false);
    }



    ui->TGCenable_checkBox->setChecked(vars.vrch_go);

    UpdateDigitalPanels();

    UpdateVarsFlag = false;
    PrepareGrid();
//    ui->big_signal_widget->update();

}

void Work::UpdateDigitalPanels()
{
    TFlawChannel &vars = FlawChannel[ActiveChannel];
    ui->RangePanel->setText(QString::number(vars.range / 100.));
    ui->GainPanel->setText(QString::number(vars.gain / 2.));
    ui->DelayPanel->setText(QString::number(vars.delay / 100.));
    ui->FreqPanel->setText(QString::number(100.0 / (vars.gzi_width + 1), 'f', 2));
    ui->PeriodCountPanel->setText(QString::number(vars.gzi_count * 0.5));
    ui->aStartPanel->setText(QString::number(vars.a_start / 100.));
    ui->aThreshPanel->setText(QString::number(vars.a_thresh));
    ui->aWidthPanel->setText(QString::number(vars.a_width / 100.));
    ui->bStartPanel->setText(QString::number(vars.b_start / 100.));
    ui->bThreshPanel->setText(QString::number(vars.b_thresh));
    ui->bWidthPanel->setText(QString::number(vars.b_width / 100.));
    ui->VelocityPanel->setText(QString::number(Velocity));
    ui->PrismDelayPanel->setText(QString::number(PrismDelay[ActiveChannel] /100., 'f', 2));


}


void __fastcall Work::UpdateTGCtableWidget()
{
     int ac = ActiveChannel;

     QTableWidgetItem *item[20][2];
     for (int i = 0; i<20; i++)
         for (int j = 0; j<2; j++)
         {
             item[i][j] = new QTableWidgetItem;
             item[i][j]->setTextAlignment(Qt::AlignCenter);
         }

     for (int i = 0; i < 20; i++)
     {
         if (i < FlawChannel[ac].vrch_points)
         {
             item[i][0]->setText(QString::number(FlawChannel[ac].vrch_times[i] * 0.5));
             ui->TGCtableWidget->setItem(i, 0, item[i][0]);
             item[i][1]->setText(QString::number(FlawChannel[ac].vrch_gains[i] * 0.5));
             ui->TGCtableWidget->setItem(i, 1, item[i][1]);
         }
         else
         {
             item[i][0]->setText("");
             ui->TGCtableWidget->setItem(i, 0, item[i][0]);
             item[i][1]->setText("");
             ui->TGCtableWidget->setItem(i, 1, item[i][1]);
         }
     }
}


void Work::on_AddTGCPointButton_clicked()
{
    int ac = ActiveChannel;

    if (FlawChannel[ac].vrch_points >= 20)
    {
       QMessageBox::information(this, "Добавить точку", "Задано максимальное число точек ВРЧ - 20!");

       return;
    }

    int p = FlawChannel[ac].vrch_points;

    if (p)
    {
        FlawChannel[ac].vrch_times[p] = FlawChannel[ac].vrch_times[p - 1] + 20;  // 10 мкс
        if (FlawChannel[ac].vrch_times[p] > 2000) FlawChannel[ac].vrch_times[p] = 2000;
        FlawChannel[ac].vrch_gains[p] = FlawChannel[ac].vrch_gains[p - 1] + 20;   // 10 дБ
    }
    else
    {
        FlawChannel[ac].vrch_times[p] = 20;  // 10 мкс
        FlawChannel[ac].vrch_gains[p] = 0;   // 10 дБ
    }
    FlawChannel[ac].vrch_points++;
    ui->TGCtableWidget->setCurrentCell(FlawChannel[ac].vrch_points, 1);

    UpdateVars();


}





void Work::on_DelTGCPointButton_clicked()
{
    int ac = ActiveChannel;

    if (!FlawChannel[ac].vrch_points) return;

    int p = ui->TGCtableWidget->currentRow();
    if (p >= FlawChannel[ac].vrch_points) return;

    if (QMessageBox::question(this, "Удалить точку ВРЧ", "Вы уверены, что хотите удалить точку ВРЧ?") == QMessageBox::No) return;
            // +
                     //         TGCtableWidget->" : " + TGCTableGrid->Cells[2][p+1] + " ?")");





    for (int i = p; i < FlawChannel[ac].vrch_points - 1; i++) {
        FlawChannel[ac].vrch_times[i] = FlawChannel[ac].vrch_times[i + 1];
        FlawChannel[ac].vrch_gains[i] = FlawChannel[ac].vrch_gains[i + 1];
    }
    FlawChannel[ac].vrch_points--;
    UpdateVars();

}



void  Work::UpdateValues()
{
    QPanel *clickedPanel = qobject_cast<QPanel *>(sender());
    QString txt;
    //QString value_txt;

    if (clickedPanel == ui->RangePanel)
    {
        txt = "Развертка";
    }
    else if (clickedPanel == ui->GainPanel)
    {
        txt = "Усиление";
    }
    else if (clickedPanel == ui->DelayPanel)
    {
        txt = "Задержка";
    }
    else if (clickedPanel == ui->FreqPanel)
    {
        txt = "Частота";
    }
    else if (clickedPanel == ui->PeriodCountPanel)
    {
        txt = "Количество периодов";
    }
    else if (clickedPanel == ui->aStartPanel)
    {
        txt = "Начало а-зоны";
    }
    else if (clickedPanel == ui->aThreshPanel)
    {
        txt = "Порог а-зоны";
    }
    else if (clickedPanel == ui->aWidthPanel)
    {
        txt = "Ширина а-зоны";
    }
    else if (clickedPanel == ui->bStartPanel)
    {
        txt = "Начало б-зоны";
    }
    else if (clickedPanel == ui->bThreshPanel)
    {
        txt = "Порог б-зоны";
    }
    else if (clickedPanel == ui->bWidthPanel)
    {
        txt = "Ширина б-зоны";
    }
    else if (clickedPanel == ui->VelocityPanel)
    {
        txt = "Скорость ультразвука, м/с" ;
    }
    else if (clickedPanel == ui->PrismDelayPanel)
    {
        txt = "Задержка в призме, мкс";
    }


    bool bOk;

       QString str =  QInputDialog::getText( 0,
                                            "УЗ канал",
                                            txt,
                                            QLineEdit::Normal,
                                            clickedPanel->text(),
                                            &bOk,
                                            Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                                           );
       if (bOk)
       {
           float flt_value = str.toFloat(&bOk);
           if (bOk)
           {
              clickedPanel->setText(str);
           }
           else QMessageBox::information(this, "Ввод числа", "Введено некорректное значение");
       }


       if (!bOk) {
           // Была нажата кнопка Cancel
       }


    TFlawChannel &fchan = FlawChannel[ActiveChannel];

bool ok;

    fchan.gain = ui->GainPanel->text().toFloat(&ok) * 2;
    fchan.range = ui->RangePanel->text().toFloat(&ok) * 100;
    fchan.delay = ui->DelayPanel->text().toInt(&ok) * 100;
    fchan.gzi_width = 100./ui->FreqPanel->text().toFloat(&ok) - 1;
    fchan.a_start = ui->aStartPanel->text().toFloat(&ok) * 100;
    fchan.a_thresh = ui->aThreshPanel->text().toInt(&ok, 10);
    fchan.a_width = ui->aWidthPanel->text().toFloat(&ok) * 100;
    fchan.b_start = ui->bStartPanel->text().toFloat(&ok) * 100;
    fchan.b_thresh = ui->bThreshPanel->text().toInt(&ok, 10);
    fchan.b_width = ui->bWidthPanel->text().toFloat(&ok) * 100;
    fchan.gzi_count = ui->PeriodCountPanel->text().toFloat(&ok) * 2;

    Velocity = ui->VelocityPanel->text().toInt(&ok);
    PrismDelay[ActiveChannel] = ui->PrismDelayPanel->text().toFloat(&ok) * 100;

    fchan.vars_ident++;

    CheckVars();
    UpdateVars();

}



void Work::on_TGCtableWidget_cellDoubleClicked(int row, int column)
{
    if (row >= FlawChannel[ActiveChannel].vrch_points) return;
    if (row == -1) row = ui->TGCtableWidget->currentRow();
    bool bOk;
    QString text;
    double value;
    int p = row;
    float flt_value;
    if (column == 0)
    {
        value = FlawChannel[ActiveChannel].vrch_times[row] / 2.;
        text = "Положение точки ВРЧ";
    }
    else
    {
        value = FlawChannel[ActiveChannel].vrch_gains[row] / 2.;
        text = "Усиление в точке ВРЧ";
    }
    QString str = QInputDialog::getText( 0,
                                         "Точка ВРЧ",
                                         text,
                                         QLineEdit::Normal,

                                         //ui->TGCtableWidget->currentItem()->text(),
                                         QString::number(value, 'f', 1),
                                         &bOk,
                                         Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                                        );

    bool ok;
    if (bOk)
    {
        flt_value = str.toFloat(&ok) * 2;
        if (ok)
        {
            if (column == 0)
            {
                if (flt_value < 0) flt_value = 0;
                if (flt_value > 1000) flt_value = 1000;
                FlawChannel[ActiveChannel].vrch_times[row] = flt_value;
                // при необходимости перемещаю точку вверх или вниз
                while (p > 0)
                {
                      if (FlawChannel[ActiveChannel].vrch_times[p - 1] <= FlawChannel[ActiveChannel].vrch_times[p]) break;

                      __int16 t = FlawChannel[ActiveChannel].vrch_times[p - 1];
                      FlawChannel[ActiveChannel].vrch_times[p - 1] = FlawChannel[ActiveChannel].vrch_times[p];
                      FlawChannel[ActiveChannel].vrch_times[p] = t;

                      t = FlawChannel[ActiveChannel].vrch_gains[p - 1];
                      FlawChannel[ActiveChannel].vrch_gains[p - 1] = FlawChannel[ActiveChannel].vrch_gains[p];
                      FlawChannel[ActiveChannel].vrch_gains[p] = t;

                      p--;
                }
                while (p < FlawChannel[ActiveChannel].vrch_points - 1)
                {
                      if (FlawChannel[ActiveChannel].vrch_times[p] <= FlawChannel[ActiveChannel].vrch_times[p + 1]) break;

                      __int16 t = FlawChannel[ActiveChannel].vrch_times[p + 1];
                      FlawChannel[ActiveChannel].vrch_times[p + 1] = FlawChannel[ActiveChannel].vrch_times[p];
                      FlawChannel[ActiveChannel].vrch_times[p] = t;

                      t = FlawChannel[ActiveChannel].vrch_gains[p + 1];
                      FlawChannel[ActiveChannel].vrch_gains[p + 1] = FlawChannel[ActiveChannel].vrch_gains[p];
                      FlawChannel[ActiveChannel].vrch_gains[p] = t;

                      p++;
                }
            }
            else
            {
                if (flt_value < -180) flt_value = -180;
                if (flt_value > 180) flt_value = 180;
                FlawChannel[ActiveChannel].vrch_gains[row] = flt_value;
            }
            FlawChannel[ActiveChannel].vars_ident++;
            UpdateTGCtableWidget();
            UpdateVars();
        }
        else QMessageBox::information(this, "Ввод числа", "Введено некорректное значение");


    }
}

#define test_min(value, min) if (value < min) value = min
#define test_max(value, max) if (value > max) value = max
#define test(value, min, max) test_min(value, min); test_max(value, max)
void Work::CheckVars()
{
     for (int i = 0; i < ChannelsCount; i++)
     {
         test(FlawChannel[i].gain, 0, 220);

         test(FlawChannel[i].range, 500, 50000);
         test(FlawChannel[i].delay, 0, 50000);

         test(FlawChannel[i].a_start, 0, 100000);
         test(FlawChannel[i].a_width, 0, 100000 - FlawChannel[i].a_start);
         test(FlawChannel[i].a_thresh, -95, 95);
         test(FlawChannel[i].a_time_mode, 0, 2);

         test(FlawChannel[i].b_start, 0, 100000);
         test(FlawChannel[i].b_width, 0, 100000 - FlawChannel[i].b_start);
         test(FlawChannel[i].b_thresh, -95, 95);
         test(FlawChannel[i].b_time_mode, 0, 2);

      /*   test(FlawChannel[i].c_start, 0, 100000);
         test(FlawChannel[i].c_width, 0, 100000 - FlawChannel[i].c_start);
         test(FlawChannel[i].c_thresh, -95, 95);
         test(FlawChannel[i].c_time_mode, 0, 2);

         test(FlawChannel[i].i_start, 0, 100000);
         test(FlawChannel[i].i_width, 0, 2000);
         test(FlawChannel[i].i_thresh, -95, 95);
         test(FlawChannel[i].i_time_mode, 0, 2);
         test(FlawChannel[i].i_sync_mode, 0, 1);*/

         test(FlawChannel[i].gzi_width, 0, 199);
         test(FlawChannel[i].gzi_count, 0, 15);

         test(FlawChannel[i].detector, 0, 3);
         test(FlawChannel[i].polosa, 0, 15);

         if (FlawChannel[i].vrch_points < 2) FlawChannel[i].vrch_go = 0;
         for (int i = 0; i < FlawChannel[i].vrch_points; i++)
         {
             __int16 min = 0;
             if (i) min = FlawChannel[i].vrch_times[i - 1];
             test(FlawChannel[i].vrch_times[i], min, 400);
             test(FlawChannel[i].vrch_gains[i], -180, 180);
         }


         FlawChannel[i].komm_input &= 7;
         FlawChannel[i].komm_output &= 7;
     }
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::on_pushButton_clicked()
{
#ifndef emulation2
    UpdateInstallationStatePanels();

    // проверяю, нет ли незаполненых полей
    if (ui->BilletLengthPanel->text() == "" ||
            ui->TopDiameterPanel->text() == "" ||
            ui->BottomDiameterPanel->text() == "" ||
            ui->ControlStepPanel->text() == "" ||
            ui->AreaLengthPanel->text() == "" ||
            ui->StartHeightPanel->text() == "")
    {
        QMessageBox::warning(0, "Не хватает данных", "Не все данные внесены. Контроль невозможен!");
        return;
    }


    if (!DefConnection)
    {
        QMessageBox::warning(0, "Дефектоскоп не готов", "Нет соединения с дефектокопом. Контроль невозможен!");
        return;
    }


    if (!EncoderCardReady)
    {
        QMessageBox::warning(0, "Ошибка", "Нет связи с платой расширения. Контроль невозможен!");
        return;
    }


    if (!InstallationIsOn)
    {
        QMessageBox::warning(0, "Установка не готова", "Нет сигнала готовности установки. Контроль невозможен!");
        return;
    }
    if (!AutomaticModeIsOn)
    {
        QMessageBox::warning(0, "Установка не готова", "Нет подтверждения автоматического режима установки. Контроль невозможен!");
        return;
    }

    if (ui->NumberlineEdit->text() == "" ||
        ui->Batch_lineEdit->text() == "" ||
        ui->Melt_lineEdit->text() == "" ||
        ui->certificate_lineEdit->text() == "" ||
        ui->material_lineEdit->text() == "" ||
        ui->accompanying_lineEdit->text() == "")
    {
        QMessageBox::warning(0, "Параметры", "Не внесены дополнительные параметры изделия. Контроль невозможен!");
        return;
    }
    else
    {
        BilletNumber = ui->NumberlineEdit->text();
        BatchNumber = ui->Batch_lineEdit->text();
        MeltNumber = ui->Melt_lineEdit->text();
        CertificateNumber = ui->certificate_lineEdit->text();
        MaterialGrade = ui->material_lineEdit->text();
        AccompanyingDocument = ui->accompanying_lineEdit->text();
    }

    if (AdvancedSettingsWarning)
    {
        if (QMessageBox::warning(0, "Параметры", "Дополнительные параметры изделия не были изменены.\n "
                                             "Оставить прежние параметры и выполнить контроль?",
                             QMessageBox::Yes, QMessageBox::No) == QMessageBox::No)
        return;
    }
#endif
    if (InspectionForm->run(INSPECTION))
        hide();
    //show();
}
//------------------------------------------------------------------------------------------------------------------------------------------
void  Work::UpdateBillet()
{
    QPanel *clickedPanel = qobject_cast<QPanel *>(sender());
    QString txt;
    //QString value_txt;

    if (clickedPanel == ui->BilletLengthPanel)
    {
        txt = "Высота заготовки";
    }
    else if (clickedPanel == ui->TopDiameterPanel)
    {
        txt = "Верхний диаметр";
    }
    else if (clickedPanel == ui->BottomDiameterPanel)
    {
        txt = "Нижний диаметр";
    }
    else if (clickedPanel == ui->ControlStepPanel)
    {
        txt = "Шаг контроля";
    }
    else if (clickedPanel == ui->AreaLengthPanel)
    {
        txt = "Высота контролируемого участка";
    }
    else if (clickedPanel == ui->StartHeightPanel)
    {
        txt = "Начало контролируемого участка по высоте";
    }


    bool bOk;

    QString str =  QInputDialog::getText( 0,
                                          "Параметры заготовки",
                                          txt,
                                          QLineEdit::Normal,
                                          clickedPanel->text(),
                                          &bOk,
                                          Qt::WindowTitleHint | Qt::WindowCloseButtonHint
                                          );
    if (bOk)
    {
        int value = str.toInt(&bOk);
        if (bOk)
        {
            clickedPanel->setText(str);
        }
        else QMessageBox::information(this, "Ввод числа", "Введено некорректное значение");
    }


    if (!bOk) {
        // Была нажата кнопка Cancel
    }


    bool ok;


    BilletHeight = ui->BilletLengthPanel->text().toInt(&ok);
    BilletBottomDiameter = ui->BottomDiameterPanel->text().toInt(&ok);
    BilletTopDiameter = ui->TopDiameterPanel->text().toInt(&ok);
    ControlStep = ui->ControlStepPanel->text().toInt(&ok);
    ControlAreaHeight = ui->AreaLengthPanel->text().toInt(&ok);
    StartInspectPosition = ui->StartHeightPanel->text().toInt(&ok);

   /* CheckVars();
    UpdateVars();
    UpdateVarsAndFon();*/
}
//------------------------------------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------------------------------------
bool Work::OpenResult()
{
    ResultFileName = QFileDialog::getOpenFileName(0, "Открыть результат", "D:/UZK-RZ/Результаты/" "*.scan");
    if (ResultFileName == "") return false;

    QFile f(ResultFileName);

    if(f.open(QIODevice::ReadOnly))
    {
        // считываю из файла геометрические параметры объекта
        // а также значение шага контроля,
        // количество линий по вертикали и точек в одной линии
        // также нужно будет записывать либо палитру, либо браковочный уровень

        f.read((char*)(&BilletTopDiameter), sizeof(int));
        f.read((char*)(&BilletBottomDiameter), sizeof(int));
        f.read((char*)(&BilletHeight), sizeof(int));
        f.read((char*)(&ControlStep), sizeof(int));
        f.read((char*)(&LinesCount), sizeof(int));
        f.read((char*)(&StepPerRevolution), sizeof(int));
        for (int ch = 0; ch < ChannelsCount; ch++)
            for (int i = 0; i < LinesCount; i++)
                f.read(CScan[ch][i], StepPerRevolution);
        f.close();
        StartInspectPosition = 0;
        //ControlAreaHeight = pow()
    }
    else return false;

    return true;
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::on_save_flaw_settings_action_triggered()
{
    QString file_path = QString("Настройки дефектоскопа/");
    if (!QDir(file_path).exists())
    {
        QDir().mkdir(file_path);
    }

    QString file_name = QFileDialog::getSaveFileName(0, "Сохранение настроек дефектоскопа", file_path, "*.flw");
    if (file_name == "") return;

    QFile f(file_name);

    if(f.open(QIODevice::WriteOnly))
    {
        f.write((char*)FlawChannel, sizeof(TFlawChannel) * MaxChannels);
        f.close();
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------
/*void Work::on_exit_action_triggered()
{
    close();
}*/
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::on_open_result_action_triggered()
{
#ifdef emulation
    if (!OpenResult()) return;
    InspectionForm->run(VIEV_RESULT);
#else
    ResultFileName = QFileDialog::getOpenFileName(0, "Открыть результат", "D:/UZK-RZ/Результаты/" "*.scan");
    if (ResultFileName == "") return;

    QFile f(ResultFileName);

    if(f.open(QIODevice::ReadOnly))
    {
        if (!InspectionForm->run(VIEV_RESULT, &f)) return;
    }
    else
    {
        QMessageBox::information(0, "Ошибка", "Не удалось открыть файл");
        return /*false*/;
    }

#endif

    setFocus();
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::on_open_flaw_settings_action_triggered()
{
    QString file_path = QString("d:/UZK-RZ/Режимы контроля/Настройки дефектоскопа/");
    if (!QDir(file_path).exists())
    {
        file_path = "";
    }

    QString file_name = QFileDialog::getOpenFileName(0, "Загрузить настройки дефектоскопа", file_path, "*.flw");
    if (file_name == "") return;

    QFile f(file_name);

    if(f.open(QIODevice::ReadOnly))
    {
        f.read((char*)FlawChannel, sizeof(TFlawChannel) * MaxChannels);
        f.close();
        UpdateVars();

        for (int ch = 0; ch < MaxChannels; ch++)
            UpdateChannelVars[ch] = 1;
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::on_save_verification_mode_action_triggered()
{
    // проверяю, все ли поля заполнены
    if (ui->BilletLengthPanel->text()!="" &&
            ui->TopDiameterPanel->text()!="" &&
            ui->BottomDiameterPanel->text()!="" &&
            ui->ControlStepPanel->text()!="" &&
            ui->AreaLengthPanel->text()!="" &&
            ui->StartHeightPanel->text()!="")
    {
        QString file_path = QString("Режимы контроля/");
        if (!QDir(file_path).exists())
        {
            QDir().mkdir(file_path);
        }

        QString file_name = QFileDialog::getSaveFileName(0, "Сохранение режима контроля", file_path, "*.ctr");


        QFile f(file_name);
        if(f.open(QIODevice::WriteOnly))
        {
            f.write((char*)&BilletHeight, sizeof(int));
            f.write((char*)&BilletBottomDiameter, sizeof(int));
            f.write((char*)&BilletTopDiameter, sizeof(int));
            f.write((char*)&ControlStep, sizeof(int));
            f.write((char*)&ControlAreaHeight, sizeof(int));

            f.write((char*)FlawChannel, sizeof(TFlawChannel));
            //f.write((char*)ChannelEnabled, sizeof(ChannelEnabled));
            f.close();
        }

    }
    else QMessageBox::critical(this, "Ошибка сохранения режима контроля", "Не все поля с параметрами заготовки заполнены");
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::on_open_verification_mode_action_triggered()
{
    QString file_path = QString("d:/UZK-RZ/Режимы контроля/");

    if (!QDir(file_path).exists())
    {
        file_path = "";
    }

    QString file_name = QFileDialog::getOpenFileName(0, "Загрузка режима контроля", file_path, "*.ctr");
    if (file_name == "") return;

    QFile f(file_name);

    if(f.open(QIODevice::ReadOnly))
    {
        f.read((char*)&BilletHeight, sizeof(int));
        f.read((char*)&BilletBottomDiameter, sizeof(int));
        f.read((char*)&BilletTopDiameter, sizeof(int));
        f.read((char*)&ControlStep, sizeof(int));
        f.read((char*)&ControlAreaHeight, sizeof(int));
        f.read((char*)FlawChannel, sizeof(TFlawChannel));
        //f.read((char*)ChannelEnabled, sizeof(ChannelEnabled));
        f.close();
        UpdateVars();

        for (int ch = 0; ch < MaxChannels; ch++)
            UpdateChannelVars[ch] = 1;
        ui->BilletLengthPanel->setText(QString::number(BilletHeight));
        ui->BottomDiameterPanel->setText(QString::number(BilletBottomDiameter));
        ui->TopDiameterPanel->setText(QString::number(BilletTopDiameter));
        ui->ControlStepPanel->setText(QString::number(ControlStep));
        ui->AreaLengthPanel->setText(QString::number(ControlAreaHeight));
        ui->StartHeightPanel->setText(QString::number(StartInspectPosition));
    }
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::run()
{

     //showFullScreen();
     //exec();
     //showFullScreen();
     setWindowFlags(Qt::FramelessWindowHint);



     showMaximized();
    //UdThread->StopFlag = true;
}
//------------------------------------------------------------------------------------------------------------------------------------------

void Work::UpdateChannelButtons()
{



}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::keyPressEvent(QKeyEvent * e)
{
    if (e->key() == Qt::Key_F10) close();
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::resizeEvent(QResizeEvent *event)
{
}
//------------------------------------------------------------------------------------------------------------------------------------------
void Work::UpdateInstallationStatePanels()
{
    // проверяю, включена ли установка
    if (EncoderCardReady)
    {
        unsigned char index = ENC6_GET_INDEX(EncoderCardNumber, X4_axis);
        InstallationIsOn = !(index & 0x01);
        ui->InstallationOnPanel->setOn(InstallationIsOn);
        // проверяю, готова ли установка к автоматическому режиму
        index = ENC6_GET_INDEX(EncoderCardNumber, X6_axis);
        AutomaticModeIsOn = !(index & 0x01);
        ui->AutomaticModePanel->setOn(AutomaticModeIsOn);
        // проверяю, нет ли аварий
        index = ENC6_GET_INDEX(EncoderCardNumber, X5_axis);
        Alarm = !(index & 0x01);
    }
    else
    {
        ui->InstallationOnPanel->setOn(false);
        ui->AutomaticModePanel->setOn(false);
    }
    ui->freq_label->setText(QString::number(freq));
#ifndef emulation2
    ui->PLC_freq_label->setText(QString::number(ModbusThread->ModbusRate));
#endif


}


void Work::UpdateVarsAndIdent()
{
    UpdateVars();

    FlawChannel[ActiveChannel].vars_ident++;
    UpdateChannelVars[ActiveChannel]++;
}

void Work::on_TGCtableWidget_cellClicked(int row, int column)
{
    UpdateVars();
}

void Work::SelectTgcRow(int row)
{
    ui->TGCtableWidget->setCurrentCell(row, 0);
    UpdateVars();
}




void Work::ShowAgain()
{
    AdvancedSettingsWarning = true;
    //ui->additional_parameters_pushButton->setIcon(QIcon(":/warning.png"));
    showMaximized();
}

void Work::ResetAdvancedSettingsWarning()
{
   AdvancedSettingsWarning = false;
}

void Work::UpdateTable()
{
    for (int chan = 0; chan < ChannelsCount; chan++)
    {
        QTableWidgetItem *item = new QTableWidgetItem;
        item->setText(QString::number(AmpA[chan]));
        item->setTextAlignment(Qt::AlignCenter);
        ui->CurrentParametersTableWidget->setItem(0, chan, item);

        QString dB = "---";
        if (AmpA[chan] && FlawChannel[chan].a_thresh)
            dB = QString::number(20 * log(AmpA[chan] / (double)FlawChannel[chan].a_thresh), 'f', 1);
        item = new QTableWidgetItem;
        item->setText(dB);
        item->setTextAlignment(Qt::AlignCenter);
        ui->CurrentParametersTableWidget->setItem(1, chan, item);

        QString us = "---";
        QString thick = "---";
        if (b_time[chan] != 0x0fffff)
        {
            us = QString::number(b_time[chan] / 100., 'f', 1);
            thick = QString::number((b_time[chan] - PrismDelay[chan]) * 1.E-5 * Velocity * 0.5, 'f', 1);
        }

        item = new QTableWidgetItem;
        item->setText(us);
        item->setTextAlignment(Qt::AlignCenter);
        ui->CurrentParametersTableWidget->setItem(2, chan, item);

        item = new QTableWidgetItem;
        item->setText(thick);
        item->setTextAlignment(Qt::AlignCenter);
        ui->CurrentParametersTableWidget->setItem(3, chan, item);
    }
}

void Work::PrepareGrid(void)
{

    grid_pxm->fill(Qt::transparent);

    QPainter p(grid_pxm);






    p.setPen(Qt::gray);


    TFlawChannel &vars = FlawChannel[ActiveChannel];


    p.drawLine(display_left, display_bottom - display_height, display_left, display_bottom);
    p.drawLine(display_left, display_bottom, display_left + display_width, display_bottom);
    p.drawLine(display_left + display_width, display_bottom, display_left + display_width, display_bottom - display_height);
    p.drawLine(display_left, display_bottom - display_height, display_left + display_width, display_bottom - display_height);

    QFontMetrics fm = p.fontMetrics();

    for (int i = 0; i <= 10; i++)
    { // вертикальные точки линейки
        int y = display_bottom - ceil(i * 10 * display_yscale);
        p.drawLine(display_left, y, display_left + 3, y);
        p.drawLine(display_left + display_width, y, display_left + display_width - 3, y);
    }

    double range_kof = 0.01;
    // int out_offset = vars.delay;
    //int out_count = vars.range;

    // формируем сетку
    // определяем координату начала экрана
    double dstart = vars.delay * range_kof;
    // считаем шаг сетки
    double net_step = 1;
    double net_step_scale = 0.1;
    double dwidth = vars.range * range_kof;
    while (net_step * net_step_scale * display_width / dwidth < 40)
    {
        if (net_step == 1) net_step = 1.5;
        else if (net_step == 1.5) net_step = 2;
        else if (net_step == 2)   net_step = 2.5;
        else if (net_step == 2.5) net_step = 3;
        else if (net_step == 3)   net_step = 4;
        else if (net_step == 4)   net_step = 5;
        else if (net_step == 5)   net_step = 7.5;
        else {net_step = 1;       net_step_scale *= 10; }
    }
    net_step *= net_step_scale;
    // находим первую линию сетки, при движении из нулевой линии вправо
    double net_x = 0;
    while (net_x < dstart) net_x += net_step;
    while (net_x - net_step > dstart) net_x -= net_step;
    while (net_x < dstart + dwidth)
    {
        int xx = display_left + display_width * (net_x - dstart) / dwidth;
        for (int y = 1; y <= 10; y++)
        {
            int yy = display_bottom - ceil(y * 10 * display_yscale);
            int xl = xx - 2; if (xl < display_left) xl = display_left;
            int xr = xx + 2; if (xr > display_left + display_width) xr = display_left + display_width;
            int yt = yy - 2;
            int yb = yy + 2;
            if (y == 10)
            {
                xl = xx;
                xr = xx;
                yt += 2;
                yb += 2;
            }
            p.drawLine(xl, yy, xr, yy);
            p.drawLine(xx, yt, xx, yb);

        }
        p.drawLine(xx, display_bottom, xx, display_bottom + 6);
        QString nx = QString::number(net_x);
        //if (fabs(net_x) >= 100) nx = FormatFloat("0.0", net_x);

        int width = fm.width(nx);
        p.drawText(xx - width / 2. , display_bottom + 20, nx);

        net_x += net_step;
    }
    p.drawText(display_left + display_width - 15, display_bottom + 33, "мкс");
    UpdateAScanPixmap();
    emit UpdateAScan();
}


void Work::UpdateAScanPixmap()
{

    QPainter p(ascan_pxm);

    QPen pen;

    TFlawChannel &vars = FlawChannel[ActiveChannel];
    // очистим фон картинки
    ascan_pxm->fill(Qt::black);

    p.drawPixmap(0,0, *grid_pxm);
    double range = vars.range;
    int rate = 2; // мышью можно будет осуществлять регулировки только в большом а-скане, размеры которго вдвое больше по ширине и высоте

    for (int i = 0; i < 20; i++)
        vrchPointRect[i] = QRect(-1, -1, -1, -1);

        // рисуем график ВРЧ
    if (vars.vrch_go)
    {
        pen.setColor(Qt::blue);
        p.setPen(pen);
        // находим минимальное усиление
        int min_gain = 0;
        if (vars.vrch_points > 0)
        {
            min_gain = vars.vrch_gains[0];
            for (int i = 1; i < vars.vrch_points; i++)
                if (min_gain > vars.vrch_gains[i]) min_gain = vars.vrch_gains[i];
            min_gain = vars.vrch_gains[0] - min_gain;
        }
        int prev_x = 0;
        int prev_y = 10 + (display_height - 2 * (min_gain + vars.gain));
        int x;
        int y;

        for (int vp = 0; vp < vars.vrch_points; vp++)
        {
            x = (vars.vrch_times[vp] * 50 - vars.delay ) * display_width / vars.range + display_left;
            y = 10 + (display_height - 2 * (vars.vrch_gains[vp] - vars.vrch_gains[0] + min_gain + vars.gain));
            p.drawLine(prev_x, prev_y, x, y);
            prev_x = x;
            prev_y = y;

            vrchPointRect[vp].setCoords((x - 5)*rate, (y - 5)*rate, (x + 5)*rate, (y + 5)*rate);
        }
        if (x < display_width) p.drawLine(x, y, display_left + display_width, y);


    }


    // рисую а-зону
    pen.setColor(Qt::red);
    p.setPen(pen);
    int x1 = vars.a_start;
    int x2 = vars.a_start + vars.a_width;
    int y = vars.a_thresh * 2;
    if (!vars.detector) y = display_height/2 - y;
    else y = display_height - abs(2*y);

    x1 *= display_width / range;
    x2 *= display_width / range;


    if (x1 < 0) x1 = 0;
    if (x2 > display_width) x2 = display_width;

    x1 += display_left;
    x2 += display_left;
    y  += 10;

    p.drawLine(x1, y, x2, y);
    p.drawLine(x1, y - 2, x1, y + 2);
    p.drawLine(x2, y - 2, x2, y + 2);
    a_start_rect.setCoords((x1 - 5)*rate, (y - 5)*rate, (x1 + 5)*rate, (y + 5)*rate);
    a_end_rect.setCoords((x2 - 5)*rate, (y - 5)*rate, (x2 + 5)*rate, (y + 5)*rate);


    // рисую b-зону
    pen.setColor(Qt::magenta);
    p.setPen(pen);
    x1 = vars.b_start;
    x2 = vars.b_start + vars.b_width;
    y = vars.b_thresh * 2;
    if (!vars.detector) y = display_height/2 - y;
    else y = display_height - abs(2*y);

    x1 *= display_width / range;
    x2 *= display_width / range;


    if (x1 < 0) x1 = 0;
    if (x2 > display_width) x2 = display_width;

    x1 += display_left;
    x2 += display_left;
    y  += 10;

    p.drawLine(x1, y, x2, y);
    p.drawLine(x1, y - 2, x1, y + 2);
    p.drawLine(x2, y - 2, x2, y + 2);
    b_start_rect.setCoords((x1 - 5)*rate, (y - 5)*rate, (x1 + 5)*rate, (y + 5)*rate);
    b_end_rect.setCoords((x2 - 5)*rate, (y - 5)*rate, (x2 + 5)*rate, (y + 5)*rate);

    pen.setColor(Qt::green);
    p.setPen(pen);


    if (FlawChannel[ActiveChannel].detector)
    {
        for (int i = 0; i < AscanLength - 1; i++)
        {
            int y1 = display_bottom - display_yscale * AScanMax[i];
            int y2 = display_bottom - display_yscale * AScanMax[i + 1];
            if (y1 < display_top) y1 = display_top;
            if (y2 < display_top) y2 = display_top;

            p.drawLine(display_left + i, y1, display_left + i + 1, y2);
        }
    }
    else
    {
        int y1_prev = AScanMin[0];
        int y2_prev = AScanMax[0];
        if (y1_prev < -100) y1_prev = -100;
        if (y1_prev >  100) y1_prev =  100;
        if (y2_prev < -100) y2_prev = -100;
        if (y2_prev >  100) y2_prev =  100;
        // int rp = 0;
        for (int i = 0; i < AscanLength; i++)
        {
            int y1 = AScanMin[i];
            int y2 = AScanMax[i];
            if (y1 < -100) y1 = -100;
            if (y1 >  100) y1 =  100;
            if (y2 < -100) y2 = -100;
            if (y2 >  100) y2 =  100;

            if (y1 > y2_prev) y1 = y2_prev;
            if (y2 < y1_prev) y2 = y1_prev;

            y1_prev = y1;
            y2_prev = y2;

            y1 = display_bottom - display_yscale * 0.5 * (100 + y1);
            y2 = display_bottom - display_yscale * 0.5 * (100 + y2);

            int x = display_left + i;
            //if (y1 == y2) p.drawPoint(x, y1);
            //else
            {
                p.drawLine(x, y1, x, y2);
            }
        }
    }





    //     p.setFont(QFont("Arial", 10));
    //     p.setPen((Qt::white));
    //     p.drawText(20, 20, QString("Pos X %1 Y %2").arg(mouse_pos.x()).arg(mouse_pos.y()));

    emit UpdateAScan();
    // WorkForm->ui->AScanFreqLabel->

}
