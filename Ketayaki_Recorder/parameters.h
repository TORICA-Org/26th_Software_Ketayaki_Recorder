#include <Arduino.h>

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
float smoothed_celsius = 0.0; //平滑化後の温度 ←直温度取得1回目の値で初期化

//各種計算用変数
unsigned long time_start_ms = 0;   //開始時刻
unsigned long time_duration_s = 0;   //経過時間
unsigned long time_hour = 0;   //経過時間・残り時間計算用(ディスプレイ表示用)
unsigned long time_minute = 0;
unsigned long time_second = 0;
double SD_time_loop;    //SD保存用[sec]

float data; //グラフプロット時に使用
int plot_index = 0;
int plot_X;
int plot_Y;
int Refresh_SD;

//bool
bool is_recording = false;  //リフローが始まっているか
bool find_SD = false;   //SDカードが認識されているか
bool open_SD = false;  //SDカードの書き込みが終わっているか確認
bool SSR_ON = false;   // ランプ点灯状態格納用

//ディスプレイのタッチ誤認識でautoリフローが止まらないように段階を設ける(「私はロボットではありません」に近いことをしたい)
//未実装
int no1 = 0; //Emargencyの計算用
int no2 = 0;
int no3 = 0;
int no_calc = 0;


#define SD_FILENAME "/Reflow_Tempdata.csv"
File myFile;    // Fileクラスのインスタンス
//SD保存用の配列
float plot[300];
float backup[1024];
int num_loop = 0; //core0のloopが回った回数を数える

enum PAGE_ENUM { // ページ用列挙型
  WELCOME,
  MENU,
  START_CONFIRM,
  RECORDING,
  GRAPH,
  STOP_CONFIRM
};
PAGE_ENUM page = WELCOME;

