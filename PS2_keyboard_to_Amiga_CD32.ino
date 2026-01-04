//PS2 to Amiga keyboard translator v 1.0 (tested on CD32)
//Keyboard: Perixx, Model No.:PERIBOARD-409, Part No.:TK525P
//Left GUI (Windows key) = Left Amiga
//Menu key = Right Amiga
//ctrl + alt + home   = reset keyboard
//ctrl + alt + delete = hard reset Amiga

//To do:
//add Amiga "soft reset".
//add ERROR code after resync (except resync after start) and send last known key code.

#include <avr/wdt.h>

//PS2KeyAdvanced.h modified library! - do not use the original one!
//keep the library files directly in the sketch directory
#include "PS2KeyAdvanced.h"

#define NOREPEATALL //The Amiga itself repeats the last key until this one is received with the "released" flag
//#define SERIALDEBUGGER
//#define ISR1DEBUGGER
//#define NOACKDEBUGGER

//DFRobot Beetle Board - compatible with Arduino Leonardo.
//You need two 4k7 resistors
//and two diodes with a very low voltage drop, maximum 0.3V (eg.: BAS85-GS08).
//CapsLock LED:
//Unsolder the LED on the Beetle,
//cut off the CapsLock LED from the keyboard board
//and connect it to the Beetle LED pads.
                     //Beetle Board                                            CD32, PS2 keyboard
                     // +                                                  <- Pin 4 (Amiga 6-Pin Mini-DIN) & PS2 keyboard Vcc
                     // -                                                  <-> Pin 3 (Amiga 3-Pin Mini-DIN) & PS2 keyboard gnd
#define DATAPIN   11 // D11                                                <-> PS2 keyboard data line
#define IRQPIN     3 // SCL                                                <-> PS2 keyboard clock line
#define HANDSHAKE  2 // SDA & 4k7 (pullup to vcc) & anode schottky (SD2)   <-> Pin 1 (CD32 6-Pin Mini-DIN) keyboard data line
#define KCLK      10 // D10 to cathode schottky (SD1)
#define KDAT       9 // D9 to cathode schottky (SD2)
#define KCLKLOW    0 // RX & 4k7 (pullup to vcc) & anode schottky (SD1)     <-> Pin 5 (CD32 6-Pin Mini-DIN) keyboard clock line
#define LED       13 //                                                      -> PS2 keyboard CapsLock LED

const uint16_t    clockDelayFalling  = 10;  //us
const uint16_t    clockLowTime       = 20;  //us
const uint16_t    clockDelayRising   = 30;  //us
const uint16_t    maxWaitForACK      = 144; //ms
unsigned long     keySentTime        = 0;   //ms
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

PS2KeyAdvanced keyboard;

void setup()
{
  detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
  detachInterrupt(digitalPinToInterrupt(KCLKLOW));

  pinMode(HANDSHAKE, INPUT_PULLUP);
  pinMode(KCLKLOW, INPUT_PULLUP);
  pinMode(KCLK, OUTPUT);
  pinMode(KDAT, OUTPUT);
  pinMode(LED, OUTPUT);

  digitalWrite(KCLK, HIGH);
  digitalWrite(KDAT, HIGH);
  digitalWrite(LED, HIGH);

  #if defined(SERIALDEBUGGER || ISR1DEBUGGER)
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
  Serial.println("INIT_END");
  Serial.flush();
  #endif

  digitalWrite(LED, LOW);

  wdt_reset();
  wdt_enable(WDTO_1S);
  while (digitalRead(KCLKLOW) == 0) 
  {
    delay(1);
  }
  wdt_reset();
  wdt_disable();

  EIFR |= (1<<INTF2);
  attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);
}

void loop( )
{
  #if defined(NOACKDEBUGGER) 
  amigaACK = true;
  #endif

  if (!amigaACK && millis() > keySentTime + maxWaitForACK)
  {
    detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
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
        digitalWrite(LED, !upDownFlag);

        //Freezes the keyboard when CapsLock pressed too often!
        /*
        if (upDownFlag == 0) 
        { 
          keyboard.setLock(4); //CapsLock LED ON
        }
        else 
        {
          keyboard.setLock(0); //CapsLock LED OFF
        }
        */
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
  detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
  detachInterrupt(digitalPinToInterrupt(KCLKLOW));
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

  EIFR |= (1<<INTF1);
  EIFR |= (1<<INTF2);
  attachInterrupt(digitalPinToInterrupt(HANDSHAKE), ISR1, LOW);
  attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);
}

void Resync()
{
  detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
  detachInterrupt(digitalPinToInterrupt(KCLKLOW));
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
  
  EIFR |= (1<<INTF1);
  EIFR |= (1<<INTF2);
  attachInterrupt(digitalPinToInterrupt(HANDSHAKE), ISR1, LOW);
  attachInterrupt(digitalPinToInterrupt(KCLKLOW), ISR2, LOW);
}

void ResetAmiga(bool resetRequest)
{
  detachInterrupt(digitalPinToInterrupt(HANDSHAKE));
  detachInterrupt(digitalPinToInterrupt(KCLKLOW));
  bool _resetRequest = resetRequest;  //use this in the future for Amiga "soft reset"

  //AMIGA HARD RESET
  digitalWrite(KCLK, LOW);
  delay(500);
  //KEYBOARD HARD RESET
  wdt_reset();
  wdt_enable(WDTO_15MS);
  while (1) {}
}

void ISR1()
{
  detachInterrupt(digitalPinToInterrupt(HANDSHAKE));

  wdt_reset();
  wdt_enable(WDTO_1S);
  while (digitalRead(HANDSHAKE) == 0) {}
  wdt_disable();

  amigaACK = true;

  #if defined(ISR1DEBUGGER)      
  Serial.println("ISR1");
  #endif
}

void ISR2() //incoming RESET_FROM_AMIGA
{
  noInterrupts();
  wdt_reset();
  wdt_enable(WDTO_15MS);
  while (1) {}
}
