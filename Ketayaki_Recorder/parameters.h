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
// #define sound 0 // 他励式ブザー用

bool spk_flag = false;
int spk_pin = 0;
int spk_freq = 0;
int spk_dur = 0;

bool ohuro_flag = false;

const int Pin_thermistor = 26; // サーミスタ用
float smoothed_celsius = 0.0; //平滑化後の温度 ←直温度取得1回目の値で初期化

//各種計算用変数
unsigned long last_time_stamp_ms = 0;
unsigned long tft_time_stamp_ms = 0;
unsigned long time_start_ms = 0;   //開始時刻
unsigned long time_duration_s = 0;   //経過時間
unsigned long time_hour = 0;   //経過時間・残り時間計算用(ディスプレイ表示用)
unsigned long time_minute = 0;
unsigned long time_second = 0;
int timer_index = 0;
int record_index = 0;
double SD_time_loop;    //SD保存用[sec]

float data; //グラフプロット時に使用
int plot_index = 0;
int plot_X;
int plot_Y;
int Refresh_SD;

//bool
bool is_recording = false;  
bool find_SD = false;  //SDカードが認識されているか
bool open_SD = false;  //SDカードの書き込みが終わっているか確認

char LOG_FILE_PATH[32]; // 記録CSV名
char LOG_FILE_NAME[32]; // 記録CSV名
File myFile;    // Fileクラスのインスタンス
int file_index = 0;
//SD保存用の配列
float plot[280];
int num_loop = 0; //core0のloopが回った回数を数える


