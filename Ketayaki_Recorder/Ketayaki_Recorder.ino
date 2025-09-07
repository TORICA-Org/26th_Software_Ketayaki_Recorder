#include <SPI.h>
#include <SD.h>
#include "parameters.h"
#include "thermistor.h"
#include "tft.h"

/****************** SDカード書き込み **************************/
void writeSdData(float SD_time[], float plot[]) {
  myFile = SD.open(LOG_FILE_NAME, FILE_WRITE); // SDカードのファイルを開く
  
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

/******************* SD認識のため ********************/
bool initializeSD() {
  //SPI.end();
  //delay(100);
  //SPI.begin();
  //delay(100);
  find_SD = SD.begin(SD_CS);
  for (int i = 0; ; i++) {
    sprintf(LOG_FILE_NAME, "/KetayakiLog%03d.csv", i);
    if (!(SD.exists(LOG_FILE_NAME))) {
      Serial.print("\n");
      Serial.print(LOG_FILE_NAME);
      break;
    }
  }
  return find_SD;
}

void reset_parameters() {
  int record_index = 0;
  int backup_index = 0;
  int plot_index = 0;
  memset(backup, 0, sizeof(plot));
  memset(plot, 0, sizeof(plot));
}


void setup(){
  analogReadResolution(12);
  pinMode(Pin_thermistor, INPUT);
  
  Serial.begin(115200); 

  pinMode(LED_BUILTIN, OUTPUT);

  //SPI0
  SPI.setTX(TFT_TOUCH_SD_MOSI);
  SPI.setSCK(TFT_TOUCH_SD_SCK);
  SPI.setRX(TOUCH_SD_MISO);
  
}

void loop(){
  update_smoothed_celsius(Pin_thermistor);
}

void setup1(){
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
  if (initializeSD()) {
    Serial.println("SDカードが初期化されました");
  } else {
    Serial.println("SDカードの初期化に失敗しました");
    drawCenteredText(120, "SD card not found!", &FreeSans12pt7b, ILI9341_RED);
  }
}

void loop1(){

  unsigned long now_ms = millis();

  if(is_recording == true){

    if (now_ms - last_time_stamp_ms >= 1000) {

      last_time_stamp_ms = now_ms;
      time_duration_s = (now_ms - time_start_ms) / 1000;
      time_hour = time_duration_s / 3600;
      time_minute = (time_duration_s % 3600) / 60;
      time_second = time_duration_s % 3600 % 60;

      if (record_index == 0 || record_index % 60 == 0) {
        if (find_SD) {
          char buff[64];
          sprintf(buff, "%02u:%02u:%02u,%.3f°C\n", time_hour, time_minute, time_second, smoothed_celsius);
          myFile = SD.open(LOG_FILE_NAME, FILE_WRITE);
          if (myFile) {
            myFile.print(buff);
            myFile.close();
            Serial.print("\nfile write done!");
          }
          else {
            Serial.print("\nfile open failed!");
          }
        }
        backup[backup_index] = smoothed_celsius;
        Serial.print("\nadd backup!");
        backup_index++;
      }

      if (record_index == 0 || record_index % 120 == 0) {
        if (plot_index > sizeof(plot)) {
          plot_index = 0;
        }
        plot[plot_index] = smoothed_celsius;
        Serial.print("\nadd plot!");
        plot_index++;
      }

      Serial.print(".");
      record_index++;
    }
  }


  if (now_ms - tft_time_stamp_ms >= 100) {
    tft_time_stamp_ms = now_ms;
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
    page_refresh();
  }
}
