
//#include <M5GFX.h>
//#include <LGFX_8BIT_CVBS.h>

//static LGFX_8BIT_CVBS display;

// Play a file from the SD card in looping mode, from the SD card.
// Example program to demonstrate the use of the MIDFile library
//
// Hardware required:
//  SD card interface - change SD_SELECT for SPI comms

#include <Arduino.h>
#include <FS.h>
#include <SdFat.h>
#include <MD_MIDIFile.h>
#include <M5Unified.h>
#include <ServoEasing.hpp>

#include <Avatar.h>  // https://github.com/meganetaaan/m5stack-avatar.git

using namespace m5avatar;
Avatar avatar;

// in the main file define the max SPI speed
#define SPI_SPEED SD_SCK_MHZ(20)                        // MHz: OK 4, 10, 20, 25  ->  too much: 29, 30, 40, 50 causes errors
#define SD_CONFIG SdSpiConfig(4, SHARED_SPI, SPI_SPEED) // TFCARD_CS_PIN is defined in M5Stack Config.h (Pin 4)

// M5Stack Core2用のサーボの設定
// Port.A X:G33, Y:G32
// スタックチャン基板 X:G19, Y:G27
#define SERVO_PIN_X 33
#define SERVO_PIN_Y 32

int servo_offset_x = 0;  // X軸サーボのオフセット（90°からの+-で設定）
int servo_offset_y = 0;  // Y軸サーボのオフセット（90°からの+-で設定）

#define START_DEGREE_VALUE_X 90
#define START_DEGREE_VALUE_Y 90

ServoEasing servo_x;
ServoEasing servo_y;

TaskHandle_t taskHandle;

#define USE_MIDI  1  // set to 1 for MIDI output, 0 for debug output

#if USE_MIDI // set up for direct MIDI serial output

#define DEBUGS(s)
#define DEBUG(s, x)
#define DEBUGX(s, x)
#define SERIAL_RATE 31250

#else // don't use MIDI to allow printing debug statements

#define DEBUGS(s)     do { Serial.print(s); } while(false)
#define DEBUG(s, x)   do { Serial.print(F(s)); Serial.print(x); } while(false)
#define DEBUGX(s, x)  do { Serial.print(F(s)); Serial.print(x, HEX); } while(false)
#define SERIAL_RATE 115200

#endif // USE_MIDI

// SD chip select pin for SPI comms.
// Default SD chip select is the SPI SS pin (10 on Uno, 53 on Mega).
const uint8_t SD_SELECT = SS;

#define ARRAY_SIZE(a) (sizeof(a)/sizeof((a)[0]))

// The files in the tune list should be located on the SD card
// or an error will occur opening the file and the next in the
// list will be opened (skips errors).
const char *loopfile = "RYDEEN.MID";  // simple and short file

SDFAT	SD;
MD_MIDIFile SMF;

int masterVolume = 64;

void midiCallback(midi_event *pev)
// Called by the MIDIFile library when a file event needs to be processed
// thru the midi communications interface.
// This callback is set up in the setup() function.
{
#if USE_MIDI
  if ((pev->data[0] >= 0x80) && (pev->data[0] <= 0xe0))
  {
    Serial2.write(pev->data[0] | pev->channel);
    Serial2.write(&pev->data[1], pev->size-1);
  }
  else
    Serial2.write(pev->data, pev->size);
#endif
  DEBUG("\nM T", pev->track);
  DEBUG(":  Ch ", pev->channel+1);
  DEBUGS(" Data");
  for (uint8_t i=0; i<pev->size; i++)
  {
    DEBUGX(" ", pev->data[i]);
  }
}

void setMasterVolume(int volume){
    Serial2.write(0xB0);
    Serial2.write(0x63);
    Serial2.write(0x37);

    Serial2.write(0xB0);
    Serial2.write(0x62);
    Serial2.write(0x07);

    Serial2.write(0xB0);
    Serial2.write(0x06);
    Serial2.write(volume);
}

void moveX(int x, uint32_t millis_for_move = 0) {
  if (millis_for_move == 0) {
    servo_x.easeTo(x + servo_offset_x);
  } else {
    servo_x.easeToD(x + servo_offset_x, millis_for_move);
  }
}

void moveY(int y, uint32_t millis_for_move = 0) {
  if (millis_for_move == 0) {
    servo_y.easeTo(y + servo_offset_y);
  } else {
    servo_y.easeToD(y + servo_offset_y, millis_for_move);
  }
}

void moveXY(int x, int y, uint32_t millis_for_move = 0) {
  if (millis_for_move == 0) {
    servo_x.setEaseTo(x + servo_offset_x);
    servo_y.setEaseTo(y + servo_offset_y);
  } else {
    servo_x.setEaseToD(x + servo_offset_x, millis_for_move);
    servo_y.setEaseToD(y + servo_offset_y, millis_for_move);
  }
  // サーボが停止するまでウェイトします。
  synchronizeAllServosStartAndWaitForAllServosToStop();
}

void moveRandom(void *) {
  for (;;) {
    // ランダムモード
    int x = random(45, 135);  // 45〜135° でランダム
    int y = random(60, 90);   // 50〜90° でランダム

    // M5.update();
    // if (M5.BtnC.wasPressed()) {
    //   break;
    // }

    int delay_time = random(10);
    moveXY(x, y, 1000 + 100 * delay_time);
    delay(2000 + 500 * delay_time);

    delay(1);
  }
}

void setup(void)
{
  int  err;

  auto cfg = M5.config();   // 設定用の情報を抽出
  cfg.output_power = true;  // Groveポートの出力をしない
  M5.begin(cfg);            // M5Stackをcfgの設定で初期化

  avatar.init();
  avatar.setBatteryIcon(false);

  Serial2.begin(SERIAL_RATE);

  DEBUGS("\n[MidiFile Looper]");

  if (!SD.begin(SD_CONFIG)) {
      DEBUGS("\nSD init fail!");
      while (true) ;
  }

  // Initialize MIDIFile
  SMF.begin(&SD);
  SMF.setMidiHandler(midiCallback);
  SMF.looping(true);

  // use the next file name and play it
  DEBUG("\nFile: ", loopfile);
  err = SMF.load(loopfile);
  if (err != MD_MIDIFile::E_OK)
  {
    DEBUG("\nSMF load Error ", err);
    while (true);
  }

  if (servo_x.attach(SERVO_PIN_X, START_DEGREE_VALUE_X + servo_offset_x,
                     DEFAULT_MICROSECONDS_FOR_0_DEGREE,
                     DEFAULT_MICROSECONDS_FOR_180_DEGREE)) {
    //Serial.print("Error attaching servo x");
  }
  if (servo_y.attach(SERVO_PIN_Y, START_DEGREE_VALUE_Y + servo_offset_y,
                     DEFAULT_MICROSECONDS_FOR_0_DEGREE,
                     DEFAULT_MICROSECONDS_FOR_180_DEGREE)) {
    //Serial.print("Error attaching servo y");
  }
  servo_x.setEasingType(EASE_QUADRATIC_IN_OUT);
  servo_y.setEasingType(EASE_QUADRATIC_IN_OUT);
  setSpeedForAllServos(60);

  xTaskCreatePinnedToCore(moveRandom,
                          "servoTask",
                          4096,
                          nullptr,
                          2,
                          &taskHandle,
                          PRO_CPU_NUM);

  setMasterVolume(masterVolume);
}

void loop(void)
{
  M5.update();
  if(M5.BtnC.wasClicked()){
    SMF.pause(false);
    SMF.restart();
    //reset midi module by using 0x01, 0xFF
    Serial2.write(0x01);
    Serial2.write(0xFF);
  }else if(M5.BtnA.wasClicked()){
    if(masterVolume > 10){
      masterVolume = masterVolume - 5;
      setMasterVolume(masterVolume);
      log_i("test1");
    }
  }else if(M5.BtnB.wasClicked()){
    if(masterVolume < 126){
      masterVolume = masterVolume + 5;
      setMasterVolume(masterVolume);
      log_i("test2");
    }
  }

  // play the file
  if (!SMF.isEOF())
  {
    SMF.getNextEvent();
  }else{
    ESP.restart();
  }

  delay(1);
}
