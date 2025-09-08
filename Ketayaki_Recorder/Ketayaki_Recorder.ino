#include <SPI.h>
#include <SD.h>
#include "parameters.h"
#include "thermistor.h"
#include "tft.h"

/******************* SD認識のため ********************/
bool initializeSD() {
  find_SD = SD.begin(SD_CS);
  for (int i = 0; 1; i++) {
    sprintf(LOG_FILE_PATH, "/KetayakiLog%03d_00.csv", i);
    if (!(SD.exists(LOG_FILE_PATH))) {
      sprintf(LOG_FILE_NAME, "/KetayakiLog%03d", i);
      break;
    }
  }
  return find_SD;
}

void reset_parameters() {
  time_duration_s = 0;
  timer_index = 0;
  plot_index = 0;
  file_index = 0;
  memset(plot, 0, sizeof(plot));
}

void setup() {
  //サーミスタ
  analogReadResolution(12);
  pinMode(Pin_thermistor, INPUT);

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
  
  Serial.begin(115200); 
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {

  update_smoothed_celsius(Pin_thermistor);

  unsigned long now_ms = millis();

  if(is_recording == true){

    if (now_ms - last_time_stamp_ms >= 1000) {

      last_time_stamp_ms = now_ms;
      time_duration_s = (now_ms - time_start_ms) / 1000;
      time_hour = time_duration_s / 3600;
      time_minute = (time_duration_s % 3600) / 60;
      time_second = time_duration_s % 3600 % 60;

      if (timer_index == 0 || timer_index % 60 == 0) {
        if (record_index % 900 == 0) {
          sprintf(LOG_FILE_PATH, "%s_%02d.csv", LOG_FILE_NAME, file_index);
          Serial.print("\nfile: ");
          Serial.print(LOG_FILE_PATH);
          file_index++;
        }
        if (find_SD) {
          char buff[64];
          sprintf(buff, "%02u:%02u:%02u,%.3f°C\n", time_hour, time_minute, time_second, smoothed_celsius);
          myFile = SD.open(LOG_FILE_PATH, FILE_WRITE);
          if (myFile) {
            myFile.print(buff);
            myFile.close();
            Serial.print("\nadd data to SD!");
          }
          else {
            Serial.print("\nfile open failed!");
          }
        }
        record_index++;
      }

      if (timer_index == 0 || timer_index % 120 == 0) {
        if (plot_index >= sizeof(plot)) {
          plot_index = 0;
        }
        plot[plot_index] = smoothed_celsius;
        Serial.print("\nadd plot!");
        plot_index++;
      }

      Serial.print(".");
      timer_index++;
    }
  }


  if (now_ms - tft_time_stamp_ms >= 200) {
    tft_time_stamp_ms = now_ms;
    detect_touch();
    update_page();
  }
}

void toneByFlag(int _pin, int _freq, int _dur) {
  spk_flag = true;
  spk_pin = _pin;
  spk_freq = _freq;
  spk_dur = _dur;
}

void ohuroByFlag(int _pin) {
  ohuro_flag = true;
  spk_pin = _pin;
}
 
void setup1() {
  pinMode(sound, OUTPUT);
}

void loop1() {
  if (spk_flag == true) {
    tone(spk_pin, spk_freq, spk_dur);
    spk_flag = false;
  }
  if (ohuro_flag == true) {
    ohuro(spk_pin);
    ohuro_flag = false;
  }
}