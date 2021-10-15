/*
    接点監視送信機 【Ver.3.6.1】
    (C)2019-2021 maita Works.
*/

#include <M5StickC.h>
#include <WiFi.h>
#include "time.h" //必須
//#include "EEPROM.h"
//#include <Wire.h> //I2C型Hatを使用する場合有効にする
//#include "esp_wps.h" //試験実装

//EEPROM
int addr = 0;
#define EEPROM_SIZE 64

//新規で背景のグレーを追加(互換性維持のため、ライブラリソースを改変しない事)
#define TFT_GREY 0x5AEB

//液晶輝度設定(7~8が最適値。15はかなりまぶしく、本体の発熱が非常に多いため非推奨)
uint8_t LCD_bl = 8; //液晶輝度の設定値(7～15まで);

//ファームウェアバージョン
const char *FwVersion = "3.6.1";

//WiFi定義

const char *ssid = "<Your SSID>";      //SSIDを入力
const char *password = "<Your Key>"; //KEYを入力(※WPSやWEPキーは使用不可)


//IFTTT定義(※privateKeyは流出注意)
const char *host = "maker.ifttt.com";                                   //IFTTTのアドレス
const char *host2 = "<Local Router>";                                     //ルーターのアドレス(接続試験用)

const char *privateKey = "<Your IFTTT PrivateKey>"; //IFTTTのWebHook接続時に必須

//数値保持変数
unsigned int eram = 0;   //接点監視用
unsigned int event = 0;  //接点監視用
unsigned int eram2 = 0;  //停電検出用
unsigned int event2 = 0; //停電検出用

//試験実装　加速度センサー変数
float accX = 0;
float accY = 0;
float accZ = 0;

const char *SendRaw = ""; //IFTTTへ送信する文字列
//const char* Send = "";

//電源管理変数
double vbat = 0.0;     //電池電圧
int discharge, charge; //充電、非充電
double temp = 0.0;     //温度
double bat_p = 0.0;    //電池出力

//画面チラツキ防止対策のスプライト描画
TFT_eSprite tftSprite = TFT_eSprite(&M5.Lcd);
/*
    tftSprite.createSprite(160, 80);
    M5.Lcd.setRotation(1);
    tftSprite.setRotation(1);

    ＊＊＊＊＊＊処理内容＊＊＊＊＊＊＊＊

    tftSprite.pushSprite(0, 0);

    で描画可能
    各項目で必要

*/

//RTCとNTPの定義
RTC_TimeTypeDef RTC_TimeStruct;        //RTC時計変数
RTC_DateTypeDef RTC_DateStruct;        //RTC日付変数
const char *ntpServer = "ntp.nict.jp"; //NTPサーバーのアドレス
const long gmtOffset_sec = 9 * 3600;
const int daylightOffset_sec = 0;
//configTime(gmtOffset_sec, daylightOffset_sec, ntpServer); で使用

//入出力ピンの定義
const int inputPin1 = 32; //M5StickCのGrove信号１(黄色)
const int inputPin2 = 33; //M5StickCのGrove信号２(白色)
const int ledPin = 10;    //M5StickCのLED(G10)
const int buttonPinA = 37; //M5StickCのA(前面)ボタン(G37)
const int buttonPinB = 39; //M5StickCのB(側面)ボタン(G39)

/*
  //Hatピンの定義(I2C型Hat使用時は使用禁止)
  const int Hat0 = 0;
  const int Hat26 = 26;
  const int Hat36 = 36;//※入力専用ピン
*/

//タイトルバー
const char *TitleBar = "";

//ステータスバー
int WiFiData = 0;
int NetData = 0;
int SendData = 0;
int offlinemode = 0;
const char *WiFiStatus = "";
const char *NetStatus = "";
const char *SendStatus = "";
const char *VinStatus = "";

//本体初期化＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊

void setup(void)
{
  //本体起動に必ず必要な項目
  M5.begin();
  M5.update();
  M5.IMU.Init(); //加速度センサー機能試験実装
  //Wire.begin(0,26);//I2C型Hatを使う時は効にする(0,26のみ使用可能、32,33,36は使用禁止)

  while (!setCpuFrequencyMhz(240))
  { //CPU周波数設定 10,20,40,80,160,240から選択 (通常は240MHz)
    ;
  }

  //シリアル通信初期化
  //Serial.begin(115200);
  //int inputchar;
  //inputchar = //Serial.read();

  //起動画面設定
  M5.Axp.ScreenBreath(LCD_bl); //液晶輝度設定
  tftSprite.createSprite(160, 80);
  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1); //本体方向設定 Grove配線が右向きの場合は1、左向きの場合は3(2、4は縦向き用のため使用不可)

  //電源管理
  M5.Axp.EnableCoulombcounter();
  float VinRaw = 0.0;
  int VinData = 0;

  //ボタン、Grove端子初期化
  int inputStateA = 0;
  int inputStateB = 0;
  int inputButtonA = 0;
  int inputButtonB = 0;

  //入出力ピンの設定
  pinMode(ledPin, OUTPUT);    //LED
  pinMode(inputPin1, INPUT);  //Grove32(黄色)
  pinMode(inputPin2, INPUT);  //Grove33(白色)
  pinMode(buttonPinA, INPUT); //Aボタン
  pinMode(buttonPinB, INPUT); //Bボタン

  /*
    pinMode(Hat0, INPUT); //0
    pinMode(Hat26, INPUT); //26
    pinMode(Hat36, INPUT); //36(入力専用)
  */

  //起動画面、メイン画面領域
  tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK); //起動画面背景色
  TitleBar = "Boot Menu";
  Title_Bar();

  tftSprite.setCursor(5, 30);
  tftSprite.setTextFont(4);
  tftSprite.setTextColor(TFT_GREEN);
  tftSprite.println("Please Wait..."); //起動中
  tftSprite.pushSprite(0, 0);

  tftSprite.setCursor(5, 50);
  tftSprite.setTextFont(1);
  tftSprite.println(" Press 3s the M5button to   start in offline mode."); //起動メッセージ
  tftSprite.pushSprite(0, 0);

  delay(3000);

  //再起動後、自動的に動かすため、これ↓に対してWhileは使用しない
  bool A_ButtonRead = (digitalRead(M5_BUTTON_HOME) == LOW) ? true : false;

  tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK); //起動画面背景色

  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  tftSprite.createSprite(160, 80);
  //デバッグ時は true 運用時は false にする
  if (A_ButtonRead == false)
  {

    M5.Lcd.setRotation(1);
    tftSprite.setRotation(1);
    tftSprite.createSprite(160, 80);
    tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK);
    TitleBar = "BootingNow...";
    Title_Bar();
    tftSprite.setTextColor(TFT_YELLOW);
    tftSprite.setCursor(5, 30);
    tftSprite.setTextFont(2);
    tftSprite.println("Wi-Fi Booting..."); //接続中
    tftSprite.pushSprite(0, 0);
    delay(1000);

    WiFi_Init();
    tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK);
    tftSprite.pushSprite(0, 0);
  }
  else
  {

    M5.Lcd.setRotation(1);
    tftSprite.setRotation(1);
    tftSprite.createSprite(160, 80);
    tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK);
    TitleBar = "BootingNow...";
    Title_Bar();
    tftSprite.setTextColor(TFT_GREEN);
    tftSprite.setCursor(5, 30);
    tftSprite.setTextFont(2);
    tftSprite.println("Offline Booting..."); //起動中
    tftSprite.pushSprite(0, 0);
    delay(1000);
    tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK);
    tftSprite.pushSprite(0, 0);
    offlinemode = 1;
  }

  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  // tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK);
  tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
  tftSprite.pushSprite(0, 0);
}

//Wi-Fi接続(初期化)＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void WiFi_Init()
{
  if (offlinemode != 1)
  {
    WiFi.begin(ssid, password);

    if (WiFi.status() != WL_CONNECTED)
    {
      delay(1000);
      //Serial.println("Wi-Fi Connected...");
      WiFiData = 0;
      WiFiStatus = "NG";
    }

    ////Serial.println("");
    //Serial.println("WiFi OK!");
    ////Serial.println("IP address: ");
    ////Serial.println(WiFi.localIP());
    WiFiData = 1;
    WiFiStatus = "OK";
    NetCheck();
  }
}

//タイトルバー＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void Title_Bar()
{
  tftSprite.createSprite(160, 80);
  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  tftSprite.fillRect(0, 0, 160, 10, TFT_BLACK); //
  tftSprite.setCursor(5, 0);
  tftSprite.setTextSize(1);
  tftSprite.setTextFont(1);
  //↑基準。いじらない ↓のみいじる
  tftSprite.setTextColor(TFT_WHITE); //文字は白
  tftSprite.println(TitleBar);       //ウィンドウタイトル
}

//Wi-Fi接続判定＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void Connect_Check()
{
  if (offlinemode != 1)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      //delay(500);
      //Serial.println("WL_CONNECTED = NG");
      WiFiData = 0;
      WiFiStatus = "NG";
      NetData = 0;
      NetStatus = "NG";
    }
    else
    {
      WiFiData = 1;
      WiFiStatus = "OK";
    }
  }
}

//IFTTT接続判定(※IFTTTのアドレスを設定しない事)＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void NetCheck()
{
  if (offlinemode != 1)
  {
    delay(1000);
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host2, httpPort))
    {
      //Serial.println("connection failed");
      NetData = 0;
      NetStatus = "NG";
    }
    else
    {
      NetData = 1;
      NetStatus = "OK";
    }
  }
}

//重複判定、IFTTTへ送信＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void Send_Check(unsigned int event)
{
  M5.update();
  //保持した内容と入力(event)が違う場合にのみIFTTT送信を実行

  if (eram != event)
  {
    eram = event;
    /*
      SendRaw = " IFTTTへ送る文字列(要IFTTT WebHook設定、記号使用不可) ";
      (いずれはCase文にしてもいいかもしれない)
    */
    //eventが0の場合
    if (eram == 0)
    {

      //IFTTTへ「monitoring」と送信(以下同文)
      SendRaw = "monitoring"; //正常監視
    }

    else if (eram == 1)
    {

      SendRaw = "caution"; //注意
    }
    else if (eram == 2)
    {
      SendRaw = "stopped"; //遮断
    }
    else if (eram == 3)
    {

      SendRaw = "error"; //異常
    }
    /* // 4は停電検出で使用中
      else if (eram == 4) {
      SendRaw = "blackout";//停電
      }
    */
    else if (eram == 9)
    {
      tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
      tftSprite.setTextColor(TFT_GREEN);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextFont(4);
      tftSprite.print("Test Now...");
      tftSprite.pushSprite(0, 0);
      delay(1000);
      SendRaw = "test"; //テスト
    }
    else
    {
      //何も記入しない
    }

    //Wi-FiがONか判定して、ONなら送信(OFFならスキップ)
    if (WiFiData != 0)
    {
      WiFi_Send(SendRaw);
      if (eram == 9)
      {
        event = 0;
        eram = 0;
      }
    }
    /*
      //初回変更確認(実用時はコメントアウトする事)
      //Serial.print("EEPROM_Data="); //Serial.println(eram);
      //Serial.print("Event_Data="); //Serial.println(event);
      //Serial.print("SendRaw="); //Serial.println(SendRaw);
    */
  }
  /*
    //状態変化がなければここまでスキップされる
    //2回目の変更確認(実用時はコメントアウトする事)
    //Serial.print("2_EEPROM_Data="); //Serial.println(eram);
    //Serial.print("2_Event_Data="); //Serial.println(event);
    //Serial.print("2_SendRaw2="); //Serial.println(SendRaw);
  */
}

//停電監視＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void BlackoutCheck()
{
  float VinRaw;
  int VinData;
  unsigned int event;

  //AC電源管理
  VinRaw = M5.Axp.GetVusbinData() * 1.7;
  if (4000.0 > VinRaw)
  {
    VinData = 0;
    VinStatus = "NG";
    event2 = 4;
  }
  else if (4000.0 < VinRaw)
  {
    VinData = 1;
    VinStatus = "OK";
    event2 = 0;
  }

  //保持した内容と入力(event2)が違う場合にのみIFTTT送信を実行
  if (eram2 != event2)
  {
    eram2 = event2;
    /*
      SendRaw = " IFTTTへ送る文字列(要IFTTT WebHook設定、記号使用不可) ";
      (いずれはCase文にしてもいいかもしれない)
    */

    //event2が0の場合
    if (eram2 == 0)
    {
      //IFTTTへ「monitoring」と送信(以下同文)
      SendRaw = "monitoring"; //正常監視
    }

    else if (eram2 == 4)
    {
      SendRaw = "blackout"; //停電
    }
    else
    {
      //何も記入しない
    }

    //Wi-FiがONか判定して、ONなら送信(OFFならスキップ)
    if (WiFiData != 0)
    {
      WiFi_Send(SendRaw);
    }
    /*
      //初回変更確認(実用時はコメントアウトする事)
      //Serial.print("EEPROM_Data="); //Serial.println(eram);
      //Serial.print("Event_Data="); //Serial.println(event);
      //Serial.print("SendRaw="); //Serial.println(SendRaw);
    */
  }
  /*
    //状態変化がなければここまでスキップさえれる
    //2回目の変更確認(実用時はコメントアウトする事)
    //Serial.print("2_EEPROM_Data="); //Serial.println(eram);
    //Serial.print("2_Event_Data="); //Serial.println(event);
    //Serial.print("2_SendRaw2="); //Serial.println(SendRaw);
  */
}

//IFTTT送信 (SendRawで受け取る)＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void WiFi_Send(const char *SendRaw)
{
  if (offlinemode != 1)
  {
    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(host, httpPort))
    {
      //delay(1000);
      //Serial.println("connection failed");
      NetData = 0;
      NetStatus = "NG";
    }
    else
    {
      NetData = 1;
      NetStatus = "OK";
      // We now create a URI for the request
      String url = "/trigger/";
      url += SendRaw;
      url += "/with";
      url += "/key/";
      url += privateKey;

      //Serial.print("Requesting URL: ");
      //Serial.println(url);

      // This will send the request to the server
      client.print(String("GET ") + url + " HTTP/1.1\r\n" +
                   "Host: " + host + "\r\n" +
                   "Connection: close\r\n\r\n");
      //Serial.print("Requesting URL: ");
      //Serial.println(url);
      unsigned long timeout = millis();
      while (client.available() == 0)
      {
        if (millis() - timeout > 5000)
        {
          //Serial.println(">>> Client Timeout !");
          client.stop();
          NetStatus = "NG";
          return;
        }
      }
      // Read all the lines of the reply from server and print them to Serial
      while (client.available())
      {
        String line = client.readStringUntil('\r');
        //Serial.print(line);
      }
      //Serial.println();
      //Serial.println("closing connection");
    }
  }
}

//電池試験などの項目(試験実装)＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void PowerTest()
{

  while (1)
  {
    M5.update();
    M5.Lcd.setRotation(1);
    tftSprite.setRotation(1); //本体方向設定 表示項目が多いため、ここだけは縦にする(※最後に必ず先頭の指定方向へ戻す)
    tftSprite.createSprite(160, 80);
    tftSprite.fillSprite(BLACK);
    tftSprite.setCursor(10, 0, 1);
    tftSprite.printf("CPU Temp: %.1fC \r\n", M5.Axp.GetTempInAXP192());
    tftSprite.setCursor(10, 10);
    tftSprite.printf("POW1:\r\n  V: %.3fv  I: %.3fma\r\n", M5.Axp.GetVBusVoltage(), M5.Axp.GetVBusCurrent());
    tftSprite.setCursor(10, 25);
    tftSprite.printf("POW2:\r\n  V: %.3fw  I: %.3fma\r\n", M5.Axp.GetVinVoltage(), M5.Axp.GetVinCurrent());
    tftSprite.setCursor(10, 40);
    tftSprite.printf("Bat:\r\n  V: %.3fv  I: %.3fma\r\n", M5.Axp.GetBatVoltage(), M5.Axp.GetBatCurrent());
    tftSprite.setCursor(10, 55);
    tftSprite.printf("BattW %.3fmw", M5.Axp.GetBatPower());

    tftSprite.setCursor(100, 70);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.println("Done:[M5]");
    tftSprite.pushSprite(0, 0);

    if (M5.BtnA.wasPressed())
    {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.pushSprite(0, 0);
      break;
    }
    delay(500);
  }
}

//ステータスバー＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void StatusBar()
{
  BlackoutCheck();

  /*
    const char* WiFiStatus = "";
    const char* NetStatus = "";
    const char* SendStatus = "";
    const char* VinStatus = "";
  */
  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  tftSprite.createSprite(160, 80);
  tftSprite.fillRect(0, 70, 160, 10, TFT_BLACK);
  tftSprite.setCursor(5, 70);
  tftSprite.setTextSize(1);
  tftSprite.setTextFont(1);

  if (offlinemode != 1)
  {
    //オンラインモード用
    tftSprite.createSprite(160, 80);
    tftSprite.setTextColor(TFT_WHITE); //文字は白
    tftSprite.print("Wi-Fi=");
    tftSprite.print(WiFiStatus);
    tftSprite.print(" Net=");
    tftSprite.print(NetStatus);
    tftSprite.print(" AC=");
    tftSprite.print(VinStatus);
    //tftSprite.print(" Send="); tftSprite.print(SendStatus);
    tftSprite.pushSprite(0, 0);
  }
  else
  {
    //オフラインモード用
    tftSprite.createSprite(160, 80);
    tftSprite.setTextColor(TFT_RED);
    tftSprite.print("OffLine Mode");
    tftSprite.setTextColor(TFT_WHITE); //文字は白
    tftSprite.print(" AC=");
    tftSprite.print(VinStatus);
    //tftSprite.print(" Send="); tftSprite.print(SendStatus);
    tftSprite.pushSprite(0, 0);
  }
  tftSprite.pushSprite(0, 0);
}

void AboutVer()
{
  while (1)
  {
    M5.update();
    M5.Lcd.setRotation(1);
    tftSprite.setRotation(1);
    tftSprite.createSprite(160, 80);
    TitleBar = "About";
    Title_Bar();
    tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
    tftSprite.setCursor(5, 20);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.println("Contact Trigger for IFTTT");
    tftSprite.setCursor(5, 30);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.print("Version:");
    tftSprite.println(FwVersion);
    tftSprite.setCursor(5, 40);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.println("(C) MaitaWorks");

    tftSprite.setCursor(5, 60);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.println("<---[Side]");
    tftSprite.setCursor(100, 60);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.println("[M5]--->");
    tftSprite.pushSprite(0, 0);

    if (M5.BtnA.wasPressed())
    {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.pushSprite(0, 0);
      AboutVer2();
    }
    else if (M5.BtnB.wasPressed())
    {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.pushSprite(0, 0);
      break;
    }
    delay(50);
  }
}

void AboutVer2()
{
  while (1)
  {
    M5.update();
    M5.Lcd.setRotation(1);
    tftSprite.setRotation(1);
    tftSprite.createSprite(160, 80);
    TitleBar = "About";
    Title_Bar();

    //試験実装　振る事でプライベートキーを15秒間表示
    M5.IMU.getAccelData(&accX, &accY, &accZ);
    if (accX > 2.5 || accY > 2.5)
    {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 30, 160, 50, TFT_GREY);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextSize(1);
      tftSprite.setTextFont(1);
      tftSprite.setTextColor(TFT_WHITE);
      tftSprite.printf("APIKey:");
      tftSprite.println(privateKey);
      tftSprite.pushSprite(0, 0);
      delay(15000);
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.pushSprite(0, 0);
    }

    tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
    tftSprite.setCursor(5, 20);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.print("WiFi:");
    tftSprite.println(ssid);
    tftSprite.setCursor(5, 30);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.print("APIKey:");
    tftSprite.println("[[\\\\Shake Me////]]");
    /*
      tftSprite.setCursor(5, 40);
      tftSprite.setTextSize(1);
      tftSprite.setTextFont(1);
      tftSprite.println("");
    */
    tftSprite.setCursor(5, 60);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.println("<---[Side]");
    tftSprite.pushSprite(0, 0);

    /*
      tftSprite.setCursor(100, 60);
      tftSprite.setTextSize(1);
      tftSprite.setTextFont(1);
      tftSprite.setTextColor(TFT_WHITE);
      tftSprite.println("*View*:[M5]");
      tftSprite.pushSprite(0, 0);
    */
    /*
      if (M5.BtnA.wasPressed())
      {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.pushSprite(0, 0);
      break;
      }
    */
    if (M5.BtnB.wasPressed())
    {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.pushSprite(0, 0);
      break;
    }

    delay(50);
  }
}

//M5ボタンで開く画面＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void MenuScreen()
{
  M5.update();
  tftSprite.fillRect(0, 10, 160, 70, TFT_GREY); //メニュー画面背景塗りつぶし
  int itemno = 1;                               //番号選択
  int itemlist = 0;                             //選択内容
  int item_y = 15;                              //カーソル初期位置
  int enterkey = 0;                             //決定ボタン判定
  int StatusText = 0;

  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  tftSprite.createSprite(160, 80);

  while (1)
  {
    M5.update();
    TitleBar = "Main Menu";
    Title_Bar();

    tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
    //カーソルキー
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.drawString(" > ", 0, item_y);

    //メニューリスト
    tftSprite.drawString("Set RTC from NTP", 16, 15);     //リスト１
    tftSprite.drawString("About Version", 16, 25);        //リスト２
    tftSprite.drawString("Power Test", 16, 35);           //リスト３
    tftSprite.drawString("Restart Menu", 16, 45);         //リスト４
    tftSprite.drawString("Return to MainScreen", 16, 55); //リスト５

    if (enterkey != 0)
    { //決定ボタン判定
      if (itemno == 1)
      {
        //List1実行
        M5.update();
        TitleBar = "Set RTC from NTP";
        Title_Bar();
        tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
        tftSprite.pushSprite(0, 0);
        RTC_Set();
      }
      else if (itemno == 2)
      {
        //List2実行
        AboutVer();
      }

      else if (itemno == 3)
      {
        //List3実行
        PowerTest();
      }
      else if (itemno == 4)
      {
        //List2実行
        RestartMenu();
      }
      else if (itemno == 5)
      {
        //List2実行
        TitleBar = "Return...";
        Title_Bar();
        tftSprite.fillRect(0, 10, 160, 70, TFT_GREEN);
        tftSprite.setTextColor(TFT_RED);
        tftSprite.setCursor(5, 40);
        tftSprite.setTextFont(1);
        tftSprite.print("PleaseWait...");
        tftSprite.pushSprite(0, 0);
        delay(1500);
        M5.update();
        tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
        break;
      }
      else
      {
      }
      enterkey = 0;
    }
    //これより↓から制御
    else if (M5.BtnA.wasPressed())
    {
      M5.update();
      enterkey = 1;
    }

    else if (M5.BtnB.wasPressed())
    {
      M5.update();
      itemno += 1;
      //カーソル、項目選択

      if (itemno == 1)
      {
        item_y = 15; //カーソル座標
      }
      else if (itemno == 2)
      {
        item_y = 25;
      }
      else if (itemno == 3)
      {
        item_y = 35;
      }
      else if (itemno == 4)
      {
        item_y = 45;
      }
      else if (itemno == 5)
      {
        item_y = 55;
      }
      else if (itemno >= 6)
      { //項目リセット
        itemno = 1;
        item_y = 15;
      }
      else
      {
      }
    }

    else if (M5.BtnA.wasPressed())
    {
      M5.update();
      enterkey = 1;
    }
    //StatusText = itemno;
    StatusBar();
    delay(50);
    tftSprite.pushSprite(0, 0);
  }
}

/*
   //SubScreenベーステンプレート
  void SubScreen2() {
  M5.update();
  while (M5.BtnA.wasPressed() == 0) {
    M5.update();
    BlackoutCheck();
    tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);//起動画面背景色
    TitleBar = "SubScreen2";
    Title_Bar();
    tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
    tftSprite.setTextColor(TFT_GREEN);
    tftSprite.setCursor(5, 30);
    tftSprite.setTextFont(4);
    tftSprite.print("SubScreen2");

    //メイン画面のステータス表示が必要ならここを外す
    //StatusBar();
    //別途定義する場合はここを外す。
    //ステータスバー(サブ画面用　※BlackoutCheckがループ内に必要)
    tftSprite.fillRect(0, 70, 160, 10, TFT_BLACK);
    tftSprite.setCursor(5, 70);
    tftSprite.setTextSize(1);
    tftSprite.setTextFont(1);
    tftSprite.setTextColor(TFT_WHITE); //文字列の色は白
    tftSprite.print("Event= "); tftSprite.print(event);//ステータスバー文字列
    tftSprite.print("      AC="); tftSprite.print(VinStatus);//ステータスバー文字列
    //ここから動作内容を書く

    RelayCheck();//接点監視(サブ画面内にも必ず入れる事)

    //ボタン操作で抜ける(必須)
    if (M5.BtnA.wasPressed() == 1) {
      M5.update();
      break;
    }
    delay(500);
  }
  //戻る(Bボタンを押すとメイン画面(loop)に移動)
  M5.update();
  tftSprite.fillScreen(TFT_GREY);
  }
*/

//NTP→RTC設定＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void RTC_Set()
{
  TitleBar = "RTC Set";
  Title_Bar();
  Connect_Check();

  M5.Lcd.setRotation(1);
  M5.update();
  //init and get the time

  if (WiFiData == 1)
  {
    M5.Lcd.fillRect(0, 11, 160, 69, TFT_GREY);
    M5.Lcd.setTextColor(TFT_GREEN);
    M5.Lcd.setCursor(5, 30);
    M5.Lcd.setTextFont(2);
    M5.Lcd.print("Setting Now...");
    delay(1500);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    struct tm timeInfo;

    if (getLocalTime(&timeInfo))
    {

      RTC_TimeTypeDef TimeStruct;
      TimeStruct.Hours = timeInfo.tm_hour;
      TimeStruct.Minutes = timeInfo.tm_min;
      TimeStruct.Seconds = timeInfo.tm_sec;
      M5.Rtc.SetTime(&TimeStruct);

      RTC_DateTypeDef DateStruct;
      DateStruct.WeekDay = timeInfo.tm_wday;
      DateStruct.Month = timeInfo.tm_mon + 1;
      DateStruct.Date = timeInfo.tm_mday;
      DateStruct.Year = timeInfo.tm_year + 1900;
      M5.Rtc.SetData(&DateStruct);
      M5.Lcd.fillRect(0, 11, 160, 69, TFT_GREY);
      M5.Lcd.setTextColor(TFT_GREEN);
      M5.Lcd.setCursor(5, 30);
      M5.Lcd.setTextFont(2);
      M5.Lcd.print("Successful!");
      delay(1500);
    }
    else if (!getLocalTime(&timeInfo))
    {
      M5.Lcd.fillRect(0, 11, 160, 69, TFT_GREY);
      M5.Lcd.setTextColor(TFT_RED);
      M5.Lcd.setCursor(5, 30);
      M5.Lcd.setTextFont(2);
      M5.Lcd.print("Failed!!");
      delay(1500);
    }
  }
  else if (WiFiData == 0)
  {
    TitleBar = "Error";
    Title_Bar();

    M5.Lcd.fillRect(0, 11, 160, 69, TFT_YELLOW);
    M5.Lcd.setTextColor(TFT_RED);
    M5.Lcd.setCursor(5, 30);
    M5.Lcd.setTextFont(2);
    M5.Lcd.print("Wi-Fi OFF!!");
    delay(2500);
    M5.Lcd.fillRect(0, 11, 160, 69, TFT_GREY);
  }
}

/*
  void RTC_Set() {
  TitleBar = "RTC Set";
  Title_Bar();

  while (1) {
    if (WiFiData != 0) {
      M5.update();
      Connect_Check();
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    }

    struct tm timeInfo;
    static const char *Week[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};

    M5.Lcd.setRotation(1); tftSprite.setRotation(1);
    tftSprite.createSprite(160, 80);
    tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);

    tftSprite.setCursor(5, 15);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.setTextFont(1);
    //tftSprite.print("NTP:"); tftSprite.printf("%d/%2d/%2d(%s) %02d:%02d:%02d\n", timeInfo.tm_year + 1900, timeInfo.tm_mon + 1, timeInfo.tm_mday, Week[timeInfo.tm_wday], timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);

    M5.Rtc.GetTime(&RTC_TimeStruct);
    M5.Rtc.GetData(&RTC_DateStruct);
    tftSprite.setCursor(5, 25);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.setTextFont(1);
    //tftSprite.print("RTC:"); tftSprite.printf("%d/%2d/%2d(%s) %02d:%02d:%02d\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, Week[RTC_DateStruct.WeekDay], RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes, RTC_TimeStruct.Seconds);

    tftSprite.setCursor(5, 45);
    tftSprite.setTextColor(TFT_WHITE);
    tftSprite.setTextFont(2);

    tftSprite.println("Set?"); tftSprite.println("Yes:[M5] No:[Side]");


    if (M5.BtnA.wasPressed()) {
      M5.update();

      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
      tftSprite.setTextColor(TFT_GREEN);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextFont(2);
      tftSprite.print("Setting Now...");
      tftSprite.pushSprite(0, 0);
      delay(1500);

      if (getLocalTime(&timeInfo)) {

        RTC_TimeTypeDef TimeStruct;
        TimeStruct.Hours   = timeInfo.tm_hour;
        TimeStruct.Minutes = timeInfo.tm_min;
        TimeStruct.Seconds = timeInfo.tm_sec;
        M5.Rtc.SetTime(&TimeStruct);

        RTC_DateTypeDef DateStruct;
        DateStruct.WeekDay = timeInfo.tm_wday;
        DateStruct.Month = timeInfo.tm_mon + 1;
        DateStruct.Date = timeInfo.tm_mday;
        DateStruct.Year = timeInfo.tm_year + 1900;
        M5.Rtc.SetData(&DateStruct);
        tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
        tftSprite.setTextColor(TFT_GREEN);
        tftSprite.setCursor(5, 30);
        tftSprite.setTextFont(2);
        tftSprite.print("Successful!");
        tftSprite.pushSprite(0, 0);
        delay(1500);
        break;
      }
      else if (!getLocalTime(&timeInfo)) {
        tftSprite.createSprite(160, 80);
        tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
        tftSprite.setTextColor(TFT_RED);
        tftSprite.setCursor(5, 30);
        tftSprite.setTextFont(2);
        tftSprite.print("Failed!!");
        tftSprite.pushSprite(0, 0);
        delay(1500);
        break;
      }
    }
    else if (M5.BtnB.wasPressed()) {
      M5.update();
      break;
    }
    else {
    }
    tftSprite.pushSprite(0, 0);
  }
  else {
    TitleBar = "Error";
    Title_Bar();
    tftSprite.createSprite(160, 80);
    tftSprite.fillRect(0, 11, 160, 69, TFT_YELLOW);
    tftSprite.pushSprite(0, 0);
    tftSprite.setTextColor(TFT_RED);
    tftSprite.setCursor(5, 30);
    tftSprite.setTextFont(2);
    tftSprite.print("Wi-Fi OFF!!");
    tftSprite.pushSprite(0, 0);
    delay(2500);
    tftSprite.fillRect(0, 10, 160, 70, TFT_GREY);
    break;
  }
  }
  }
*/

//再起動画面＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void RestartMenu()
{
  M5.update();
  tftSprite.createSprite(160, 80);
  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);

  //ボタン選択while
  while (M5.BtnA.wasPressed() == 0)
  {
    M5.update();
    tftSprite.fillRect(0, 11, 160, 69, TFT_YELLOW); //背景
    TitleBar = "Restart Menu";
    Title_Bar();
    BlackoutCheck();
    RelayCheck();

    /*
      //ステータスバー
      tftSprite.fillRect(0, 70, 160, 10, TFT_BLACK);
      tftSprite.setCursor(5, 70);
      tftSprite.setTextSize(1);
      tftSprite.setTextFont(1);
      tftSprite.setTextColor(TFT_WHITE); //文字列の色は白
      tftSprite.print("Event= "); tftSprite.print(event);//ステータスバー文字列
      tftSprite.print("      AC="); tftSprite.print(VinStatus);//ステータスバー文字列
    */

    tftSprite.setTextColor(TFT_RED);
    tftSprite.setCursor(20, 25);
    tftSprite.setTextFont(4);
    tftSprite.println("Restart?");
    tftSprite.setTextColor(TFT_RED);
    tftSprite.setCursor(10, 45);
    tftSprite.setTextFont(2);
    tftSprite.println("No = [UP Side] Button");
    tftSprite.setCursor(10, 55);
    tftSprite.setTextFont(2);
    tftSprite.println("Yes = [M5]Button");

    if (M5.BtnA.wasPressed())
    {
      M5.update();
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 0, 160, 80, TFT_BLACK);
      tftSprite.setTextColor(TFT_WHITE);
      tftSprite.setCursor(10, 30);
      tftSprite.setTextFont(2);
      tftSprite.println("Restarting...");
      tftSprite.pushSprite(0, 0);
      delay(2000);
      esp_restart();
    }

    else if (M5.BtnB.wasPressed())
    {
      M5.update();
      break; //ボタン選択whileから抜けてメイン画面に戻る
    }
    delay(50);
    tftSprite.pushSprite(0, 0);
  }

  M5.update();
  M5.Lcd.fillScreen(TFT_GREY);
  tftSprite.pushSprite(0, 0);
}

//接点入力・ボタン入力(接点入力と送信のみ)＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void RelayCheck()
{
  //接点入力・ボタン入力(予備)設定
  int inputStateA, inputStateB;
  inputStateA = digitalRead(inputPin1);
  inputStateB = digitalRead(inputPin2);
  //int inputButtonA, inputButtonB;
  //inputButtonA = digitalRead(buttonPinA); inputButtonB = digitalRead(buttonPinB);

  //接点判定ここから
  if (inputStateA == 0)
  {

    if (inputStateB == 0)
    {
      event = 0; //監視状態
      digitalWrite(ledPin, HIGH);
    }
    if (inputStateB == 1)
    {
      event = 3; //異常(遮断のみ)
      digitalWrite(ledPin, HIGH);
      delay(500);
      digitalWrite(ledPin, LOW);
      delay(500);
    }
  }

  if (inputStateA == 1)
  {

    if (inputStateB == 1)
    {
      event = 2; //注意＆遮断
      delay(1000);
    }
    if (inputStateB == 0)
    {
      event = 1; //注意
      digitalWrite(ledPin, HIGH);
      delay(1000);
      digitalWrite(ledPin, LOW);
      delay(1000);
    }
  }
}

//接点入力・ボタン入力(画面表示込み)＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void RelayCheckPrint()
{
  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  tftSprite.createSprite(160, 80);

  //接点入力・ボタン入力(予備)設定
  int inputStateA, inputStateB;
  inputStateA = digitalRead(inputPin1);
  inputStateB = digitalRead(inputPin2);
  //int inputButtonA, inputButtonB;
  //inputButtonA = digitalRead(buttonPinA); inputButtonB = digitalRead(buttonPinB);

  //接点判定ここから
  if (inputStateA == 0)
  {

    //監視状態
    if (inputStateB == 0)
    {
      digitalWrite(ledPin, HIGH);
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
      tftSprite.setTextColor(TFT_GREEN);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextFont(4);
      tftSprite.print("Monitoring...");
      tftSprite.pushSprite(0, 0);
      event = 0;
    }

    //異常(遮断のみ)
    if (inputStateB == 1)
    {
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
      tftSprite.setTextColor(TFT_RED);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextFont(4);
      tftSprite.print("Error!");
      tftSprite.pushSprite(0, 0);
      event = 3;
      digitalWrite(ledPin, HIGH);
      delay(500);
      digitalWrite(ledPin, LOW);
      delay(500);
    }
  }

  if (inputStateA == 1)
  {

    //注意＆遮断
    if (inputStateB == 1)
    {
      digitalWrite(ledPin, LOW);
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
      tftSprite.setTextColor(TFT_ORANGE);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextFont(4);
      tftSprite.print("Stopped!");
      tftSprite.pushSprite(0, 0);
      event = 2;
      delay(1000);
    }

    //注意
    if (inputStateB == 0)
    {
      tftSprite.createSprite(160, 80);
      tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
      tftSprite.setTextColor(TFT_YELLOW);
      tftSprite.setCursor(5, 30);
      tftSprite.setTextFont(4);
      tftSprite.print("Caution!");
      tftSprite.pushSprite(0, 0);
      event = 1;
      digitalWrite(ledPin, HIGH);
      delay(1000);
      digitalWrite(ledPin, LOW);
      delay(1000);
    }
  }
  else if (WiFiStatus == 0)
  {
    tftSprite.createSprite(160, 80);
    tftSprite.fillRect(0, 30, 160, 25, TFT_GREY);
    tftSprite.setTextColor(TFT_RED);
    tftSprite.setCursor(5, 30);
    tftSprite.setTextFont(4);
    tftSprite.print("Net Error!");
    tftSprite.pushSprite(0, 0);
    event = 3;
    digitalWrite(ledPin, LOW);
  }
  tftSprite.pushSprite(0, 0);
}

void RTCView()
{
  //RTCから日付時刻を取得してタイトルバーの下に表示
  static const char *Week[7] = {"Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat"};
  M5.Lcd.setRotation(1);
  tftSprite.setRotation(1);
  tftSprite.createSprite(160, 80);
  tftSprite.fillRect(0, 15, 160, 15, TFT_GREY);
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);
  tftSprite.setCursor(5, 15);
  tftSprite.setTextColor(TFT_WHITE);
  tftSprite.setTextFont(1);
  tftSprite.printf("%d/%2d/%2d(%s) %02d:%02d:%02d\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, Week[RTC_DateStruct.WeekDay], RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes, RTC_TimeStruct.Seconds);
  tftSprite.pushSprite(0, 0);

  //自動でNTPと時刻合わせするときに使う(正時にセットすると輻輳の原因になるため正時からずらして使う。実行しない場合はコメントアウトが望ましい)
  if (RTC_TimeStruct.Hours == 8) //時
  {
    if (RTC_TimeStruct.Minutes == 30) //分
    {
      if (RTC_TimeStruct.Seconds == 0) //秒
      {
        RTC_Set();
      }
    }
  }
}

//メインプログラム＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
void loop()
{

  while (!setCpuFrequencyMhz(240))
  { //CPU速度設定 10,20,40,80,160,240から選択 (通常は240MHz)
    ;
  }

  TitleBar = "Contact Trigger for IFTTT";
  Title_Bar(); //タイトルバー(上とセット)
  StatusBar(); //ステータスバー
  M5.update(); //必要
  RTCView();
  Connect_Check();

  //接点監視(サブ画面内にも必ず入れる事)
  RelayCheckPrint();

  //Aボタンを押す事でサブ画面2に移動
  if (M5.BtnA.wasPressed())
  {
    M5.update();
    //SelfTest();
    Send_Check(9); //ボタンテスト
  }

  //Bボタンを押す事でサブ画面に移動
  if (M5.BtnB.wasPressed())
  {
    M5.update();
    //SelfTest();
    MenuScreen();
  }

  //状態を確認して重複でなければ送信
  Send_Check(event);

  //ここで終わり。これより下には何も書かない事＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊＊
}
//[EOF]