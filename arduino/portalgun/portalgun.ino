#include <Wire.h> // Include the I2C library (required)
// for the sound board
#include <SoftwareSerial.h>
#include "Adafruit_Soundboard.h"
#include <Adafruit_NeoPixel.h>
#include <Adafruit_DRV2605.h> //haptic driver board

// rumble motor pin (transistor on D3) - circuit not yet built, reserved for future
// #define RUMBLE_MOTOR 3

// pins for 3 red leds and lights in buttons
int Leds = A0;
int BbtnLed = A1;
int ObtnLed = A2;
int WbtnLed = A3;  // white cancel button LED

// neopixel pins / setup
#define NEO_PIXELS 2
Adafruit_NeoPixel NeoPixels = Adafruit_NeoPixel(31, NEO_PIXELS, NEO_GRB + NEO_KHZ800);

// Index numbers for the LEDs in the neopixels. since i used 2 jewels for the center, one ring for the end, and a single for the indicator, 
//i set them as below. if you use a different number or type of pixels, change these numbers accordingly. the indicator light is first in the chain
//so it is number 0
int centerStart = 1;
int centerEnd = 14;
int ringStart = 15;
int ringEnd = 30;

// inputs for switches and buttons
const int SONGBTN = 5;
const int POWERSW = 6;
const int BLUEBTN = 7;
const int ORANGEBTN = 8;
const int CANCELBTN = 9;

// soundboard pins and setup
#define SFX_RST 10
#define SFX_RX 11
#define SFX_TX 12
const int ACT = 13;    // this allows us to know if the audio is playing

SoftwareSerial ss = SoftwareSerial(SFX_TX, SFX_RX);
Adafruit_Soundboard sfx = Adafruit_Soundboard( &ss, &Serial, SFX_RST);

// Possible states
bool Firing = false;    // firing animation is going
bool Portal = false;  // is there an open portal
bool Power = false; // power on or off
bool Song = false; //playing the song

// audio track names on soundboard
char powerup[] =              "T00     WAV";
char hum[] =                  "T01     WAV";
char bluefire[] =             "T02     WAV";
char orangefire[] =           "T03     WAV";
char cancelportal[] =         "T04     WAV";
char powerdown[] =            "T05     WAV";
char alivesong[] =            "T06     OGG";

//Adafruit DRV2605 setup for haptic motor feedback
Adafruit_DRV2605 drv;
bool drvReady = false;  // tracks whether DRV2605 initialized successfully

// Arduino setup function
void setup() {
  Serial.begin(9600);
  Serial.println("Setup starting...");

  // softwareserial MUST start before soundboard reset
  // so we can catch the boot string
  ss.begin(9600);
  Serial.println("SoftwareSerial started");

  // now reset the soundboard - library will listen for boot string
  Serial.println("Testing soundboard...");
  if (!sfx.reset()) {
    Serial.println("Soundboard NOT responding - check TX/RX wiring and UG to GND");
  } else {
    Serial.println("Soundboard OK");
  }
  pinMode(ACT, INPUT);
  pinMode(Leds, OUTPUT);
  pinMode(BbtnLed, OUTPUT);
  pinMode(ObtnLed, OUTPUT);
  pinMode(WbtnLed, OUTPUT);

  // configure neo pixels
  NeoPixels.begin();
  NeoPixels.setBrightness(255);
  NeoPixels.show(); // Initialize all pixels to 'off'

  // set the modes for the switches/buttons
  pinMode(SONGBTN, INPUT_PULLUP);
  pinMode(POWERSW, INPUT_PULLUP);
  pinMode(BLUEBTN, INPUT_PULLUP);
  pinMode(ORANGEBTN, INPUT_PULLUP);
  pinMode(CANCELBTN, INPUT_PULLUP);

  // Adafruit DRV2605 setup for haptic motor feedback
  // only initialize if the chip is present on I2C
  if (drv.begin()) {
    drvReady = true;
    drv.selectLibrary(1);
    drv.setMode(DRV2605_MODE_INTTRIG);
    Serial.println("DRV2605 ready");
  } else {
    Serial.println("DRV2605 not found - skipping haptic");
  }
  // raw serial test - send L command and print whatever comes back
  Serial.println("Testing soundboard raw...");
  ss.println("L");
  delay(500);
  String response = "";
  while (ss.available()) {
    response += (char)ss.read();
  }
  if (response.length() > 0) {
    Serial.print("Soundboard replied: ");
    Serial.println(response);
  } else {
    Serial.println("No response from soundboard");
  }
  Serial.println("Setup complete");
}

/* ************* Haptic Board Helper Functions ************ */

// fire: strong jolt, then stacked buzz sustain, then fade down
void hapticFire() {
  if (!drvReady) return;
  drv.setWaveform(0, 1);   // strong click 100% - initial jolt
  drv.setWaveform(1, 47);  // buzz 100%
  drv.setWaveform(2, 47);  // buzz 100% - stacked for sustain
  drv.setWaveform(3, 47);  // buzz 100% - stacked for sustain
  drv.setWaveform(4, 49);  // buzz 60% - begin fade
  drv.setWaveform(5, 50);  // buzz 40%
  drv.setWaveform(6, 51);  // buzz 20% - fade out
  drv.setWaveform(7, 0);   // end
  drv.go();
}

// power on/off: single strong click
void hapticPower() {
  if (!drvReady) return;
  drv.setWaveform(0, 1);   // strong click 100%
  drv.setWaveform(1, 0);   // end
  drv.go();
}

// cancel: long double sharp click
void hapticCancel() {
  if (!drvReady) return;
  drv.setWaveform(0, 37);  // long double sharp click strong 100%
  drv.setWaveform(1, 0);   // end
  drv.go();
}



// rumble motor kick - reserved for future transistor circuit on D3
// void rumbleKick() {
//   analogWrite(RUMBLE_MOTOR, 255);
//   delay(150);
//   analogWrite(RUMBLE_MOTOR, 80);
// }


/* ************* Audio Board Helper Functions ************* */
// helper function to play a track by name on the audio board
void playAudio( char* trackname, int playing ) {
  // stop track if one is going
  if (playing == 0) {
    sfx.stop();
  }

  // now go play
  if (sfx.playTrack(trackname)) {
    sfx.unpause();
  }
}


/* ************* Main Loop ************* */

void loop() {
  // find out of the audio board is playing audio
  int playing = digitalRead(ACT);

  // get the current switch states
  int SongBtn = digitalRead(SONGBTN);

  // if you press song button play song
  if (SongBtn == 0 && Power == true) {
    if (Song == false) {
      sfx.stop();
      delay(200);
      boolean result = sfx.playTrack("T06     OGG");
      Serial.print("Song play result: ");
      Serial.println(result ? "success" : "failed");
      Song = true;
    }
  } else {
    Song = false;
  }

  int PowerSw = digitalRead(POWERSW);
  int BlueBtn = digitalRead(BLUEBTN);
  int OrangeBtn = digitalRead(ORANGEBTN);
  int CancelBtn = digitalRead(CANCELBTN);

  // debug pin states every 2 seconds
  static unsigned long lastDebug = 0;
  if (millis() - lastDebug > 2000) {
    Serial.print("PWR:"); Serial.print(PowerSw);
    Serial.print(" BLU:"); Serial.print(BlueBtn);
    Serial.print(" ORG:"); Serial.print(OrangeBtn);
    Serial.print(" CAN:"); Serial.print(CancelBtn);
    Serial.print(" SONG:"); Serial.print(SongBtn);
    Serial.print(" Power state:"); Serial.println(Power);
    lastDebug = millis();
  }

  // while the startup switch is set on
  if (PowerSw == 0) {
    // play the hum sound when on and idle - loops continuously as background
    if (playing == 1 && Power == true) {
      playAudio(hum, playing);
      delay(100);
    }
  }

  // if we are not powered up we should play the power up sound and begin the loop
  if (PowerSw == 0 && Power == false) {
    Serial.println("Power ON triggered");
    Power = true;
    analogWrite(Leds, 175);    // turn 3 red leds on
    analogWrite(BbtnLed, 200); // turn blue button led on
    analogWrite(ObtnLed, 200); // turn orange button led on
    analogWrite(WbtnLed, 200); // turn white cancel button led on
    playAudio(powerup, playing);
    hapticPower();
  }

  // if we are powered up and turn the power switch off then power down
  if (PowerSw == 1 && Power == true) {
    Power = false;
    playAudio(powerdown, playing);
    hapticPower();
    analogWrite(Leds, 0);    // turn 3 red leds off
    analogWrite(BbtnLed, 0); // turn blue button led off
    analogWrite(ObtnLed, 0); // turn orange button led off
    analogWrite(WbtnLed, 0); // turn white cancel button led off
    setLightsState(2); //set blue or orange light to off
  }

  // Fire Blue
  if (BlueBtn == 0 && Power == true && Firing == false) {
    Serial.println("Blue fire triggered");
    Firing = true;
    Portal = true;
    playAudio(bluefire, playing);
    hapticFire();
    setLightsState(1); //set light to blue

  } else {
    if (CancelBtn == 1 && BlueBtn == 1 && OrangeBtn == 1 && Firing == true)
      Firing = false;
  }

  // Fire Orange
  if (OrangeBtn == 0 && Power == true && Firing == false) {
    Serial.println("Orange fire triggered");
    Firing = true;
    Portal = true;
    playAudio(orangefire, playing);
    hapticFire();
    setLightsState(0); //set lights to orange

  } else {
    if (CancelBtn == 1 && BlueBtn == 1 && OrangeBtn == 1 && Firing == true)
      Firing = false;
  }


  // Cancel Portal
  if (CancelBtn == 0 && Power == true && Firing == false && Portal == true) {
     Serial.println("White cancel triggered");   Firing = true;
    Portal = false;
    playAudio(cancelportal, playing);
    hapticCancel();
    setLightsState(2); //set light to off

  } else {
    if (CancelBtn == 1 && BlueBtn == 1 && OrangeBtn == 1 && Firing == true)
      Firing = false;
  }
}


/* ************* Lights States************* */
void setLightsState(int state)
{
  switch ( state )
  {

    case 0: // orange fire
      for (int d = 25, b = 25; (d > 0) && (b < 50); d--, b++) //starts at 50 percent, center light dims then brightens, end light brightens then dims
      {
        for (int j = centerStart; j <= centerEnd; j++)
        {
          NeoPixels.setPixelColor(j, NeoPixels.Color(d * 5, d, 0)); //i get orange to dim properly by setting green to a number and setting red to 5 times what green is
        }
        {
          for (int k = ringStart; k <= ringEnd; k++)
          {
            NeoPixels.setPixelColor(k, NeoPixels.Color(b * 5, b, 0));
          }
          NeoPixels.show();
          delay(0);
        }
      }
      for (int d = 0, b = 50; (d < 25) && (b > 25); d++, b--) {
        for (int j = centerStart; j <= centerEnd; j++) {
          NeoPixels.setPixelColor(j, NeoPixels.Color(d * 5, d, 0));
        }

        {
          for (int k = ringStart; k <= ringEnd; k++) {
            NeoPixels.setPixelColor(k, NeoPixels.Color(b * 5, b, 0));
          }

          NeoPixels.setPixelColor(0, NeoPixels.Color(255, 50, 0));
          NeoPixels.show();
          delay(20);
        }
      }
      break;

    case 1: // blue fire

      for (int d = 128, b = 128; (d > 0) && (b < 255); d--, b++)// same as orange above, except you just have to set blue between half and full
      {
        for (int j = centerStart; j <= centerEnd; j++)
        {
          NeoPixels.setPixelColor(j, NeoPixels.Color(0, 0, d));
        }
        {
          for (int k = ringStart; k <= ringEnd; k++)
          {
            NeoPixels.setPixelColor(k, NeoPixels.Color(0, 0, b));
          }
          NeoPixels.setPixelColor(0, 0, 0, 255);
          NeoPixels.show();
          delay(0);
        }
      }
      for (int d = 0, b = 255; (d < 128) && (b > 128); d++, b--) {
        for (int j = centerStart; j <= centerEnd; j++) {
          NeoPixels.setPixelColor(j, NeoPixels.Color(0, 0, d));
        }

        {
          for (int k = ringStart; k <= ringEnd; k++) {
            NeoPixels.setPixelColor(k, NeoPixels.Color(0, 0, b));
          }

          NeoPixels.setPixelColor(0, NeoPixels.Color(0, 0, 255));
          NeoPixels.show();
          delay(4);
        }
      }
      break;


    case 2: // set led off
      for (int j = 0; j < NeoPixels.numPixels(); j++) {
        NeoPixels.setPixelColor(j, 0);
      }
      break;
  }
  NeoPixels.show();
}
