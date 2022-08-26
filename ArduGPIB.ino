/*
 * GIPB Ether Adapter with Arduino Nano (Every)
*/

/*
 * BYE, QUI, EXI
 * RES, RST
 * STA
 * IFC
 * REM
 * LOC
 * DCL
 * SRQ
 * TIM ms
 * CLE(:add)
 * TAL(:add) option
 * LIS(:add)(:del1+del2+...)
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
//#include <Ethernet.h>
#include <EthernetENC.h>
byte ip[] = { 10, 77, 0, 123 };
byte mac[] = { 0xFE, 0xFF, 0x0A, 0x4D, 0x00, 0x7B }; // locally administered {0xFe, 0xFF, 10d, 77d, 0d, 123d}
EthernetServer server(1234); // same port as PROLOGIX GPIB-ETHERNET-CONTROLLER
EthernetClient client;

// for GPIB
#include "GPIB.h"
GPIB gpib;

// for Command Interpreter
String line, verb, address, delimiters, option;

void (*resetController) (void) = 0; // reset function

void setup() {
  // initialize the ethernet device
  Ethernet.init(10); // CS pin10
  Ethernet.begin(mac, ip);
  // start listening for clients
  server.begin();
  
  // initialize gpib line
  gpib.init();

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
      if (verb.startsWith("BYE") || verb.startsWith("QUI") || verb.startsWith("EXI")) { client.stop(); // クライアント停止
      } else if (verb.startsWith("RES") || verb.startsWith("RST")) { delay(1000); resetController(); // リセット操作
      } else if (verb.startsWith("STA")) { client.println(gpib.getLineStatus()); // ライン状態の取得と送信
      } else if (verb.startsWith("IFC")) { gpib.sendIFC(); // IFC
      } else if (verb.startsWith("REM")) { gpib.sendREM(); // REM
      } else if (verb.startsWith("LOC")) { gpib.sendLOC(); // LOC
      } else if (verb.startsWith("DCL")) { gpib.sendDCL(); // DCL
      } else if (verb.startsWith("SRQ")) { client.println(gpib.getSRQ()?"HIGH":"LOW"); // SRQ
      } else if (verb.startsWith("TIM")) { // TIM ms
        if (option.toInt() > 0) gpib.ms_timeout = option.toInt(); // 1ms以上なら有効なのでタイムアウト定数を入れ替える
      } else if (verb.startsWith("CLE")) { gpib.sendSDC((byte)address.toInt()); // CLE(:add)
      } else if (verb.startsWith("LIS")) { // LIS(:add)(:del1+del2+...)
        String reply = String();
        gpib.listen((byte)address.toInt(), reply, del);
        client.print(reply);
      } else if (verb.startsWith("TAL")) { // TAL(:add) option
        gpib.talk((byte)address.toInt(), option);
      } else { ; } // 上記以外なら何もしない
      line = ""; // バッファを空にする
    } else { line += String(c); } // 終端じゃないなら
  }
  if (client && !client.connected()) client.stop(); // 切断されていたら解放する




}
