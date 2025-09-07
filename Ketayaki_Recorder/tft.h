#include <Arduino.h>
#include "ohuro.h"
extern bool initializeSD();

#include <Adafruit_GFX.h> // ライブラリマネージャで"Adafruit GFX Library"と依存ライブラリをインストール(Install all)
#include <Adafruit_ILI9341.h> // ライブラリマネージャで"Adafruit ILI9341"と依存ライブラリをインストール(Install all)
#include <XPT2046_Touchscreen.h> // ライブラリマネージャで"XPT2046_Touchscreen"をインストール

#include <JPEGDecoder.h> // ライブラリマネージャで"JPEGDecoder"をインストール
// =====注意======
//【要確認】JPEGの保存形式のうち「プログレッシブ形式」には非対応

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
#define JPEG_FILENAME "/TORICA_LOGO.jpg"

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);   //グラフィックのインスタンス
XPT2046_Touchscreen ts(TOUCH_CS);   //タッチパネルのインスタンス
JPEGDecoder jpegDec;    //JPEGDecoderのインスタンス

GFXcanvas16 canvas(TFT_WIDTH, TFT_HEIGHT);    //スプライト（16bit）通常タッチ画面用
GFXcanvas16 canvas1(TFT_WIDTH, TFT_HEIGHT);   //スプライト（16bit）通常画像表示用

enum TOUCH_STATUS { // タッチ状態の保持
  TOUCH_START,
  TOUCHING,
  RELEASE
};
TOUCH_STATUS touch_status = RELEASE;

int16_t touch_x; // タッチx座標の保持
int16_t touch_y; // タッチy座標の保持

enum PAGE_ENUM { // ページ用列挙型
  WELCOME,
  MENU,
  CONFIRM_START,
  RECORDING,
  GRAPH,
  CONFIRM_STOP
};
PAGE_ENUM page = WELCOME;

/******************** テキスト描画関数 ********************/
void drawText(int16_t x, int16_t y, const char* text, const GFXfont* font, uint16_t color) {
  canvas.setFont(font);       // フォント
  canvas.setTextColor(color); // 文字色
  canvas.setCursor(x, y);     // 表示座標
  canvas.println(text);       // 表示内容
}

void drawTextCenterAlign(int16_t x, int16_t y, const char* text, const GFXfont* font, uint16_t color) {
  // テキストの幅と高さを取得
  int16_t textX, textY; // テキスト位置取得用
  uint16_t textWidth, textHeight; // テキストサイズ取得用
  canvas.setFont(font);  // フォント指定
  canvas.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);
  canvas.setTextColor(color); // 文字色
  canvas.setCursor((x - textWidth/2), (y + textHeight/2));     // 表示座標
  canvas.println(text);       // 表示内容
}

void drawCenteredText(int16_t y, const char* text, const GFXfont* font, uint16_t color) {
  // テキストの幅と高さを取得
  int16_t textX, textY; // テキスト位置取得用
  uint16_t textWidth, textHeight; // テキストサイズ取得用
  canvas.setFont(font);  // フォント指定
  canvas.getTextBounds(text, 0, 0, &textX, &textY, &textWidth, &textHeight);
  canvas.setTextColor(color); // 文字色
  canvas.setCursor((TFT_WIDTH/2 - textWidth/2), y);     // 表示座標
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
  drawButton(20, 165, 280, 60, "Mount SD", &FreeSansBold12pt7b, ILI9341_WHITE, ILI9341_BLUE); // SDカードマウントボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (BUTTON_TOUCH(20, 95, 280, 60)){
      tone(sound,3000,100);
      page = CONFIRM_START;
    }
    if (BUTTON_TOUCH(20, 165, 280, 60)){
      tone(sound,3000,100);
      initializeSD();
    }

  }

  return page;
}


/******************** スタート確認 ********************/
int confirm_start(){
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
      page = RECORDING;
      time_start_ms = millis();
      time_duration_s = 0;
      record_index = 0;
      backup_index = 0;
      is_recording = true;
      tone(sound,3000,100);
      delay(100);
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

  char buff[20];
  sprintf(buff, "%02uh %02um %02us", time_hour, time_minute, time_second);
  drawCenteredText(153, buff, &FreeSans18pt7b, ILI9341_WHITE);
  
  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 185, 140, 50, "Graph", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE);
  drawButton(170, 185, 140, 50, "Stop", &FreeSans18pt7b, ILI9341_RED, ILI9341_WHITE);

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (BUTTON_TOUCH(25, 185, 140, 50)){
      page = GRAPH;   // 範囲内ならpage11 グラフ
      tone(sound,3000,100);
    }
    if (BUTTON_TOUCH(170, 185, 140, 50)){
      page = CONFIRM_STOP;
      tone(sound,3000,100);
    }
  }
  return page;
}

/******************** グラフ ********************/
const int ox = 30;
const int oy = 20;
int vSpace = 15;
int hSpace = 30;
int gx (int input) {return input + ox;}
int gy (int input) {return TFT_HEIGHT - (input + oy);}

void makeGraphArea() {
  //グラフ軸
  canvas.drawFastHLine(gx(0), gy(0), 280, ILI9341_WHITE);     //横軸
  canvas.drawFastVLine(gx(0), gy(0), -(TFT_HEIGHT - oy - 5), ILI9341_WHITE);     //縦軸

  //グラフ目盛
  //縦軸
  canvas.drawFastHLine(gx(-2), gy(vSpace*14), 5, ILI9341_WHITE);   //150
  drawText(gx(-33), gy(vSpace*14 - 6), "150", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(gx(-1), gy(vSpace*13), 3, ILI9341_WHITE);   //140
  canvas.drawFastHLine(gx(-1), gy(vSpace*12), 3, ILI9341_WHITE);   //130
  canvas.drawFastHLine(gx(-1), gy(vSpace*11), 3, ILI9341_WHITE);   //120
  canvas.drawFastHLine(gx(-1), gy(vSpace*10), 3, ILI9341_WHITE);   //110
  canvas.drawFastHLine(gx(-2), gy(vSpace*9), 5, ILI9341_WHITE);    //100
  drawText(gx(-33), gy(vSpace*9 - 6), "100", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(gx(-1), gy(vSpace*8), 3, ILI9341_WHITE);    //90
  canvas.drawFastHLine(gx(-1), gy(vSpace*7), 3, ILI9341_WHITE);    //80
  canvas.drawFastHLine(gx(-1), gy(vSpace*6), 3, ILI9341_WHITE);    //70
  canvas.drawFastHLine(gx(-1), gy(vSpace*5), 3, ILI9341_WHITE);    //60
  canvas.drawFastHLine(gx(-2), gy(vSpace*4), 5, ILI9341_WHITE);    //50
  drawText(gx(-23), gy(vSpace*4 - 6), "50", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastHLine(gx(-1), gy(vSpace*3), 3, ILI9341_WHITE);    //40
  canvas.drawFastHLine(gx(-1), gy(vSpace*2), 3, ILI9341_WHITE);    //30
  canvas.drawFastHLine(gx(-1), gy(vSpace*1), 3, ILI9341_WHITE);    //20
  drawText(gx(-23), gy(-6), "20", &FreeSans9pt7b, ILI9341_WHITE);

  //横軸
  canvas.drawFastVLine(gx(hSpace*1), gy(2), 5, ILI9341_WHITE);    //1h
  canvas.drawFastVLine(gx(hSpace*2), gy(2), 5, ILI9341_WHITE);    //2h
  canvas.drawFastVLine(gx(hSpace*3), gy(2), 5, ILI9341_WHITE);    //3h
  drawText(gx(hSpace*3 - 10), gy(-18), "3h", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(gx(hSpace*4), gy(2), 5, ILI9341_WHITE);   //4h
  canvas.drawFastVLine(gx(hSpace*5), gy(2), 5, ILI9341_WHITE);   //5h
  canvas.drawFastVLine(gx(hSpace*6), gy(2), 5, ILI9341_WHITE);   //6h
  drawText(gx(hSpace*6 - 10), gy(-18), "6h", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(gx(hSpace*7), gy(2), 5, ILI9341_WHITE);   //7h
  canvas.drawFastVLine(gx(hSpace*8), gy(2), 5, ILI9341_WHITE);   //8h
  canvas.drawFastVLine(gx(hSpace*9), gy(2), 5, ILI9341_WHITE);   //9h
  drawText(gx(hSpace*9 - 10), gy(-18), "9h", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawFastVLine(gx(280), gy(-2), -(TFT_HEIGHT - oy - 5), ILI9341_WHITE);  //10h
}

int graph() {
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  makeGraphArea(); // グラフ領域描写

  // グラフへのプロット
  for (int i = 0; i < 280; i++) {
    if (plot[i] > 20) {
      canvas.fillCircle(gx(i), gy((plot[i] - 20)*135/100), 1, ILI9341_ORANGE);
    }
  }
  
  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  //drawButton(30, 10, 280, 50, "Touch the screen to exit", &FreeSans9pt7b, (uint16_t)NULL, ILI9341_WHITE); // page7に戻る

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  
  if (touch_status == TOUCH_START) {  // タッチされていれば
    tone(sound,3000,100);
    page = RECORDING;
  }
  
  return page;
}

int confirm_stop() {
  static int confirm_progress = 0;
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット

  drawCenteredText(50, "Are you sure?", &FreeSans18pt7b, ILI9341_WHITE);

  canvas.drawCircle(50, 120, 45, ILI9341_WHITE);
  canvas.drawCircle(160, 120, 45, ILI9341_WHITE);
  canvas.drawCircle(270, 120, 45, ILI9341_WHITE);

  switch (confirm_progress) {
    case 0:
      drawTextCenterAlign(50, 120, "TOUCH", &FreeSans9pt7b, ILI9341_WHITE);
      if (touch_status == TOUCH_START) {  // タッチされていれば
        // ボタンタッチエリア検出
        if (BUTTON_TOUCH(30, 70, 100, 100)){
          confirm_progress = 1;
          tone(sound,3000,100);
        }
      }
      break;
    case 1:
      canvas.fillCircle(50, 120, 40, ILI9341_GREEN);
      drawTextCenterAlign(160, 120, "TOUCH", &FreeSans9pt7b, ILI9341_WHITE);
      if (touch_status == TOUCH_START) {  // タッチされていれば
        // ボタンタッチエリア検出
        if (BUTTON_TOUCH(120, 70, 100, 100)){
          confirm_progress = 2;
          tone(sound,3000,100);
        }
      }
      break;
    case 2:
      canvas.fillCircle(50, 120, 40, ILI9341_GREEN);
      canvas.fillCircle(160, 120, 40, ILI9341_GREEN);
      drawTextCenterAlign(270, 120, "TOUCH", &FreeSans9pt7b, ILI9341_WHITE);
      if (touch_status == TOUCH_START) {  // タッチされていれば
        // ボタンタッチエリア検出
        if (BUTTON_TOUCH(220, 70, 100, 100)){
          confirm_progress = 3;
          tone(sound,3000,100);
        }
      }
      break;
    case 3:
      canvas.fillCircle(50, 120, 40, ILI9341_GREEN);
      canvas.fillCircle(160, 120, 40, ILI9341_GREEN);
      canvas.fillCircle(270, 120, 40, ILI9341_GREEN);
      confirm_progress = 4;
      break;
    default:
      break;
  }

  drawButton(90, 185, 140, 50, "Cancel", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE);

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (BUTTON_TOUCH(90, 185, 140, 50)){
      confirm_progress = 0;
      page = RECORDING;
      tone(sound,3000,100);
    }
  }

  if (confirm_progress >= 4) {
      confirm_progress = 0;
      page = MENU;
      is_recording = false;
      ohuro(sound);
  }

  return page;
}

void page_refresh() {
  switch(page){
    case WELCOME:
      welcome();
      break;
    case MENU:
      menu();
      break;
    case CONFIRM_START:
      confirm_start();
      break;
    case RECORDING:
      recording();
      break;
    case GRAPH:
      graph();
      break;
    case CONFIRM_STOP:
      confirm_stop();
      break;
    default:
      Serial.print("Page number error!!");
      page = MENU;
      break;
  }
}