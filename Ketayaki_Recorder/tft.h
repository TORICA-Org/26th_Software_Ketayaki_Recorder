extern float smoothed_celsius;
extern bool find_SD;
extern bool open_SD;  //SDカードの書き込みが終わっているか確認
extern int Refresh_SD;
extern double SD_time[900];
extern void writeSdData(double SD_time[], double plot[]);
extern bool reinitializeSD();
extern int l;
extern unsigned long time_start;   //リフロー開始時刻
extern unsigned long time_now; //autoリフロー中の経過時間・残り時間計算用(ディスプレイ表示用)
extern unsigned long time_remaining;   //autoリフロー残り時間(ディスプレイ表示用)
extern unsigned long time_start_to_now;   //autoリフロー経過時間(ディスプレイ表示用)
extern double data; //グラフプロット時に使用
extern int num_loop; //core0のloopが回った回数を数える
extern double plot[900];
extern int plot_X;
extern int plot_Y;
#include "ohuro.h"

#include <Adafruit_GFX.h> // ライブラリマネージャで"Adafruit GFX Library"と依存ライブラリをインストール(Install all)
#include <Adafruit_ILI9341.h> // ライブラリマネージャで"Adafruit ILI9341"と依存ライブラリをインストール(Install all)
#include <XPT2046_Touchscreen.h> // ライブラリマネージャで"XPT2046_Touchscreen"をインストール

#include <JPEGDecoder.h> // ライブラリマネージャで"JPEGDecoder"をインストール
// =====注意======
//【要確認】JPEGの保存形式のうち「プログレッシブ形式」には非対応

#include <SPI.h>
// 以下のフォントは"Adafruit GFX Library"に付属
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSans12pt7b.h>
#include <Fonts/FreeSans18pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/Tiny3x3a2pt7b.h>
#define TFT_WIDTH 320    // Displayのx方向ビット数
#define TFT_HEIGHT 240   // Displayのy方向ビット数
#define SD_FILENAME "/Reflow_Tempdata.csv"
#define JPEG_FILENAME "TORICA_LOGO.jpg"

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);   //グラフィックのインスタンス
XPT2046_Touchscreen ts(TOUCH_CS);   //タッチパネルのインスタンス
JPEGDecoder jpegDec;    //SDのインスタンス
File myFile;    // Fileクラスのインスタンスを宣言

GFXcanvas16 canvas(TFT_WIDTH, TFT_HEIGHT);    //スプライト（16bit）通常タッチ画面用
GFXcanvas16 canvas1(TFT_WIDTH, TFT_HEIGHT);   //スプライト（16bit）通常画像表示用

enum PAGE_ENUM { // ページ用列挙型
  WELCOME,
  MENU,
  START_CONFIRM,
  RECORDING_PAGE,
  GRAPH,
  STOP_CONFIRM
};
PAGE_ENUM page = WELCOME;

enum TOUCH_STATUS { // タッチ状態の保持
  TOUCH_START,
  TOUCHING,
  RELEASE
};
TOUCH_STATUS touch_status = RELEASE;

int16_t touch_x; // タッチx座標の保持
int16_t touch_y; // タッチy座標の保持

/******************** テキスト描画関数 ********************/
void drawText(int16_t x, int16_t y, const char* text, const GFXfont* font, uint16_t color) {
  canvas.setFont(font);       // フォント
  canvas.setTextColor(color); // 文字色
  canvas.setCursor(x, y);     // 表示座標
  canvas.println(text);       // 表示内容
}

void drawCenteredText(int16_t _y, const char* text, const GFXfont* font, uint16_t color) {
  // テキストの幅と高さを取得
  int16_t textX, textY; // テキスト位置取得用
  uint16_t textWidth, textHeight; // テキストサイズ取得用
  canvas.setFont(font);  // フォント指定
  canvas.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);
  canvas.setTextColor(color); // 文字色
  canvas.setCursor((TFT_WIDTH/2 - textWidth/2), _y);     // 表示座標
  canvas.println(text);       // 表示内容
}

/******************** ボタン描画関数 ********************/
void drawButton(int x, int y, int w, int h, const char* label, const GFXfont* font, uint16_t bgColor, uint16_t labelColor) {
  canvas.fillRect(x, y, w, h, ILI9341_DARKGREY);      // 外枠
  canvas.fillRect(x + 3, y + 3, w-6, h-6, ILI9341_WHITE); // 境界線
  canvas.fillRect(x + 6, y + 6, w-12, h-12, bgColor);       // 操作部
  canvas.setFont(font); // 表示ラベル

  // テキストの幅と高さを取得
  int16_t textX, textY; // テキスト位置取得用
  uint16_t textWidth, textHeight; // テキストサイズ取得用
  canvas.getTextBounds(label, x, y, &textX, &textY, &textWidth, &textHeight);
  
  // 中央揃えのための新しいx, y座標の計算
  int16_t centeredX = x + (w - textWidth) / 2;               // xを中央へ
  int16_t centeredY = y + (h - textHeight) / 2 + textHeight; // yを下げて中央へ
  
  canvas.setTextColor(labelColor);        // 文字色
  canvas.setCursor(centeredX, centeredY); // 新しいカーソル位置を設定
  canvas.print(label);                    // テキストを描画
}

/******************** ボタンタッチ判定用関数 ********************/
bool BUTTON_TOUCH(int _x, int _y, int _w, int _h) {
  return (_x <= touch_x && touch_x <= _x + _w && _y <= touch_y && touch_y <= _y + _h);
}

/********************* JPEG表示 *************************/
void jpegDraw(const char* filename) {
  myFile = SD.open(filename, FILE_READ);
  JpegDec.decodeSdFile(myFile); // JPEGファイルをSDカードからデコード実行
  
  // シリアル出力、デコード画像情報(MCU [Minimum Coded Unit]：JPEG画像データの最小処理単位
  Serial.printf("Size : %d x %d\nMCU : %d x %d\n", JpegDec.width, JpegDec.height, JpegDec.MCUWidth, JpegDec.MCUHeight);
  Serial.printf("Components: %d\nMCU / row: %d\nMCU / col: %d\nScan type: %d\n\n", JpegDec.comps, JpegDec.MCUSPerRow, JpegDec.MCUSPerCol, JpegDec.scanType);  

  uint16_t *pImg;   // ピクセルデータ用のポインタ
  while (JpegDec.read()) {  // JPEGデータを読み込む
    pImg = JpegDec.pImage;  // 現在のピクセルデータのポインタを取得
    for (int h = 0; h < JpegDec.MCUHeight; h++) { // MCU高さ分ループ
      for (int w = 0; w < JpegDec.MCUWidth; w++) {  // MCU幅分ループ
        // 現在のピクセルのx, y座標を計算
        int x = JpegDec.MCUx * JpegDec.MCUWidth + w;  // x座標
        int y = JpegDec.MCUy * JpegDec.MCUHeight + h; // y座標
        if (x < JpegDec.width && y < JpegDec.height) {  // ピクセルが画像範囲内なら
          canvas.drawRGBBitmap(x, y, pImg, 1, 1); // 液晶画面にピクセルを描画
        }
        pImg += JpegDec.comps;  // ポインタを次のピクセルデータへ進める
      }
    }
  }
  myFile.close();
}

/******************** TORICA ********************/
void welcome(){
  jpegDraw(JPEG_FILENAME);
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  delay(3000);
  page = MENU;
}


/******************** メニュー ********************/
int menu(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(40, 30, "SD Status :", &FreeSans9pt7b, ILI9341_WHITE);
  if (find_SD) {
    drawText(140, 30, "Available", &FreeSans9pt7b, ILI9341_GREEN);
  }
  else {
    drawText(140, 30, "Not Available", &FreeSans9pt7b, ILI9341_RED);
  }
  
  drawText(42, 60, "Temp [ C] :", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawCircle(100, 50, 2, ILI9341_WHITE);
  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(140, 60);          // 表示座標指定
  canvas.print(smoothed_celsius);

  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 80, 320, ILI9341_WHITE);

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(20, 95, 280, 60, "Start Recording", &FreeSansBold12pt7b, ILI9341_WHITE, ILI9341_BLUE); // 記録開始ボタン
  drawButton(20, 165, 280, 60, "Remount SD", &FreeSansBold12pt7b, ILI9341_WHITE, ILI9341_BLUE); // SDカード再マウントボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (BUTTON_TOUCH(20, 95, 280, 60)){
      tone(sound,3000,100);
      page = START_CONFIRM;
      l = 0;
    }
    if (BUTTON_TOUCH(20, 165, 280, 60)){
      tone(sound,3000,100);
      find_SD = reinitializeSD();
    }

  }

  return page;
}


/******************** スタート確認 ********************/
int start_confirm(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(40, 30, "SD Status :", &FreeSans9pt7b, ILI9341_WHITE);
  if (find_SD) {
    drawText(140, 30, "Available", &FreeSans9pt7b, ILI9341_GREEN);
  }
  else {
    drawText(140, 30, "Not Available", &FreeSans9pt7b, ILI9341_RED);
  }
  
  drawText(42, 60, "Temp [ C] :", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawCircle(100, 50, 2, ILI9341_WHITE);
  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(140, 60);          // 表示座標指定
  canvas.print(smoothed_celsius);

  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 80, 320, ILI9341_WHITE);

  drawCenteredText(120, "Start Recording?", &FreeSans18pt7b, ILI9341_WHITE);

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(15, 140, 140, 85, "Cancel", &FreeSansBold18pt7b, ILI9341_WHITE, ILI9341_RED); // OFFボタン
  drawButton(165, 140, 140, 85, "Start", &FreeSansBold18pt7b, ILI9341_WHITE, ILI9341_BLUE); // ONボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (BUTTON_TOUCH(15, 130, 140, 85)){
      page = MENU;
      tone(sound,3000,100);
    }
    if (BUTTON_TOUCH(165, 130, 140, 85)){
      page = RECORDING_PAGE;
      l = 0;
      time_start_to_now = 0.0;
      tone(sound,3000,100);
      delay(50);
      tone(sound,3000,700);
    }

  }
  
  return page;
}

/******************** 記録中 ********************/

int recording(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット

  canvas.fillRect(0, 0, TFT_WIDTH, 3, ILI9341_RED);
  canvas.fillRect(0, 0, 3, TFT_HEIGHT, ILI9341_RED);
  canvas.fillRect(0, (TFT_HEIGHT - 3), TFT_WIDTH, 3, ILI9341_RED);
  canvas.fillRect((TFT_WIDTH - 3), 0, 3, TFT_HEIGHT, ILI9341_RED);
  
  canvas.fillCircle(80, 22, 10, ILI9341_RED); // 記録中の赤丸
  drawCenteredText(30, "Recording...", &FreeSans12pt7b, ILI9341_RED);

  drawText(40, 60, "SD Status :", &FreeSans9pt7b, ILI9341_WHITE);
  if (find_SD) {
    drawText(140, 60, "Available", &FreeSans9pt7b, ILI9341_GREEN);
  }
  else {
    drawText(140, 60, "Not Available", &FreeSans9pt7b, ILI9341_RED);
  }
  
  drawText(42, 90, "Temp [ C] :", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawCircle(100, 80, 2, ILI9341_WHITE);
  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(140, 90);          // 表示座標指定
  canvas.print(smoothed_celsius);


  time_now = millis();
  
  time_start_to_now = time_now - time_start;
  if(time_start_to_now >= 210000){
    time_remaining = 0;
  }
  else{
    time_remaining = 210000 - time_start_to_now;
  }
  
  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(3, 153);          // 表示座標指定
  canvas.print(time_start_to_now / 1000);    //リフロー開始からの経過時間 
  canvas.print("s");

  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(280, 153);          // 表示座標指定
  canvas.print(time_remaining / 1000);    //リフロー終了までの残り時間
  canvas.print("s");
  




  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 185, 140, 50, "Graph", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // OFFボタン
  drawButton(170, 185, 140, 50, "Emergency", &FreeSansBold9pt7b, ILI9341_RED, ILI9341_YELLOW); // ONボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (BUTTON_TOUCH(25, 185, 140, 50)){
      page = GRAPH;   // 範囲内ならpage11 グラフ
      tone(sound,3000,100);
    }
    if (BUTTON_TOUCH(170, 185, 140, 50)){
      page = STOP_CONFIRM; // 範囲内ならpage8 Emargency
      ohuro(sound);
    }

  }



  return page;
}

/******************** グラフ ********************/
int graph(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット

  //グラフ軸
  canvas.drawFastHLine(5, 230, 315, ILI9341_WHITE);     //横軸
  canvas.drawFastVLine(5, 10, 220, ILI9341_WHITE);     //縦軸

  //グラフ目盛
  //縦軸
  canvas.drawFastHLine(4, 220, 3, ILI9341_WHITE);   //30
  canvas.drawFastHLine(4, 210, 3, ILI9341_WHITE);   //40
  canvas.drawFastHLine(3, 200, 5, ILI9341_WHITE);   //50
  drawText(8, 205, "50", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(4, 190, 3, ILI9341_WHITE);   //60
  canvas.drawFastHLine(4, 180, 3, ILI9341_WHITE);    //70
  canvas.drawFastHLine(4, 170, 3, ILI9341_WHITE);   //80
  canvas.drawFastHLine(4, 160, 3, ILI9341_WHITE);   //90
  canvas.drawFastHLine(3, 150, 5, ILI9341_WHITE);   //100
  drawText(8, 155, "100", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(4, 140, 3, ILI9341_WHITE);   //110
  canvas.drawFastHLine(4, 130, 3, ILI9341_WHITE);   //120
  canvas.drawFastHLine(4, 120, 3, ILI9341_WHITE);   //130
  canvas.drawFastHLine(4, 110, 3, ILI9341_WHITE);   //140
  canvas.drawFastHLine(4, 100, 3, ILI9341_WHITE);   //150
  drawText(8, 105, "150", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(4, 90, 3, ILI9341_WHITE);    //160
  canvas.drawFastHLine(4, 80, 3, ILI9341_WHITE);    //170
  canvas.drawFastHLine(4, 70, 3, ILI9341_WHITE);    //180
  canvas.drawFastHLine(4, 60, 3, ILI9341_WHITE);    //190
  canvas.drawFastHLine(3, 50, 5, ILI9341_WHITE);    //200
  canvas.drawFastHLine(4, 40, 3, ILI9341_WHITE);    //210
  canvas.drawFastHLine(4, 30, 3, ILI9341_WHITE);    //220
  canvas.drawFastHLine(4, 20, 3, ILI9341_WHITE);    //230
  canvas.drawFastHLine(4, 10, 3, ILI9341_WHITE);    //240


  //横軸
  drawText(41, 225, "30", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(50, 228, 5, ILI9341_WHITE);    //30
  canvas.drawFastVLine(95, 228, 5, ILI9341_WHITE);    //60
  canvas.drawFastVLine(140, 228, 5, ILI9341_WHITE);    //90
  drawText(170, 225, "120", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(185, 228, 5, ILI9341_WHITE);   //120
  canvas.drawFastVLine(230, 228, 5, ILI9341_WHITE);    //150
  canvas.drawFastVLine(275, 228, 5, ILI9341_WHITE);    //180
  drawText(291, 225, "210", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(319, 228, 5, ILI9341_WHITE);


  // グラフへのプロット
  if(num_loop != 1 && num_loop % 2 == 1){ //loopをまわした回数 = plot[]の要素数が2の倍数の時
    for(int a = 0; a <= (num_loop - 3); a += 2){
      data = (plot[a] + plot[a + 1]) / 2;
      plot_X = 5 + ((3*a) / 2) + 1;
      plot_Y = 230 - (data - 20.0 + 0.5);

      canvas.drawFastHLine(plot_X - 1, plot_Y - 1, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y + 1, 3, ILI9341_ORANGE);
    }
  }
  
  
  if(num_loop != 0 && num_loop % 2 == 0){ //loopをまわした回数 = plot[]の要素数が2の倍数じゃない時
    for(int a = 0; a <= (num_loop - 2); a += 2){
      data = (plot[a] + plot[a + 1]) / 2;
      plot_X = 5 + ((3*a) / 2) + 1;
      plot_Y = 230 - (data - 20.0 + 0.5);

      canvas.drawFastHLine(plot_X - 1, plot_Y - 1, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y, 3, ILI9341_ORANGE);
      canvas.drawFastHLine(plot_X - 1, plot_Y + 1, 3, ILI9341_ORANGE);

    }
  }

  
  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(30, 10, 280, 50, "Touch the screen to exit", &FreeSans9pt7b, (uint16_t)NULL, ILI9341_WHITE); // page7に戻る
  

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  
  if (touch_status == TOUCH_START) {  // タッチされていれば
    tone(sound,3000,100);
    page = RECORDING_PAGE;
  }
  
  return page;
}

/******************* 12ページ目 autoリフロー終了画面 ********************/
int page_12(){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  
  if(open_SD == true){
    drawText(20, 40, "Data saving...", &FreeSans18pt7b, ILI9341_WHITE);
    drawText(20, 90, "Don't turn off the power yet!!", &FreeSans18pt7b, ILI9341_WHITE);
  }
  else{
    drawText(20, 40, "Reflow is finished!!", &FreeSans12pt7b, ILI9341_WHITE);
    drawText(20, 90, "Check if data is saved on the SD card!", &FreeSans12pt7b, ILI9341_WHITE);

    // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
    drawButton(25, 185, 140, 50, "Menu", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // OFFボタン
    drawButton(170, 185, 140, 50, "Resave", &FreeSansBold9pt7b, ILI9341_RED, ILI9341_YELLOW); // ONボタン

  if (touch_status == TOUCH_START) {  // タッチされていれば
      // ボタンタッチエリア検出
      if (touch_x >= 25 && touch_x <= 165 && touch_y >= 185 && touch_y <= 235){
        page = MENU;   // 範囲内ならpage2
        tone(sound,3000,100);
      }
      if (touch_x >= 170 && touch_x <= 310 && touch_y >= 185 && touch_y <= 235){
        tone(sound,3000,100);
        Refresh_SD = 0;
        reinitializeSD();
      }

    }

  }

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  return page;
}

/******************* 13ページ目 manualリフロー終了画面 ********************/

int page_13(){
  if (!SD.begin(SD_CS)) {
    Serial.println("SDカードの初期化に失敗しました");
    drawText(50, 50, "SD card not found!", &FreeSans12pt7b, ILI9341_RED);
    digitalWrite(LED_BUILTIN, LOW);
    find_SD = false;
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
    find_SD = true;
    Serial.println("SDカードの初期化に成功しました");
  }
  


  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (touch_x >= 25 && touch_x <= 165 && touch_y >= 102 && touch_y <= 187){
      page = MENU;   // 範囲内なら保存せずpage2へ
      tone(sound,3000,100);
    }
    if (touch_x >= 170 && touch_x <= 310 && touch_y >= 102 && touch_y <= 187) {
      if(find_SD == true){
        writeSdData(SD_time, plot);
        //page = 12; // 範囲内なら保存してpage12へ
      }
      else{
        page = MENU;
        Serial.println("SDカードが見つかりませんでした");
      }

      tone(sound,3000,100);
    }
  }


  if (!myFile) {
    Serial.println("ファイルオープンに失敗");
    if (!SD.begin(SD_CS)) {
      Serial.println("SDカード再初期化も失敗");
    } else {
      Serial.println("再初期化には成功したがファイルオープン失敗");
    }
  }


  Serial.println(plot[3]);
  return page;
}
