#include <SPI.h>
#include <SD.h>
#include "parameters.h"
#include "thermistor.h"
#include "tft.h"

/****************** SDカード書き込み **************************/
void writeSdData(float SD_time[], float plot[]) {
  myFile = SD.open(SD_FILENAME, FILE_WRITE); // SDカードのファイルを開く
  
  // データ書き込み
  if (myFile) { // ファイルが開けたら
    open_SD = true;
    for(int d = 0; d <= num_loop; d++)
      myFile.printf("%f,%f\n", backup[d], plot[d]); // テキストと整数データを書き込み
    myFile.close(); // ファイルを閉じる
    open_SD = false;
  } else { // ファイルが開けなければ
    Serial.println("ファイルを開けませんでした");
    
  }
}

/******************* SD再認識のため ********************/
bool reinitializeSD() {
  SPI.end();               // SPIを完全に停止

  //pinMode(TFT_TOUCH_SD_MOSI, OUTPUT);
  //digitalWrite(TFT_TOUCH_SD_MOSI, LOW);
  //pinMode(TOUCH_SD_MISO, OUTPUT);
  //digitalWrite(TOUCH_SD_MISO, LOW);
  //pinMode(TFT_TOUCH_SD_SCK, OUTPUT);
  //digitalWrite(TFT_TOUCH_SD_SCK, LOW);
  //pinMode(SD_CS, OUTPUT);
  //digitalWrite(SD_CS, LOW);
  
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

void loop(){
  update_smoothed_celsius(Pin_thermistor);

  /********************グラフプロット & SD保存用********************/
  static unsigned long last_time_stamp_ms = 0;
  static int backup_index = 0;

  if(is_recording == true){
    unsigned long now_ms = millis();
    if (now_ms - last_time_stamp_ms >= 1000) {
      last_time_stamp_ms = now_ms;
      time_duration_s = (now_ms - time_start_ms) / 1000;
      time_hour = time_duration_s / 3600;
      time_minute = (time_duration_s % 3600) / 60;
      time_second = (time_duration_s % 3600 % 60);
    }
    if (now_ms - last_time_stamp_ms >= 60 * 1000) {
      Serial.println("add backup!");
      backup[backup_index] = smoothed_celsius;
      backup_index++;
    }
    if (now_ms - last_time_stamp_ms >= 2 * 60 * 1000) {
      Serial.println("add plot!");
      plot[plot_index] = smoothed_celsius;
      plot_index++;
    }
  }
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
      is_recording = false;
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
    case STOP_CONFIRM:
      stop_confirm();
    default:
      Serial.print("Page number error!!");
      page = MENU;
      break;
  }
  delay(200);
}
