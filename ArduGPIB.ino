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
String line, verb, address, delimiters, option;

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

  // load device default GPIB address from EEPROM
  byte addr = EEPROM.read(10); // default GPIB address = 10
  if (addr < 1 || addr > 30) gpib.address_default = addr; // 許されるGPIBアドレスは1-30

  // load default delimiters from GPIB
  byte del[] = {0x0D, 0x0A}; // \r+\n
  del[0] = EEPROM.read(11); del[1] = EEPROM.read(12); // default delimiters = 11, 12
  if (del[0] == 0) { del[0] = 0x0D; del[1] = 0x0A; } // もし1文字目がゼロだったら「\r\n」に強制する
  String s = String();
  for (int i = 0 ; i < 2 ; i++) if (del[i] != 0) s += '+'+String(del[i], DEC); else break; // 文字列化
  gpib.delimiters_default = s.substring(1);
  
}

void loop() {
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

  // シリアル側コマンドインタプリタ
  // ?MAC -> XX:XX:XX:XX:XX:XX
  // ?IPA -> XXX.XXX.XXX.XXX
  // ?ADD -> X
  // ?DEL -> XX+XX+XX...
  // !MAC XX:XX:XX:XX:XX:XX
  // !IPA XXX.XXX.XXX.XXX
  // !ADD X
  // !DEL XX+XX+XX...
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
      } else if (v.equals("?ADD")) {
        Serial.print(gpib.address_default+"\r\n");
      } else if (v.equals("?DEL")) {
        Serial.print(gpib.delimiters_default+"\r\n");
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
      } else if (v.equals("!ADD")) {
        if (o.toInt() > 0 && o.toInt() < 31) { // 許されるGPIBアドレスは1-30
          EEPROM.write(10, (byte)(o.toInt())); // 10バイト目に1バイト書き込み
          Serial.print(o.toInt()+" saved!\r\n");
        } else Serial.print("Invalid address!\r\n");
      } else if (v.equals("!DEL")) {
        o += '+'; // 末尾に'+'を追加
        long t[] = {0, 0};
        for (int i = o.indexOf('+'), j = 0 ; i >= 0 && j < 2 ; i = o.indexOf('+'), j++) {
          t[j] = o.substring(0, i).toInt();
          if (t[j] < 0 || t[j] > 255) t[j] = 0; // 1バイトの範囲外ならゼロにする
          o = o.substring(i+1);
        }
        if (t[0] > 0) {
          EEPROM.write(11, (byte)t[0]); // 1文字目を11バイト目に書き込み
          Serial.print(t[0], DEC);
          EEPROM.write(12, (byte)t[1]); // 2文字目を12バイト目に書き込み
          if (t[1] > 0) { Serial.print("+"); Serial.print(t[1], DEC); }
          Serial.print(" saved!\r\n");
        } else Serial.print("Invalid delimiters!\r\n");
      } else {
        Serial.print("Unknown command.\r\n");
      }
    } else com += c;
  }

  // Etherrnet側コマンドインタプリタ
  if (client && client.available()) {
    char c = client.read(); 
    if (c == '\n') { // 終端文字なら
      line.trim(); line += " "; // 切り詰めてから末尾に空白を追加
      
      // コマンド行の分割
      option = line.substring(line.indexOf(' ')); option.trim();
      verb = line.substring(0, line.indexOf(' ')); verb.trim(); verb += ":: ";
      delimiters = verb.substring(verb.indexOf(':')+1); delimiters.trim();
      address = delimiters.substring(0, delimiters.indexOf(':'));
      if (address.length() == 0 || address.toInt() < 1 || address.toInt() > 30) address = gpib.address_default;
      delimiters = delimiters.substring(delimiters.indexOf(':')+1);
      delimiters = delimiters.substring(0, delimiters.indexOf(':'));
      if (delimiters.length() == 0) delimiters = gpib.delimiters_default;
      verb = verb.substring(0, verb.indexOf(':')); verb.trim(); verb.toUpperCase();

      // デリミタのString化
      String del = String();
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
      } else if (verb.startsWith("TIM")) { // TIM ms
        if (option.toInt() > 0) {
          gpib.ms_timeout = option.toInt(); // 1ms以上なら有効なのでタイムアウト定数を入れ替える
          client.println("OK");
        } else client.println("ERROR");
      } else if (verb.startsWith("CLE")) { // CLE(:add)
        client.println(gpib.sendSDC((byte)address.toInt())?"OK":"ERROR");
      } else if (verb.startsWith("LIS")) { // LIS(:add)(:del1+del2+...)
        String reply = String();
        gpib.listen((byte)address.toInt(), reply, del);
        client.print(reply);
      } else if (verb.startsWith("TAL")) { // TAL(:add) option
        client.println(gpib.talk((byte)address.toInt(), option)?"OK":"ERROR");
      } else { ; } // 上記以外なら何もしない
      line = ""; // バッファを空にする
    } else { line += String(c); } // 終端じゃないなら
  }
  if (client && !client.connected()) client.stop(); // 切断されていたら解放する
  
}
