// loadcell_codevalue_check
// loadcellの値から重量に変換するためのテスト用プログラムです
// ADコンバータのコード値をそのまま出力します
// ワークショップ用のサンプルプログラムです
// ディスプレイには表示されずにシリアルコンソールにだけ表示されます
// Cボタン：オフセット
// ロードセルのサンプルプログラム　https://akizukidenshi.com/catalog/g/gK-12370/

#include <Arduino.h>

// 回帰分析で取得した説明変数をこちらに記入します
// float ExplanatoryVariable = 0.0026;    // 傾き 

// ピンの設定
// 好ましくはないが実験的に使用
#define DATA_PIN  21          // データ出力ピン
#define CLK_PIN   22          // クロック入力ピン

// ボタン設定
#define BtnC 37

// 重量計の動作
void AE_HX711_Init(void);                 // ロードセルアンプの初期化
void AE_HX711_Reset(void);                // ロードセルアンプのリセット
long AE_HX711_Read(void);                 // ロードセルアンプの値を読む
long AE_HX711_Averaging(char num);        // ばらつきを抑えるため平均をとる
int convetToWeight(long AE_HX711_Value);  // ADCの出力値を重量に変換

long offset_raw;     // オフセット（ADCの生値）


void setup() {
  
  Serial.begin(115200);
  Serial.println("system start");

  pinMode(BtnC, INPUT);   // ボタンのセット
  
  AE_HX711_Init();        // 重量計の初期化
  
  // オフセットの取得
  offset_raw = AE_HX711_Averaging(30);    // 多少時間がかかる
  Serial.printf("offset raw value : %ld\n\n", offset_raw);  // kaku
}

void loop() { 
  
  // 重量の取得
  long raw_value = AE_HX711_Averaging(5);
  long true_raw_value = raw_value - offset_raw;
  Serial.printf("code_value : %ld\n", true_raw_value);
  
  delay(1000);

  // // 重量に変換 
  // int weight = convetToWeight(true_raw_value);
  // Serial.printf("weight : %d g\n", weight);                   // 確認用の表示

  if(digitalRead(BtnC) == LOW){

    delay(500);   // チャタリング防止
    Serial.println("button C pushed");
    offset_raw = AE_HX711_Averaging(5);
    Serial.printf("offset raw value : %ld\n\n", offset_raw);
  }
}

  
// 重量計の初期化
void AE_HX711_Init(void){

  pinMode(CLK_PIN, OUTPUT);
  pinMode(DATA_PIN, INPUT);
  AE_HX711_Reset();
}

// 重量計のリセット
void AE_HX711_Reset(void){

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

