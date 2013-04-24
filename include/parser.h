﻿#ifndef PARSER_H
#define PARSER_H

#include <qregexp>
#include <qstring>

#pragma pack(push, 1)

struct argsComPOS
{
  float posX;       //позиция игрока по горизонтали.
  float posY;       //позиция игрока по вертикали.
  float posZ;       //высота по отношению к морю.
  int Dir;          //направление взгляда игрока.
  int Mode;         //громкость речи (0 – шёпот, 1 – нормальный голос, 2 – крик)
  QString vehId;  	//идентификатор техники (если 0 - на ногах).
  int isOut;      	//индикатор высунности (не используется если vehId = 0)
  float kvChan0;  	//частота первого канала КВ.
  float kvVol0;   	//громкость первого канал КВ.
  int kvPos0;     	//в каком ухе слышно первый канал КВ.
  float kvChan1;  	//частота второго канал КВ.
  float kvVol1;   	//громкость второго канала КВ.
  int kvPos1;     	//в каком ухе слышно второй канал КВ.
  float kvChan2;  	//частота третьего канала КВ.
  float kvVol2;   	//громкость третьего канала КВ.
  int kvPos2;     	//в каком ухе слышно третий канал КВ.
  float kvChan3;  	//частота четвёртого канала КВ.
  float kvVol3;   	//громкость четвёртого канала КВ.
  int kvPos3;     	//в каком ухе слышно четвёртый канал КВ.
  int kvActive;     //указание какой канал КВ активен (в какой мы говорим).
  int kvSide;     	//указание какой стороне принадлежит данная КВ рация.
  float dvChan0;  	//частота первого канала ДВ.
  float dvVol0;   	//громкость первого канал ДВ.
  int dvPos0;     	//в каком ухе слышно первый канал ДВ.
  float dvChan1;  	//частота второго канал ДВ.
  float dvVol1;   	//громкость второго канала ДВ.
  int dvPos1;     	//в каком ухе слышно второй канал ДВ.
  float dvChan2;  	//частота третьего канала ДВ.
  float dvVol2;   	//громкость третьего канала ДВ.
  int dvPos2;     	//в каком ухе слышно третий канал ДВ.
  float dvChan3;  	//частота четвёртого канала ДВ.
  float dvVol3;   	//громкость четвёртого канала ДВ.
  int dvPos3;     	//в каком ухе слышно четвёртый канал КВ.
  int dvActive;     //указание какой канал КВ активен (в какой мы говорим).
  int dvSide;     	//указание какой стороне принадлежит данная КВ рация.
  int TAN;          //Тангента (какую кнопку нажал пользователь) (0 - Голос, 1 - КВ, 2 - ДВ)
};

struct argsComOTH
{
  float posX;         //позиция игрока по горизонтали.
  float posY;         //позиция игрока по вертикали.
  float posZ;         //высота по отношению к морю.
  int Dir;          //направление взгляда игрока.
  int Mode;         //громкость речи (0 – шёпот, 1 – нормальный голос, 2 – крик)
  QString vehId;  	//идентификатор техники (если 0 - на ногах).
  int isOut;      	//индикатор высунности (не используется если vehId = 0)
  float kvChan0;  	//частота первого канала КВ.
  float kvChan1;  	//частота второго канал КВ.
  float kvChan2;  	//частота третьего канала КВ.
  float kvChan3;  	//частота четвёртого канала КВ.
  int kvActive;     //указание какой канал КВ активен (в какой мы говорим).
  int kvSide;     	//указание какой стороне принадлежит данная КВ рация.
  float dvChan0;  	//частота первого канала ДВ.
  float dvChan1;  	//частота второго канал ДВ.
  float dvChan2;  	//частота третьего канала ДВ.
  float dvChan3;  	//частота четвёртого канала ДВ.
  int dvActive;     //указание какой канал КВ активен (в какой мы говорим).
  int dvSide;     	//указание какой стороне принадлежит данная КВ рация.
  int TAN;          //Тангента (какую кнопку нажал пользователь) (0 - Голос, 1 - КВ, 2 - ДВ)
  int hearableKV;   // Канал КВ на котором этот игрок может быть услышан.
  int hearableDV;   // Канал ДВ на котором этот игрок может быть услышан.
};

struct argsComMIN
{
  float posX;         //позиция игрока по горизонтали.
  float posY;         //позиция игрока по вертикали.
  float posZ;         //высота по отношению к морю.
  int Dir;          //направление взгляда игрока.
  int Mode;         //громкость речи (0 – шёпот, 1 – нормальный голос, 2 – крик)
  int TAN;          //Тангента (какую кнопку нажал пользователь) (0 - Голос, 1 - КВ, 2 - ДВ)
};
#pragma pack(pop)

#define I_COMMAND_POSITION      10      //первый символ команды
#define I_COMMAND_LENGTH        3       //длина команды
#define I_ARGS_POSITION         34      //первый символ блока аргументов
#define I_ARGS_IN_POS           36      //количество аргументов в POS
#define I_ARGS_IN_OTH           20      //количество аргументов в OTH

//! Парсер
/*! Коды возврата:
    0     - неизвестная команда;
    1     - невалидное сообщение;

    10    - успешно обработана команда POS;
    11    - успешно обработана команда OTH;
    12    - успешно обработана команда MIN;
    13    - успешно обработана команда REQ;

    102   - ошибка преобразования аргумента в команде POS;
    112   - ошибка преобразования аргумента в команде OTH;
    122   - ошибка преобразования аргумента в команде MIN;
    */
int commandCheck( std::string messageWS, argsComPOS &args_pos,
                  argsComOTH &args_oth, argsComMIN &args_min );

#endif // PARSER_H