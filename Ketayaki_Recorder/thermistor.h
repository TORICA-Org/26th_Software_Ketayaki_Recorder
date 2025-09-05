float smoothed_celsius = 0.0; //平滑化後の温度 ←直温度取得1回目の値で初期化
int duty = 0; //デューティー比 
float PWM_OUT_V; //PWM制御による平均出力電圧
int i;
int num_temp_V = 0;

int left = 0;
int right = 1495; // 要素数-1
int mid;

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
  int adcRaw = analogRead(Pin_thermistor_num); //0~3.3Vを0~4095に分割した値
  //int adcRaw = 2048;

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
float update_smoothed_celsius(int Pin_thermistor_num){
  static bool isFirstRun = false; // 初回処理のフラグ
  float celsius = get_celsius(Pin_thermistor_num);
  if (isFirstRun == false) { // 初回処理
    smoothed_celsius = celsius;
  }
  else { // 2回目以降の処理
    const float alpha = 0.3; // スムージング係数（0 < α < 1）
    smoothed_celsius = (alpha * celsius + (1.0 - alpha) * smoothed_celsius);
    isFirstRun = true;
  }
  Serial.print("前平滑温度:");
  Serial.println(smoothed_celsius);
  return smoothed_celsius;
}