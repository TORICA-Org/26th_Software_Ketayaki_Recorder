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

const int Pin_thermistor = 26; // サーミスタ用

// =======================================

Adafruit_ILI9341 tft = Adafruit_ILI9341(&SPI, TFT_DC, TFT_CS, TFT_RST);   //グラフィックのインスタンス
XPT2046_Touchscreen ts(TOUCH_CS);   //タッチパネルのインスタンス
JPEGDecoder jpegDec;    //SDのインスタンス
File myFile;    // Fileクラスのインスタンスを宣言

GFXcanvas16 canvas(TFT_WIDTH, TFT_HEIGHT);    //スプライト（16bit）通常タッチ画面用
GFXcanvas16 canvas1(TFT_WIDTH, TFT_HEIGHT);   //スプライト（16bit）通常画像表示用

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

enum PAGE_ENUM { // ページ用列挙型
  WELCOME,
  MENU,
  START_CONFIRM,
  RECORDING,
  GRAPH,
  STOP_CONFIRM
};
PAGE_ENUM page = WELCOME;

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

/***************** auto用　ランプ点灯状態更新関数 ***********************/
void auto_updateLamp(){
  if(SSR_ON == true){
    canvas.fillCircle(70, 22, 17, ILI9341_RED);         // Heatimg点灯
    canvas.fillCircle(210, 22, 17, ILI9341_DARKGREY); // Waiting消灯
  }
  if(SSR_ON == false){
    canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heating消灯
    canvas.fillCircle(210, 22, 17, ILI9341_BLUE);    // Waiting点灯
  }
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

/******************** 進行状況の三角描画関数 ********************/
void triangle(int triangle_x){
  canvas.drawFastHLine(triangle_x, 156, 9, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 1, 157, 7, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 2, 158, 5, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 3, 159, 3, ILI9341_WHITE);
  canvas.drawFastHLine(triangle_x + 4, 160, 1, ILI9341_WHITE);
}



/******************** 1ページ目 TORICA ********************/
void welcome(){
  jpegDraw(JPEG_FILENAME);
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);
  delay(3000);
  page = MENU;
}


/******************** 2ページ目 manual or auto ********************/
int menu(double smoothed_celsius){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(20, 30, "SD Status :", &FreeSans9pt7b, ILI9341_WHITE);
  if (find_SD) {
    drawText(120, 30, "Detected", &FreeSans9pt7b, ILI9341_GREEN);
  }
  else {
    drawText(120, 30, "Not Detected", &FreeSans9pt7b, ILI9341_RED);
  }
  
  drawText(22, 60, "Temp [ C] :", &FreeSans9pt7b, ILI9341_WHITE);
  canvas.drawCircle(80, 50, 2, ILI9341_WHITE);
  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans9pt7b);  // フォント指定
  canvas.setCursor(120, 60);          // 表示座標指定
  canvas.print(smoothed_celsius);

  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 80, 320, ILI9341_WHITE);

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(20, 90, 280, 60, "Start Recording", &FreeSansBold12pt7b, ILI9341_ORANGE, ILI9341_WHITE); // 記録開始ボタン
  drawButton(20, 160, 280, 60, "Remount SD", &FreeSansBold12pt7b, ILI9341_GREEN, ILI9341_WHITE); // SDカード再マウントボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (touch_x >= 20 && touch_x <= 280 && touch_y >= 60 && touch_y <= 120){
      tone(sound,3000,100);
      page = START_CONFIRM;
      l = 0;
    }
    if (touch_x >= 20 && touch_x <= 280 && touch_y >= 130 && touch_y <= 190){
      tone(sound,3000,100);
      find_SD = reinitializeSD();
    }

  }

  return page;
}


/******************** 6ページ目 autoリフロースタート最終確認 ********************/
int start_confirm(double smoothed_celsius){
  SSR_ON = false;
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(60, 230, "Temp:", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(200, 230, "[degC]", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(20, 100, "Start Auto-Reflow?", &FreeSans18pt7b, ILI9341_WHITE);

  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans12pt7b);  // フォント指定
  canvas.setCursor(140, 230);          // 表示座標指定
  canvas.print(smoothed_celsius);

  
  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);

  // Heating_Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(70, 22, 17, ILI9341_DARKGREY); // Heatingランプ部消灯
  canvas.fillCircle(210, 22, 17, ILI9341_BLUE); // Waitingランプ部点灯


  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 120, 140, 85, "Start", &FreeSansBold18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // ONボタン
  drawButton(170, 120, 140, 85, "Cancel", &FreeSansBold18pt7b, ILI9341_GREEN, ILI9341_WHITE); // OFFボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (touch_x >= 25 && touch_x <= 165 && touch_y >= 120 && touch_y <= 205){
      l = 0;
      time_start_to_now = 0.0;
      tone(sound,3000,100);
      page = RECORDING;
    }
    if (touch_x >= 170 && touch_x <= 310 && touch_y >= 120 && touch_y <= 205){
      page = MENU;
      tone(sound,3000,100);
    }

  }
  
  return page;
}

/******************** 7ページ目 autoリフロー中 ********************/

int recording(double smoothed_celsius){
  canvas.fillScreen(ILI9341_BLACK);   //背景色リセット
  drawText(92, 30, "Heating", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(232, 30, "Waiting", &FreeSans9pt7b, ILI9341_WHITE);
  drawText(8, 85, "Temp:", &FreeSans18pt7b, ILI9341_WHITE);
  drawText(245, 88, "[degC]", &FreeSans12pt7b, ILI9341_WHITE);
  drawText(8, 130, "Phase:", &FreeSans18pt7b, ILI9341_WHITE);

  canvas.setTextColor(ILI9341_WHITE); 
  canvas.setFont(&FreeSans18pt7b);  // フォント指定
  canvas.setCursor(135, 85);          // 表示座標指定
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
  

  
  if(0 <= time_start_to_now && time_start_to_now <= 30000){
    drawText(130, 130, "Heat up to 100", &FreeSans12pt7b, ILI9341_YELLOW);
  }
  
  if(30000 < time_start_to_now && time_start_to_now <= 120000){
    drawText(130, 130, "Heat up to 150", &FreeSans12pt7b, ILI9341_ORANGE); 
  }
  if(120000 < time_start_to_now && time_start_to_now <= 210000){
    drawText(130, 130, "Heat up to 235", &FreeSans12pt7b, ILI9341_RED);
  }
  if(210000 < time_start_to_now){
    /*
    check_reflow_start = false;
    canvas.fillRect(120, 30, 80, 30, ILI9341_BLACK);    //温度の表示を黒塗りで隠したい
    if(find_SD == true){
      writeSdData(SD_time, plot);
    }
    */
    ohuro();
    //delay(8000);
    //page = 12;
  }
  
  triangle_x = ((time_start_to_now / 1000) *297 / 210) + 8;
  triangle(triangle_x);


  // 平行線(x始点，y始点，長さ)
  canvas.drawFastHLine(0, 46, 320, ILI9341_WHITE);
  canvas.drawFastHLine(0, 100, 320, ILI9341_WHITE);
  canvas.drawFastHLine(0, 180, 320, ILI9341_WHITE);

  // スライドボリューム描画
  canvas.fillRect(10, 162, 300, 11, ILI9341_WHITE);    // 枠
  canvas.fillRect(12, 164, 42, 7, ILI9341_YELLOW);
  canvas.fillRect(54, 164, 127, 7, ILI9341_ORANGE);
  canvas.fillRect(181, 164, 127, 7, ILI9341_RED);

  // Heating-Waitingランプ描画(x, y, 半径, 色)
  canvas.fillCircle(70, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(70, 22, 18, ILI9341_WHITE);    // 境界線
  canvas.fillCircle(210, 22, 20, ILI9341_DARKGREY); // 外枠
  canvas.fillCircle(210, 22, 18, ILI9341_WHITE);    // 境界線
  auto_updateLamp(); // ボタン点灯状態更新関数呼び出し

  // ボタン描画（左上x, 左上y, wide, high, ラベル, フォント, ボタン色, ラベル色）
  drawButton(25, 185, 140, 50, "Graph", &FreeSans18pt7b, ILI9341_ORANGE, ILI9341_WHITE); // OFFボタン
  drawButton(170, 185, 140, 50, "Emergency", &FreeSansBold9pt7b, ILI9341_RED, ILI9341_YELLOW); // ONボタン

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);


  if (touch_status == TOUCH_START) {  // タッチされていれば
    // ボタンタッチエリア検出
    if (touch_x >= 25 && touch_x <= 165 && touch_y >= 185 && touch_y <= 235){
      page = GRAPH;   // 範囲内ならpage11 グラフ
      tone(sound,3000,100);
    }
    if (touch_x >= 170 && touch_x <= 310 && touch_y >= 185 && touch_y <= 235){
      page = STOP_CONFIRM; // 範囲内ならpage8 Emargency
      tone(sound,3000,100);
    }

  }



  return page;
}

/******************** 11ページ目 グラフ ********************/
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
  drawButton(30, 10, 280, 50, "Touch the screen to exit", &FreeSans9pt7b, NULL, ILI9341_WHITE); // page7に戻る
  

  //スプライトをディスプレイ表示
  tft.drawRGBBitmap(0, 0, canvas.getBuffer(), TFT_WIDTH, TFT_HEIGHT);

  
  if (touch_status == TOUCH_START) {  // タッチされていれば
    tone(sound,3000,100);
    page = RECORDING;
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


/******************** 焼き終了音 ********************************/
void ohuro(){
  tone(0, 806.964, 250); //ソ
  delay(250);
  tone(0, 718.923, 250);  //ファ
  delay(250);
  tone(0, 678.573, 500);  //ミ
  delay(500);

  tone(0, 806.964, 250); //ソ
  delay(250);
  tone(0, 1077.164, 250); //ド
  delay(250);
  tone(0, 1016.710, 500); //シ
  delay(500);
  
  tone(0, 806.964, 250);  //ソ
  delay(250);
  tone(0, 1209.079, 250); //レ
  delay(250);
  tone(0, 1077.164, 500); //ド
  delay(500);
  tone(0, 1357.146, 500); //ミ
  delay(500);
  delay(500);

  tone(0, 1077.164, 250); //ド
  delay(250);
  tone(0, 1016.710, 250); //シ
  delay(250);
  tone(0, 905.786, 500); //ラ
  delay(500);

  tone(0, 1437.846, 250); //ファ
  delay(250);
  tone(0, 1209.079, 250); //レ
  delay(250);
  tone(0, 1077.167, 500); //ド
  delay(500);
  tone(0, 1016.710, 500);  //シ
  delay(500);

  tone(0, 1077.164, 1000); //ド
  delay(1000);

}



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

/******************* SD再認識のため ********************/
bool reinitializeSD() {
  SPI.end();               // SPIを完全に停止
  delay(100);
  SPI.begin();             // 再開
  delay(100);
  // 1. SD.begin()が失敗したら、まずカードはない
  if (!SD.begin(SD_CS)) {
    return false;
  }

  // 2. ダミーファイルを開いてみる
  File testFile = SD.open("/_test.tmp", FILE_WRITE);
  
  if (!testFile) {
    // ファイルが正常に開けない = カードが物理的にないか、故障している
    return false;
  }
  
  // 3. テストが成功したら、後始末をしてtrueを返す
  testFile.close();
  SD.remove("/_test.tmp"); // 作成したダミーファイルを削除
  
  return true;
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

float celsius = 1;
float smoothed_celsius = 23.0; //平滑化後の温度 ←直温度取得1回目の値で初期化した方がいいか？
int duty = 0; //デューティー比 
float PWM_OUT_V; //PWM制御による平均出力電圧
int i;
int under_temp_V , over_temp_V = 0; //Voltageのtemp_V配列内近似値　温度低い方 = under_temp_V, 温度高い方 = over_temp_V，最近似値 = neaest_temp_V
int Voltage;
int num_temp_V = 0;
int Pin_thermistor_num = 0;

int left = 0;
int right = 1495; // 要素数-1
int mid;



/*
//↓PID用
const float P =2; //Pゲイン
const float I =9; //Iゲイン
const float D =0; //Dゲイン
float target_temp = 7.0; //目標温度
float diff_temp_before = 0.0; //微分初期値 ←計測1回目の目標との差[℃]に変更する
float diff_temp = 0.0; //目標温度との最新の差
float T = 0.01; //PID制御周期
float I_diff_temp = 0.0; //I要素（積分）
float D_diff_temp = 0.0; //D要素（微分）
float u = 0.0; //PIDからの出力[%]
//↑
*/

const float VCC = 3.3;         // Picoの電源電圧
const float R_FIXED = 1000.0; // 回路に接続した1kΩの固定抵抗
//サーミスタの抵抗値が1kΩになる約84.5℃で最も精度が高くなる

// ----- サーミスタの特性 -----
const float R0_THERMISTOR = 10000.0; // 仕様温度1における抵抗値 (10kΩ)
const float T0_THERMISTOR_C = 25.0;  // 仕様温度1 (25℃)
const float B_CONSTANT = 4126.0;     // B定数

// 摂氏をケルビンに変換するための定数
const float KELVIN_OFFSET = 273.15;

float get_celsius(int Pin_thermistor_num){
  int adcRaw = analogRead(Pin_thermistor); //0~3.3Vを0~4095に分割した値

  // 2. ADCの値を電圧に変換する
  //    (float)adcRaw とすることで浮動小数点数として計算
  float vOut = (float)adcRaw * VCC / 4095.0;

  // 3. 電圧値からサーミスタの現在の抵抗値を計算する
  //    分圧回路の計算式 Vout = Vcc * R2 / (R1 + R2) を R2 について解く
  //    R1 = R_FIXED, R2 = rThermistor
  float rThermistor = R_FIXED * vOut / (VCC - vOut);

  // 4. B定数の計算式を使い、抵抗値から温度（ケルビン）を算出
  //    1/T = 1/T0 + (1/B) * ln(R/R0)
  float t0Kelvin = T0_THERMISTOR_C + KELVIN_OFFSET;
  float tempKelvin = 1.0 / ( (1.0 / t0Kelvin) + (1.0 / B_CONSTANT) * log(rThermistor / R0_THERMISTOR) );
  
  // 5. 温度をケルビンから摂氏（℃）に変換
  return tempKelvin - KELVIN_OFFSET;
}


//指数移動平均
float get_smoothed_celsius(float celsius, float smoothed_celsius){
  const float alpha = 0.3; // スムージング係数（0 < α < 1）
  Serial.print("前平滑温度:");
  Serial.println(smoothed_celsius);
  //Serial.print(", ");
  return (alpha * celsius + (1 - alpha) * smoothed_celsius);
}



/********************* ただの制御 ******************/
//void control_SSR(){





//}



//PWM制御
/*
float get_PWM_OUT_V(float smoothed_celsius){
  analogWriteFreq(25000);
  analogWriteRange(256);
  duty = map(smoothed_celsius, -15, 50, 0, 255);
  duty = constrain(duty, 0, 255);
  analogWrite(Pin_Peltier, duty); //dutyは0~255の値
  return (3.3*((float)duty / 255));
}
*/



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////





void setup(){
  analogReadResolution(12);
  Serial.begin(115200);
  pinMode(Pin_thermistor, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}


void loop(){
  Pin_thermistor_num = Pin_thermistor; //温度を読みたいサーミスタのピン番号を代入，サーミスターを何個か並列使用するときのためにPin_thermistor_numに代入するようにした
  celsius = get_celsius(Pin_thermistor_num);
  smoothed_celsius = get_smoothed_celsius(celsius, smoothed_celsius);

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
      menu(smoothed_celsius);
      break;
    case START_CONFIRM:
      start_confirm(smoothed_celsius);
      break;
    case RECORDING:
      recording(smoothed_celsius);
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
