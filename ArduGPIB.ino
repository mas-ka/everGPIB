/*
 * GIPB Ether Adapter with Arduino Nano (Every)
*/
#include <EEPROM.h>

// for Ethernet
//#include <Ethernet.h>
#include <EthernetENC.h>
byte ip[] = { 192, 168, 0, 1 }; // dummy address
byte mac[] = { 0xFE, 0xFF, 0x00, 0x00, 0x00, 0x00 }; // dummy locally administered
EthernetServer server(1234); // same port(1234) as PROLOGIX GPIB-ETHERNET-CONTROLLER
EthernetClient client;

// for GPIB
#include "GPIB.h"
GPIB gpib;

// for Serial Command Interpreter
String com;

// for Ethernet Command Interpreter
String line, verb, delimiters, del, option;
byte address;
boolean assertEOI;

void (*resetController) (void) = 0; // reset function

void setup() {
  // Setup serial
  Serial.begin(9600);
  com = String();
  
  // initialize the ethernet device
  Ethernet.init(10); // CS pin10

  // load ip address from EEPROM
  for (byte i = 0 ; i < 4 ; i++) ip[i] = EEPROM.read(i); // IP address = 0 - 3

  // load mac address from EEPROM
  for (byte i = 4 ; i < 10 ; i++) mac[i] = EEPROM.read(i); // IP address = 4 - 9
  
  Ethernet.begin(mac, ip);
  server.begin();
  
  // initialize gpib
  gpib.init();
  
}

void loop() {
  // シリアル側コマンドインタプリタ
  // ?MAC -> XX:XX:XX:XX:XX:XX
  // ?IPA -> XXX.XXX.XXX.XXX
  // !MAC XX:XX:XX:XX:XX:XX
  // !IPA XXX.XXX.XXX.XXX
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      com.trim();
      String v = com.substring(0, 4); v.toUpperCase();
      String o = com.substring(com.indexOf(' ')); o.trim();
      if (v.equals("?MAC")) {
         char buff[8];
         for (int i = 0 ; i < 5 ; i++) { sprintf(buff, "%02x:", mac[i]); Serial.print(buff); }
         sprintf(buff, "%02x\r\n", mac[5]); Serial.print(buff);
      } else if (v.equals("?IPA")) {
        for (int i = 0 ; i < 3 ; i++) { Serial.print(ip[i], DEC); Serial.print('.'); }
        Serial.print(ip[3], DEC); Serial.print("\r\n");
      } else if (v.equals("!MAC")) {
        o += ':'; // 末尾に':'を追加
        int j = 0;
        boolean isInvalid = false;
        char buf[3];
        long t[] = {-1, -1, -1, -1, -1, -1};
        for (int i = o.indexOf(':') ; i >= 0 && j < 6 ; i = o.indexOf(':'), j++) {
          (o.substring(0, i)).toCharArray(buf, 3);
          t[j] = strtol(buf, NULL, 16);
          isInvalid |= (t[j] < 0 || t[j] > 255); // 0-255の範囲外ならInvalid
          o = o.substring(i+1);
        }
        isInvalid |= (j != 6); // 6つ読めなかった場合にInvalidとする
        if(!isInvalid){
          for (int i = 0 ; i < 5 ; i++) {
            EEPROM.write(4+i, (byte)t[i]);
            sprintf(buf, "%02X", t[i]); Serial.print(String(buf)+":");
          }
          EEPROM.write(9, (byte)t[5]);
          sprintf(buf, "%02X", t[5]); Serial.print(String(buf)+" saved!\r\n");
        } else Serial.print("Invalid MAC address!\r\n");
      } else if (v.equals("!IPA")) {
        o += '.'; // 末尾に'.'を追加
        int j = 0;
        boolean isInvalid = false;
        long t[] = {-1, -1, -1, -1};
        for (int i = o.indexOf('.') ; i >= 0 && j < 4 ; i = o.indexOf('.'), j++) {
          t[j] = o.substring(0, i).toInt();
          isInvalid |= (t[j] < 0 || t[j] > 255); // 0-255の範囲外ならInvalid
          o = o.substring(i+1);
        }
        isInvalid |= (j != 4); // 4つ読めなかった場合にInvalidとする
        if(!isInvalid){
          for (int i = 0 ; i < 3 ; i++) {
            EEPROM.write(i, (byte)t[i]);
            Serial.print(t[i], DEC); Serial.print(".");
          }
          EEPROM.write(3, (byte)t[3]);
          Serial.print(t[3], DEC); Serial.print(" saved!\r\n");
        } else Serial.print("Invalid IP address!\r\n");
      } else {
        Serial.print("Unknown command.\r\n");
      }
    } else com += c;
  }

  // 新規Ethernet接続の管理
  EthernetClient new_client = server.accept();
  if (new_client) { // 新しいクライアントが接続してきた
    if (client) { // 既にクライアントが接続していたら
      new_client.print("BUSY\r\n"); new_client.stop(); // "BUSY"を返して切断する
    } else { // これが1つめのクライアントなら
      new_client.print("ACCEPT\r\n"); // "ACCEPT"を返す
      client = new_client;
      line = String();
    }
  }

  // Etherrnet側コマンドインタプリタ
  // BYE || QUI || EXI
  // RES || RST
  // STA
  // IFC
  // REM || REN
  // LOC
  // DCL
  // SRQ
  // SPO
  // TIM [ms]
  // CLE:addr || SDC:addr
  // LIS:addr:[del1][+del2] :delが指定されてない場合には必ずEOIまで読む
  // TAL:addr:[del1][+del2][-] option :'-'があるとEOIをアサートしない
  if (client && client.available()) {
    char c = client.read(); 
    if (c == '\n') { // 終端文字なら
      line.trim(); line += " "; // 切り詰めてから末尾に空白を追加
      
      // コマンド行の分割
      option = line.substring(line.indexOf(' ')); option.trim();
      verb = line.substring(0, line.indexOf(' ')); verb.trim(); verb += ":: ";
      delimiters = verb.substring(verb.indexOf(':')+1); delimiters.trim();
      address = (byte)(delimiters.substring(0, delimiters.indexOf(':')).toInt());
      delimiters = delimiters.substring(delimiters.indexOf(':')+1);
      delimiters = delimiters.substring(0, delimiters.indexOf(':'));
      verb = verb.substring(0, verb.indexOf(':')); verb.trim(); verb.toUpperCase();

      // EOIを送信するかどうか
      assertEOI = !delimiters.endsWith("-");

      // デリミタのString化
      del = "";
      delimiters += '+'; // 末尾に'+'追加
      while (delimiters.length() > 0) {
        del += (char)delimiters.substring(0, delimiters.indexOf('+')).toInt();
        delimiters = delimiters.substring(delimiters.indexOf('+')+1);
      }
      
      // コマンドの解釈
      if (verb.startsWith("BYE") || verb.startsWith("QUI") || verb.startsWith("EXI")) { // クライアント停止
        client.stop();
      } else if (verb.startsWith("RES") || verb.startsWith("RST")) { // リセット操作
        delay(1000); resetController();
      } else if (verb.startsWith("STA")) {// ライン状態の取得と送信
        client.println(gpib.getLineStatus()); 
      } else if (verb.startsWith("IFC")) { // IFC
        gpib.sendIFC(); client.println("OK");
      } else if (verb.startsWith("REM") || verb.startsWith("REN")) { // REM
        gpib.sendREM(); client.println("OK");
      } else if (verb.startsWith("LOC")) { // LOC
        gpib.sendLOC(); client.println("OK");
      } else if (verb.startsWith("DCL")) { // DCL
        gpib.sendDCL(); client.println("OK");
      } else if (verb.startsWith("SRQ")) { // SRQ
        client.println(gpib.getSRQ()?"HIGH":"LOW");
      } else if (verb.startsWith("SPO")) { // Serial Poll
        byte addr, status;
        if (gpib.searchBySerialPoll(addr, status)) { // found SRQ device
          client.println(addr+":"+("0000000"+String(status, BIN)).substring(String(status, BIN).length()-1));
        } else client.println("NONE"); // not found
      } else if (verb.startsWith("TIM")) { // TIM [ms]
        if (!option.equals("")) gpib.ms_timeout = (option.toInt() < 1)?0:option.toInt();
          // 1ms未満ならゼロ、そうでないなら与えられたミリ秒をタイムアウトに入れる
        client.println(gpib.ms_timeout); // 現在のタイムアウトミリ秒数を返す
      } else if (verb.startsWith("CLE") || verb.startsWith("SDC")) { // CLE:addr || SDC:addr
        if (address < 1 || address > 30) client.println("ERROR");
        else client.println(gpib.sendSDC(address)?"OK":"ERROR");
      } else if (verb.startsWith("LIS")) { // LIS:addr:[del1][+del2]
        if (address < 1 || address > 30) client.println("ERROR");
        else {
          String reply = String();
          gpib.listen(address, reply, del);
          client.print(reply);
        }
      } else if (verb.startsWith("TAL")) { // TAL:addr:[del1][+del2][-] option
        if (address < 1 || address > 30) client.println("ERROR");
        else client.println(gpib.talk(address, option, del, assertEOI)?"OK":"ERROR");
      } else client.println("ERROR"); // 上記以外
      line = ""; // バッファを空にする
    } else { line += String(c); } // 終端じゃないなら
  }
  if (client && !client.connected()) client.stop(); // 切断されていたら解放する
  
}
