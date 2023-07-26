// loadcell_adc_mqtt
// M5stackでHX711ロードセルを使用する時の動作確認用のプログラムです
// loadcellの生の出力値をmqttで送信します。
// ワークショップ用のサンプルプログラムです
// Cボタン：オフセット
// ロードセルのサンプルプログラム https://akizukidenshi.com/catalog/g/gK-12370/


#include <Arduino.h>
#include <M5Stack.h>
#include <WiFi.h>
#include <PubSubClient.h>


// 回帰分析で取得した説明変数をこちらに記入します
float ExplanatoryVariable = 0.0026;    // 傾き 


// 通信用の設定
//接続するアクセスポイントの情報
const char *ssid = "your_ssid";
const char *password = "your_pass";
// Pub/Subの設定
const char* mqttHost = "your_server";         //ipアドレスかドメインで指定する
const int mqttPort = 1883;                    //通常は1883か8883
// 送信トピック
const char* topic = "your_topic";             // 送信するトピック名（変更）


// ピンの設定
// 好ましくはないが実験的に使用
#define DATA_PIN  21                         // データ出力ピン
#define CLK_PIN   22                         // クロック入力ピン
// ボタン設定
#define BtnA 39
#define BtnB 38
#define BtnC 37

// 通信の間隔
#define PUBLISH_INTERVAL 5                  // 送信間隔を秒で指定


// 割り込みフラグ
volatile bool timerflg = 0;
// 画面の状態
volatile int display_st = 99;              // 最初は必ず実行されるように
volatile float old_value = 0;              // 画面を変更させるために過去のデータを持たせる
// 通信が確立しているかどうかのフラグ
volatile bool communication_connect_flg = false;


// 重量計の動作
void AE_HX711_Init(void);                 // ロードセルアンプの初期化
void AE_HX711_Reset(void);                // ロードセルアンプのリセット
long AE_HX711_Read(void);                 // ロードセルアンプの値を読む
long AE_HX711_Averaging(char num);        // ばらつきを抑えるため平均をとる
int convetToWeight(long AE_HX711_Value);  // ADCの出力値を重量に変換

// ボタンが押されたときの動作
void buttonInit();
void buttonLoop();                          // メインルーチンに置いておく必要があります
void pushAbutton();
void pushBbutton();
void pushCbutton();

// 画面の設定
void initScreen();                            // 画面の初期化
void startupScreen();                         // 立ち上がり画面の作成
void createMeasureScreen();                   // センサーの表示画面の作成
void indicateMeasureValue(int *measureValue); // 測定した値の表示

// 通信に関すること
void connectWiFi();                           // Wi-Fiに接続
void connectMqtt();                           // ブローカーサーバーに接続
void MqttPublish(const char *payload);        // mqttを送信

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);


long offset_raw;     // オフセット（ADCの生値）


void setup() {
  
  Serial.begin(115200);
  Serial.println("system start");

  // ボタンの初期化
  buttonInit();
  
  // 立ち上がり画面の作成
  initScreen();
  startupScreen();
  
  // 重量計の初期化
  AE_HX711_Init();
  M5.Lcd.print("\nsensor init done\n\n connecting wi-fi");
  
  // 通信の開始
  connectWiFi();    //Ｗｉ-Ｆｉの接続確認 
  connectMqtt();    //mQTTサーバーへの接続確認
  M5.Lcd.print("\nwi-fi connect done!\nGet offset");
  
  // オフセットの取得
  offset_raw = AE_HX711_Averaging(30);    // 多少時間がかかる
  Serial.printf("offset raw value : %ld\n\n", offset_raw);
}

void loop() { 
  
  buttonLoop();

  // タイマーの作成
  static long old_mills = 0;
  static int pub_counter = 0;

  // 重量の取得
  long raw_value = AE_HX711_Averaging(5);
  long true_raw_value = raw_value - offset_raw;
  // Serial.printf("measure value : %d\n", true_raw_value);  // 確認用の表示
  
  // 重量に変換 
  int weight = convetToWeight(true_raw_value);
  // Serial.printf("weight : %d g\n", weight);                   // 確認用の表示

  // 画面表示について
  if ( display_st != 1){
    
    // 測定値の表示モードになっていないときは表示画面を作成する
    display_st = 1;
    Serial.println("create screen");
    createMeasureScreen();
  }

  // 画面の更新の頻度を考える(0.5秒に一回)
  if(old_mills == 0){
    old_mills = millis();
  }

  if (millis() - old_mills  >= 500){
    old_mills = millis();
    pub_counter += 1;     // 送信用のカンターのカウントアップ
    // Serial.println("time slapsed"); // 確認用の表示
    indicateMeasureValue(&weight);
  }
  
  // 指定した間隔でデータを送信
  if (pub_counter >= PUBLISH_INTERVAL * 2){
    pub_counter = 0;
    char payload[256];
    sprintf(payload, "{\"value\": %ld}", true_raw_value);
    MqttPublish(payload);    //mqtを送信する
  }
}

  
// 重量計に関すること------------------------------
// 重量計の初期化
void AE_HX711_Init(void)
{
  pinMode(CLK_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT);
  AE_HX711_Reset();
}

// 重量計のリセット
void AE_HX711_Reset(void)
{
  digitalWrite(CLK_PIN,1);
  delayMicroseconds(100);
  digitalWrite(CLK_PIN,0);
  delayMicroseconds(100); 
}

// 重量計の値を読み取る
long AE_HX711_Read(void){

  long data=0;
  while(digitalRead(DATA_PIN)!=0);
  delayMicroseconds(10);
  for(int i=0;i<24;i++)
  {
    digitalWrite(CLK_PIN,1);
    delayMicroseconds(5);
    digitalWrite(CLK_PIN,0);
    delayMicroseconds(5);
    data = (data<<1)|(digitalRead(DATA_PIN));
  }
  //Serial.println(data,HEX);   
  digitalWrite(CLK_PIN,1);
  delayMicroseconds(10);
  digitalWrite(CLK_PIN,0);
  delayMicroseconds(10);
  return data^0x800000; 
}

// ばらつきを抑えるため平均をとる
long AE_HX711_Averaging(char num){
  long sum = 0;
  for (int i = 0; i < num; i++) sum += AE_HX711_Read();
  return sum / num;
}

// 重量計の値を重量に変換
int convetToWeight(long AE_HX711_Value){
  float weight; 
  weight = AE_HX711_Value * ExplanatoryVariable;
  // Serial.printf("relative weigth : %f\n", weight); // 確認用の表示
  return (int)weight; // 小数点以下は切り捨て
}


// 画面表示に関すること---------------------------------------------------
// 画面の初期化
void initScreen(){

  M5.Lcd.begin();  //Initialize M5Stack
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(5,30);
  M5.Lcd.setTextSize(2);
}

// 立ち上がり画面の作成
void startupScreen(){

  M5.Lcd.print("smart gravimeter\n");
  M5.Lcd.setTextColor(RED);
  M5.Lcd.print("\n");
  M5.Lcd.print("system start");
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.print("\n");
  M5.Lcd.print("\n\nsensor init\n");

}

// センサーの表示画面の作成
void createMeasureScreen(){
  
  M5.Lcd.clear();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.drawString("Coffee Weighing Scale", 1, 1, 2);
  M5.Lcd.drawString("g", 280, 135, 2);
  // 通信状態の表示（未実装）
  // M5.Lcd.setTextSize(1);
  // M5.Lcd.drawString("signal", 195, 190, 4);
  // M5.Lcd.drawFastHLine(0, 60, 320, WHITE);
  // M5.Lcd.drawFastHLine(0, 185, 320, WHITE);
  // // 〇の表示
  // M5.Lcd.fillCircle(295, 213, 18, GREEN);
}

// 測定した値の表示
void indicateMeasureValue(int *measureValue){

  // 値を消去
  M5.Lcd.fillRect(10, 65, 250, 120, BLACK);   //表示部分のクリア  

  // 値を表示
  String buf = ""; 
  buf = String(*measureValue, DEC);   // 整数(10進数)を文字列に変換
  M5.Lcd.drawString(buf, 70, 90, 6);
  M5.Lcd.drawFastHLine(0, 60, 320, WHITE);
  M5.Lcd.drawFastHLine(0, 185, 320, WHITE);
}


// ボタンの動作----------------------------------------------------
// ボタン初期化
void buttonInit(){

  pinMode(BtnA, INPUT);
  pinMode(BtnB, INPUT);
  pinMode(BtnC, INPUT);
}

// ボタンが押されたときの処理
void buttonLoop(){
  
  if(digitalRead(BtnA) == LOW){
    delay(500);   // チャタリング防止
    Serial.println("button A pushed");
    pushAbutton();
  }else if(digitalRead(BtnB) == LOW){
    delay(500);   // チャタリング防止
    Serial.println("button B pushed");
    pushBbutton();
  }else if(digitalRead(BtnC) == LOW){
    delay(500);   // チャタリング防止
    Serial.println("button C pushed");
    pushCbutton();
  }
}

void pushAbutton() {
  Serial.println("button A pshed");
  // Aボタンに割り当てたい処理を書きます
}

void pushBbutton() {
  Serial.println("button B pshed");
  // Bボタンに割り当てたい処理を書きます
}

void pushCbutton() {
  
  Serial.println("button C pshed");
  Serial.println("get offset");
  
  // Cボタンに割り当てたい処理
  // オフセットを取得する
  offset_raw = AE_HX711_Averaging(10);
  Serial.printf("offset raw value : %ld\n\n", offset_raw);
}


//通信関係の処理--------------------------------------------
//Wi-Fiを接続する
void connectWiFi(){

  //Wi-Fiのアクセスポイントに接続
  //第一引数：ssid
  //第二引数：パスワード      
  WiFi.begin(ssid, password);
  Serial.print("WiFi connecting...");

  int i = 0 ;   //接続確認の時間
  //Wi-Fiの接続確認
  while(WiFi.status() != WL_CONNECTED) {
    i += 1;
    Serial.print(".");
    delay(1000);
    // 5秒間Wi-Fiが接続できないときは接続をやり直す
    if (i == 5){
      Serial.println("WiFi reset");
      connectWiFi();
    }
  }
  
  //Wi-Fiの接続が確認出来たらコンソールに表示して確認する      
  Serial.print(" connected. ");
  Serial.println(WiFi.localIP());
}

//MQTTの接続
void connectMqtt(){
  
  //Wi-Fiの接続確認
  while(WiFi.status() != WL_CONNECTED) {
    Serial.print("WiFi is not connect");
    connectWiFi();
  }
  
  //brokerサーバーに接続する
  //第一引数：ブローカーサーバーのドメインもしくはipアドレス
  //第二引数：接続するポート（通常は1883か8883）
  mqttClient.setServer(mqttHost, mqttPort);

  //clientIDを作成してサーバーに接続する
  while( ! mqttClient.connected() ) {

    Serial.println("Connecting to MQTT...");
    
    //MacアドレスからクライアントIDを作成する
    String clientId = "ESP32-" +  getMacAddr();
    //確認用の表示
    Serial.print("clientID : "); 
    Serial.println(clientId.c_str()); 
    
    //接続の確認
    //if ( mqttClient.connect(clientId.c_str(),mqtt_username, mqtt_password) ) {  //ユーザー認証を行う時はこちらを利用する
    if ( mqttClient.connect(clientId.c_str())) { 
      
      Serial.println("connected"); 
    }
  }
}

//Mqttの送信
void MqttPublish(const char *payload){

  //mqttの接続を確認
  while( ! mqttClient.connected() ) {
    Serial.println("Mqtt is not connect");
    connectMqtt();
  }
 
  //mqttの送信
  //第一引数：トピック名
  //第二引数：送信するデータ
  mqttClient.publish(topic, payload);
  //コンソールに送信するデータを表示して確認
  Serial.print("published ");
  Serial.println(payload);
}

// Macアドレスを文字列で取得する mqttのクライアントIDに利用する
String getMacAddr(){

    byte mac[6];
    char buf[50];
    WiFi.macAddress(mac);
    sprintf(buf, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}
