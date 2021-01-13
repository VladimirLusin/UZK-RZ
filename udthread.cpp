
//#include <winsock.h>
#include "vars.h"
#include "UdThread.h"
#include "enc600.h"
#include <math.h>

//---------------------------------------------------------------------------
TUdChannelThread *UdThread;


//---------------------------------------------------------------------------
double UdThread_UpdateFreq;
//---------------------------------------------------------------------------
TUdChannelThread::TUdChannelThread(QObject *parent)
        : QThread(parent)
{
     WSADATA WSAData;
     if (WSAStartup(0x101,(LPWSADATA)&WSAData)!=0) {
        //ShowMessage("Ошибка инициализации библиотеки TCP/IP !");
     }
     Def_IP_Addres = "192.168.1.200";

     /*    Ready = false;
         Active = false;
         ErrorCount = 0;*/




}
//---------------------------------------------------------------------------
TUdChannelThread::~TUdChannelThread()
{
    WSACleanup();
}

//---------------------------------------------------------------------------
DWORD NextSWriterLine = 0;
DWORD NextDrawSWriter = 0;
DWORD LastAScanTime = 0;


//---------------------------------------------------------------------------
DWORD confirmation_time = 0;
DWORD ScopeConnectTime = 0;
void TUdChannelThread::run()
{
    SOCKET ud_data_socket = INVALID_SOCKET;

    timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    fd_set fdSockSet;
    FD_ZERO(&fdSockSet);

    TResultPacket res_packet;


    // структура адреса команд
    sockaddr_in cmd_addr;
    cmd_addr.sin_family = AF_INET;
    cmd_addr.sin_addr.s_addr = inet_addr("192.168.1.200");
    cmd_addr.sin_port = htons(8800);

    // структура адреса результатов
    sockaddr_in res_addr;
    res_addr.sin_family = AF_INET;
    res_addr.sin_addr.s_addr = INADDR_ANY;
    res_addr.sin_port = htons(7800);


    freq_count = 0;
    freq_time = GetTickCount() + 1000;

    while (WorkFlag)
    {

        // если сокет не создан, пытаемся его создать
        if ((ud_data_socket == INVALID_SOCKET))
        {
            ud_data_socket = socket(AF_INET, SOCK_DGRAM, 0);
            if (ud_data_socket != INVALID_SOCKET)
            {
                if (bind(ud_data_socket, (sockaddr *)&res_addr, sizeof(res_addr)))
                {
                    closesocket(ud_data_socket);
                    ud_data_socket = INVALID_SOCKET;
                }
            }
        }
        else
        {
            // пытаюсь отправить команду или получить результат от дефектскопа


            TCommandPacket cmd_packet;

            if (GetTickCount() > confirmation_time)
            {
                confirmation_time = GetTickCount() + 100;

                // определим потерю соединения с прибором и установим параметры идентификатора так,
                // чтобы записать параметры во все каналы
                if (GetTickCount() > ScopeConnectTime)
                {
                    for (int i = 0; i < MaxChannels; i++)
                    {
                       UpdateChannelVarsIDENT[i] = FlawChannel[i].vars_ident - 1;
                    }
                    DefConnection = false;

                }

                cmd_addr.sin_addr.s_addr = inet_addr("192.168.1.200");
                cmd_packet.cmd.Command = Confirmation;
                cmd_packet.cmd.AscanChannel = ActiveChannel;


                // определяю, надо ли в каком-либо канале установить параметры
                // если да, то добавляю эти параметры и номер канала в команду
                for (int chan = 0; chan < ChannelsCount; chan++)
                {

                    if (UpdateChannelVarsIDENT[chan] != FlawChannel[chan].vars_ident)
                    {
                        // добавим флаг настройки канала в команду
                        cmd_packet.cmd.Command = SetParamsCmd;
                        cmd_packet.cmd.Channel = chan; // номер канала в дефектоскопе
                        cmd_packet.cmd.vars = FlawChannel[chan];
                        break;
                    }

                }
                // отправим команду в прибор
                /*int tx_count = */sendto(ud_data_socket, (char*)&cmd_packet, sizeof(TCommandPacket), 0, (sockaddr *)&cmd_addr, sizeof(cmd_addr));

            }
        }

        // проверяю, не пришли ли данные от дефектоскопа
        FD_ZERO(&fdSockSet);
        FD_SET(ud_data_socket, &fdSockSet);
        if (select(1, &fdSockSet, 0, 0, &tv) != SOCKET_ERROR)
        {
            // если ответ пришел - прочитаем его
            if (FD_ISSET(ud_data_socket, &fdSockSet))
            {
                DWORD rx_count;
                ioctlsocket(ud_data_socket, FIONREAD, &rx_count);
                if (rx_count >= sizeof(TResultPacket))
                {
                    //struct sockaddr def_addr;
                    //int from_len = sizeof(def_addr);

                    int rd_count = recv(ud_data_socket, (char*)&res_packet, sizeof(TResultPacket), 0);


                    if (rd_count == sizeof(TResultPacket))
                    {
                        ScopeConnectTime = GetTickCount() + 200;
                        DefConnection = true;

                        // обработаем полученные результаты
                        NewResultData(res_packet);
                    }
                    else
                    {
                        closesocket(ud_data_socket);
                        ud_data_socket = INVALID_SOCKET;
                    }
                }

            }
        }

    }

    // закроем сокет при завершении работы
    if (ud_data_socket != INVALID_SOCKET)
    {
        closesocket(ud_data_socket);
        ud_data_socket = INVALID_SOCKET;
    }
}


//---------------------------------------------------------------------------
int count = 0;
DWORD count_time = 0;
DWORD max_update_time[2] = {0,0};


void TUdChannelThread::NewResultData(TResultPacket &res_packet)
{
    DWORD tick = GetTickCount();

    // забираю А-скан из пакета результата
    int a_min[AscanLength], a_max[AscanLength];
    int p = 0;
    for (int i = 0; i < AscanLength; i++)
    {
        a_min[i] = res_packet.res.AScan[p++];
        a_max[i] = res_packet.res.AScan[p++];
    }


    // записываю результат в С-скан, если установлен флаг записи
    // буду накапливать результаты измерений, пока не изменится шаг
    // если выставлен флаг записи результатов, определяю координату по энкодеру
    // и записываю результат в С-скан
    if (WriteResult)
    {
        int CurrentStep = CurrentCirclePosition / PulsesPerStep;
        int DrawCurrentStep = CurrentCirclePosition / DrawPulsesPerStep;
        int DrawCurrentLine = double(CurrentVertCoord - StartVertPosition) / cos(Angle) /  LinearMovementPulsesPerMM / (double)DrawControlStep ;

        for (int chan = 0; chan < ChannelsCount; chan++)
        {

                if (res_packet.res.ChannelResult[chan].a_max/4 > CScan[chan][CurrentLine][CurrentStep])
                    CScan[chan][CurrentLine][CurrentStep] = res_packet.res.ChannelResult[chan].a_max/4;

                // записываю время сигнала в б-зоне, фиксирую минимальное значение
                if(res_packet.res.ChannelResult[chan].b_front < bTime[chan][CurrentLine][CurrentStep])
                    bTime[chan][CurrentLine][CurrentStep] = res_packet.res.ChannelResult[chan].b_front;

                // записываю данные в сжатый С-скан для отрисовки
                if (res_packet.res.ChannelResult[chan].a_max/4 > PressedCScan[chan][DrawCurrentLine][DrawCurrentStep])
                    PressedCScan[chan][DrawCurrentLine][DrawCurrentStep] = res_packet.res.ChannelResult[chan].a_max/4;

        }
    }



    // если времени с последнего А-скана прошло достаточно, выведем А-скан на экран
    if (LastAScanTime < tick)
    {
       memcpy(AScanMin, a_min, sizeof(AScanMin));
       memcpy(AScanMax, a_max, sizeof(AScanMax));
       LastAScanTime = tick + 20;
       emit ascan_ready();

    }

    // выведем сигналы на самописец канала

    for (int pch = 0; pch < ChannelsCount; pch++)
    {
        // сохраняю идентификатор параметров каналов
        UpdateChannelVarsIDENT[pch] = res_packet.res.ChannelResult[pch].vars_ident;

        int amp = res_packet.res.ChannelResult[pch].a_max/4;
        //int amp_b = res_packet.res.ChannelResult[pch].b_max / 4;

        if (amp > 100) AmpA[pch] = 100;
        else if (amp < 0) AmpA[pch] = 0;
        else AmpA[pch] = amp;

        b_time[pch] = res_packet.res.ChannelResult[pch].b_front;
        if (AmpA[pch] > temp_maximum[pch] || GetTickCount() > max_update_time[pch])
        {
            temp_maximum[pch] = AmpA[pch];
            max_update_time[pch] = GetTickCount() + 500;
        }

        //if (amp_b > 100) AmpB[pch] = 100;
        //else if (amp_b < 0) AmpB[pch] = 0;
        //else AmpB[pch] = amp_b;
    }
    // считаю частоту посылок
    count++;
    if (GetTickCount() >= count_time)
    {
       freq = count;
       count = 0;
       count_time = GetTickCount() + 1000;
    }

}
//---------------------------------------------------------------------------
