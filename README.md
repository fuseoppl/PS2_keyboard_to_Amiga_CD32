# PS/2 keyboard to Amiga CD32

PS/2 to Amiga keyboard translator tested on:
* Amiga CD32
* PS/2 keyboard, Brand: Perixx, Model No.:PERIBOARD-409P, Part No.:TK525P
* DFRobot Beetle Board compatible with Arduino Leonardo

Keys info:
* Left GUI (Windows key) = Left Amiga
* Menu key = Right Amiga
* ctrl + alt + home = reset keyboard
* ctrl + alt + delete = hard reset Amiga

To do (this is not required for normal use):
* add Amiga "soft reset".
* add ERROR code after resync (except resync after start) and send last known key code.

The project used a modified library: https://github.com/techpaul/PS2KeyAdvanced
* PS2KeyAdvanced library is modified, so do not use the original one!
* keep the library files directly in the sketch directory.

You need:
* two 4k7 resistors
* two diodes with a very low voltage drop, maximum 0.3V (eg.: BAS85-GS08).

CapsLock LED:
* unsolder the LED on the Beetle, cut off the CapsLock LED from the keyboard board and connect it to the Beetle LED pads.

Beetle Board <-> Amiga, PS/2 keyboard
* '+' <- Pin 4 (CD32 6-Pin Mini-DIN) & PS/2 keyboard Vcc
* '-' <-> Pin 3 (CD32 3-Pin Mini-DIN) & PS/2 keyboard gnd
* D11 <-> PS2 keyboard data line
* SCL <-> PS2 keyboard clock line
* SDA & anode schottky (SD2) <-> Pin 1 (CD32 6-Pin Mini-DIN) keyboard data line
* D10 to cathode schottky (SD1)
* D9 to cathode schottky (SD2)
* RX & anode schottky (SD1) <-> Pin 5 (CD32 6-Pin Mini-DIN) keyboard clock line
* D10 pull up to vcc with a 4k7
* SDA pull up to vcc with a 4k7

![schema](https://github.com/fuseoppl/PS2_keyboard_to_Amiga_CD32/blob/master/PS2keyboardToCD32.png)
