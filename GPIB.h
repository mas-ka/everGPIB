#ifndef _GPIB_
  #define _GPIB_
  #include "Arduino.h"
  
  // Define PIN assign
  #define DIO1  3
  #define DIO2  4
  #define DIO3  5
  #define DIO4  6
  #define DIO5  21
  #define DIO6  20
  #define DIO7  19
  #define DIO8  18
  #define EOI   7
  #define DAV   8
  #define NRFD  16
  #define NDAC  9
  #define IFC   15
  #define ATN   14
  #define REN   17
  #define SRQ   2
  
  class GPIB {
    public:
      String address_default = "1",         // default GPIB address
             delimiters_default = "13+10";  // default delimiters
      unsigned long ms_timeout = 10000; // 10 sec. for default timeout
      
      GPIB(); // インスタンス
      void init(void); // 初期化
      void sendIFC(void); // IFC
      void sendREM(void); // REM
      void sendLOC(void); // LOC
      boolean sendDCL(void); // DCL
      boolean sendSDC(const byte addr); // SDC
      boolean getSRQ(void); // SRQ
      boolean searchBySerialPoll(byte &addr, byte &status); // SRQのシリアルポール
      String getLineStatus(void); // ライン状態の取得
      boolean talk(const byte addr, const String com, const String del); // トーカ
      boolean listen(const byte addr, String &reply, const String del); // リスナ
      
    private:
      byte get_dio(void);
      void set_dio(byte x);
      boolean write(const byte data);
      boolean read(byte *data, boolean *eoi);
      
  };
#endif
