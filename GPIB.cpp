#include "GPIB.h"
#include <Arduino.h>

// Constructor
GPIB::GPIB() {}

// public functions
void GPIB::init(void) {
  pinMode(EOI, OUTPUT); digitalWrite(EOI, HIGH);
  pinMode(DAV, OUTPUT); digitalWrite(DAV, HIGH); 
  pinMode(NDAC, OUTPUT); digitalWrite(NDAC, LOW);
  pinMode(NRFD, OUTPUT); digitalWrite(NRFD, LOW);
}

boolean GPIB::talk(const byte addr, const String com, const String del) { // 送信に失敗したらfalseを返す
  // set EOI to FALSE (HIGH)
  pinMode(EOI, OUTPUT); digitalWrite(EOI, HIGH);
  
  // attention
  pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(30);
  
  // unlisten
  if (!write(0x3F)) return false; delayMicroseconds(20);
  
  // talker address (0 == self)
  if (!write(0x40)) return false; delayMicroseconds(20);
  
  // listener address
  if (!write((byte)(0x20 + addr))) return false; delayMicroseconds(20);
  
  // end of attention
  digitalWrite(ATN, HIGH); delayMicroseconds(20);
    
  // write string
  com.concat(del); // デリミタを末尾に連結する
  int i;
  for (i = 0 ; i < com.length()-1 ; i++) {
    if (!write((byte)com.indexOf(i))) return false; delayMicroseconds(20);
  }
  
  // write last char
  digitalWrite(EOI, LOW);
  if (!write((byte)com.indexOf(i))) return false; delayMicroseconds(20);
  digitalWrite(EOI, HIGH);

  return true;
}

boolean GPIB::listen(const byte addr, String &reply, const String del) {
  // 返り値: タイムアウト等でfalse
  // &reply : 読めたStringを入れる。
  // del  : デリミタ文字列
  
  unsigned long start = millis();
  
  // attention
  pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(30);
  
  // unlisten
  if (!write(0x3F)) return false; delayMicroseconds(20);
  
  // talker address
  if (!write((byte)(0x40 + addr))) return false; delayMicroseconds(20);
  
  // listener address (0 == self)
  if (!write(0x20)) return false; delayMicroseconds(20);
  
  // end of attention
  pinMode(NRFD, OUTPUT); digitalWrite(NRFD, LOW);
  pinMode(NDAC, OUTPUT); digitalWrite(NDAC, LOW); delayMicroseconds(10);
  digitalWrite(ATN, HIGH); delayMicroseconds(20);
  
  // recieve data
  reply = ""; // 空にする
  byte c;
  boolean eoi;
  while (true) {
    if (!read(&c, &eoi)) return false; // バイト読み込みに失敗したのでfalseを返す
    if (millis()-start > ms_timeout) return false; // タイムアウトしたんでfalseで返す
    reply += (char)c; // 読めた文字を追加
    if (eoi) return true; // EOIが来たんで読めたとこまででtrueで返す
    if (reply.endsWith(del)) { // デリミタが来た
      reply = reply.substring(0, reply.indexOf(del));
      return true;
    }
  }
}

void GPIB::sendIFC(void) {
  pinMode(IFC, OUTPUT); digitalWrite(IFC, LOW); delayMicroseconds(128);
  digitalWrite(IFC, HIGH); delayMicroseconds(20);
}

void GPIB::sendREM(void) {
  pinMode(REN, OUTPUT); digitalWrite(REN, LOW); delayMicroseconds(128);
}

void GPIB::sendLOC(void) {
  pinMode(REN, OUTPUT); digitalWrite(REN, HIGH); delayMicroseconds(128);
}

boolean GPIB::sendDCL(void) { // 成功したらtrue、タイムアウト等でfalse
  // attention
  pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(10);
  
  // send DCL
  if (!write(0x14)) return false; delayMicroseconds(10);
  
  // end of attention
  digitalWrite(ATN, HIGH); delayMicroseconds(20);

  return true;
}

boolean GPIB::sendSDC(const byte addr) { // 成功したらtrue、タイムアウト等でfalse
  // attention
  pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(10);
  
  // unlisten
  if (!write(0x3F)) return false; delayMicroseconds(10);
  
  // send ADDRESS for device clear
  if (!write((byte)(32 + addr))) return false; delayMicroseconds(10);
  
  // send SDC
  if (!write(0x04)) return false; delayMicroseconds(10);
  
  // end of attention
  digitalWrite(ATN, HIGH); delayMicroseconds(20);

  return true;
}

String GPIB::getLineStatus(void) {
  String ret = String("Management bus lines :\r");
  pinMode(ATN,  INPUT_PULLUP); ret+="  ATN="; ret+=digitalRead(ATN)?"HIGH":"LOW"; ret+=",\r";
  pinMode(EOI,  INPUT_PULLUP); ret+="  EOI="; ret+=digitalRead(EOI)?"HIGH":"LOW"; ret+=".\r";
  ret += "Handshake lines :\r";
  pinMode(DAV,  INPUT_PULLUP); ret+="  DAV="; ret+=digitalRead(DAV)?"HIGH":"LOW"; ret+=",\r";
  pinMode(NRFD, INPUT_PULLUP); ret+=" NRFC="; ret+=digitalRead(NRFD)?"HIGH":"LOW"; ret+=",\r";
  pinMode(NDAC, INPUT_PULLUP); ret+=" NDAC="; ret+=digitalRead(NDAC)?"HIGH":"LOW"; ret+=".\r";
  ret += "Data lines :\r";
  pinMode(DIO8, INPUT_PULLUP); ret+=" DIO8="; ret+=digitalRead(DIO8)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO7, INPUT_PULLUP); ret+=" DIO7="; ret+=digitalRead(DIO7)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO6, INPUT_PULLUP); ret+=" DIO6="; ret+=digitalRead(DIO6)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO5, INPUT_PULLUP); ret+=" DIO5="; ret+=digitalRead(DIO5)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO4, INPUT_PULLUP); ret+=" DIO4="; ret+=digitalRead(DIO4)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO3, INPUT_PULLUP); ret+=" DIO3="; ret+=digitalRead(DIO3)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO2, INPUT_PULLUP); ret+=" DIO2="; ret+=digitalRead(DIO2)?"HIGH":"LOW"; ret+=",\r";
  pinMode(DIO1, INPUT_PULLUP); ret+=" DIO1="; ret+=digitalRead(DIO1)?"HIGH":"LOW"; ret+=".";
  return ret;
}

boolean GPIB::getSRQ(void) {
  pinMode(SRQ, INPUT); return (boolean)digitalRead(SRQ);
}

boolean GPIB::searchBySerialPoll(byte &addr, byte &status) {
  //  loop for searching RQS bit
  for (int i = 1 ; i < 31 ; i++) { // address == 0 is self
    unsigned long start = millis();
    
    // attention
    pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(30);

    // unlisten
    if (!write(0x3F)) return false; delayMicroseconds(10);
  
    // send SPE (Serial Poll Enable)
    if (!write(0x18)) return false; delayMicroseconds(10);

    // talker address
    if (!write((byte)(0x40 + addr))) return false; delayMicroseconds(20);

    // end of attention
    digitalWrite(ATN, HIGH); delayMicroseconds(128);

    // read DIO
    byte c;
    boolean eoi;
    if (!read(&c, &eoi)) return false; // バイト読み込みに失敗したのでfalseを返す
    if (millis()-start > ms_timeout) return false; // タイムアウトしたんでfalseで返す
    if (bitRead(c, 6)) { // RQSビットが立っていれば
      addr = (byte)i; // アドレスを返す
      status = c; // ステータスを返す
      return true; // trueを返す
    }
    
    // attention
    pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(30);
  
    // send SPD (Serial Poll Disable)
    if (!write(0x19)) return false; delayMicroseconds(10);

    // end of attention
    digitalWrite(ATN, HIGH); delayMicroseconds(128);
  }
  return false; // RQS立ってるデバイスが見つからなかったのでfalseを返す
}

// private functions
byte GPIB::get_dio() {
  byte x = 0;
  pinMode(DIO1, INPUT); bitWrite(x, 0, !digitalRead(DIO1));
  pinMode(DIO2, INPUT); bitWrite(x, 1, !digitalRead(DIO2));
  pinMode(DIO3, INPUT); bitWrite(x, 2, !digitalRead(DIO3));
  pinMode(DIO4, INPUT); bitWrite(x, 3, !digitalRead(DIO4));
  pinMode(DIO5, INPUT); bitWrite(x, 4, !digitalRead(DIO5));
  pinMode(DIO6, INPUT); bitWrite(x, 5, !digitalRead(DIO6));
  pinMode(DIO7, INPUT); bitWrite(x, 6, !digitalRead(DIO7));
  pinMode(DIO8, INPUT); bitWrite(x, 7, !digitalRead(DIO8));
  return x;
}

void GPIB::set_dio(byte x) {
  pinMode(DIO1, OUTPUT); digitalWrite(DIO1, bitRead(~x, 0));
  pinMode(DIO2, OUTPUT); digitalWrite(DIO2, bitRead(~x, 1));
  pinMode(DIO3, OUTPUT); digitalWrite(DIO3, bitRead(~x, 2));
  pinMode(DIO4, OUTPUT); digitalWrite(DIO4, bitRead(~x, 3));
  pinMode(DIO5, OUTPUT); digitalWrite(DIO5, bitRead(~x, 4));
  pinMode(DIO6, OUTPUT); digitalWrite(DIO6, bitRead(~x, 5));
  pinMode(DIO7, OUTPUT); digitalWrite(DIO7, bitRead(~x, 6));
  pinMode(DIO8, OUTPUT); digitalWrite(DIO8, bitRead(~x, 7));
}

boolean GPIB::write(const byte data) { // 与えられた1バイトが書き込めたらtrue、タイムアウト等でfalseを返す
  unsigned long start = millis();
  
  // wait until (LOW == NRFD && LOW == NDAC)
  pinMode(NRFD, INPUT); pinMode(NDAC, INPUT);
  while (HIGH == digitalRead(NRFD) && HIGH == digitalRead(NDAC)) {
    if (millis()-start > ms_timeout) return false; // タイムアウト監視
  }
  delayMicroseconds(10);
  
  // output data to DIO
  set_dio(data); delayMicroseconds(300);
  
  // wait until (HIGH == NRFD)
  while (LOW == digitalRead(NRFD)) {
    if (millis()-start > ms_timeout) return false; // タイムアウト監視
  }
  
  // validate data
  pinMode(DAV, OUTPUT); digitalWrite(DAV, LOW);
  
  // wait until (HIGH == NDAC)
  while (LOW == digitalRead(NDAC)) {
    if (millis()-start > ms_timeout) return false; // タイムアウト監視
  }
  delayMicroseconds(20);
  
  digitalWrite(DAV, HIGH);
  set_dio(0); delayMicroseconds(10);

  return true;
}


boolean GPIB::read(byte *data, boolean *eoi) {
  // 返り値: タイムアウト等でfalse
  // data : 読んだ1バイトを入れる。
  // eoi  : 最終バイトだったならtrueを返す

  unsigned long start = millis();
  boolean ret = false;
  
  // prepare to listen
  pinMode(NRFD, OUTPUT); digitalWrite(NRFD, HIGH);
  
  // wait until (LOW == DAV)
  pinMode(DAV, INPUT); while (HIGH == digitalRead(DAV)) {
    if (millis()-start > ms_timeout) return false; // タイムアウト監視
  }
  
  // Ready for data
  digitalWrite(NRFD, LOW);
  
  // read from DIO
  *data = get_dio();
  
  // check EOI
  pinMode(EOI, INPUT); *eoi = (LOW == digitalRead(EOI));
  
  // data accepted
  pinMode(NDAC, OUTPUT); digitalWrite(NDAC, HIGH);
  
  // wait until invalid data
  while (LOW == digitalRead(DAV)) {
    if (millis()-start > ms_timeout) return false; // タイムアウト監視
  }
  digitalWrite(NDAC, LOW);
  
  return true;
}
