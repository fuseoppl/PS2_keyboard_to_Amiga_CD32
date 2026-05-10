# PS/2 keyboard to Amiga CD32

PS/2 to Amiga keyboard translator tested on:
* Amiga CD32
* PS/2 keyboard, Brand: Perixx, Model No.:PERIBOARD-409P, Part No.:TK525P
* PS/2 keyboard, Brand: NEW3P, Model No.:AC-59
* DFRobot Beetle Board compatible with Arduino Leonardo
* Arduino UNO nano (mini ultra, china clone https://pl.aliexpress.com/item/1005007492500542.html )

Keys info:
* Left GUI (Windows key) = Left Amiga key
* Menu key = Right Amiga key
* Home key = Help key
* ctrl + alt + home = reset keyboard
* ctrl + alt + delete = hard reset Amiga

To do (not required):
* add Amiga "soft reset".
* add ERROR code after resync (except resync after start) and send last known key code.

The project used a modified library: https://github.com/techpaul/PS2KeyAdvanced
* PS2KeyAdvanced library is modified, so do not use the original one!
* keep the library files directly in the sketch directory.

You need:
* two 4k7 resistors
* two diodes with a very low voltage drop, maximum 0.3V (eg.: BAS85-GS08).

Beetle Board ↔ Amiga, PS/2 keyboard
(#define ATMEGA32 //uncomment for ATmega32)
* '+' ↔ Pin 4 (CD32 6-Pin Mini-DIN) & PS/2 keyboard Vcc
* '-' ↔ Pin 3 (CD32 3-Pin Mini-DIN) & PS/2 keyboard gnd
* D11 ↔ PS2 keyboard data line
* SCL ↔ PS2 keyboard clock line
* D10 to cathode schottky (SD1)
* RX & 4k7 (pullup to vcc) & anode schottky (SD1) ↔ Pin 5 (CD32 6-Pin Mini-DIN) keyboard clock line
* D9 to cathode schottky (SD2)
* SDA & 4k7 (pullup to vcc) & anode schottky (SD2) ↔ Pin 1 (CD32 6-Pin Mini-DIN) keyboard data line

mini ultra, china clone https://pl.aliexpress.com/item/1005007492500542.html select Tools->Processor->ATmega328P (Old Bootloader)
(//#define ATMEGA32 //comment for ATmega328P)
* 5V ↔ Pin 4 (CD32 6-Pin Mini-DIN) & PS/2 keyboard Vcc
* GND ↔ Pin 3 (CD32 3-Pin Mini-DIN) & PS/2 keyboard gnd
* D5 ↔ PS2 keyboard data line
* D3 ↔ PS2 keyboard clock line
* D8 to cathode schottky (SD1)
* D7 & 4k7 (pullup to vcc) & anode schottky (SD1) ↔ Pin 5 (CD32 6-Pin Mini-DIN) keyboard clock line
* D10 to cathode schottky (SD2)
* D9 & 4k7 (pullup to vcc) & anode schottky (SD2) ↔ Pin 1 (CD32 6-Pin Mini-DIN) keyboard data line

DFRobot Beetle Board
![schema 1](https://github.com/fuseoppl/PS2_keyboard_to_Amiga_CD32/blob/master/PS2keyboardToCD32.png)

NEW3P, Model No.:AC-59 and mini ultra (china Arduino UNO nano clone)
![schema 2](https://github.com/fuseoppl/PS2_keyboard_to_Amiga_CD32/blob/master/PS2keyboardToCD32_AC59.jpg)
