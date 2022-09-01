/*
 * GIPB Ether Adapter with Arduino Nano Every (※ NOT FOR NANO)
*/
#include <EEPROM.h>

// for Ethernet
#include <Ethernet.h>
//#include <EthernetENC.h>
byte ip[] = { 192, 168, 0, 1 }; // dummy address
byte mac[] = { 0xFE, 0xFF, 0x00, 0x00, 0x00, 0x00 }; // dummy locally administered
EthernetServer server(2345); // different port from PROLOGIX GPIB-ETHERNET-CONTROLLER(1234)!
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

  // load default target GPIB address from EEPROM
  byte addr = EEPROM.read(10); // default GPIB address = 10
  if (0 < addr && addr < 31) gpib.target_address_default = addr; // 許されるGPIBアドレスは1-30

  // load default delimiters for GPIB from EEPROM
  byte del[] = {0x0D, 0x0A, 0x01}; // \r+\n & assertEOI
  del[0] = EEPROM.read(11); del[1] = EEPROM.read(12);  del[2] = EEPROM.read(13); // default delimiters = 11, 12, 13
  if (del[0] == 0) { del[1] = 0x00; del[2] = 0x01; } // もし1文字目がゼロだったら2文字目もゼロにしEOIを強制する
  if (del[0] > 126) del[0] = 0x0D;
  if (del[1] > 126) del[1] = 0x0A;
  String s = String();
  for (int i = 0 ; i < 2 ; i++) if (del[i] > 0) s += '+'+String(del[i], DEC); else break; // デリミタのシリアライズ
  s = s.substring(1); s += (del[2]>0)?".":""; // EOIのシリアライズ
  gpib.delimiters_default = s;

  // load AIC (Automatic IfC) for GPIB from EEPROM
  gpib.use_automatic_IFC = (0 != EEPROM.read(14));

  // load ARE (Automatic REn) for GPIB from EEPROM
  gpib.use_automatic_REN = (0 != EEPROM.read(15));

}

void loop() {
  // シリアル側コマンドインタプリタ
  // ? || ?HEL || HELP
  // ?MAC -> XX:XX:XX:XX:XX:XX
  // ?IPA -> XXX.XXX.XXX.XXX
  // ?TAD -> XXX
  // ?DEL -> XXX[+XXX][.]
  // ?AIC -> Y|N
  // ?ARE -> Y|N
  // !MAC XX:XX:XX:XX:XX:XX
  // !IPA XXX.XXX.XXX.XXX
  // !TAD XXX
  // !DEL XXX[+XXX}[.]
  // !AIC Y|N
  // !ARE Y|N
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n') {
      com.trim();
      String v = com.substring(0, 4); v.toUpperCase();
      String o = com.substring(com.indexOf(' ')); o.trim();
      if (com.equals("?") || v.equals("?HEL") || v.equals("HELP")) {
        Serial.print("? || ?HEL || HELP : to display this help.\r");
        Serial.print("?MAC : to display MAC address.\r");
        Serial.print("?IPA : to display IP address.\r");
        Serial.print("?TAD : to display default target GPIB address.\r");
        Serial.print("?DEL : to display default delimiters & EOI assertion.\r");
        Serial.print("?AIC : to display whether automatically IFC before TAL | LIS | CHA.\r");
        Serial.print("?ARE : to display whether automatically REN before TAL | LIS | CHA.\r");
        Serial.print("!MAC %02X:%02X:%02X:%02X:%02X:%02X : to set MAC address.\r ex. !MAC fe:ff:00:00:00:01\r");
        Serial.print("!IPA %d.%d.%d.%d : to set IP address.\r ex. !IPA 192.0.2.1\r");
        Serial.print("!TAD %d : to set default target GPIB address.\r ex. !TAD 12\r");
        Serial.print("!DEL [%d][+%d][.] : to set default delimiters & EOI assertion.\r");
        Serial.print("      |    |   ^ assert EOI when period\r");
        Serial.print("      |    ^ decimal ascii code for 2nd char of delimiters\r");
        Serial.print("      ^ decimal ascii code for 1st char of delimiters\r ex. !DEL 13+10. (for CR+LF with EOI assertion)\r");
        Serial.print("!AIC [Y|N] : to set whether automatically IFC before TAL|LIS|CHA.\r ex. !AIC Y\r");
        Serial.print("!ARE  {Y|N] : to set whether automatically REN before TAL|LIS|CHA.\r ex. !ARE Y\r");
        Serial.print("\n");
      }else if (v.equals("?MAC")) {
         char buff[8];
         for (int i = 0 ; i < 5 ; i++) { sprintf(buff, "%02x:", mac[i]); Serial.print(buff); }
         sprintf(buff, "%02x\r\n", mac[5]); Serial.print(buff);
      } else if (v.equals("?IPA")) {
        for (int i = 0 ; i < 3 ; i++) { Serial.print(ip[i], DEC); Serial.print('.'); }
        Serial.print(ip[3], DEC); Serial.print("\r\n");
      } else if (v.equals("?TAD")) {
        Serial.print(gpib.target_address_default, DEC); Serial.print("\r\n");
      } else if (v.equals("?DEL")) {
        Serial.print(gpib.delimiters_default); Serial.print("\r\n");
      } else if (v.equals("?AIC")) {
        Serial.print(gpib.use_automatic_IFC?"Y":"N"); Serial.print("\r\n");
      } else if (v.equals("?ARE")) {
        Serial.print(gpib.use_automatic_REN?"Y":"N"); Serial.print("\r\n");
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
      } else if (v.equals("!TAD")) {
        if (0 < o.toInt() && o.toInt() < 31) { // 許されるGPIBアドレスは1-30
          EEPROM.write(10, (byte)(o.toInt())); // 10バイト目に1バイト書き込み
          Serial.print(o.toInt(), DEC); Serial.print(" saved!\r\n");
        } else Serial.print("Invalid address!\r\n");
      } else if (v.equals("!DEL")) {
        byte del[] = {0, 0, 0};
        del[0] = (byte)(o.substring(0, o.indexOf('+'))).toInt();
        del[1] = (byte)(o.substring(o.indexOf('+'))).toInt();
        del[2] = (o.endsWith("."))?1:0;
        if ((0 < del[0] && del[0] < 127) || (del[0] == 0 && del[1] == 0 && del[2] == 1)) { // 入力が適正な場合
          EEPROM.write(11, del[0]); // 11バイト目に1文字目書き込み
          EEPROM.write(12, del[1]); // 12バイト目に2文字目書き込み
          EEPROM.write(13, del[2]); // 13バイト目にEOI書き込み
          String s = String();
          for (int i = 0 ; i < 2 ; i++) if (del[i] > 0) s += '+'+String(del[i], DEC); else break; // デリミタのシリアライズ
          s = s.substring(1); s += (del[2]>0)?".":""; // EOIのシリアライズ
          Serial.print(s+" saved!\r\n");
        } else Serial.print("Invalid address!\r\n");
      } else if (v.equals("!AIC")) {
        if (o.startsWith("Y") || o.startsWith("N")) {
          EEPROM.write(14, (o.startsWith("Y"))?1:0); // 14バイト目にAIC書き込み
          Serial.print((o.startsWith("Y"))?"Y":"N"); Serial.print(" saved!\r\n");
        } else Serial.print("Invalid parameter!\r\n");
      } else if (v.equals("!ARE")) {
        if (o.startsWith("Y") || o.startsWith("N")) {
          EEPROM.write(15, (o.startsWith("Y"))?1:0); // 15バイト目にARE書き込み
          Serial.print((o.startsWith("Y"))?"Y":"N"); Serial.print(" saved!\r\n");
        } else Serial.print("Invalid parameter!\r\n");
      } else Serial.print("Unknown command.\r\n");
      com = "";
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
  // CLE:[addr] || SDC:[addr]
  // LIS:[addr]:[del1][+del2][.] :delが指定されてない場合には必ずEOIまで読む
  // TAL:[addr]:[del1][+del2][.] option :'.'があると最後の文字と同時にEOIをアサートする
  // CHA:[addr]:[del1][+del2][.] option :TAL and LIS
  
  if (client && client.available()) {
    char c = client.read(); 
    if (c == '\n') { // 終端文字なら
      line.trim(); line += " "; // 切り詰めてから末尾に空白を追加
      
      // コマンド行の分割
      option = line.substring(line.indexOf(' ')); option.trim();
      verb = line.substring(0, line.indexOf(' ')); verb.trim(); verb += ":: ";
      delimiters = verb.substring(verb.indexOf(':')+1); delimiters.trim();
      address = (byte)(delimiters.substring(0, delimiters.indexOf(':')).toInt());
      if (address < 1 || address > 30) address = gpib.target_address_default; // アドレスが不適切ならデフォルトを使用
      delimiters = delimiters.substring(delimiters.indexOf(':')+1);
      delimiters = delimiters.substring(0, delimiters.indexOf(':'));
      if (delimiters.length() == 0) delimiters = gpib.delimiters_default; // デリミタが空ならデフォルトを使用
      verb = verb.substring(0, verb.indexOf(':')); verb.trim(); verb.toUpperCase();

      // 区切りの空白がない場合にはデリミタが不正になりオプションが空になるのをエラーで弾く
      if (!delimiters.equals(".") && !(delimiters.toInt() > 0 && delimiters.toInt() < 127)) verb = "";
      
      // EOIを送信するかどうか
      assertEOI = delimiters.endsWith(".");

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
        byte a, s;
        if (gpib.searchBySerialPoll(a, s)) { // found SRQ device
          client.print((int)a);
          client.println(":"+("0000000"+String(s, BIN)).substring(String(s, BIN).length()-1));
        } else client.println("NONE"); // not found
      } else if (verb.startsWith("TIM")) { // TIM [ms]
        if (!option.equals("")) gpib.ms_timeout = (option.toInt() < 1)?0:option.toInt();
          // 1ms未満ならゼロ、そうでないなら与えられたミリ秒をタイムアウトに入れる
        client.println(gpib.ms_timeout); // 現在のタイムアウトミリ秒数を返す
      } else if (verb.startsWith("CLE") || verb.startsWith("SDC")) { // CLE:[addr] || SDC:[addr]
        if (address < 1 || address > 30) client.println("ERROR");
        else client.println(gpib.sendSDC(address)?"OK":"ERROR");
      } else if (verb.startsWith("LIS")) { // LIS:[addr]:[del1][+del2][.]
        if (address < 1 || address > 30) client.println("ERROR");
        else {
          if (gpib.use_automatic_IFC) gpib.sendIFC(); // 自動IFCならIFCする
          boolean in_remote_saved = gpib.in_remote; // 現在の状態を保存
          if (!in_remote_saved && gpib.use_automatic_REN) gpib.sendREM(); // 現在LOCかつ自動RENならRENする
          String reply = String();
          gpib.listen(address, reply, del);
          client.print(reply);
          if (!in_remote_saved) gpib.sendLOC(); // 元々LOCならLOCに戻す
        }
      } else if (verb.startsWith("TAL")) { // TAL:[addr]:[del1][+del2][.] option
        if (address < 1 || address > 30) client.println("ERROR");
        else if (!assertEOI && del.equals("")) client.println("ERROR"); // デリミタもEOIもなしは許さない
        else {
          if (gpib.use_automatic_IFC) gpib.sendIFC(); // 自動IFCならIFCする
          boolean in_remote_saved = gpib.in_remote; // 現在の状態を保存
          if (!in_remote_saved && gpib.use_automatic_REN) gpib.sendREM(); // 現在LOCかつ自動RENならRENする
          client.println(gpib.talk(address, option, del, assertEOI)?"OK":"ERROR");
          if (!in_remote_saved) gpib.sendLOC(); // 元々LOCならLOCに戻す
        }
      } else if (verb.startsWith("CHA")) { // CHA:[addr]:[del1][+del2][.] option
        if (address < 1 || address > 30) client.println("ERROR");
        else if (!assertEOI && del.equals("")) client.println("ERROR"); // デリミタもEOIもなしは許さない
        else {
          if (gpib.use_automatic_IFC) gpib.sendIFC(); // 自動IFCならIFCする
          boolean in_remote_saved = gpib.in_remote; // 現在の状態を保存
          if (!in_remote_saved && gpib.use_automatic_REN) gpib.sendREM(); // 現在LOCかつ自動RENならRENする
          // 1. TALK
          gpib.talk(address, option, del, assertEOI);
          // 2. LISTEN
          String reply = String();
          gpib.listen(address, reply, del);
          client.print(reply);
          if (!in_remote_saved) gpib.sendLOC(); // 元々LOCならLOCに戻す
        }
      } else client.println("ERROR"); // 上記以外
      line = ""; // バッファを空にする
    } else { line += String(c); } // 終端じゃないなら
  }
  if (client && !client.connected()) client.stop(); // 切断されていたら解放する
  
}
