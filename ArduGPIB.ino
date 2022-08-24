/*
 * GIPB Ether Adapter with Arduino Nano (Every)
*/

/*
 * BYE, QUI, EXI
 * RST
 * STA
 * IFC
 * REN
 * LOC
 * DCL
 * SRQ
 * TIM ms
 * DEL code1( code2 ...)
 * CLE(:add)
 * LIS(:add)
 * TAL(:add) txt
 */

// Define PIN assign
#define DIO1  19
#define DIO2  18
#define DIO3  17
#define DIO4  16
#define DIO5  15
#define DIO6  14
#define DIO7  9
#define DIO8  8
#define EOI   6
#define DAV   5
#define NRFD  4
#define NDAC  3
#define IFC   7
#define ATN   2
#define REN   GND
#define SRQ   GND

// for Ethernet
#include <EthernetENC.h>
byte ip[] = { 10, 77, 0, 123 };
byte mac[] = { 0xFE, 0xFF, 0x0A, 0x4D, 0x00, 0x7B }; // locally administered {0xFe, 0xFF, 10d, 77d, 0d, 123d}
EthernetServer server(1234); // same port as PROLOGIX GPIB-ETHERNET-CONTROLLER
EthernetClient client;

// for GPIB
byte address = 20; // default GPIB address
unsigned long ms_timeout = 10000; // 10 sec. for default timeout

// for Command Interpreter
String line, cmd, arg;

void setup() {
  // initialize the ethernet device
  Ethernet.init(10); // CS pin10
  Ethernet.begin(mac, ip);
  // start listening for clients
  server.begin();
  
  // initialize gpib line
  pinMode(EOI, OUTPUT); digitalWrite(EOI, HIGH);
  pinMode(DAV, OUTPUT); digitalWrite(DAV, HIGH); 
  pinMode(NDAC, OUTPUT); digitalWrite(NDAC, LOW);
  pinMode(NRFD, OUTPUT); digitalWrite(NRFD, LOW);

}

void loop() {
  int i;

  // 新規接続の管理
  EthernetClient new_client = server.accept();
  if (new_client) { // 新しいクライアントが接続してきた
    if (client) { // 既にクライアントが接続していたら
      new_client.print("BUSY\r\n");
      new_client.stop();
    } else { // これが1つめのクライアントなら
      new_client.print("ACCEPT\r\n");
      client = new_client;
      line = String();
    }
  } 

  // コマンドインタプリタ
  if (client && client.available()) {
    char c = client.read(); 
    if (c == '\n') { // 終端文字なら
      line.trim(); // 切り詰める
      cmd = line.substring(0, 3); cmd.toUpperCase(); // 最初3文字をcmdとして取り出して大文字化
      // コマンドの解釈と実行
      if (cmd.equals("BYE") || cmd.equals("QUI") || cmd.equals("EXI")) { client.stop(); // クライアント停止
      } else if (cmd.equals("RST")) { delay(1000); // リセット操作
      } else if (cmd.equals("STA")) { gpibLineStatus(); // ライン状態の取得と送信
      } else if (cmd.equals("IFC")) { gpibIFC(); // IFC
      } else if (cmd.equals("REN")) { // REN
      } else if (cmd.equals("LOC")) { // LOC
      } else if (cmd.equals("DCL")) { gpibDCL(); // DCL
      } else if (cmd.equals("SRQ")) { // SRQ
      } else if (cmd.equals("TIM")) { // TIM ms
        int p = line.indexOf(' ', 3);
        if (p >= 0) { // コマンド部以降に空白が見つかった
          arg = line.substring(p); arg.trim(); // 空白以降をargとして切り出す
          if (arg.toInt() > 0) ms_timeout = arg.toInt(); // 0以上の場合のみ有効なので変数を入れ替える
        }
      } else if (cmd.equals("DEL")) { // DEL code1( code2 ...)
        
      } else if (cmd.equals("CLE")) { // CLE(:add)
        byte a = address; // アドレスとしてデフォルトアドレスを用意
        int p = line.indexOf(':', 3); // 3文字目以降の最初のコロン位置を探す
        if (p >= 0) { // コロンがあった
          if (line.substring(0, p).indexOf(' ') < 0) { // コロンより前に空白はなかった
            int b = line.substring(p+1).toInt(); // コロンより後ろを整数化
            if (b > 0 && b < 31) a = b; // 有効なアドレスだったので置換する
          }
        } // コロンがなかった場合はデフォルトアドレスを使う
        gpibSDC((byte)a); // アドレスを指定してクリアする
      } else if (cmd.equals("LIS")) { // LIS(:add)
        byte a = address; // アドレスとしてデフォルトアドレスを用意
        int p = line.indexOf(':', 3); // 3文字目以降の最初のコロン位置を探す
        if (p >= 0) { // コロンがあった
          if (line.substring(0, p).indexOf(' ') < 0) { // コロンより前に空白はなかった
            int b = line.substring(p+1).toInt(); // コロンより後ろを整数化
            if (b > 0 && b < 31) a = b; // 有効なアドレスだったので置換する
          }
        } // コロンがなかった場合はデフォルトアドレスを使う
        char str[255] = "";
        gpibListen(a, str, "\r\n");
        client.print(str); client.print("\r\n");
      } else if (cmd.equals("TAL")) { // TAL (arg) option
        byte a = address; // アドレスとしてデフォルトアドレスを用意
        int p = line.indexOf(':', 3); // 3文字目以降の最初のコロン位置を探す
        if (p < 0) { // コロンがなかった
          p = line.indexOf(' ', 3); // 3文字目以降の最初の空白位置を探す
          if (p >= 0) { // 空白があった
            arg = line.substring(p); arg.trim(); // 空白以降をargとして切り出す
            //gpibTalk(a, arg); // デフォルトアドレスに空白以降をコマンドとして送信
          }
        } else { // コロンがあった
          int q = line.substring(0, p).indexOf(' '); // コロンより前の空白位置を探す
          if (q >= 0) { // コロンより前に空白があった
            arg = line.substring(q); arg.trim(); // 空白以降をargとして切り出す
            //gpibTalk(a, arg); // デフォルトアドレスに空白以降をコマンドとして送信
          } else { // コロンより前に空白はなかった
            q = line.substring(p).indexOf(' '); // コロンより後の空白位置を探す
            if (q >= 0) { // コロンより後に空白があった
              int b = line.substring(p+1).toInt(); // コロンより後ろを整数化
              if (b > 0 && b < 31) a = b; // 有効なアドレスだったので置換する
              arg = line.substring(p+q); arg.trim(); // 空白以降をargとして切り出す
              //gpibTalk(a, arg); // デフォルトアドレスに空白以降をコマンドとして送信
            }
          }
        }
      } else { // other cmd
        ; // 何もしないし何も返さない
      }
      
      line = ""; // バッファを空にする
    } else { // 終端じゃないなら
      line += String(c);
    }
  }
  if (client && !client.connected()) client.stop();




}

void gpibLineStatus(void) {
  pinMode(ATN, INPUT); digitalWrite(ATN, HIGH); Serial.print(" ATN="); Serial.print(digitalRead(ATN));
  pinMode(DAV, INPUT); digitalWrite(DAV, HIGH); Serial.print(", DAV="); Serial.print(digitalRead(DAV));
  pinMode(NRFD, INPUT); digitalWrite(NRFD, HIGH); Serial.print(", NRFD="); Serial.print(digitalRead(NRFD));
  pinMode(NDAC, INPUT); digitalWrite(NDAC, HIGH); Serial.print(", NDAC="); Serial.print(digitalRead(NDAC));
  pinMode(EOI, INPUT); digitalWrite(EOI, HIGH); Serial.print(", EOI="); Serial.print(digitalRead(EOI));
  Serial.print(", DIO8-1=");
  pinMode(DIO8, INPUT); digitalWrite(DIO8, HIGH); Serial.print(digitalRead(DIO8));
  pinMode(DIO7, INPUT); digitalWrite(DIO7, HIGH); Serial.print(digitalRead(DIO7));
  pinMode(DIO6, INPUT); digitalWrite(DIO6, HIGH); Serial.print(digitalRead(DIO6));
  pinMode(DIO5, INPUT); digitalWrite(DIO5, HIGH); Serial.print(digitalRead(DIO5));
  pinMode(DIO4, INPUT); digitalWrite(DIO4, HIGH); Serial.print(digitalRead(DIO4));
  pinMode(DIO3, INPUT); digitalWrite(DIO3, HIGH); Serial.print(digitalRead(DIO3));
  pinMode(DIO2, INPUT); digitalWrite(DIO2, HIGH); Serial.print(digitalRead(DIO2));
  pinMode(DIO1, INPUT); digitalWrite(DIO1, HIGH); Serial.println(digitalRead(DIO1));

}

byte get_dio() {
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

void set_dio(byte x) {
  
  pinMode(DIO1, OUTPUT); digitalWrite(DIO1, bitRead(~x, 0));
  pinMode(DIO2, OUTPUT); digitalWrite(DIO2, bitRead(~x, 1));
  pinMode(DIO3, OUTPUT); digitalWrite(DIO3, bitRead(~x, 2));
  pinMode(DIO4, OUTPUT); digitalWrite(DIO4, bitRead(~x, 3));
  pinMode(DIO5, OUTPUT); digitalWrite(DIO5, bitRead(~x, 4));
  pinMode(DIO6, OUTPUT); digitalWrite(DIO6, bitRead(~x, 5));
  pinMode(DIO7, OUTPUT); digitalWrite(DIO7, bitRead(~x, 6));
  pinMode(DIO8, OUTPUT); digitalWrite(DIO8, bitRead(~x, 7));
  
}

void gpibWrite(byte data) {
  // wait until (LOW == NDAC)
  pinMode(NDAC, INPUT);
  while (HIGH == digitalRead(NDAC)) { ; }
  
  // output data to DIO
  set_dio(data);
  
  // wait until (HIGH == NRFD)
  pinMode(NRFD, INPUT);
  while (LOW == digitalRead(NRFD)) { ; }
  
  // validate data
  pinMode(DAV, OUTPUT);
  digitalWrite(DAV, LOW);
  
  // wait until (HIGH == NDAC)
  while (LOW == digitalRead(NDAC)) { ; }
  
  digitalWrite(DAV, HIGH);
  set_dio(0); delayMicroseconds(10);
  
  return;
}

boolean gpibRead(byte *data) {
  
  boolean ret = false;
  
  // prepare to listen
  pinMode(NRFD, OUTPUT);
  digitalWrite(NRFD, HIGH);
  
  // wait until (LOW == DAV)
  pinMode(DAV, INPUT);
  while (HIGH == digitalRead(DAV)) { ; }
  
  // Ready for data
  digitalWrite(NRFD, LOW);
  
  // read from DIO
  *data = get_dio();
  
  // check EOI
  pinMode(EOI, INPUT);
  if (LOW == digitalRead(EOI)) {
    ret = true;
  } else {
    ret = false;
  }
  
  // data accepted
  pinMode(NDAC, OUTPUT);
  digitalWrite(NDAC, HIGH);
  
  // wait until invalid data
  while (LOW == digitalRead(DAV)) { ; }
  digitalWrite(NDAC, LOW);
  
  return ret; // return true when EOI==LOW
}

void gpibTalk(byte addr, char *str) {
  // attention
  pinMode(EOI, OUTPUT);
  digitalWrite(EOI, HIGH);
  pinMode(ATN, OUTPUT);
  digitalWrite(ATN, LOW); delayMicroseconds(30);
  
  // unlisten
  gpibWrite(0x3F); delayMicroseconds(20);
  
  // talker address
  gpibWrite(0x40); delayMicroseconds(20);
  
  // listener address
  gpibWrite((byte)(0x20 + addr)); delayMicroseconds(20);
  
  // end of attention
  digitalWrite(ATN, HIGH); delayMicroseconds(20);
    
  // write string
  while (0 != *(str + 1)) {
    gpibWrite(*str); delayMicroseconds(20);
    str++;
  }
  
  // write last char
  digitalWrite(EOI, LOW);
  gpibWrite(*str); delayMicroseconds(20);  
  digitalWrite(EOI, HIGH);
  
}

boolean gpibListen(byte addr, char *str, char* del) {
  // attention
  pinMode(ATN, OUTPUT);
  digitalWrite(ATN, LOW); delayMicroseconds(30);
  
  // unlisten
  gpibWrite(0x3F); delayMicroseconds(20);
  
  // talker address
  gpibWrite((byte)(0x40 + addr)); delayMicroseconds(20);
  
  // listener address
  gpibWrite(0x20); delayMicroseconds(20);
  
  // end of attention
  pinMode(NRFD, OUTPUT); digitalWrite(NRFD, LOW);
  pinMode(NDAC, OUTPUT); digitalWrite(NDAC, LOW);
  
  delayMicroseconds(10);
  digitalWrite(ATN, HIGH); delayMicroseconds(20);
  
  // recieve data
  int i, s, len = strlen(del);
  byte c;
  boolean isLast, isDel;
  
  s = 0;
  do {
    isLast = gpibRead(&c); *(str) = c;
    isDel = true;
    for (i = 0 ; i < len ; i++) {
      isDel = isDel & (*(str - i) == *(del + len - i - 1));
    }
    str++;
  } while (!(isLast || isDel));
  *(str) = 0;

  return true;
}

void gpibIFC(void) {
  pinMode(IFC, OUTPUT); digitalWrite(IFC,LOW); delayMicroseconds(128);
  digitalWrite(IFC, HIGH);
}

void gpibDCL(void) {
  // attention
  pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(10);
  
  // send DCL
  gpibWrite(0x14); delayMicroseconds(10);
  
  // end of attention
  digitalWrite(ATN, HIGH); delayMicroseconds(20);
}

void gpibSDC(byte addr) {
  // attention
  pinMode(ATN, OUTPUT); digitalWrite(ATN, LOW); delayMicroseconds(10);
  
  // unlisten
  gpibWrite(0x3F); delayMicroseconds(10);
  
  // send ADDRESS for device clear
  gpibWrite((byte)(32 + addr)); delayMicroseconds(10);
  
  // send SDC
  gpibWrite(0x04); delayMicroseconds(10);
  
  // end of attention
  digitalWrite(ATN, HIGH); delayMicroseconds(20);
}
