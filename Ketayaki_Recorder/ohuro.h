#include <Arduino.h>

/******************** 焼き終了音 ********************************/
void ohuro(int SPEAKER){
  tone(SPEAKER, 806.964, 250); //ソ
  delay(250);
  tone(SPEAKER, 718.923, 250);  //ファ
  delay(250);
  tone(SPEAKER, 678.573, 500);  //ミ
  delay(500);

  tone(SPEAKER, 806.964, 250); //ソ
  delay(250);
  tone(SPEAKER, 1077.164, 250); //ド
  delay(250);
  tone(SPEAKER, 1016.710, 500); //シ
  delay(500);
  
  tone(SPEAKER, 806.964, 250);  //ソ
  delay(250);
  tone(SPEAKER, 1209.079, 250); //レ
  delay(250);
  tone(SPEAKER, 1077.164, 500); //ド
  delay(500);
  tone(SPEAKER, 1357.146, 500); //ミ
  delay(500);
  delay(500);

  tone(SPEAKER, 1077.164, 250); //ド
  delay(250);
  tone(SPEAKER, 1016.710, 250); //シ
  delay(250);
  tone(SPEAKER, 905.786, 500); //ラ
  delay(500);

  tone(SPEAKER, 1437.846, 250); //ファ
  delay(250);
  tone(SPEAKER, 1209.079, 250); //レ
  delay(250);
  tone(SPEAKER, 1077.167, 500); //ド
  delay(500);
  tone(SPEAKER, 1016.710, 500);  //シ
  delay(500);

  tone(SPEAKER, 1077.164, 1000); //ド
  delay(1000);

}