//PS2 to Amiga CD32 keyboard translator v 1.3
//Keyboard: Perixx, Model No.:PERIBOARD-409, Part No.:TK525P
//Keyboard: NEW3P, Model No.:AC-59
//Left GUI (Windows key) = Left Amiga key
//Menu key = Right Amiga key
//Home key = Help key
//ctrl + alt + home   = reset keyboard
//ctrl + alt + delete = hard reset Amiga

//To do (not required):
//add Amiga "soft reset".
//add ERROR code after resync (except resync after start) and send last known key code.

#include <avr/wdt.h>

//PS2KeyAdvanced.h modified library! - do not use the original one!
//keep the library files directly in the sketch directory
#include "PS2KeyAdvanced.h"

//#define ATMEGA32 //uncomment for ATmega32, comment for ATmega328P
#define NOREPEATALL //The Amiga itself repeats the last key until this one is received with the "released" flag
//#define SERIALDEBUGGER
//#define ISR1DEBUGGER
//#define ISR2DEBUGGER
//#define NOACKDEBUGGER

//You need two 4k7 resistors
//and two diodes with a very low voltage drop, maximum 0.3V (eg.: BAS85-GS08).

//Arduino Leonardo
//DFRobot Beetle Board - compatible with Arduino Leonardo.
                           // +                                                <-  Pin 4 (Amiga 6-Pin Mini-DIN) & PS2 keyboard Vcc
                           // -                                                <-> Pin 3 (Amiga 3-Pin Mini-DIN) & PS2 keyboard gnd
#if defined(ATMEGA32)
  #define DATAPIN   11 //     D11                                              <-> PS2 keyboard data line
  #define IRQPIN     3 // int SCL                                              <-> PS2 keyboard clock line
  #define HANDSHAKE  2 // int SDA & 4k7 (pullup to vcc) & anode schottky (SD2) <-> Pin 1 (CD32 keyboard Mini-DIN) keyboard data line
  #define KCLK      10 //     D10 to cathode schottky (SD1)
  #define KDAT       9 //     D9 to cathode schottky (SD2)
  #define KCLKLOW    0 // int RX & 4k7 (pullup to vcc) & anode schottky (SD1)  <-> Pin 5 (CD32 keyboard Mini-DIN) keyboard clock line
  #define LED       13 //                                                       -> PS2 keyboard CapsLock LED
#else
//Arduino UNO nano
//mini ultra, china clone https://pl.aliexpress.com/item/1005007492500542.html select Tools->Processor->ATmega328P (Old Bootloader)
  #define DATAPIN    5 //     D5                                               <-> PS2 keyboard data line
  #define IRQPIN     3 // int D3                                               <-> PS2 keyboard clock line
  #define KCLK       8 //     D8 to cathode schottky (SD1)
  #define KCLKLOW    7 // int D7 & 4k7 (pullup to vcc) & anode schottky (SD1)  <-> Pin 5 (CD32 keyboard Mini-DIN) keyboard clock line
  #define KDAT      10 //     D10 to cathode schottky (SD2)
  #define HANDSHAKE  9 // int D9 & 4k7 (pullup to vcc) & anode schottky (SD2)  <-> Pin 1 (CD32 keyboard Mini-DIN) keyboard data line
  #define LED        4 //     D4                                                -> PS2 keyboard CapsLock LED
#endif
const uint16_t    clockDelayFalling  = 5;   //us
const uint16_t    clockLowTime       = 5;   //us
const uint16_t    clockDelayRising   = 10;  //us
const uint16_t    maxWaitForACK      = 300; //ms
unsigned long     keySentTime        = 0;   //ms
unsigned long     resetFromAmigaTime = 0;   //ms
const uint16_t    powerUpKeyStream   = 0xFD;
const uint16_t    terminateKeyStream = 0xFE;
const uint16_t    lostSyncCode       = 0xF9;
uint16_t          keystroke          = 0;
volatile bool     amigaACK           = false;
volatile bool     reSyncInProgress   = false;
volatile bool     reSyncFinal        = false;
volatile bool     internalTestPassed = false;
volatile bool     resetRequest       = false;
volatile uint8_t  ledState           = 0;
volatile uint16_t codeToSend         = 0;
uint16_t          lastCode           = 0;

const char* firmwareRevision         = "1.3";
PS2KeyAdvanced keyboard;

void setup()
{
  pinMode(KCLKLOW,   INPUT_PULLUP);
  pinMode(HANDSHAKE, INPUT_PULLUP);
  pinMode(IRQPIN,    INPUT_PULLUP);
  pinMode(KCLK,      OUTPUT);
  pinMode(KDAT,      OUTPUT);
  pinMode(LED,       OUTPUT);

#if defined(ATMEGA32)
  detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
  detachInterrupt(digitalPinToInterrupt(KCLKLOW));
#else
  bitSet(PCICR, PCIE2);      // Enable PCINT for D port
  bitClear(PCMSK2, PCINT23); // Disable D7 KCLKLOW
  bitSet(PCICR, PCIE0);      // Enable PCINT for B port
  bitClear(PCMSK0, PCINT1);  // Disable D9 HANDSHAKE
#endif

  digitalWrite(KCLK, HIGH);
  digitalWrite(KDAT, HIGH);
  digitalWrite(LED,  HIGH);

#if defined(SERIALDEBUGGER) || defined (ISR1DEBUGGER)
  Serial.begin(250000);
#endif

  keyboard.begin(DATAPIN, IRQPIN);

  while (!internalTestPassed)
  {
    keyboard.echo(); //ping keyboard
    delay(6);
    keystroke = keyboard.read();

    if((keystroke & 0xFF) == PS2_KEY_ECHO || (keystroke & 0xFF) == PS2_KEY_BAT)
    {
    #if defined(SERIALDEBUGGER)
      Serial.println("OK_KEYBOARD");
      Serial.flush();
    #endif
      internalTestPassed = true;
    }
    else if((keystroke & 0xFF) == 0)
    {
    #if defined(SERIALDEBUGGER)
      Serial.println("NO_KEYBOARD");
      Serial.flush();
    #endif
    }
    else
    {
    #if defined(SERIALDEBUGGER)
      Serial.print("INVALID_CODE:0x");
      Serial.println(keystroke, HEX);
      Serial.flush();
    #endif
    }
  }

  keyboard.setNoRepeat(1);
  delay(6);
  
#if defined(SERIALDEBUGGER)
  Serial.print("VER:");
  Serial.println(firmwareRevision);
  Serial.println("INIT_END");
  Serial.flush();
#endif

  digitalWrite(LED, LOW);

  wdt_reset();
  wdt_enable(WDTO_1S);
  while (digitalRead(KCLKLOW) == 0) {delay(1);}
  wdt_reset();
  wdt_disable();

#if defined(ATMEGA32)
  attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);
#else
  bitSet(PCMSK2, PCINT23); // Enable D7 KCLKLOW
#endif

  sei();
}

void loop( )
{
#if defined(NOACKDEBUGGER)
  amigaACK = true;
#endif

  if (!amigaACK && millis() > keySentTime + maxWaitForACK)
  {
  #if defined(ATMEGA32)
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
  #else
    bitClear(PCMSK0, PCINT1);  // Disable D9 HANDSHAKE
  #endif

    reSyncInProgress = true;
    reSyncFinal      = false;
    Resync();
  }
  else if (amigaACK && reSyncInProgress)
  {
    if (!reSyncFinal) 
    {
      codeToSend = powerUpKeyStream << 1;
      SendKeyToAmiga(codeToSend);
      reSyncFinal = true;
    }
    else 
    {
      codeToSend = terminateKeyStream << 1;
      SendKeyToAmiga(codeToSend);
      reSyncInProgress = false;
      reSyncFinal      = false;
      digitalWrite(LED, LOW);
    }
  }
  else if (amigaACK && keyboard.available())
  {
    keystroke = keyboard.read();

    #if defined(SERIALDEBUGGER) 
    Serial.print("RAW:0x");     
    Serial.println(keystroke, HEX);
    #endif

    if (keystroke == 0x2846 || keystroke == 0x3846) // ctrl + alt + delete
    {
      ResetAmiga(false); //true = soft (ToDo), false = hard
    }

    if (keystroke == 0x295F || keystroke == 0x395F) // ctrl + alt + home
    {
      keyboard.resetKey();
    }

    if (keystroke > 0 && (keystroke & 0xFF) < 0x80)
    {
      uint16_t upDownFlag = keystroke >> 15;
      uint16_t keyNr = keystroke & 0xFF;

      if (keyNr == PS2_KEY_CAPS)
      { 
        if (upDownFlag == 0) 
        { 
          keyboard.setLock(4); //CapsLock LED ON
        }
        else 
        {
          keyboard.setLock(0); //CapsLock LED OFF
        }

        delay(100); // needed to prevent the keyboard from freezing when the CapsLock key is pressed quickly and frequently
        
        // Instead of the keyboard LED, we can also use the Beetle LED
        //digitalWrite(LED, !upDownFlag);
      }

      if (keyNr == 0x70) keyNr = 0;

      keyNr = keyNr << 1;
      keyNr = keyNr | upDownFlag;

      #if defined(SERIALDEBUGGER)      
      Serial.println(keyNr);
      #endif
      
      #if defined(NOREPEATALL)
      if (keyNr != lastCode)
      {
        lastCode = keyNr;
        SendKeyToAmiga(keyNr);
      }
      #else
      SendKeyToAmiga(keyNr);
      #endif
    }
  }
}

void SendKeyToAmiga(uint16_t keyNrToSend)
{
  #if defined(ATMEGA32)
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
    detachInterrupt(digitalPinToInterrupt(KCLKLOW));
  #else
    bitClear(PCMSK2, PCINT23); // Disable D7 KCLKLOW
    bitClear(PCMSK0, PCINT1);  // Disable D9 HANDSHAKE
  #endif

  uint16_t keyNrToSendInverted = ~keyNrToSend;
  amigaACK = false;
  keySentTime = millis();

  for (int bit = 7; bit >= 0; bit--) 
  {
    digitalWrite(KDAT, bitRead(keyNrToSendInverted, bit));
    delayMicroseconds(clockDelayFalling);
    digitalWrite(KCLK, LOW);
    delayMicroseconds(clockLowTime);
    digitalWrite(KCLK, HIGH);
    delayMicroseconds(clockDelayRising);
  }
  digitalWrite(KDAT, HIGH);
  delayMicroseconds(clockLowTime);

  #if defined(ATMEGA32)
    attachInterrupt(digitalPinToInterrupt(HANDSHAKE), ISR1, LOW);
    attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);
  #else
    bitSet(PCMSK2, PCINT23); // Enable D7 KCLKLOW
    bitSet(PCMSK0, PCINT1);  // Enable D9 HANDSHAKE
  #endif
}

void Resync()
{
  #if defined(ATMEGA32)
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
    detachInterrupt(digitalPinToInterrupt(KCLKLOW));
  #else
    bitClear(PCMSK2, PCINT23); // Disable D7 KCLKLOW
    bitClear(PCMSK0, PCINT1);  // Disable D9 HANDSHAKE
  #endif

  ledState = ~ledState;
  digitalWrite(LED, ledState);
  amigaACK = false;
  keySentTime = millis();
  digitalWrite(KDAT, LOW);
  delayMicroseconds(clockDelayFalling);
  digitalWrite(KCLK, LOW);
  delayMicroseconds(clockLowTime);
  digitalWrite(KCLK, HIGH);
  delayMicroseconds(clockDelayRising);
  digitalWrite(KDAT, HIGH);
  delayMicroseconds(clockLowTime);
  
  #if defined(ATMEGA32)
    attachInterrupt(digitalPinToInterrupt(HANDSHAKE), ISR1, LOW);
    attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);
  #else
    bitSet(PCMSK2, PCINT23); // Enable D7 KCLKLOW
    bitSet(PCMSK0, PCINT1);  // Enable D9 HANDSHAKE
  #endif
}

void ResetAmiga(bool resetRequest)
{
  #if defined(ATMEGA32)
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
    detachInterrupt(digitalPinToInterrupt(KCLKLOW));
  #else
    bitClear(PCMSK2, PCINT23); // Enable D7 KCLKLOW
    bitClear(PCMSK0, PCINT1);  // Enable D9 HANDSHAKE
  #endif

  bool _resetRequest = resetRequest;  // use this in the future for Amiga "soft reset"

  // AMIGA HARD RESET
  digitalWrite(KCLK, LOW);
  delay(500);
  // KEYBOARD HARD RESET
  wdt_reset();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

#if defined(ATMEGA32)
  void ISR1()
  {
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
    #if defined(ISR1DEBUGGER)
      Serial.println("HANDSHAKE");
      Serial.flush();
    #endif

    wdt_reset();
    wdt_enable(WDTO_1S);
    while (digitalRead(HANDSHAKE) == 0) {}
    wdt_disable();

    amigaACK = true;
  }

  void ISR2() // incoming RESET_FROM_AMIGA
  {
    detachInterrupt(digitalPinToInterrupt(KCLKLOW));
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));

    resetFromAmigaTime = millis();

    int resetTimeCounter = 0;

    while (digitalRead(KCLKLOW) == 0) 
    {
      delay(1);
      resetTimeCounter ++;
    }

    if (resetTimeCounter > 100)
    {
      wdt_reset();
      wdt_enable(WDTO_15MS);
      while (1) {}
    }

    attachInterrupt(digitalPinToInterrupt(HANDSHAKE), ISR1, LOW);
    attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);  
  }
#else
  // KCLKLOW incoming RESET_FROM_AMIGA
  ISR(PCINT2_vect) { // port D: D0-D7

    if (!bitRead(PIND,7)) {
      bitClear(PCMSK2, PCINT23); // Disable D7 KCLKLOW
      bitClear(PCMSK0, PCINT1);  // Disable D9 HANDSHAKE
      resetFromAmigaTime = millis();

      int resetTimeCounter = 0;

      while (digitalRead(KCLKLOW) == 0) 
      {
        delay(1);
        resetTimeCounter ++;
      }

      if (resetTimeCounter > 100)
      {
        wdt_reset();
        wdt_enable(WDTO_15MS);
        while (1) {}
      }

      bitSet(PCMSK2, PCINT23); // Enable D7 KCLKLOW
      bitSet(PCMSK0, PCINT1);  // Enable D9 HANDSHAKE
    }
  }

  // HANDSHAKE
  ISR(PCINT0_vect) { // port B: D8-D13 

    if (!bitRead(PINB,1)) {
      bitClear(PCMSK0, PCINT1);  // Disable D9 HANDSHAKE

      #if defined(ISR1DEBUGGER)
        Serial.println("HANDSHAKE");
        Serial.flush();
      #endif

      wdt_reset();
      wdt_enable(WDTO_1S);
      while (digitalRead(HANDSHAKE) == 0) {}
      wdt_disable();

      amigaACK = true;
    }
  }
#endif
