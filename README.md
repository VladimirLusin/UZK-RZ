# UZK-RZ
Программа для ПК - управление установкой ультразвукового контроля
В установке установленн программируемый логический контроллер (ПЛК, "промышленный контроллер"). Взаимодействие с ним происходит посредством Modbus TCP (поток modbusthread). 
Взаимодействие с ультразвуковым дефектоскопом происходит с помощью обмена пакетами UDP - так исторически сложилось в нашей организации (поток udthread). Но в одном из своих проектов я использовал и протокол TCP/IP - так же для взаимодействия ПК с прибором - писал код для ПК и для прибора (в нем установлен одноплатный компьютер с WindowsCE).
Work - форма для настройки параметров, ввода данных.
Newinspection - основной рабочий модуль, в котором производится контроль изделия с 3D визуализацией в реальном времени.
