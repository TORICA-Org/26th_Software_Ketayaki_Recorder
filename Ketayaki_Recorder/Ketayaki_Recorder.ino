// ============ Picoのピン設定============

// SPI通信用
#define TFT_TOUCH_SD_MOSI 19 // MOSI(SDI) 送信(TX) 共通
#define TOUCH_SD_MISO 16     // MISO(SDO) 受信(RX) 共通
#define TFT_TOUCH_SD_SCK 18  // SCK クロック
#define TFT_CS 22            // TFT液晶のチップセレクト
#define TOUCH_CS 17          // タッチスクリーンのチップセレクト
#define SD_CS 27             // SDカードスロットのチップセレクト

// TFT液晶用
#define TFT_RST 21 // Reset 
#define TFT_DC 20  // D/C

#define sound 28 // 他励式ブザー用

#include "thermistor.h"
#include "tft.h"

const int Pin_thermistor = 26; // サーミスタ用

//各種計算用変数
unsigned long time_start = 0;   //リフロー開始時刻
unsigned long time_now = 0;   //autoリフロー中の経過時間・残り時間計算用(ディスプレイ表示用)
unsigned long time_start_to_now = 0;   //autoリフロー経過時間(ディスプレイ表示用)
unsigned long time_remaining = 0;   //autoリフロー残り時間(ディスプレイ表示用)
unsigned long time_now_loop = 0;  //(SD保存・グラフプロット用)
unsigned long time_start_to_now_loop = 0;   //(SD保存・グラフプロット用)
double SD_time_loop;    //SD保存用[sec]

int triangle_x;   //リフローの進行状況を示す移動する三角形のx座標
int l = 0;    //リフロー開始時特有の音を鳴らすため，page3とpage7を何回loopしたかカウント
double data; //グラフプロット時に使用
int plot_X;
int plot_Y;
int Refresh_SD;

//bool
bool check_reflow_start = false;  //リフローが始まっているか
bool find_SD = false;   //SDカードが認識されているか
bool open_SD = false;  //SDカードの書き込みが終わっているか確認
bool SSR_ON = false;   // ランプ点灯状態格納用


//ディスプレイのタッチ誤認識でautoリフローが止まらないように段階を設ける(「私はロボットではありません」に近いことをしたい)
//未実装
int no1 = 0; //Emargencyの計算用
int no2 = 0;
int no3 = 0;
int no_calc = 0;

//SD保存用の配列
double plot[900];
double SD_time[900];
int num_loop = 0; //core0のloopが回った回数を数える

enum RECODE_STATUS {
  STANDBY,
  RECORDING,
  PAUSE
};
RECORD_STATUS record_status = STANDBY;

/****************** SDカード書き込み **************************/
void writeSdData(double SD_time[], double plot[]) {
  myFile = SD.open(SD_FILENAME, FILE_WRITE); // SDカードのファイルを開く
  
  // データ書き込み
  if (myFile) { // ファイルが開けたら
    open_SD = true;
    for(int d = 0; d <= num_loop; d++)
      myFile.printf("%f,%f\n", SD_time[d], plot[d]); // テキストと整数データを書き込み
    myFile.close(); // ファイルを閉じる
    open_SD = false;
  } else { // ファイルが開けなければ
    Serial.println("ファイルを開けませんでした");
    
  }
}

/******************* SD再認識のため ********************/
bool reinitializeSD() {
  SPI.end();               // SPIを完全に停止
  delay(100);
  SPI.begin();             // 再開
  delay(100);
  return SD.begin(SD_CS);
}







void setup(){
  analogReadResolution(12);
  Serial.begin(115200);
  pinMode(Pin_thermistor, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

unsigned long paused_time = 0;

void loop(){
  update_smoothed_celsius(Pin_thermistor);

  //PWM_OUT_V = get_PWM_OUT_V(smoothed_celsius);

  //Serial.print("直温度:");
  //Serial.println(celsius);
  //Serial.print(", ");
  //Serial.print("平滑温度：");
  //Serial.print(smoothed_celsius); // 平滑化された温度の出力
  //Serial.print("℃, ");
  //Serial.print("PWM出力電圧:");
  //Serial.print(PWM_OUT_V);
  //Serial.println("V");


  //PID制御
  /*
  diff_temp_before = target_temp - celsius; //現在直温度と目標温度の差

  diff_temp = target_temp - smoothed_celsius;
  I_diff_temp = I_diff_temp + (diff_temp + diff_temp_before)*T/2;
  D_diff_temp = (diff_temp - diff_temp_before)/T;
  u = P*diff_temp + I*I_diff_temp + D*D_diff_temp;
  diff_temp_before = diff_temp;
  Serial.print(diff_temp);
  Serial.print(", ");
  Serial.print(diff_temp_before);
  Serial.print(", ");
  Serial.print("PID:");
  Serial.println(u);
  */


  //PIDからPWM
  /*
  u = constrain(u, -15, 50);
  if()  
  

  */

  /********************グラフプロット & SD保存用********************/
  if(check_reflow_start == true){
    time_now_loop = millis();
    time_start_to_now_loop = time_now_loop - time_start;
    SD_time_loop = time_start_to_now_loop / 1000;
    SD_time[num_loop] = SD_time_loop;
    plot[num_loop] = smoothed_celsius;
    num_loop++;
  }
  
  //Serial.print(time_start_to_now_loop);
  //Serial.print(", ");
  //Serial.print(SD_time_loop);

  delay(1000);
}




void setup1(){
  Serial.begin(115200); 
  pinMode(LED_BUILTIN, OUTPUT);

  //SPI0
  SPI.setTX(TFT_TOUCH_SD_MOSI);
  SPI.setSCK(TFT_TOUCH_SD_SCK);
  SPI.setRX(TOUCH_SD_MISO);

  //グラフィック設定
  tft.begin();
  tft.setRotation(1);                         //画面回転（0~3）
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  canvas1.fillScreen(ILI9341_BLACK);
  tft.setTextSize(1);                      //デフォテキストサイズ

  //タッチパネル設定
  ts.begin();                   // タッチパネル初期化
  ts.setRotation(3); // タッチパネルの回転(画面回転が3ならここは1)

  // SDカードの初期化
  if (!(find_SD = SD.begin(SD_CS))) {
    Serial.println("SDカードの初期化に失敗しました");
    drawText(50, 50, "SD card not found!", &FreeSans12pt7b, ILI9341_RED);
  } else {
    Serial.println("SDカードが初期化されました");
  }
}

void loop1(){
  if (ts.touched() == true) {  // タッチされていれば
    digitalWrite(LED_BUILTIN, HIGH);
    switch(touch_status){
      case RELEASE:
      {
        TS_Point tPoint = ts.getPoint();  // タッチ座標を取得
        touch_status = TOUCH_START;
        touch_x = (tPoint.x-400) * TFT_WIDTH / (4095-550);  // タッチx座標をTFT画面の座標に換算
        touch_y = (tPoint.y-230) * TFT_HEIGHT / (4095-420); // タッチy座標をTFT画面の座標に換算
      }
        break;
      case TOUCH_START:
        touch_status = TOUCHING;
        break;
      case TOUCHING:
        break;
    }
  }
  else {
    digitalWrite(LED_BUILTIN, LOW);
    touch_status = RELEASE;
  }
  //Serial.print("TOUCH_STATUS: ");
  //Serial.println(touch_status);

  switch(page){
    case WELCOME:
      check_reflow_start = false;
      SSR_ON = false;
      welcome();
      break;
    case MENU:
      digitalWrite(LED_BUILTIN, LOW);
      menu();
      break;
    case START_CONFIRM:
      start_confirm();
      break;
    case RECORDING:
      recording();
      break;
    case GRAPH:
      graph();
      break;
    /*
    case 3:
      for(; l < 1; l++){
        tone(sound,3000,700);
        time_start = millis();
        num_loop = 0;
        check_reflow_start = true;
      }
      page_3(smoothed_celsius);
      break;
    case 4:
      page_4();
      break;
    case 5:
      page_5();
      break;
    case 6:
      page_6(smoothed_celsius);
      break;
    case 7:
      for(; l < 1; l++){
        tone(sound,3000,700);
        time_start = millis();
        num_loop = 0;
        check_reflow_start = true;
      }
      page_7(smoothed_celsius);
      break;
    case 8:
      page_2(smoothed_celsius);
      break;
    case 9:
      page_2(smoothed_celsius);
      break;
    case 10:
      page_2(smoothed_celsius);
      break;
    case 11:
      page_11();
      break;
    case 12:
      page_12();
      break;
    case 13:
      page_13();
      break;
    */
    default:
      Serial.print("Page number error!!");
      page = MENU;
      break;
  }
  delay(200);
}
