#include "modbusthread.h"
#include "newinspection.h"
#include "enc600.h"

QModbusThread *ModbusThread;

QModbusThread::QModbusThread(QObject *parent)
        : QThread(parent)
{
    Terminated = false;
    WriteCoordinats = false;
    HaveAnswer = true; // условно считаю, что ответ от ПЛК есть, чтобы запустить процесс обмена
    WaitingAlarmRegister = false;
}

DWORD alarm_polling_time = 0;
DWORD time_counter = 0;
bool prev_alarm = false;
void QModbusThread::run()
{

    while (!Terminated)
    {
        if (ModbusSocket == INVALID_SOCKET)
        {
            ModbusSocket = socket(AF_INET, SOCK_STREAM, 0);
        }
        else if (!Connection)
        {
            int result = ::connect(ModbusSocket, (struct sockaddr *)&peer, sizeof(peer) );
            if(!result) // если функция connect возвращает 0, значит, соединение установлено
            {
                Connection = true;
                HaveAnswer = true;
            }

        }
        else // проверяю, не пришел ли ответ от ПЛК
        {
         /*  int rx_count = recv(ModbusSocket, RxBuffer, sizeof(RxBuffer), 0);
            if (rx_count >= 10)
            {
                HaveAnswer = true;
                // посчитаю частоту обмена
                modbus_counter++;
                if (GetTickCount() > time_counter)
                {
                    time_counter = GetTickCount() + 1000;
                    ModbusRate = modbus_counter;
                    modbus_counter = 0;
                }
            }

*/
            // проверяю, не пришли ли данные от ПЛК



            fd_set allreads;
            FD_ZERO (&allreads);
            FD_SET(ModbusSocket, &allreads);
            if(select(1, &allreads, 0, 0, &tv) != SOCKET_ERROR)
            {
                if( FD_ISSET(ModbusSocket, &allreads ) )
                {
                    int result = recv(ModbusSocket, RxBuffer, sizeof(RxBuffer), 0);

                    if (result == 0)
                    {
                        Connection = false;
                    }
                    else
                    {
                        HaveAnswer = true;
                        // посчитаю частоту обмена
                        modbus_counter++;
                        if (GetTickCount() > time_counter)
                        {
                            time_counter = GetTickCount() + 1000;
                            ModbusRate = modbus_counter;
                            modbus_counter = 0;
                        }
                        // разбираю полученный пакет
                        // если это пакет со значениями входных сигналов, эти значения содержатся, по идее, в 9-10 байтах
                        // если ждал регистр аварий, проверю он ли это
                        if (WaitingAlarmRegister)
                        {
                            // проверяю идентификатор транзакции - он расположен в самом начале пакета
                            if (*(DWORD*)RxBuffer == ALARM_PACKET_ID)
                            {
                                AlarmRegister = *(WORD*)(&RxBuffer[9]); // надо проверить, правильный ли порядок байт
                                WaitingAlarmRegister = false;
                                emit UpdateAlarms();
                            }
                        }
                    }


                }
            }
        }
        // полезная нагрузка: раз этот поток всегда работает, пусть он опрашивает энкодер
        if (EncoderCardReady)
            AbsCirclePosition = ENC6_GET_ENCODER(EncoderCardNumber, X1_axis);

        CurrentCirclePosition = AbsCirclePosition % PulsesPerRevolution;

        //
        //if (GetTickCount() > send_encoder_time)
        if (HaveAnswer /*&& (GetTickCount() > send_encoder_time)*/)
        {
            //send_encoder_time = GetTickCount() + 10;
            if (EncoderCardReady)
                CurrentVertCoord = ENC6_GET_ENCODER(EncoderCardNumber, X2_axis);

            __int32 coord_data[] = {CurrentCirclePosition, TargetCircleCoord, CurrentVertCoord, TargetVertCoord};
            if ((Alarm || prev_alarm) && (GetTickCount() > alarm_polling_time)) // если пришел сигнал аварии или был при последнем опросе, проверяю, какая авария - считываю регистр аварий
            {                       // буду раз в полсекунды опрашивать аварии, чтобы обрабатывать и остальные запросы
                ReadAlarmRegister();
                WaitingAlarmRegister = true;
                prev_alarm = Alarm;  // запоминаю последнее состояние, чтобы опросить еще раз регистр аварий, когда пропадет сигнал аварии
                alarm_polling_time = GetTickCount() + 200;
            }
            else if (SendVelocityAndStep)
            {
                WORD data[] = {RotationSpeed, PulsesPerCoil};
                SendModbusData(2, data, RotationSpeedAddress);
                SendVelocityAndStep = false;
            }
            else if (SetGoToFlaw && !WriteCoordinats)
            {
                WriteCoordinats = true;
                SendModbusData(8, (WORD*)coord_data, 1002); // сначала отправляю координаты, затем Coils
            }
            else if (StartInspection)
            {
                WriteModbusCoil(true, 1000);
                StartInspection = false;
            }
            else if (StopInspection)
            {
                WriteModbusCoil(false, 1000);
                StopInspection = false;
            }
            else if (SetNextLine)
            {
                WriteModbusCoil(true, 1001);
                SetNextLine = false;
            }
            else if (ResetNextLine)
            {
                WriteModbusCoil(false, 1001);
                ResetNextLine = false;
            }
            else if (SetGoToFlaw && WriteCoordinats)
            {
                WriteModbusCoil(true, 1003);
                SetGoToFlaw = false;
                WriteCoordinats = false;
            }
            else if (ResetGoToFlaw)
            {
                WriteModbusCoil(false, 1003);
                ResetGoToFlaw = false;
            }
            else if (ResetAlarm)
            {
                WriteModbusCoil(true, 1002);
                ResetAlarm = false;
            }
            else if (SetSignalAboveStrobe)
            {
                WriteModbusCoil(true, 1005);
                SetSignalAboveStrobe = false;
            }
            else if (ResetSignalAboveStrobe)
            {
                WriteModbusCoil(false, 1005);
                ResetSignalAboveStrobe = false;
            }
            else SendModbusData(8, (WORD*)coord_data, 1002);

            HaveAnswer = false;

        }

    } // окончание цикла while (!Terminated)
    closesocket(ModbusSocket);
    ModbusSocket = INVALID_SOCKET;
    Connection = false;
}

QModbusThread::~QModbusThread()
{
    WSACleanup();
}

bool QModbusThread::WriteModbusCoil(bool state, WORD address)
{
    if (Connection)
    {
        // запись одной Coil
        BYTE buf[12];// =
        *(WORD*)&buf[0] = 0; // идентификатор транзакции
        *(WORD*)&buf[2] = 0; // идентификатор протокола
        buf[4] = 0; // размер посылки - старший байт
        buf[5] = 6;//0x0B;//5 + 2*words_count; // размер посылки
        buf[6] = 1; // ID устройства
        buf[7] = 5; // запись значений нескольких регистров
        buf[8] = address>>8; // Адрес регистра, с которого нужно начинать запись - старший байт
        buf[9] = address & 0xFF; // Адрес регистра, с которого нужно начинать запись - младший байт
        buf[10] = 0xFF*state; // Передаваемое значение для Coil - старший байт; 0 - выключить катушку, FF - включить катушку
        buf[11] = 0; // Передаваемое значение для Coil - младший байт - всегда 0

        int result = send(ModbusSocket, (char*)buf, sizeof(buf), 0);
        if( result == SOCKET_ERROR )
        {
            ModbusSocket = INVALID_SOCKET;
            Connection = false;
            return false;
        }
        else return true;
    }
    else
        return false;
}

bool QModbusThread::SendModbusData(int words_count, WORD *data, WORD address)
{
    if (Connection)
    {
        // запись нескольких 16-разрядных регистров
        BYTE buf[30];// =
        *(WORD*)&buf[0] = 0; // идентификатор транзакции
        *(WORD*)&buf[2] = 0; // идентификатор протокола
        buf[4] = 0; // размер посылки - старший байт
        buf[5] = 7+2*words_count;//0x0B;//5 + 2*words_count; // размер посылки
        buf[6] = 1; // ID устройства
        buf[7] = 16; // запись значений нескольких регистров
        buf[8] = address>>8; // Адрес регистра, с которого нужно начинать запись - старший байт
        buf[9] = address & 0xFF; // Адрес регистра, с которого нужно начинать запись - младший байт
        buf[10] = 0; // Количество регистров, которые нужно записать - старший байт
        buf[11] = words_count; // Количество регистров, которые нужно записать - младший байт
        buf[12] = 2*words_count; // количество пересылаемых байт
        for (int i = 0; i < words_count; i++)
        {
            buf[13+2*i] = data[i]>>8;
            buf[13+2*i+1] = data[i]&0xFF;
        }
        //buf[13] = do_value;
        // buf[14] = do_value>>8;
        // рассчитываю размер посылки в байтах
        int size = 13 + 2*words_count;
        int result = send(ModbusSocket, (char*)buf, size, 0);
        if (result == SOCKET_ERROR )
        {
            ModbusSocket = INVALID_SOCKET;
            Connection = false;
            return false;
        }
        else return true;
    }
    else
        return false;
}

bool  QModbusThread::ReadAlarmRegister()
{
    if (Connection)
    {
        BYTE buf[12];
        *(WORD*)&buf[0] = ALARM_PACKET_ID; // идентификатор транзакции
        *(WORD*)&buf[2] = 0; // идентификатор протокола
        buf[4] = 0; // размер посылки - старший байт
        buf[5] = 6; // размер посылки
        buf[6] = 1; // ID устройства
        buf[7] = 3; // чтение значения Holding Register.
        buf[8] = AlarmRegisterAddress >> 8; // Адрес Input регистра, с которого нужно начинать чтение - старший байт
        buf[9] = AlarmRegisterAddress & 0xFF; // Адрес Input регистра, с которого нужно начинать чтение - младший байт
        buf[10] = 0; // Количество регистров, которые нужно прочитать - старший байт
        buf[11] = 1; // Количество регистров, которые нужно прочитать - младший байт
        int result = send(ModbusSocket, (char*)buf, sizeof(buf), 0);
        if (result == SOCKET_ERROR)
        {
            ModbusSocket = INVALID_SOCKET;
            Connection = false;
            return false;
        }
        else return true;
    }
    else
        return false;
}
