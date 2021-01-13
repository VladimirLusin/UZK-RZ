#include "newinspection.h"
#include "ui_newinspection.h"
#include <QPainter>
#include <windows.h>
#include "view3d.h"
#include "udthread.h"
#include "modbusthread.h"
#include "QTime"
#include "QTimer"
#include "QFile"
#include <QString>
#include <QMessageBox>
//#include <QPrinter>
//#include <QPrintDialog>
#include "enc600.h"

//#include "progressdialog.h"
#include "qprintselectionpicturesdialog.h"
#include "work.h"

QString ResultFileIdent = "УЗК-РЗ: результат контроля заготовки 01-08-2020";

Inspection *InspectionForm;

Inspection::Inspection(QWidget *parent/*,  Qt::WindowFlags flags*/) :
    QMainWindow(parent/*, flags*/),
    ui(new Ui::Inspection)
{
    ui->setupUi(this);


    for (int i = 0; i < ChannelsCount; i++)
    {
        view_3d[i] = new View3d(this, i);
        view_3d[i + ChannelsCount] = new View3d(this, i);
        ChannelNumberLabel[i] = new QLabel(this);
        ChannelNumberLabel[i]->setText("Канал " + QString::number(i+1));

        QFont font =  ChannelNumberLabel[0]->font();
        font.setPointSize(12); //установка высоты шрифта,
        font.setBold(true);
        ChannelNumberLabel[i]->setFont(font);
        ChannelNumberLabel[i]->adjustSize();
    }

    connect(this, SIGNAL(redraw_scene0()), this->view_3d[0],  SLOT(update()));// SLOT(DrawScene()));//
    connect(this, SIGNAL(redraw_scene1()), this->view_3d[1], SLOT(update()));// SLOT(DrawScene()));//
    connect(this, SIGNAL(redraw_scene2()), this->view_3d[2], SLOT(update()));// SLOT(DrawScene()));//
    connect(this, SIGNAL(redraw_scene3()), this->view_3d[3], SLOT(update()));// SLOT(DrawScene()));//


    QTime midnight(0,0,0);
    qsrand(midnight.secsTo(QTime::currentTime()));




    zoom_scan_form = new NewZoomScan();
    connect(this, SIGNAL(redraw_zoom_scan()), zoom_scan_form, SLOT(UpdateScan()));


    connect(this->view_3d[0], SIGNAL(zoom_c_scan(int, int)), this, SLOT(OpenZoomCscan(int, int)));
    connect(this->view_3d[1], SIGNAL(zoom_c_scan(int, int)), this, SLOT(OpenZoomCscan(int, int)));
    connect(this->view_3d[2], SIGNAL(zoom_c_scan(int, int)), this, SLOT(OpenZoomCscan(int, int)));
    connect(this->view_3d[3], SIGNAL(zoom_c_scan(int, int)), this, SLOT(OpenZoomCscan(int, int)));

    timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(UpdateWidgets()));
    timer->start(40);



    alarm_widget = new AlarmWidget(this);




    alarm_widget->resize(400, 265);

    alarm_widget->setAutoFillBackground(true);
    alarm_widget->setVisible(false);
    connect(ModbusThread, SIGNAL(UpdateAlarms()), alarm_widget, SLOT(UpdateIndicators()));


//    CurrentCoordLabel = new QLabel(this);
//    CurrentCoordLabel->setText("Текущие координаты: высота --- мм, угол --- град.");

//    CurrentCoordLabel->setFont(QFont("Arial", 12, QFont::Normal));
//    CurrentCoordLabel->move(1275, 40);
//    CurrentCoordLabel->adjustSize();

    ui->CurrentParametersTableWidget->setSpan(4,0,1,2);
    ui->CurrentParametersTableWidget->setSpan(5,0,1,2);

    ui->FlawAnalysisTitleLabel->adjustSize();
    ui->CurrentValuesTitleLabel->adjustSize();

    for (int i = 0; i < ChannelsCount; i++)
    {
        label[i] = new QLabel(this);
        label[i]->resize(50, 120);
        ui->CurrentParametersTableWidget->setCellWidget(0,i,label[i]);
        //ui->CurrentParametersTableWidget->setColumnWidth(i,140);

    }
    ui->CurrentParametersTableWidget->setRowHeight(0,125);
    //signal_widget = new SignalWidget(this, true);
    ui->signal_widget->BlockSignals = true;
    ui->signal_widget->big_size = false;
    ui->signal_widget->setGeometry(1100, 480, 530, 450);
    //connect(WorkForm, SIGNAL(UpdateAScan()), signal_widget, SLOT(update()));

    connect(ui->Chan1Button, SIGNAL(clicked()), SLOT(AScanChannelSelect()));
    connect(ui->Chan2Button, SIGNAL(clicked()), SLOT(AScanChannelSelect()));

}

Inspection::~Inspection()
{

    for (int i = 0; i < ChannelsCount*2; i++)
        delete view_3d[i];

    delete zoom_scan_form;


    timer->stop();
    delete timer;
    //delete alarm_widget;
//    delete signal_widget;
    delete ui;

}

//------------------------------------------------------------------------------------------------------------------------------------------
bool Inspection::run(int mode, QFile *file)
{
    memset(print_it, 0, sizeof(print_it));
    Mode = mode;
    AnalysisCompleted = false;
    ui->FlawAnalysisTitleLabel->setVisible(false);
    ui->tableWidget->setVisible(false);
    ui->go_to_flaw_Button->setVisible(false);
    ui->HighlightFlaws_checkBox->setEnabled(false);
    ui->HighlightFlaws_checkBox->setChecked(false);
    HighlightFlaws = false;
    WriteResult = false;
    int diameter = BilletBottomDiameter;
    if (BilletTopDiameter > BilletBottomDiameter) diameter = BilletTopDiameter;

    if (ControlStep)
    {
        StepPerRevolution = M_PI * diameter / ControlStep + 1; // количество шагов на каждый оборот

    }

  /*  if (StepPerRevolution > MAX_POINTS_PER_REV)
    {
        CircumStep = M_PI * diameter / MAX_POINTS_PER_REV + 1;
        StepPerRevolution = M_PI * diameter / CircumStep + 1;
    }*/
    PulsesPerStep = PulsesPerRevolution * ControlStep / M_PI / diameter; // количество импульсов энкодера на один шаг контроля

    // вычисляю количество линий контроля: по теореме Пифагора вычисляю длину образующей конуса и делю на шаг контроля
    //LinesCount = pow(pow((BilletBottomDiameter-BilletTopDiameter)/2., 2) + pow(BilletHeight, 2), 0.5) / ControlStep;

    // в новой редакции рассчитываю количество линий как протяженность участка контроля, поделенную на величину шага,
    // потому что и то, и другое отсчитываю по образующей
    if (ControlStep)
        LinesCount = ControlAreaHeight / ControlStep;

    // если количество шагов контроля превышает 500 по окружности или 170 по вертикали, устанавливаю шаг контроля
    // для отрисовки равным 20 мм, иначе приравниваю шаг контроля для отрисовки к заданному шагу контроля
    if (StepPerRevolution > MAX_DRAW_POINTS_PER_REV || LinesCount > MAX_DRAW_LINES)
    {
        DrawControlStep = 15;
        DrawPulsesPerStep = PulsesPerRevolution * DrawControlStep / M_PI / diameter; // количество импульсов энкодера на один шаг отрисовки
        DrawStepPerRevolution = M_PI * diameter / DrawControlStep + 1; // количество шагов отрисовки на каждый оборот
        if (DrawStepPerRevolution > MAX_DRAW_POINTS_PER_REV) DrawStepPerRevolution = MAX_DRAW_POINTS_PER_REV;
        DrawLinesCount = ControlAreaHeight / DrawControlStep;
        if (DrawLinesCount > MAX_DRAW_LINES) DrawLinesCount = MAX_DRAW_LINES;
    }
    else
    {
        DrawControlStep = ControlStep;
        DrawPulsesPerStep = PulsesPerStep;
        DrawStepPerRevolution = StepPerRevolution;
        DrawLinesCount = LinesCount;
    }



    // рассчитываю общее количество линий для отрисовки: по теореме Пифагора вычисляю длину образующей конуса и делю на шаг контроля

    if (DrawControlStep) DrawTotalLinesCount = pow(pow((BilletBottomDiameter/2-BilletTopDiameter/2), 2) + pow(BilletHeight, 2), 0.5) / DrawControlStep;
    // рассчитываю номер линии, с которой начинается контроль
    if (DrawControlStep) DrawStartLine = StartInspectPosition / DrawControlStep;

    Calc3dView();
    Calc2dView();


    //resizeEvent(NULL);
    ui->CurrentValuesTitleLabel->setVisible(false);
    ui->CurrentParametersTableWidget->setVisible(false);
    ui->groupBox->setVisible(false);
    ui->signal_widget->setVisible(false);
    ui->resume_Button->setVisible(false);

    if (mode == INSPECTION)
    {
        ReportOperator = "";
        if (PersonnelCount) ReportOperator = Personnel[CurrentOperatorIndex];
        InspectionResume = "";
        memset(CScan, -1, sizeof(CScan));
        memset(bTime, 0xFF, sizeof(bTime));
        memset(PressedCScan, -1, sizeof(PressedCScan));
        for (int i = 0; i < ChannelsCount; i++)
        {
            view_3d[i]->Mode = MODE_3D;
            view_3d[ChannelsCount + i]->Mode = MODE_2D;
        }
        ui->abort_control_pushButton->setVisible(true);
        ui->AlarmButton->setVisible(true);
        // копирую браковочные уровни
        for (int ch = 0; ch < ChannelsCount; ch++)
        {
            Threshold[ch] = FlawChannel[ch].a_thresh;
        }
        //CurrentCoordLabel->setVisible(true);
        ui->CurrentValuesTitleLabel->setVisible(true);
        ui->CurrentParametersTableWidget->setVisible(true);
        ui->groupBox->setVisible(true);
        ui->signal_widget->setVisible(true);

        ui->HighlightFlaws_checkBox->setEnabled(false);

        exit = false;

        // считаю, укладываюсь ли я в массив С-скана и другие геометрические параметры
        if (ControlStep == 0)
        {
            QMessageBox::warning(0, "Некорректные данные", "Шаг контроля не должен быть равен нулю!");
            return false;
        }

        if (ControlAreaHeight > pow(pow((BilletBottomDiameter-BilletTopDiameter)/2., 2) + pow(BilletHeight, 2), 0.5))
        {
            QMessageBox::warning(0, "Некорректные данные", "Высота контролируемого участка больше длины образующей изделия!");
            return false;
        }

        if (ControlStep > ControlAreaHeight)
        {
            QMessageBox::warning(0, "Некорректные данные", "Шаг контроля больше высоты контролируемой зоны!");
            return false;
        }
        if (ControlStep > diameter * M_PI)
        {
            QMessageBox::warning(0, "Некорректные данные", "Шаг контроля больше длины окружности изделия!");
            return false;
        }
        if (LinesCount > MAX_LINES_COUNT)
        {
            QMessageBox::warning(0, "Некорректные данные","Количество шагов контроля по высоте изделия слишком велико - число линий более 3500! Необоходимо увеличить шаг или уменьшить протяженность зоны контроля.");
            return false;
        }
        if (StepPerRevolution > MAX_POINTS_PER_REV)
        {

            QMessageBox::warning(0,"Некорректные данные", "Количество точек контроля по окружности изделия слишком велико - число точек более 10500! Необоходимо увеличить шаг контроля.");
            return false;
        }


        // угол между образующей и вертикалью
        Angle = atan((BilletBottomDiameter-BilletTopDiameter)/2./BilletHeight);

        //divizor = cos(Angle) * DrawPulsesPerStep;
#ifndef emulation2
        // рассчитываю скорость вращения изделия, например, в импульсах в секунду
        //WORD rotation_speed = PulsesPerStep * UdThread->FreqPeriod / 3.; // из расчета 3 посылок на шаг контроля
        // заказчик попросил задавать в герцах, помноженных на 100 - это черт знает что такое!!!                            оборотах в минуту
        const double rate = 15 * 38; // при 15 Гц планшайба совершает 1 оборот за 38 секунд
        RotationSpeed = freq * 100. * rate / 3. / (double)StepPerRevolution; // из расчета 3 посылок на шаг контроля
        if (RotationSpeed > 2500) RotationSpeed = 2500; // ограничиваю обороты, а то все навернется
        // вычисляю количество импульсов при перемещении датчика на один шаг контроля
        // это уже другой энкодер, и для него надо знать количество шагов на единицу длины
        PulsesPerCoil = LinearMovementPulsesPerMM * ControlStep * cos(Angle);

        SendVelocityAndStep = true; // выставляю флаг, по которому поток модбаса отправит данные

        // проверяю, нет ли сигнала аварии
#ifdef alarm
        //unsigned char index = ENC6_GET_INDEX(EncoderCardNumber, X5_axis);
        //Alarm = !(index & 0x01);
        if (Alarm)
        {

            QMessageBox::warning(0, "Авария", "Контроль невозможен. Авария на установке.");
            return false;
        }
#endif
        // отправляю команду по Modbus для старта контроля
        StartInspection = true;

        ConfirmStartInspection = false;


#endif
        QTimer::singleShot(5, this, SLOT(ControlProcess()));


    }

    ui->Chan1Button->setChecked(ActiveChannel == 0);
    ui->Chan2Button->setChecked(ActiveChannel == 1);

    // почему-то окно глючило, если я при создании или открытии задавал ему сразу полноэкранный режим
    // при повторном вызове оно не показывало таблицу дефектов, то есть таблица не выводилась на экран
    // после проведения анализа дефектов, чекбоксы тоже отображались некорректно, например, выглядели
    // как неактивные (серые), но на щелчки реагировали, хотя галочка не проставлялась
    // пришлось извратиться - сначала вывожу окно в просто развернутом виде - с заголовком, рамкой,
    // кнопками закрытия, потом сразу же даю команду на вывод в полноэкранном режиме

    setWindowState(Qt::WindowMaximized);
    show();
    showFullScreen();

    if (mode == VIEV_RESULT)
    {
        for (int i = 0; i < ChannelsCount; i++)
        {
            view_3d[i]->Mode = VIEW_RESULT_3D;
            view_3d[ChannelsCount + i]->Mode = VIEW_RESULT_2D;
        }
        ui->abort_control_pushButton->setVisible(false);
        ui->AlarmButton->setVisible(false);
        //CurrentCoordLabel->setVisible(false);
        if (file != NULL)
            if (!OpenResult(*file)) return false;
    }


    return true;
}

//------------------------------------------------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::ControlProcess()
{
#ifndef emulation2
    // в течение 5 секунд проверяю, не пришло ли подтверждение от ПЛК о начале контроля
    if (!ConfirmStartInspection)
    {
        DWORD time_out = GetTickCount() + 5000;
        while (GetTickCount() < time_out)
        {
            QApplication::processEvents();
            // проверяю, не пришло ли подтверждение от контроллера о старте контроля
            // т.е. проверяю вход HR1
            unsigned char x1_index = ENC6_GET_INDEX(EncoderCardNumber, X1_axis);
            if (!(x1_index & 0x01))
            {
                ConfirmStartInspection = true;
                break;
            }
        }
    }

    // задержка 3 секунды на помехи
    DWORD time_out = GetTickCount() + 3000;
    while (GetTickCount() < time_out)
    {
        QApplication::processEvents();
    }
#else
    AbsCirclePosition = 0;
    CurrentVertCoord = 250;
#endif

    CurrentLine = 0; // текущая линия (виток) контроля
#ifndef emulation2
    if (ConfirmStartInspection)
#endif
    {

        // положение энкодера - поворот стола
        //CirclePosition = ENC6_GET_ENCODER(EncoderCardNumber, X1_axis);
        // фиксирую начальную координату по вертикали, чтобы потом можно было вернуться на дефект
        StartVertPosition = CurrentVertCoord;
        // считаю, что эта координата соответствует заданному начальному положению зоны контроля
        ScannerFormingCoord = StartInspectPosition; // StartInspectPosition - это заданеное перед контролем начальное положение зоны контроля

        //ScannerVertCoord = StartInspectPosition * cos(Angle);
        //ScannerHorCoord = BilletBottomDiameter/2. + StartInspectPosition * sin(Angle);

        //WriteResult = true; // разрешаю запись результатов
        //while (ScannerFormingCoord < StartInspectPosition + ControlAreaHeight)//
#ifdef emulation2
        DWORD tick = GetTickCount();
#endif
        while (CurrentLine < LinesCount)
        {
            if (exit) break;
            int start_circle_position = AbsCirclePosition;
            WriteResult = true;
            while (AbsCirclePosition < start_circle_position + PulsesPerRevolution * 1.1)
            {
#ifdef emulation2
                if (GetTickCount() > tick)
                {
                    tick = GetTickCount() + 10;
                    AbsCirclePosition += 200;
                    CurrentCirclePosition = AbsCirclePosition % PulsesPerRevolution;
                }
#endif

                QApplication::processEvents();
                CurrentAngle = 360.* CurrentCirclePosition/PulsesPerRevolution;

#ifdef alarm
                // проверяю, нет ли сигнала аварии
//                unsigned char index = ENC6_GET_INDEX(EncoderCardNumber, X5_axis);
//                Alarm = !(index & 0x01);
                if (Alarm)
                {
                    QMessageBox::warning(0, "Авария", "Контроль прерван. Авария на установке.");
                    exit = true;
                }
#endif

                if (exit) break;

            }
            // проверяю, вся линия просканирована или нет
            // если есть пропуски, совершаю еще один оборот, и так, пока не устраню все пропуски
            bool full_line_scaned = true;
            for (int ch = 0; ch < ChannelsCount; ch++)
                for (int i = 0; i < StepPerRevolution; i++)
                    if (CScan[ch][CurrentLine][i] == -1)
                    {
                        full_line_scaned = false;
                        break;
                    }
            if (full_line_scaned)
            {
                WriteResult = false;
#ifndef emulation2
                if (GoScannerToNextCoil()) // переход сканера на следующий виток
#else
                CurrentVertCoord += LinearMovementPulsesPerMM * ControlStep * cos(Angle)+0.5;
                ScannerFormingCoord = StartInspectPosition + (CurrentVertCoord - StartVertPosition) / cos(Angle) / LinearMovementPulsesPerMM;
#endif
                {
                    CurrentLine++;
                }
#ifndef emulation2
                else exit = true;
#endif
            }
        }
//#endif
        WriteResult = false;

        // снимаю сигнал автоматического режима
        StopInspection = true;


        ui->abort_control_pushButton->setVisible(false);
        ui->resume_Button->setVisible(true);
        //ui->print_Button->setVisible(true);
        if (exit)
        {
            if (QMessageBox::question(this, "Контроль прерван", "Контроль был прерван. Сохранить результаты?") == QMessageBox::No) return;
        }
        else QMessageBox::information(this, "Окончание контроля", "Контроль окончен. Результаты будут сохранены в файл.");

        // произвожу анализ дефектов
        FlawAnalysis();

        SaveResult();


    }
#ifndef emulation2
    else QMessageBox::warning(this, "Ошибка начала контроля", "Нет подтвеждения от ПЛК!");
#endif
}
//------------------------------------------------------------------------------------------------------------------------------------------
bool Inspection::GoScannerToNextCoil() // переход сканера на следующий виток
{                                      // даю команду ПЛК и жду от него исполнения
#ifdef alarm
    // проверяю, нет ли сигнала аварии
//    unsigned char index = ENC6_GET_INDEX(EncoderCardNumber, X5_axis);
//    Alarm = !(index & 0x01);
    if (Alarm)
    {
        QMessageBox::warning(0, "Авария", "Контроль прерван. Авария на установке.");
        return false;
    }
#endif

    SetNextLine = true;

    DWORD timeout = GetTickCount() + 1000;
    bool confirm = false;
    while (GetTickCount() < timeout)
    {
        QApplication::processEvents();
        // считываю координату сканера по вертикали
        // и рассчитываю координату в миллиметрах
        ScannerFormingCoord = StartInspectPosition + (CurrentVertCoord - StartVertPosition) / cos(Angle) / LinearMovementPulsesPerMM;
        CurrentAngle = 360.* CurrentCirclePosition/PulsesPerRevolution;

        // проверяю, не пришло ли подтверждение от контроллера о переходе на новый виток
        // т.е. проверяю вход HR2
        unsigned char x2_index = ENC6_GET_INDEX(EncoderCardNumber, X2_axis);
        if (!(x2_index & 0x01)) confirm = true;
    }
    // спустя 1 секунду снимаю управляющий сигнал
    ResetNextLine = true;

    // в течение еще 10 секунд жду подтверждение от ПЛК
    timeout = GetTickCount() + 10000;

    while (GetTickCount() < timeout)
    {
        QApplication::processEvents();
        CurrentAngle = 360.* CurrentCirclePosition/PulsesPerRevolution;

        // считываю координату сканера на образующей
        // и рассчитываю координату в миллиметрах
        ScannerFormingCoord = StartInspectPosition + (CurrentVertCoord - StartVertPosition) / cos(Angle) / LinearMovementPulsesPerMM;
        // проверяю, не пришло ли подтверждение от контроллера о переходе на новый виток
        // т.е. проверяю вход HR2
        unsigned char x2_index = ENC6_GET_INDEX(EncoderCardNumber, X2_axis);
        if (!(x2_index & 0x01))
        {
            confirm = true;
            break;
        }
    }
    // введу еще паузу 3 секунды, потому что проходят какие-то помехи
    timeout = GetTickCount() + 3000;

    while (GetTickCount() < timeout)
    {
        QApplication::processEvents();
        CurrentAngle = 360.* CurrentCirclePosition/PulsesPerRevolution;
    }
    if (!confirm)
    {
        QMessageBox::warning(0, "Ошибка контроля", "Нет подтверждения от ПЛК!");
        return false;
    }
    return true;

}


//-------------------------------------------------------------------------------------------------------------------------------------------

/*void Inspection::on_Inspection_rejected()
{
    exit = true;
}*/
//-------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::SaveResult()
{

    QTime time(20, 4, 23, 3);
    QString file_path = QString("d:/UZK-RZ/Результаты/") + QDate::currentDate().toString("dd_MM_yyyy");

    if (!QDir(file_path).exists())
    {
        QDir().mkdir(file_path);
    }


    ResultFileName = file_path + QString("/Uzk_rz ") + QTime::currentTime().toString("hh_mm") + QString(".scan");

    QFile f(ResultFileName);

    if(f.open(QIODevice::WriteOnly))
    {
        wchar_t name[50];
        // запишу идентификатор файла
        //QString label = "УЗК-РЗ: результат контроля заготовки 23-06-2020";
        QStringToWChar(ResultFileIdent, name);
        f.write((char*)name, 50*sizeof(wchar_t));

        // записываю фамилию оператора
        QStringToWChar(ReportOperator, name);
        f.write((char*)name, 50*sizeof(wchar_t));

        // записываю дату контроля
        ReportDate = QDate::currentDate();
        f.write((char*)&ReportDate, sizeof(QDate::currentDate()));

        // записываю в файл геометрические параметры заготовки
        // а также значение шага контроля,
        // количество линий по вертикали и точек в одной линии
        // также нужно будет записывать либо палитру, либо браковочный уровень

        f.write((char*)(&BilletTopDiameter), sizeof(int));
        f.write((char*)(&BilletBottomDiameter), sizeof(int));
        f.write((char*)(&BilletHeight), sizeof(int));
        f.write((char*)(&ControlStep), sizeof(int));
        f.write((char*)(&LinesCount), sizeof(int));
        f.write((char*)(&StepPerRevolution), sizeof(int));
        f.write((char*)(&StartInspectPosition), sizeof(int));
        f.write((char*)(&DrawControlStep), sizeof(int));
        f.write((char*)(&DrawStepPerRevolution), sizeof(int));
        f.write((char*)(&DrawLinesCount), sizeof(int));

        // записываю задействованные каналы
       // bool ChannelEnabled[] = {true, true, true, true, true, true};
       // f.write((char*)(ChannelEnabled), sizeof(ChannelEnabled));

        // записываю браковочные уровни
        for (int ch = 0; ch < ChannelsCount; ch++)
        {

                f.write((char*)(&Threshold[ch]), sizeof(int));
        }

        // записываю уровень палитры

        //f.write((char*)&PaletteRelation, sizeof(double));

        // записываю С-скан активных каналов
        for (int ch = 0; ch < ChannelsCount; ch++)

                for (int i = 0; i < LinesCount; i++)
                    f.write(CScan[ch][i], StepPerRevolution);

        // записываю результаты анализа дефектов
        for (int ch = 0; ch < ChannelsCount; ch++)
        {

            f.write((char*)(&FlawCount[ch]), sizeof(int));

            for (int i = 0; i < FlawCount[ch]; i++)
            {
                f.write((char*)(&DefFormStart[ch][i]), sizeof(int));
                f.write((char*)(&DefFormEnd[ch][i]), sizeof(int));
                f.write((char*)(&DegreeCircStart[ch][i]), sizeof(int));
                f.write((char*)(&DegreeCircEnd[ch][i]), sizeof(int));
                f.write((char*)(&DefDepth[ch][i]), sizeof(double));
                f.write((char*)(&DefAmplitude[ch][i]), sizeof(int));
            }

        }
        // записываю данные заготовки: номер, плавку, и т.д.
        QStringToWChar(BilletNumber, name);
        f.write((char*)name, 25*sizeof(wchar_t));
        QStringToWChar(BatchNumber, name);
        f.write((char*)name, 25*sizeof(wchar_t));
        QStringToWChar(MeltNumber, name);
        f.write((char*)name, 25*sizeof(wchar_t));
        QStringToWChar(CertificateNumber, name);
        f.write((char*)name, 25*sizeof(wchar_t));
        QStringToWChar(MaterialGrade, name);
        f.write((char*)name, 25*sizeof(wchar_t));
        QStringToWChar(AccompanyingDocument, name);
        f.write((char*)name, 25*sizeof(wchar_t));
        QStringToWChar(InspectionResume, name);
        f.write((char*)name, 25*sizeof(wchar_t));

        f.close();
    }

}

//--------------------------------------------------------------------------------------------------------------------------------
void Inspection::OpenZoomCscan(int number, int mode)
{
    zoom_scan_form->run(number, mode);
}

//--------------------------------------------------------------------------------------------------------------------------------

void Inspection::closeEvent(QCloseEvent *event)
{
    if (Mode == INSPECTION)
    {
        exit = true;
    }
    if (zoom_scan_form->isVisible())
        zoom_scan_form->close();
    WorkForm->activateWindow();
}
//------------------------------------------------------------------------------------------------------------------------------------------
const int top_margin = 50;
const int vert_space = 15;
void Inspection::resizeEvent(QResizeEvent *event)
{
    int hor_space = 15;
    int ww = width();
    widget_width = (width() - 2 * 20 - hor_space) / 2;
    widget_height = (height() - top_margin - vert_space * 3 - ui->abort_control_pushButton->height()) / 2;


        widget_width = (width() - 2 * 20 - hor_space - ui->abort_control_pushButton->width()) / 2;
        widget_height = (height() - top_margin - vert_space * 2) / 2;


    if (widget_width > widget_height)
    {
        widget_width = widget_height;
    }
    else
    {
        widget_height = widget_width;
    }

    int number = 0;
    for (int ch = 0; ch < ChannelsCount; ch++)
    {
            view_3d[ch]->setVisible(true);
            view_3d[ch]->setGeometry(20 + (widget_width + hor_space) * number, top_margin, widget_width, widget_height);
            view_3d[ch]->UpdateViewRadius();
            // второй ряд виджетов
            view_3d[ChannelsCount + ch]->setVisible(true);
            view_3d[ChannelsCount + ch]->setGeometry(20 + (widget_width + hor_space) * number, widget_height + top_margin + vert_space, widget_width, widget_height);

            ChannelNumberLabel[ch]->setVisible(true);

            ChannelNumberLabel[ch]->move(view_3d[ch]->x() + (view_3d[ch]->width() - ChannelNumberLabel[ch]->width())/2, 25);
            number++;
    }

    alarm_widget->move(20 + widget_width * 2 + hor_space - alarm_widget->width(), 100);

    // располагаю кнопку и чекбокс
    // верхнее положение динамически созданных виджетов с изображениями отсчитывается от верхнего края окна,
    // а верхнее положение кнопок, которые созданы в конструкторе - от нижнего края главного меню


    ui->CurrentValuesTitleLabel->move(20+widget_width * 2 + hor_space + 20 + (ui->CurrentParametersTableWidget->width() - ui->CurrentValuesTitleLabel->width())/2, 140);

    ui->CurrentParametersTableWidget->move(20+widget_width * 2 + hor_space + 20,
                                           ui->CurrentValuesTitleLabel->y() + ui->CurrentValuesTitleLabel->height() + 5);
    ui->groupBox->move(ui->CurrentParametersTableWidget->x() + ui->CurrentParametersTableWidget->width() + 30 , 10);
    ui->signal_widget->move(ui->groupBox->x(), ui->groupBox->y() + ui->groupBox->height() + 10);

    ui->Scale_checkBox->move(ui->CurrentParametersTableWidget->x() + 30, 30);
    ui->HighlightFlaws_checkBox->move(ui->CurrentParametersTableWidget->x() + 30, 60);

    ui->AlarmButton->move(ui->CurrentParametersTableWidget->x() + 30, 90);
    ui->abort_control_pushButton->move(ui->CurrentParametersTableWidget->x() + 35, 490);
    ui->resume_Button->move(ui->abort_control_pushButton->pos());

}

//-------------------------------------------------------------------------------------------------------------------------------------------------------


//-------------------------------------------------------------------------------------------------------------------------------------------------------
// отправка команд на плату, встроенную в компьютер, для управления дискретными выходами
BYTE EncoderCard_DO_value = 0;
void Inspection::SendByteToEncoderCard(BYTE set_do, BYTE reset_do)
{
    EncoderCard_DO_value |= set_do;
    EncoderCard_DO_value &= ~reset_do;
    //if (EncoderCardReady)
        ENC6_DO(EncoderCardNumber, EncoderCard_DO_value);

}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::on_abort_control_pushButton_clicked()
{
    exit = true;
}
//--------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::FlawAnalysis()
{
    timer->stop();
    row_count = 0; // число строк в таблице - соответсвует каналу с наибольшим количеством дефектов
    for (int ch = 0; ch < ChannelsCount; ch++)
//        if (ChannelEnabled[ch])
        {
            FlawCount[ch] = 0;
            memset(DefMap, 0, sizeof(DefMap));
            // пытаемся найти единичный дефект, чтобы потом начать с ним работать
            for (int line = 0; line < LinesCount; line++)
            {
                if (FlawCount[ch] == ScanMaxDefCount) break;
                //ui->CurrentValuesTitleLabel->setText("Анализ " + QString::number(line) + " линии");
                QApplication::processEvents();
                for (int step = 0; step < StepPerRevolution; step++)
                {
                    //if (UserStop) break;
                    if (CScan[ch][line][step] < 0) continue;
                    if (DefMap[line][step] != 0) continue;
                    if (FlawCount[ch] == ScanMaxDefCount) break;
#ifndef emulation
                    if (CScan[ch][line][step] < Threshold[ch]) continue;
#else
                    if (CScan[ch][line][step] < 50) continue;
#endif



                    // нашлась какая-то точка, еще не обработанная, с уровнем сигнала выше порога
                    DefMap[line][step] = FlawCount[ch] + 1;
                    bool have_new_points = true;
                    while (have_new_points)
                    {
                        // пока есть незадействованные точки, пробегаю всю область контроля, и смотрю, нет ли
                        // дефектных точек вблизи уже выявленных дефектных точек с текущим номером дефекта
                        // если такие точки есть, включаю их в текущий дефект, присваивая им текущий номер дефекта
                        have_new_points = false;
                        for (int fx = 0; fx < LinesCount; fx++)
                        {
                            QApplication::processEvents();
                            for (int fy = 0; fy < StepPerRevolution; fy++)
                            {
                                if (DefMap[fx][fy] == FlawCount[ch] + 1)
                                {
                                    // проверяю 8 точек в окрестности уже включенной в дефект точки
                                    for (int frx = fx - 1; frx <= fx + 1; frx++)
                                    {
                                        if ((frx < 0) || (frx >= LinesCount)) continue;
                                        for (int fry = fy - 1; fry <= fy + 1; fry++)
                                        {
                                            // определяю координаты точки на С-скане
                                            int yy = fry;
                                            if (yy < 0) yy = StepPerRevolution + yy;  // это для определения точек, которые оказались на другом
                                            if (yy == StepPerRevolution) yy = 0;   // краю скана, т.е. если дефект получился разорванным
                                            //if ((yy < 0) || (yy >= fry_count)) continue;
                                            if (DefMap[frx][yy] != 0) continue;
                                            if (CScan[ch][frx][yy] < 0) continue;
#ifndef emulation
                                            if (CScan[ch][frx][yy] < Threshold[ch]) continue;
#else
                                            if (CScan[ch][frx][yy] < 50) continue;
#endif
                                            // если точка еще свободная и значение в ней выше порога, включаю ее в дефект
                                            DefMap[frx][yy] = FlawCount[ch] + 1;
                                            have_new_points = true;
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // определяю крайние координаты дефекта
                    //int def_summ = 0;
                    int posf_min = LinesCount, posf_max = 0;  // крайние координаты дефекта по образующей (forming - образующая)
                    int posc_min = StepPerRevolution, //MaxInt,
                            posc_max = 0;  // крайние координаты дефекта по окружности (circle)
                    //int degy_min = MaxInt, degy_max = 0;  // крайние координаты по окружности для шкалы в градусах
                    bool def_left = false;
                    bool def_right = false;
                    int amplitude = 0; // здесь запомню максимальную амплитуду
                    int min_b_time = 100000000; // запоминаю минимальное и максимальное время для оценки глубины залегания
                    int max_b_time = 0;

                    for (int sx = 0; sx < LinesCount; sx++)
                    {
                        for (int sy = 0; sy < StepPerRevolution; sy++)
                        {
                            if (DefMap[sx][sy] == FlawCount[ch] + 1)
                            {
                                if (sy == 0) def_left = true;
                                if (sy == StepPerRevolution - 1) def_right = true;
                                if (posf_min > sx) posf_min = sx;
                                if (posf_max < sx) posf_max = sx;
                                if (max_b_time < bTime[ch][sx][sy]) max_b_time = bTime[ch][sx][sy];
                                if (min_b_time > bTime[ch][sx][sy]) min_b_time = bTime[ch][sx][sy];
                                //int su = sy * uk;
                                if (posc_min > sy) posc_min = sy;
                                if (posc_max < sy) posc_max = sy;
                                if (CScan[ch][sx][sy] > amplitude) amplitude = CScan[ch][sx][sy];

                                //posx += sx;
                                // posy += sy;
                                //def_summ++;
                            }
                        }
                    }

                    posf_min *= ControlStep; // координаты по высоте пересчитываю в миллиметры
                    posf_max = (posf_max + 1)*ControlStep; // прибавляю 1, чтобы учесть размер последнего шага


                    DefFormStart[ch][FlawCount[ch]] = posf_min + StartInspectPosition;
                    DefFormEnd[ch][FlawCount[ch]]   = posf_max + StartInspectPosition;
                    //DefYStart[FlawCount] = posy_min;
                    //DefYEnd[FlawCount]   = posy_max;
                    DegreeCircStart[ch][FlawCount[ch]] = posc_min  * 360 * 100/ (double)StepPerRevolution; // координаты по окружности пересчитываю в градусы
                    DegreeCircEnd[ch][FlawCount[ch]] = (posc_max+ 1) * 360 *100/ (double)StepPerRevolution; // домножаю на 100, чтобы повысить точность

                    DefCenterVert[ch][FlawCount[ch]] = (posf_min + posf_max) / 2. * cos(Angle); // пересчитываю в вертикальную координату, т.к. в ПЛК нужно передавать ее
                    DefCenterCirc[ch][FlawCount[ch]] = (posc_min + posc_max + 1); // в шагах контроля - удвоенное значение
                    // если дефект переходит через край, пересчитываю угловые координаты
                    // буду считать, что дефект не настолько протяженный, чтобы заходить за отметку 180 градусов с той или другой стороны,
                    // то есть полностью находится вблизи отметки 0-360 градусов
                    if (def_left && def_right)
                    {
                        posc_min = StepPerRevolution;
                        posc_max = 0;
                        for (int sx = 0; sx < LinesCount; sx++)
                        {
                            for (int sy = 0; sy < StepPerRevolution; sy++)
                            {
                                if (DefMap[sx][sy] == FlawCount[ch] + 1)
                                {
                                    if (posc_min > sy && sy > StepPerRevolution / 2) posc_min = sy;
                                    if (posc_max < sy && sy < StepPerRevolution / 2) posc_max = sy;
                                }
                            }
                        }

                        DegreeCircStart[ch][FlawCount[ch]] = posc_min  * 360 * 100 / (double)StepPerRevolution; // координаты по окружности пересчитываю в градусы
                        DegreeCircEnd[ch][FlawCount[ch]] = (posc_max + 1) * 360 * 100 / (double)StepPerRevolution;

                        DefCenterCirc[ch][FlawCount[ch]] = (posc_min + posc_max + 1); // в шагах контроля - удвоенное значение

                        if (DefCenterCirc[ch][FlawCount[ch]] > StepPerRevolution) DefCenterCirc[ch][FlawCount[ch]] -= StepPerRevolution;
                        else DefCenterCirc[ch][FlawCount[ch]] += StepPerRevolution;
                    }
                    DefAmplitude[ch][FlawCount[ch]] = amplitude;
                    DefDepth[ch][FlawCount[ch]] = ((min_b_time + max_b_time) / 2. - PrismDelay[ch]) * 1.E-5 * Velocity * 0.5;
                    FlawCount[ch]++;

                }
            }

            if (FlawCount[ch]>row_count) row_count = FlawCount[ch];
        }
    AnalysisCompleted = true;
    CreateDefectTable();

#ifndef emulation
    if (Mode==INSPECTION) // кнопку "выйти на дефект" вывожу только в режиме контроля, при просмотре ранее полученных результатов ее не будет
#endif
        ui->go_to_flaw_Button->setVisible(true);

    timer->start(40);
}

bool GoToFlawProcess = false;
bool StopGoToFlaw = false;

void Inspection::on_go_to_flaw_Button_clicked()
{
    if (!GoToFlawProcess)
    {

        // выход на дефект по нажатию на кнопку
        // возможность выхода на дефект будет ТОЛЬКО В РЕЖИМЕ КОНТРОЛЯ, ТО ЕСТЬ КОГДА РЕЗУЛЬТАТ ТОЛЬКО ЧТО ПОЛУЧЕН
        // круговую координату в импульсах я легко определю, умножив координату в градусах на количество импульсов на 1 градус
        // координату по образующей надо расчитать, зная либо текущую координату, либо стартовую координату при контроле
        int channel_number = (ui->tableWidget->currentColumn() - 1) / 4;
        if (channel_number < 0) return;

        bool SignalAboveStrobe = false;

        int flaw_number = ui->tableWidget->currentRow() - 3;
        if (flaw_number < 0) return;
        if (flaw_number >= FlawCount[channel_number]) return;


#ifndef emulation2
        TargetCircleCoord = DefCenterCirc[channel_number][flaw_number] * PulsesPerRevolution / 2 / StepPerRevolution;
        TargetVertCoord = DefCenterVert[channel_number][flaw_number] * LinearMovementPulsesPerMM + StartVertPosition;
#endif
#ifdef alarm
        // проверяю, нет ли сигнала аварии
        if (Alarm)
        {
            QMessageBox::warning(0, "Ошибка при выходе на дефект", "Выход на дефект невозможен. Авария на установке.");
            return;
        }
#endif




        GoToFlawProcess = true;
        StopGoToFlaw = false;
        ui->go_to_flaw_Button->setText("ОСТАНОВИТЬ");
        // отправлю по Modbus сигнал начала выхода на дефект
#ifndef emulation2
        SetGoToFlaw = true;
#endif
        bool ConfirmPLC = false; // подтверждение от ПЛК о выходе на дефект
        DWORD time_out = GetTickCount() + 120000; // 120 секунд жду подтверждение от ПЛК

        while (GetTickCount() < time_out)
        {
            QApplication::processEvents();
            if (StopGoToFlaw) break;
            // считываю координату сканера на образующей
            // и рассчитываю координату в миллиметрах
            ScannerFormingCoord = StartInspectPosition + (CurrentVertCoord - StartVertPosition) / cos(Angle) / LinearMovementPulsesPerMM;
            CurrentAngle = 360.* CurrentCirclePosition/PulsesPerRevolution;
#ifndef emulation2
            // проверяю, не пришло ли подтверждение от контроллера об окончании выхода на дефект
            // т.е. проверяю вход HR3
            unsigned char x3_index = ENC6_GET_INDEX(EncoderCardNumber, X3_axis);
            if (!(x3_index & 0x01))
            {
                ConfirmPLC = true;
                break;
            }
#else
            if (GetTickCount()>time_out-110000)
            {
                ConfirmPLC = true;
                break;
            }
#endif
            // проверяю, не превысил ли сигнал уровень строба
            if (AmpA[channel_number] >= FlawChannel[channel_number].a_thresh && !SignalAboveStrobe)
            {
                SetSignalAboveStrobe = true;
                SignalAboveStrobe = true;
            }
            else if (AmpA[channel_number] < FlawChannel[channel_number].a_thresh && SignalAboveStrobe)
            {
                ResetSignalAboveStrobe = true;
                SignalAboveStrobe = false;
            }
        }

        // снимаю сигнал
        ResetGoToFlaw = true;
        GoToFlawProcess = false;
        ui->go_to_flaw_Button->setText("Перейти к дефекту");

        if (StopGoToFlaw)
        {
            QMessageBox::warning(this, "Выход на дефект", "Выход сканера на выбранный дефект отменен пользователем");
        }
        else if (ConfirmPLC)
        {
            QMessageBox::information(this, "Выход на дефект", "Выход сканера на выбранный дефект успешно выполнен");
        }
        else
        {
            QMessageBox::warning(this, "Ошибка при выходе на дефект", "При выходе сканера на дефект произошла ошибка. Превышено время ожидания");
        }
    }
    else
    {
        //GoToFlawProcess = false;
        StopGoToFlaw = true;
    }
}
//--------------------------------------------------------------------------------------------------------------------------
/*void Inspection::keyPressEvent(QKeyEvent * e)
{
    if (e->key() == Qt::Key_F10) close();
}*/

//--------------------------------------------------------------------------------------------------------------------------

void Inspection::PrintReport(QPrinter* printer)
{
    printer->setPageSize(QPrinter::A4);
    printer->setOrientation(QPrinter::Portrait);
    printer->setPageMargins (15,15,15,15,QPrinter::Millimeter);
    printer->setFullPage(false);

    QPainter painter(printer);
    QRect r(painter.viewport());

    double ms = printer->resolution() / 25.4;
    int top = 0;

    PrintHeader(painter, ms, top);

   /* painter.setFont(QFont("Arial", 15, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(r, Qt::AlignHCenter, "Результат контроля заготовки");
    top += 10 * ms;*/

    // рассчитываю ширину картинки для печати
    int picture_width = (r.width() - ms * 5) / 2;


    // если в высоту не умещаются два ряда картинок, сокращаю их размеры
    if ((r.height() - 40 * ms) < picture_width) picture_width = r.height() - 40 * ms;

    QPixmap gl_pixmap(2500, 2500);
    int number = 0;
    //int channel_number = 1;
    painter.setFont(QFont("Arial", 12, QFont::Normal));
    for (int num = 0; num < ChannelsCount; num++)
    {

            QRect signature((picture_width + ms * 5) * number, top, picture_width, 10*ms);
            painter.drawText(signature, Qt::AlignCenter, "Канал " + QString::number(num + 1));
            View3d printCscan(0, num, false);
            printCscan.Mode = VIEW_RESULT_2D;
            printCscan.print = true;
            printCscan.print_scale = ms;
            printCscan.setVisible(false);
            printCscan.setGeometry(0,0,100,100);
            printCscan.update();
            gl_pixmap = printCscan.renderPixmap(2500, 2500);
            painter.drawPixmap((picture_width + ms * 5) * number, top + 10*ms, picture_width, picture_width, gl_pixmap);
            number++;
            //channel_number++;

    }

    top += picture_width + ms * 15;
    bool have_non_printing_rows = true;
    int start_row = 0;
    while (have_non_printing_rows)
    {
        PrintFlawTable(painter, top, ms, r, start_row, have_non_printing_rows);
        if (have_non_printing_rows)
        {
            printer->newPage();
            top = 0;
        }
    }

    // печатаю дату контроля и фамилию оператора
    r.setRect(0, r.bottom() - ms*10 , r.width(), ms*10 );
    painter.drawText(r, Qt::AlignLeft, "Дефектоскопист: ________ / " + ReportOperator + " /");
    painter.drawText(r, Qt::AlignRight, "Дата: ________");// ReportDate.toString("dd.MM.yyyy"));
    painter.end(); // завершаем рисование
}

void Inspection::CreateDefectTable()
{
//row_count = 50;
    // заполняю таблицу
    ui->tableWidget->clear();
    ui->tableWidget->setColumnCount(9);
    ui->tableWidget->setRowCount(row_count + 3);
    ui->tableWidget->setSpan(0,1,1,8);
    QTableWidgetItem *item = new QTableWidgetItem;
    item->setText("Каналы");
    item->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(0,1, item);

    ui->tableWidget->setSpan(0,0,3,1);
    ui->tableWidget->setColumnWidth(0,30);
    QTableWidgetItem *itm = new QTableWidgetItem;
    itm->setText("№ де- фекта");
    itm->setTextAlignment(Qt::AlignCenter);
    ui->tableWidget->setItem(0,0, itm);
    // нумерую строки с дефектами
    for (int i = 1; i <= row_count; i++)
    {
        //ui->tableWidget->setRowHeight(i+1, 40);
        QTableWidgetItem *item = new QTableWidgetItem;
        item->setText(QString::number(i));
        item->setTextAlignment(Qt::AlignCenter);
        ui->tableWidget->setItem(i+2,0, item);

    }

    int number = 0;
    for (int ch = 0; ch < ChannelsCount; ch++)
    {
      //  if (ChannelEnabled[ch])
        {
            ui->tableWidget->setSpan(1,number*4+1, 1,4);
            QTableWidgetItem *item = new QTableWidgetItem;
            item->setText(QString::number(ch+1));
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(1,number*4+1, item);
            QTableWidgetItem *item1 = new QTableWidgetItem;
            item1->setText("по образ., мм");
            item1->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(2,number*4+1, item1);
            ui->tableWidget->setColumnWidth(number*4+1,85);
            QTableWidgetItem *item2 = new QTableWidgetItem;
            item2->setText("круг., град.");
            item2->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(2,number*4+2, item2);
            ui->tableWidget->setColumnWidth(number*4+2,85);
            item = new QTableWidgetItem;
            item->setText("глубина, мм");
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(2,number*4+3, item);
            ui->tableWidget->setColumnWidth(number*4+3,85);
            item = new QTableWidgetItem;
            item->setText("амплит., дБ");
            item->setTextAlignment(Qt::AlignCenter);
            ui->tableWidget->setItem(2,number*4+4, item);
            ui->tableWidget->setColumnWidth(number*4+4,85);


            for (int i = 0; i < row_count; i++)
            {
                QTableWidgetItem *mmitem = new QTableWidgetItem;
                mmitem->setTextAlignment(Qt::AlignCenter);
                QString text;
                if (i < FlawCount[ch])
                    text = QString::number(DefFormStart[ch][i]) + "..." + QString::number(DefFormEnd[ch][i]);
                else
                    text = "---";

                mmitem->setText(text);
                ui->tableWidget->setItem(i+3, number*4 + 1, mmitem);

                QTableWidgetItem *graditem = new QTableWidgetItem;
                graditem->setTextAlignment(Qt::AlignCenter);
                if (i < FlawCount[ch])
                    text = QString::number(DegreeCircStart[ch][i]/100., 'f', 1) + "..." + QString::number(DegreeCircEnd[ch][i]/100., 'f', 1);
                graditem->setText(text);
                ui->tableWidget->setItem(i+3, number*4 + 2, graditem);

                mmitem = new QTableWidgetItem; // глубина
                mmitem->setTextAlignment(Qt::AlignCenter);
                if (i < FlawCount[ch])
                    text = QString::number(DefDepth[ch][i], 'f', 1);
                else
                    text = "---";

                mmitem->setText(text);
                ui->tableWidget->setItem(i+3, number*4 + 3, mmitem);

                QTableWidgetItem *dBitem = new QTableWidgetItem;
                dBitem->setTextAlignment(Qt::AlignCenter);
                if (i < FlawCount[ch])
                    text = QString::number(10 * log(DefAmplitude[ch][i]/(double)Threshold[ch]), 'f', 1);
                dBitem->setText(text);
                ui->tableWidget->setItem(i+3, number*4 + 4, dBitem);

            }
            number++;
        }
    }

    ui->FlawAnalysisTitleLabel->setVisible(true);

    ui->HighlightFlaws_checkBox->setEnabled(true);
    int tab_width = 0;
    for (int i = 0; i < ui->tableWidget->columnCount(); i++)
        tab_width += ui->tableWidget->columnWidth(i);

    int tab_x;// = ui->abort_control_pushButton->x() + ui->abort_control_pushButton->geometry().width() + 25;
    int tab_y = ui->AlarmButton->y() + ui->AlarmButton->height() + 20;//= widget_height * 2 + top_margin + vert_space*2 - centralWidget()->y();
    int tab_h ;//= centralWidget()->height() - tab_y - vert_space;

        tab_x = width() - tab_width - 15;


        tab_h = (row_count +3) * 30 + 2;
        if (tab_h > 510) tab_h = 510;



        if (Mode == INSPECTION)
            tab_y = ui->signal_widget->y() + ui->signal_widget->height() + 30;


        ui->FlawAnalysisTitleLabel->move(tab_x + (ui->tableWidget->width() - ui->FlawAnalysisTitleLabel->width())/2, tab_y);
        tab_y += ui->FlawAnalysisTitleLabel->height() + 5;

        int btn_y = tab_y + tab_h + 10; // положение кнопки выхода на дефект

        if (btn_y + ui->go_to_flaw_Button->height() + 10 > ui->centralwidget->height())
        {
            // если кнопка уходит за пределы экрана, уменьшаю высоту таблицы
            btn_y = ui->centralwidget->height() - ui->go_to_flaw_Button->height() - 10;
            tab_h = btn_y - tab_y - 10;
        }



      /*  if (tab_y + tab_h + 20 > ui->go_to_flaw_Button->y())
        {

            if (btn_y + ui->go_to_flaw_Button->height() > ui->centralwidget->height() - 10)
            {
                tab_h -= btn_y + ui->go_to_flaw_Button->height() - ui->centralwidget->height() + 10 ;
                btn_y = ui->centralwidget->height() - ui->go_to_flaw_Button->height() - 10 ;

            }
            ui->go_to_flaw_Button->move((35+widget_width * 2 + width() - ui->go_to_flaw_Button->width())/2, btn_y);
        }*/



        // считаю, не наползла ли таблица на картинки, и если да, то уменьшаю картинки
        int hor_space = 15;
    /*    if (width() - tab_width - 15 < 20 + 2 * (widget_width+15))
        {
            widget_width = (width() - 2 * 20 - hor_space - tab_width - 15) / 2;
            widget_height = widget_width;

            int number = 0;
            for (int ch = 0; ch < ChannelsCount; ch++)
            {


                    view_3d[ch]->setGeometry(20 + (widget_width + hor_space) * number, top_margin, widget_width, widget_height);
                    view_3d[ChannelsCount + ch]->setGeometry(20 + (widget_width + hor_space) * number, top_margin + widget_height + vert_space, widget_width, widget_height);
                    ChannelNumberLabel[ch]->move(view_3d[ch]->x() + (view_3d[ch]->width() - ChannelNumberLabel[ch]->width())/2, 25);
                    number++;

            }
        }
        // иначе располагаю таблицу посередине оставшегося пространства (по ширине)
        else*/
        {
            tab_x = (20 + widget_width * 2 + hor_space + width() - tab_width-30)/2;
            ui->FlawAnalysisTitleLabel->move(10+(widget_width * 2 + hor_space)/2 +(width() - ui->FlawAnalysisTitleLabel->width())/2, ui->FlawAnalysisTitleLabel->y());
            ui->tableWidget->setGeometry(tab_x, tab_y, tab_width+30, tab_h);

        }
          ui->go_to_flaw_Button->move(tab_x + (tab_width + 30 - ui->go_to_flaw_Button->width())/2, btn_y);
        ui->tableWidget->setVisible(true);

#ifdef emulation
    ui->go_to_flaw_Button->setVisible(true); // кнопку "выйти на дефект" вывожу только в режиме отладки во всех случаях, когда сформирована таблица дефектов
#endif
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------

void Inspection::on_open_result_action_triggered()
{
    // читаю файл с результатами в новом формате: с оператором, со списком задействованных каналов, с таблицей дефектов
    ResultFileName = QFileDialog::getOpenFileName(0, "Открыть результат", "D:/UZK-RZ/Результаты/" "*.scan");
    if (ResultFileName == "") return;

    QFile f(ResultFileName);

    if(f.open(QIODevice::ReadOnly))
    {
        OpenResult(f);
    }
    else
    {
        QMessageBox::information(0, "Ошибка", "Не удалось открыть файл");
        return /*false*/;
    }
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::on_print_total_report_action_triggered()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this, Qt::Window);
    preview.setWindowState(Qt::WindowMaximized);
    // соединяем сигнал, активизирующий рисование, с выводом на печать
    connect(&preview, SIGNAL(paintRequested(QPrinter*)), this, SLOT(PrintReport(QPrinter*)));
    // запускаем предварительный просмотр
    preview.exec();
    setFocus();
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::on_print_flaw_table_action_triggered()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this, Qt::Window);
    preview.setWindowState(Qt::WindowMaximized);
    // соединяем сигнал, активизирующий рисование, с выводом на печать
    connect(&preview, SIGNAL(paintRequested(QPrinter*)), this, SLOT(PrintOnlyFlawTable(QPrinter*)));
    // запускаем предварительный просмотр
    preview.exec();
    setFocus();
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::on_print_selected_defectograms_action_triggered()
{
    // вывожу предварительный просмотр выбранных дефектограмм
    QPrinter printer(QPrinter::HighResolution);
    QPrintPreviewDialog preview(&printer, this, Qt::Window);
    preview.setWindowState(Qt::WindowMaximized);
    // соединяем сигнал, активизирующий рисование, с выводом на печать
    connect(&preview, SIGNAL(paintRequested(QPrinter*)), this, SLOT(PrintSelectedPictures(QPrinter*)));
    // запускаем предварительный просмотр
    preview.exec();
    setFocus();
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::PrintSelectedPictures(QPrinter* printer)
{
    printer->setPageSize(QPrinter::A4);
    printer->setOrientation(QPrinter::Landscape);
    printer->setPageMargins (15,15,15,15,QPrinter::Millimeter);
    printer->setFullPage(false);

    QPainter painter(printer);
    QRect r(painter.viewport());

    double ms = printer->resolution() / 25.4;
    int top = 0;

    PrintHeader(painter, ms, top);

    /*painter.setFont(QFont("Arial", 15, QFont::Bold));
    painter.setPen(Qt::black);
    painter.drawText(r, Qt::AlignHCenter, "Результат контроля заготовки");
    top += 10 * ms;*/

    // рассчитываю ширину картинки для печати
    int picture_width = (r.width() - ms * 5) / 2;
    // если картинок больше 2, печатаю в 2 ряда

    // если в высоту не умещаются два ряда картинок, сокращаю их размеры

    if ((r.height() - 40 * ms) < picture_width) picture_width = r.height() - 40 * ms;

    QPixmap gl_pixmap(2500, 2500);
    int number = 0;
    //int channel_number = 1;
    painter.setFont(QFont("Arial", 12, QFont::Normal));
    for (int num = 0; num < ChannelsCount; num++)
    {


        QRect signature((picture_width + ms * 5) * number, top, picture_width, 10*ms);
        painter.drawText(signature, Qt::AlignCenter, "Канал " + QString::number(num + 1));
        View3d printCscan(0, num, false);
        printCscan.Mode = VIEW_RESULT_2D;
        printCscan.print = true;
        printCscan.print_scale = ms;
        //printCscan.ViewAngleX = view_3d[]->ViewAngleX;
        //print3d.ViewAngleY = view_3d[0]->ViewAngleY;
        printCscan.setVisible(false);
        printCscan.setGeometry(0,0,100,100);
        printCscan.update();
        gl_pixmap = printCscan.renderPixmap(2500, 2500);
        painter.drawPixmap((picture_width + ms * 5) * number, top + 10*ms, picture_width, picture_width, gl_pixmap);
        number++;
        //channel_number++;

    }

    top += 100 * ms;

    // печатаю дату контроля и фамилию оператора
    r.setRect(0, r.bottom() - ms*10 , r.width(), ms*10 );
    painter.drawText(r, Qt::AlignLeft, "Дефектоскопист: ________ / " + ReportOperator + " /");
    painter.drawText(r, Qt::AlignRight, "Дата: " + ReportDate.toString("dd.MM.yyyy"));
    painter.end(); // завершаем рисование
}

void Inspection::PrintHeader(QPainter &painter, double &ms, int &top)
{
    //top = 0;
    painter.setFont(QFont("Arial", 12, QFont::Bold));
    painter.setPen(Qt::black);
    QRect r(painter.viewport());
    painter.drawText(r, Qt::AlignHCenter, "Протокол ультразвукового контроля №_______ от _______");
    top += 10 * ms;
    r.setRect(r.x(), top, r.width(), r.height());
    painter.setFont(QFont("Arial", 12, QFont::Normal));
    painter.drawText(r, Qt::AlignLeft, "АО Красмаш");
    painter.drawText(r, Qt::AlignRight, "Лист 1");
    top += 10 * ms;
    int rect_top = top - ms;
    QRect rect(r.left() + 2*ms, top, r.width(), r.height());
    painter.setFont(QFont("Arial", 10, QFont::Normal));
    painter.drawText(rect, Qt::AlignLeft, "Установка УЗК тел вращения");
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Дефектоскоп: УМД-8-8К");
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Индекс детали: _____________ Шифр заготовки: ______________");
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Дата контроля: " + ReportDate.toString("dd.MM.yyyy"));
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Выявлено дефектов: " + QString::number(FlawCount[0] + FlawCount[1]));
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Заключение: " + InspectionResume);

    rect.setRect(r.left() + 117*ms, top, r.width() - 117*ms, r.height());
    painter.drawText(rect, Qt::AlignLeft, "Номер заготовки: " + BilletNumber);
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Номер партии: " + BatchNumber);
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Номер плавки: " + MeltNumber);
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Номер серификата: " + CertificateNumber);
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Марка материала: " + MaterialGrade);
    rect.setRect(rect.left(), rect.top() + 6 * ms, rect.width(), rect.height());
    painter.drawText(rect, Qt::AlignLeft, "Сопровод.: " + AccompanyingDocument);

    int btm = rect.top() + 6 * ms;
    rect.setRect(r.left(), rect_top, 115*ms, btm - rect_top);
    painter.drawRect(rect);
    rect.setRect(r.left() + 115*ms, rect_top, r.width() - 115*ms, btm - rect_top);
    painter.drawRect(rect);
    top = rect.top() + rect.height() + 5*ms;



}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::PrintFlawTable(QPainter &painter, int top, double &ms, QRect &r, int &start_row, bool &go_on)
{
    // печатаю данные из таблицы дефектов
    if (ui->tableWidget->isVisible())
    {
        int cell_width = (r.width() - 12 * ms)/ 8;
        int cell_left = 12*ms;
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        QRect rect(0, top, cell_width * 2 * 4 + 12 * ms, 10*ms);
        painter.drawText(rect, Qt::AlignHCenter, "Обнаруженные дефекты");
        //painter.drawRect(rect);
        top = rect.bottom();
        rect.setRect(cell_left, top, cell_width * 2 * 4, 8*ms);
        painter.drawRect(rect);
        painter.setFont(QFont("Arial", 10, QFont::Normal));
        painter.drawText(rect, Qt::AlignCenter, "Номер канала");

        int cell_top = rect.y() + rect.height();

        // расcтавляю номера каналов
        for (int ch = 0; ch < ChannelsCount; ch++ )
        {
            rect.setRect(cell_left + ch*cell_width*4, cell_top, cell_width*4, 8*ms);
            painter.drawRect(rect);
            QString check;
            QTableWidgetItem *item = ui->tableWidget->item(1,ch*4+1);
            if (NULL != item)
            {
               check = item->text();
               painter.drawText(rect, Qt::AlignCenter, check);
            }
        }

        // к каждому каналу делаю подписи столбцов: вертикальная координата и круговая, глубина и амплитуда
        painter.setFont(QFont("Arial", 8, QFont::Normal));
        cell_top = rect.y() + rect.height();

        //painter.setFont(QFont("Arial", 8, QFont::Normal));
        for (int ch = 0; ch < ChannelsCount * 4; ch++ )
        {
            rect.setRect(cell_left + ch*cell_width, cell_top, cell_width, 8*ms);
            painter.drawRect(rect);
            QString check;
            QTableWidgetItem *item = ui->tableWidget->item(2,ch+1);
            if (NULL != item)
            {
               check = item->text();
               painter.drawText(rect, Qt::AlignCenter, check);
            }
        }
        cell_top = rect.y() + rect.height();
        rect.setRect(0, top, cell_left, cell_top - top );
        painter.drawRect(rect);
        painter.drawText(rect, Qt::AlignCenter, "№ де-\nфекта");
        painter.setFont(QFont("Arial", 10, QFont::Normal));
        // нумерация строк
        int btm = rect.y() + rect.height();
        int last_row = -1;
        for (int def = start_row; def < row_count; def++)
        {
            rect.setRect(0, btm, cell_left,  8*ms);
            painter.drawRect(rect);
            QString check;
            QTableWidgetItem *item = ui->tableWidget->item(3+def, 0 );
            if (NULL != item)
            {
               check = item->text();
               painter.drawText(rect, Qt::AlignCenter, check);
            }
//        }

//        // заполняю все координаты
//        for (int def = 0; def < row_count; def++)
//        {
            int new_left = cell_left;
            for (int chan = 0; chan < ChannelsCount*4; chan++)
            {
                rect.setRect(new_left, rect.y(), cell_width,  8*ms);
                painter.drawRect(rect);
                QString check;
                QTableWidgetItem *item = ui->tableWidget->item(3+def, 1 + chan );
                if (NULL != item)
                {
                   check = item->text();
                   painter.drawText(rect, Qt::AlignCenter, check);
                }
                new_left = rect.x() + rect.width();
            }
            btm = rect.y() + rect.height();
            if (btm > r.bottom() - 15*ms)
            {
                // начинаю новую страницу
                start_row = def + 1;
                go_on = true;
                return;
            }
        }

    }
    go_on = false;
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::PrintOnlyFlawTable(QPrinter *printer)
{
    printer->setPageSize(QPrinter::A4);
    printer->setOrientation(QPrinter::Portrait);
    printer->setPageMargins (15,15,15,15,QPrinter::Millimeter);
    printer->setFullPage(false);


    QPainter painter(printer);


    QRect r(painter.viewport());
    //QRect page = printer->pageRect();

    double ms = printer->resolution() / 25.4;
    int top = 0;

    PrintHeader(painter, ms, top);

    bool have_non_printing_rows = true;
    int start_row = 0;
    int page_number = 1;
    while (have_non_printing_rows)
    {
        PrintFlawTable(painter, top, ms, r, start_row, have_non_printing_rows);
        if (have_non_printing_rows)
        {
            printer->newPage();
            page_number++;
            painter.setFont(QFont("Arial", 12, QFont::Normal));
          //  painter.drawText(r, Qt::AlignLeft, "АО Красмаш");
            painter.drawText(r, Qt::AlignRight, "Лист " + QString::number(page_number));
            top = 10;
        }
    }

    // печатаю дату контроля и фамилию оператора
    r.setRect(0, r.bottom() - ms*10 , r.width(), ms*10 );
    painter.drawText(r, Qt::AlignLeft, "Дефектоскопист: ________ / " + ReportOperator + " /");
    painter.drawText(r, Qt::AlignRight, "Дата: " + ReportDate.toString("dd.MM.yyyy"));
    painter.end(); // завершаем рисование*/
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::on_HighlightFlaws_checkBox_stateChanged(int arg1)
{
    HighlightFlaws = ui->HighlightFlaws_checkBox->isChecked();
    emit redraw_scene0();
    emit redraw_scene1();
    emit redraw_scene2();
    emit redraw_scene3();
    emit redraw_scene4();
    emit redraw_scene5();
}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
void Inspection::on_Scale_checkBox_stateChanged(int arg1)
{
    DrawScale = ui->Scale_checkBox->isChecked();
    emit redraw_scene0();
    emit redraw_scene1();

}
//-----------------------------------------------------------------------------------------------------------------------------------------------------------
bool Inspection::OpenResult(QFile &f)
{
    // пытаюсь считать идентификатор файла
    wchar_t name[50];
    if (f.read((char*)name, 50*sizeof(wchar_t)) > 0)
    {
        if(QString::fromWCharArray(name) == ResultFileIdent)
        {
            memset(CScan, -1, sizeof(CScan));
            // считываю фамилию оператора
            f.read((char*)name, 50 * sizeof(wchar_t));
            ReportOperator = QString::fromWCharArray(name);

            // считываю дату контроля
            f.read((char*)&ReportDate, sizeof(QDate::currentDate()));

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
            f.read((char*)(&StartInspectPosition), sizeof(int));
            f.read((char*)(&DrawControlStep), sizeof(int));
            f.read((char*)(&DrawStepPerRevolution), sizeof(int));
            f.read((char*)(&DrawLinesCount), sizeof(int));

            // считываю задействованные каналы
            //bool ChannelEnabled[6];
            //f.read((char*)(ChannelEnabled), sizeof(ChannelEnabled));

            // считываю браковочные уровни
            for (int ch = 0; ch < ChannelsCount; ch++)
            {


#ifndef emulation
                    f.read((char*)&Threshold[ch], sizeof(int));
#else
                    f.read((char*)(&FlawChannel[ch].a_thresh), 2);
#endif


            }
/*#ifdef emulation
            // считываю уровень палитры
            f.read((char*)&PaletteRelation, sizeof(double));
#endif*/
            // считываю С-скан активных каналов
            for (int ch = 0; ch < ChannelsCount; ch++)
                for (int i = 0; i < LinesCount; i++)
                    f.read(CScan[ch][i], StepPerRevolution);
            // теперь надо все обновить
            ui->tableWidget->setVisible(false);
            // надо обновить количество каналов
            resizeEvent(NULL);
            AnalysisCompleted = false;
            //ui->flaw_analysis_Button->setEnabled(true);
            //ui->HighlightFlaws_checkBox->setEnabled(false);

//#ifndef emulation2
            // считываю результаты анализа дефектов
            row_count = 0;
            for (int ch = 0; ch < ChannelsCount; ch++)
            {
                f.read((char*)(&FlawCount[ch]), sizeof(int));
                if (FlawCount[ch] > row_count) row_count = FlawCount[ch];
                for (int i = 0; i < FlawCount[ch]; i++)
                {
                    f.read((char*)(&DefFormStart[ch][i]), sizeof(int));
                    f.read((char*)(&DefFormEnd[ch][i]), sizeof(int));
                    f.read((char*)(&DegreeCircStart[ch][i]), sizeof(int));
                    f.read((char*)(&DegreeCircEnd[ch][i]), sizeof(int));
                    f.read((char*)(&DefDepth[ch][i]), sizeof(double));
                    f.read((char*)(&DefAmplitude[ch][i]), sizeof(int));
                }
            }
            AnalysisCompleted = true;



//#else
            Mode = VIEV_RESULT;
            ui->CurrentValuesTitleLabel->setVisible(false);
            ui->CurrentParametersTableWidget->setVisible(false);
            ui->groupBox->setVisible(false);
            ui->signal_widget->setVisible(false);
            ui->resume_Button->setVisible(false);
            CreateDefectTable();

//#endif
            // считываю данные заготовки: номер, плавку, и т.д.

            f.read((char*)name, 25 * sizeof(wchar_t));
            BilletNumber = QString::fromWCharArray(name);
            f.read((char*)name, 25*sizeof(wchar_t));
            BatchNumber = QString::fromWCharArray(name);
            f.read((char*)name, 25*sizeof(wchar_t));
            MeltNumber = QString::fromWCharArray(name);
            f.read((char*)name, 25*sizeof(wchar_t));
            CertificateNumber = QString::fromWCharArray(name);
            f.read((char*)name, 25*sizeof(wchar_t));
            MaterialGrade = QString::fromWCharArray(name);
            f.read((char*)name, 25*sizeof(wchar_t));
            AccompanyingDocument = QString::fromWCharArray(name);
            f.read((char*)name, 25*sizeof(wchar_t));
            InspectionResume = QString::fromWCharArray(name);



            f.close();

            if (DrawControlStep) DrawTotalLinesCount = pow(pow((BilletBottomDiameter/2-BilletTopDiameter/2), 2) + pow(BilletHeight, 2), 0.5) / DrawControlStep;
            // рассчитываю номер линии, с которой начинается контроль
            if (DrawControlStep) DrawStartLine = StartInspectPosition / DrawControlStep;
            // пересчитываю массивы с вершинами для вывода моделей
            Calc3dView();
            Calc2dView();

            // рассчитываю сжатый с-скан для вывода изображения
            CalcPressedCScan();
            return true;
        }
        else
        {
            QMessageBox::information(0, "Ошибка", "Файл не является результатом контроля");
            f.close();
            return false;
        }
    }
    else
    {
        QMessageBox::information(0, "Ошибка", "Файл не является результатом контроля");
        f.close();
        return false;
    }

}


void Inspection::UpdateWidgets()
{
    emit redraw_scene0();
    emit redraw_scene1();
    emit redraw_scene2();
    emit redraw_scene3();
    emit redraw_zoom_scan();
    //CurrentCoordLabel->setText("Текущие координаты: высота по образующей " + QString::number(ScannerFormingCoord) + " мм, угол " + QString::number(CurrentAngle) + " град.");
    //CurrentCoordLabel->adjustSize();


    for (int chan = 0; chan < ChannelsCount; chan++)
    {
       QTableWidgetItem *item = new QTableWidgetItem;
       /*  item->setText(QString::number(AmpA[chan]));*/


        QPixmap pxmp = QPixmap(50, 125);

        QPainter pntr(&pxmp);

        pntr.fillRect(0,0,50,125, Qt::white);
        pntr.setPen(Qt::black);
        QFontMetrics fm = pntr.fontMetrics();
        QString signature = QString::number(AmpA[chan]);
        pntr.drawText(35 - fm.width(signature), 125 - 3 - fm.height()/2, signature);
        for (int i = 0; i < AmpA[chan]; i++)
        {
            QColor clr = QColor(255 * (double)i / 100., 0, 255 * double(100-i)/100.);
            pntr.setPen(clr);
            pntr.drawLine(15, 100-i, 35, 100-i);
        }

        for (int i = temp_maximum[chan] - 3; i < temp_maximum[chan]; i++)
        {
            if (i >= 0)
            {
                QColor clr = QColor(255 * (double)i / 100., 0, 255 * double(100-i)/100.);
                pntr.setPen(clr);
                pntr.drawLine(15, 100-i, 35, 100-i);
            }
        }


        label[chan]->setPixmap(pxmp);



      /*  item->setTextAlignment(Qt::AlignCenter);
        ui->CurrentParametersTableWidget->setItem(0,chan, item);*/
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

    QTableWidgetItem *item = new QTableWidgetItem;
    item->setText(QString::number(ScannerFormingCoord));
    item->setTextAlignment(Qt::AlignCenter);
    ui->CurrentParametersTableWidget->setItem(4,0, item);

    item = new QTableWidgetItem;
    item->setText(QString::number(CurrentAngle, 'f', 0));
    item->setTextAlignment(Qt::AlignCenter);
    ui->CurrentParametersTableWidget->setItem(5,0, item);

    if (Alarm)
    {
        if (ui->AlarmButton->isChecked())
            ui->AlarmButton->setIcon(QIcon(":/RightRed.png"));
        else
            ui->AlarmButton->setIcon(QIcon(":/LeftRed.png"));
    }
    else
    {
        if (ui->AlarmButton->isChecked())
            ui->AlarmButton->setIcon(QIcon(":/RightGreen.png"));
        else
            ui->AlarmButton->setIcon(QIcon(":/LeftGreen.png"));
    }

}



void Inspection::on_exit_action_triggered()
{
    emit show_work_form();
}


/*void Inspection::test()
{
   ui->tect_label->setVisible(!ui->tect_label->isVisible());
}*/

void Inspection::on_AlarmButton_clicked()
{
    if (ui->AlarmButton->isChecked())
    {
        if (Alarm)
            ui->AlarmButton->setIcon(QIcon(":/RightRed.png"));
        else
            ui->AlarmButton->setIcon(QIcon(":/RightGreen.png"));
        alarm_widget->setVisible(true);
    }
    else
    {
        if (Alarm)
            ui->AlarmButton->setIcon(QIcon(":/LeftRed.png"));
        else
            ui->AlarmButton->setIcon(QIcon(":/LeftGreen.png"));
        alarm_widget->setVisible(false);
    }
}

void Inspection::AScanChannelSelect()
{
    if(ui->Chan1Button->isChecked())  ActiveChannel = 0;
    else if(ui->Chan2Button->isChecked())  ActiveChannel = 1;
    emit SwitchChannel();
}

void Inspection::on_resume_Button_clicked()
{
    bool bOk;
    InspectionResume = QInputDialog::getText( 0, "Заключение", "Введите заключение:",
                                         QLineEdit::Normal, "", &bOk,
                                         Qt::WindowTitleHint | Qt::WindowCloseButtonHint);
    if (bOk) // если данные внесены, дописываю их в файл
    {
        // открываю файл с результатами для дозаписи
        QFile f(ResultFileName);
        f.open(QIODevice::ReadWrite);
        // я должен переместиться на то место, где записано резюме
        f.seek(f.size() - 25*sizeof(wchar_t));
        wchar_t buffer[30];
        QStringToWChar(InspectionResume, buffer);

        f.write((char*)buffer, 25*sizeof(wchar_t));
        f.close();

    }
}

void Inspection::UpdateAScan()
{
    ui->signal_widget->update();
}

void Calc3dView()
{
    double R = BilletBottomDiameter/2;
    double r = BilletTopDiameter/2;
    double length = BilletHeight;
    // здесь рассчитываю массив вершин для построения изображения 3-д модели
    //вычисление вершин
       // буду разбивать поверхность изделия на "пояса", высота которых равна шагу контроля

       // вершины боковой поверхности
       for (int j = 0; j<=DrawTotalLinesCount; j++)
       {
           for (int i=0; i<=DrawStepPerRevolution; i++)
           {
               SideVertex[j][i].x = (r-(r-R)*(DrawTotalLinesCount-j)/DrawTotalLinesCount)*cos(i*2*M_PI/DrawStepPerRevolution);
               SideVertex[j][i].y = (r-(r-R)*(DrawTotalLinesCount-j)/DrawTotalLinesCount)*sin(i*2*M_PI/DrawStepPerRevolution);
               SideVertex[j][i].z = j*BilletHeight/DrawTotalLinesCount;
           }
       }
       // буду рисовать без освещения, поэтому нормали не считаю
       //вычисление нормалей к граням

       // вершины внутренней поверхности
       for (int i=0; i <= DrawStepPerRevolution; i++)
       {
           InV[0][i].x = (r-20)*cos(i*2*M_PI/DrawStepPerRevolution);
           InV[0][i].y = (r-20)*sin(i*2*M_PI/DrawStepPerRevolution);
           InV[1][i].x = (R-20)*cos(i*2*M_PI/DrawStepPerRevolution);
           InV[1][i].y = (R-20)*sin(i*2*M_PI/DrawStepPerRevolution);
           InV[0][i].z = length;
           InV[1][i].z = 0;
       }

}

void Calc2dView()
{
    double R = BilletBottomDiameter * M_PI;
    double r = BilletTopDiameter * M_PI;
    //double length = BilletHeight;
    // здесь рассчитываю массив вершин для построения изображения 2д скана (развертки)
    for (int j=0; j<=DrawTotalLinesCount; j++)
    {
        for (int i=0; i<= DrawStepPerRevolution; i++)
        {
            // 30.01.2020 проверил - расчет верный
            // смысл такой: развертку рисую в виде трапеции
            // вычисляю координату левой границы трапеции на высоте, соответствующей текущей линии ("поясу")
            // и прибавляю к этому значению такую часть длины текущей горизонтальной линии трапеции, которая соответствует тому,
            // какую часть составляет текущая координата по окружности от полной окружности
            Vertex[i][j] = (R-r)*(j)/2./DrawTotalLinesCount + (R - (R-r)*(j)/(float)DrawTotalLinesCount)*i/DrawStepPerRevolution - R/2.;
            //Vertex[i][j] = (r-R)*j/2./LinesCount + (r - (r-R)*j/LinesCount)*i/RevolutionStepCount - r/2.;
        }
    }

}

void CalcPressedCScan()
{
    memset(PressedCScan, -1, sizeof(PressedCScan));
    for (int ch = 0; ch < ChannelsCount; ch++)
        for (int i = 0; i < LinesCount; i++)
        {
            int current_line = i*DrawLinesCount/(double)LinesCount;
            for (int j = 0; j < StepPerRevolution; j++)
            {
                int current_step = j*DrawStepPerRevolution/(double)StepPerRevolution;
                if (CScan[ch][i][j] > PressedCScan[ch][current_line][current_step])
                    PressedCScan[ch][current_line][current_step] = CScan[ch][i][j];
            }
        }
}

