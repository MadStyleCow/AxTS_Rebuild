#include "include/parser.h"

int commandCheck( std::string messageWS,
                  argsComPOS &args_pos,
                  argsComOTH &args_oth,
                  argsComMIN &args_min )
{
   QString messageIN;
   //общее регулярное выражение для команд
   QRegExp commandRegExp("\\\[A2TS_CMD\\\][a-zA-Z]{3}\\\[\\\/A2TS_CMD\\\](\\\[A2TS_ARG\\\](\\\-?(([0-9]+)|([0-9]+\\\.[0-9]+)|([a-zA-Z0-9]+);))*\\\[\\\/A2TS_ARG\\\])?");
   messageIN = messageWS.c_str();
   if( !commandRegExp.exactMatch( messageIN ) )
   {
     return 1;
   }
   QString command = messageIN.mid( I_COMMAND_POSITION, I_COMMAND_LENGTH );
   command.toUpper();

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//

   if( command == "POS" )
   {
     int current_end,
         current_begin = I_ARGS_POSITION;
     bool notError = true;

     //парсим аргументы
     for( int i = 0; ( i < I_ARGS_IN_POS ) && notError; ++i )
     {
       current_end = messageIN.indexOf( ";", current_begin );
       if ( current_end != -1 )
       {
         QString arg = messageIN.mid( current_begin, current_end - current_begin );
         current_begin = current_end + 1;
         switch( i )
         {

           case 0:
             args_pos.posX = arg.toFloat( &notError);
             //заполнение posX
             break;

           case 1:
             args_pos.posY = arg.toFloat( &notError);
             //заполнение posY
             break;

           case 2:
             args_pos.posZ = arg.toFloat( &notError);
             //заполнение posZ
             break;

           case 3:
             args_pos.Dir = arg.toInt( &notError, 10 );
             //заполнение Dir
             break;

           case 4:
             args_pos.Mode = arg.toInt( &notError, 10 );
             //заполнение Mode
             break;

           case 5:
             args_pos.vehId = arg;
             //заполнение vehId
             break;

           case 6:
             args_pos.isOut = arg.toInt( &notError, 10 );
             //заполнение isOut
             break;

             //#################=KV=0=#####################
           case 7:
             args_pos.kvChan0 = arg.toFloat( &notError );
             //заполнение kvChan0
             break;

           case 8:
             args_pos.kvVol0 = arg.toFloat( &notError );
             //заполнение kvVol0
             break;

           case 9:
             args_pos.kvPos0 = arg.toInt( &notError, 10 );
             //заполнение kvPos0
             break;

             //#################=KV=1=#####################
           case 10:
             args_pos.kvChan1 = arg.toFloat( &notError );
             //заполнение kvChan1
             break;

           case 11:
             args_pos.kvVol1 = arg.toFloat( &notError );
             //заполнение kvVol1
             break;

           case 12:
             args_pos.kvPos1 = arg.toInt( &notError, 10 );
             //заполнение kvPos1
             break;

             //#################=KV=2=#####################
           case 13:
             args_pos.kvChan2 = arg.toFloat( &notError );
             //заполнение kvChan2
             break;

           case 14:
             args_pos.kvVol2 = arg.toFloat( &notError );
             //заполнение kvVol2
             break;

           case 15:
             args_pos.kvPos2 = arg.toInt( &notError, 10 );
             //заполнение kvPos2
             break;

             //#################=KV=3=#####################
           case 16:
             args_pos.kvChan3 = arg.toFloat( &notError );
             //заполнение kvChan3
             break;

           case 17:
             args_pos.kvVol3 = arg.toFloat( &notError );
             //заполнение kvVol3
             break;

           case 18:
             args_pos.kvPos3 = arg.toInt( &notError, 10 );
             //заполнение kvPos3
             break;

           case 19:
             args_pos.kvActive = arg.toInt( &notError, 10 );
             //заполнение kvActive
             break;

           case 20:
             args_pos.kvSide = arg.toInt( &notError, 10 );
             //заполнение kvSide
             break;

             //#################=DV=0=#####################
           case 21:
             args_pos.dvChan0 = arg.toFloat( &notError );
             //заполнение dvChan0
             break;

           case 22:
             args_pos.dvVol0 = arg.toFloat( &notError );
             //заполнение dvVol0
             break;

           case 23:
             args_pos.dvPos0 = arg.toInt( &notError, 10 );
             //заполнение dvPos0
             break;

             //#################=DV=1=#####################
           case 24:
             args_pos.dvChan1 = arg.toFloat( &notError );
             //заполнение dvChan1
             break;

           case 25:
             args_pos.dvVol1 = arg.toFloat( &notError );
             //заполнение dvVol1
             break;

           case 26:
             args_pos.dvPos1 = arg.toInt( &notError, 10 );
             //заполнение dvPos1
             break;

             //#################=DV=2=#####################
           case 27:
             args_pos.dvChan2 = arg.toFloat( &notError );
             //заполнение dvChan2
             break;

           case 28:
             args_pos.dvVol2 = arg.toFloat( &notError );
             //заполнение dvVol2
             break;

           case 29:
             args_pos.dvPos2 = arg.toInt( &notError, 10 );
             //заполнение dvPos2
             break;

             //#################=DV=3=#####################
           case 30:
             args_pos.dvChan3 = arg.toFloat( &notError );
             //заполнение dvChan3
             break;

           case 31:
             args_pos.dvVol3 = arg.toFloat( &notError );
             //заполнение dvVol3
             break;

           case 32:
             args_pos.dvPos3 = arg.toInt( &notError, 10 );
             //заполнение dvPos3
             break;

           case 33:
             args_pos.dvActive = arg.toInt( &notError, 10 );
             //заполнение dvActive
             break;

           case 34:
             args_pos.dvSide = arg.toInt( &notError, 10 );
             //заполнение dvSide
             break;

           case 35:
             args_pos.TAN = arg.toInt( &notError, 10 );
             //заполнение TAN
             break;

         } //конец switch
       }
     } //конец for для аргументов
     if( !notError ) return 102; //ошибка преобразования аргумента
     else return 10; //POS обработан
   } //конец обработки команды POS

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//

   if( command == "OTH" )
   {
     int current_end,
         current_begin = I_ARGS_POSITION;
     bool notError = true;

     //парсим аргументы
     for( int i = 0; ( i < I_ARGS_IN_OTH ) && notError; ++i )
     {
       current_end = messageIN.indexOf( ";", current_begin );
       if ( current_end != -1 )
       {
         QString arg = messageIN.mid( current_begin, current_end - current_begin );
         current_begin = current_end + 1;
         switch( i )
         {

           case 0:
             args_oth.posX = arg.toFloat( &notError );
             //заполнение posX
             break;

           case 1:
             args_oth.posY = arg.toFloat( &notError );
             //заполнение posY
             break;

           case 2:
             args_oth.posZ = arg.toFloat( &notError );
             //заполнение posZ
             break;

           case 3:
             args_oth.Dir = arg.toInt( &notError, 10 );
             //заполнение Dir
             break;

           case 4:
             args_oth.Mode = arg.toInt( &notError, 10 );;
             //заполнение Mode
             break;

           case 5:
             args_oth.vehId = arg;
             //заполнение vehId
             break;

           case 6:
             args_oth.isOut = arg.toInt( &notError, 10 );
             //заполнение isOut
             break;

             //#################=KV=0=#####################
           case 7:
             args_oth.kvChan0 = arg.toFloat( &notError );
             //заполнение kvChan0
             break;

             //#################=KV=1=#####################
           case 8:
             args_oth.kvChan1 = arg.toFloat( &notError );
             //заполнение kvChan1
             break;

             //#################=KV=2=#####################
           case 9:
             args_oth.kvChan2 = arg.toFloat( &notError );
             //заполнение kvChan2
             break;

             //#################=KV=3=#####################
           case 10:
             args_oth.kvChan3 = arg.toFloat( &notError );
             //заполнение kvChan3
             break;

           case 11:
             args_oth.kvActive = arg.toInt( &notError, 10 );
             //заполнение kvActive
             break;

           case 12:
             args_oth.kvSide = arg.toInt( &notError, 10 );
             //заполнение kvSide
             break;

             //#################=DV=0=#####################
           case 13:
             args_oth.dvChan0 = arg.toFloat( &notError );
             //заполнение dvChan0
             break;

             //#################=DV=1=#####################
           case 14:
             args_oth.dvChan1 = arg.toFloat( &notError );
             //заполнение dvChan1
             break;

             //#################=DV=2=#####################
           case 15:
             args_oth.dvChan2 = arg.toFloat( &notError );
             //заполнение dvChan2
             break;

             //#################=DV=3=#####################
           case 16:
             args_oth.dvChan3 = arg.toFloat( &notError );
             //заполнение dvChan3
             break;

           case 17:
             args_oth.dvActive = arg.toInt( &notError, 10 );
             //заполнение dvActive
             break;

           case 18:
             args_oth.dvSide = arg.toInt( &notError, 10 );
             //заполнение dvSide
             break;

           case 19:
             args_oth.TAN = arg.toInt( &notError, 10 );
             //заполнение TAN
             break;

         } //конец switch
       }
     } //конец for для аргументов
     if( !notError ) return 112; //ошибка преобразования аргумента
     else return 11; //OTH обработан
   } //конец обработки команды OTH

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//

   if( command == "MIN" )
   {
     int current_end,
         current_begin = I_ARGS_POSITION;
     bool notError = true;

     //парсим аргументы
     for( int i = 0; ( i < I_ARGS_IN_OTH ) && notError; ++i )
     {
       current_end = messageIN.indexOf( ";", current_begin );
       if ( current_end != -1 )
       {
         QString arg = messageIN.mid( current_begin, current_end - current_begin );
         current_begin = current_end + 1;
         switch( i )
         {

           case 0:
             args_min.posX = arg.toFloat( &notError );
             //заполнение posX
             break;

           case 1:
             args_min.posY = arg.toFloat( &notError );
             //заполнение posY
             break;

           case 2:
             args_min.posZ = arg.toFloat( &notError );
             //заполнение posZ
             break;

           case 3:
             args_min.Dir = arg.toInt( &notError, 10 );
             //заполнение Dir
             break;

           case 4:
             args_min.Mode = arg.toInt( &notError, 10 );;
             //заполнение Mode
             break;

           case 5:
             args_min.TAN = arg.toInt( &notError, 10 );
             //заполнение TAN
             break;

         } //конец switch
       }
     } //конец for для аргументов
     if( !notError ) return 122; //ошибка преобразования аргумента
     else return 12; //MIN обработан
   } //конец обработки команды MIN

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//

   if( command == "REQ" )
   {
     return 13; //REQ обработан
   }   //конец обработки команды REQ

//----------------------------------------------------------------------------//
//----------------------------------------------------------------------------//
   return 0;
}
