/*
 * Logitech TrackMan Marble FX wheel driver
 *  tested on Arduino Micro
 *  patched by gtalpo@gmail.com
 *  original source from http://playground.arduino.cc/uploads/ComponentLib/mouse.txt
 *  PS2++ protocol specs from http://web.archive.org/web/20030714000535/http://dqcs.com/logitech/ps2ppspec.htm
 *  
 *  
 *  default HW setup
 *   wire PS/2 connector to arduino
 *   see: http://playground.arduino.cc/ComponentLib/Ps2mouse
 *   
 *  driver limitations:
 *   use at your own risk. 
 *   super hack. tested on my own TrackMan Marble FX(T-CJ12) only
 *   
 *  functionality:
 *   press red button to emulate wheel movement with the ball
 */

/*
 * an arduino sketch to interface with a ps/2 mouse.
 * Also uses serial protocol to talk back to the host
 * and report what it finds.
 */
#include "Mouse.h"

#define REVERT_Y_AXIS

#ifdef REVERT_Y_AXIS
#define Y_MULT -1
#else
#define Y_MULT 1
#endif

/*
 * Pin 5 is the mouse data pin, pin 6 is the clock pin
 * Feel free to use whatever pins are convenient.
 */
#define MDATA 5
#define MCLK 6

/*
 * according to some code I saw, these functions will
 * correctly set the mouse clock and data pins for
 * various conditions.
 */
void gohi(int pin)
{
  pinMode(pin, INPUT);
  digitalWrite(pin, HIGH);
}

void golo(int pin)
{
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);
}

void mouse_write(char data)
{
  char i;
  char parity = 1;

  //  Serial.print("Sending ");
  //  Serial.print(data, HEX);
  //  Serial.print(" to mouse\n");
  //  Serial.print("RTS");
  /* put pins in output mode */
  gohi(MDATA);
  gohi(MCLK);
  delayMicroseconds(300);
  golo(MCLK);
  delayMicroseconds(300);
  golo(MDATA);
  delayMicroseconds(10);
  /* start bit */
  gohi(MCLK);
  /* wait for mouse to take control of clock); */
  while (digitalRead(MCLK) == HIGH)
    ;
  /* clock is low, and we are clear to send data */
  for (i=0; i < 8; i++) {
    if (data & 0x01) {
      gohi(MDATA);
    } 
    else {
      golo(MDATA);
    }
    /* wait for clock cycle */
    while (digitalRead(MCLK) == LOW)
      ;
    while (digitalRead(MCLK) == HIGH)
      ;
    parity = parity ^ (data & 0x01);
    data = data >> 1;
  }  
  /* parity */
  if (parity) {
    gohi(MDATA);
  } 
  else {
    golo(MDATA);
  }
  while (digitalRead(MCLK) == LOW)
    ;
  while (digitalRead(MCLK) == HIGH)
    ;
  /* stop bit */
  gohi(MDATA);
  delayMicroseconds(50);
  while (digitalRead(MCLK) == HIGH)
    ;
  /* wait for mouse to switch modes */
  while ((digitalRead(MCLK) == LOW) || (digitalRead(MDATA) == LOW))
    ;
  /* put a hold on the incoming data. */
  golo(MCLK);
  //  Serial.print("done.\n");
}

/*
 * Get a byte of data from the mouse
 */
char mouse_read(void)
{
  char data = 0x00;
  int i;
  char bit = 0x01;

  //  Serial.print("reading byte from mouse\n");
  /* start the clock */
  gohi(MCLK);
  gohi(MDATA);
  delayMicroseconds(50);
  while (digitalRead(MCLK) == HIGH)
    ;
  delayMicroseconds(5);  /* not sure why */
  while (digitalRead(MCLK) == LOW) /* eat start bit */
    ;
  for (i=0; i < 8; i++) {
    while (digitalRead(MCLK) == HIGH)
      ;
    if (digitalRead(MDATA) == HIGH) {
      data = data | bit;
    }
    while (digitalRead(MCLK) == LOW)
      ;
    bit = bit << 1;
  }
  /* eat parity bit, which we ignore */
  while (digitalRead(MCLK) == HIGH)
    ;
  while (digitalRead(MCLK) == LOW)
    ;
  /* eat stop bit */
  while (digitalRead(MCLK) == HIGH)
    ;
  while (digitalRead(MCLK) == LOW)
    ;

  /* put a hold on the incoming data. */
  golo(MCLK);
  //  Serial.print("Recvd data ");
  //  Serial.print(data, HEX);
  //  Serial.print(" from mouse\n");
  return data;
}

void mouse_init()
{
  gohi(MCLK);
  gohi(MDATA);
  //  Serial.print("Sending reset to mouse\n");
  mouse_write(0xff);
  mouse_read();  /* ack byte */
  //  Serial.print("Read ack byte1\n");
  mouse_read();  /* blank */
  mouse_read();  /* blank */
  //  Serial.print("Sending remote mode code\n");
  mouse_write(0xf0);  /* remote mode */
  mouse_read();  /* ack */
  //  Serial.print("Read ack byte2\n");
  delayMicroseconds(100);
}

int isPs2pp4ThButtonDown = false;
bool oldLeftButton = false;
bool oldRightButton = false;
bool oldMiddleButton = false;

// PS2++, extended ps/2 protocol spec.
// http://web.archive.org/web/20030714000535/http://dqcs.com/logitech/ps2ppspec.htm
void ps2pp_write_magic_ping()
{
  //e8 00 e8 03 e8 02 e8 01 e6 e8 03 e8 01 e8 02 e8 03
  mouse_write(0xe8);
  mouse_write(0x00);

  mouse_write(0xe8);
  mouse_write(0x03);
  
  mouse_write(0xe8);
  mouse_write(0x02);
  
  mouse_write(0xe8);
  mouse_write(0x01);
  
  mouse_write(0xe6);
  
  mouse_write(0xe8);
  mouse_write(0x03);
  
  mouse_write(0xe8);
  mouse_write(0x01);

  mouse_write(0xe8);
  mouse_write(0x02);

  mouse_write(0xe8);
  mouse_write(0x03);
}

void setup()
{
  Serial.begin(9600);
  mouse_init();
  ps2pp_write_magic_ping();
  Mouse.begin();
}

bool ps2pp_decode(char r0, char r1, char r2)
{
  if ((r0 & 0x48) != 0x48) {
    return false;
  }
  int t = ((r0 & 0x30) << 4) || (r1 & 0x30);
  int check = r1 & 0x0f;
  int data = r2;
  // if ((check & 0x03 == 2) && (check >> 2) == (data & 0x03)) {
  //   Serial.print("\t valid");
  // }
  // else {
  //   Serial.print("\t ignore");
  // }
  // mouse extra info
  if (t == 1) {
    if (data & 0x10) {
      isPs2pp4ThButtonDown = HIGH;
    }
    else {
      isPs2pp4ThButtonDown = LOW;
    }
  }
  return true;
}

/*
 * get a reading from the mouse and report it back to the
 * host via the serial line.
 */
void loop()
{
  char mstat;
  char mx;
  char my;

  mouse_write(0xeb);  /* give me data! */
  mouse_read();      /* ignore ack */
  mstat = mouse_read();
  mx = mouse_read();
  my = mouse_read();

  bool isPs2pp = ps2pp_decode(mstat, mx, my);
  if (!isPs2pp) {
    if (isPs2pp4ThButtonDown) {
      // translate y scroll into wheel-scroll
      Mouse.move(0, 0, my);
    }
    else {
      Mouse.move(mx, Y_MULT * my, 0);
    }

    // handle left button
    bool leftButton = mstat & 0x01;
    if (!oldLeftButton && leftButton) {
      Mouse.press(MOUSE_LEFT);
    }
    else if (oldLeftButton && !leftButton) {
      Mouse.release(MOUSE_LEFT);
    }
    oldLeftButton = leftButton;

    // handle right button
    bool rightButton = mstat & 0x02;
    if (!oldRightButton && rightButton) {
      Mouse.press(MOUSE_RIGHT);
    }
    else if (oldRightButton && !rightButton) {
      Mouse.release(MOUSE_RIGHT);
    }
    oldRightButton = rightButton;

    // handle middle button
    bool middleButton = mstat & 0x04;
    if (!oldMiddleButton && middleButton) {
      Mouse.press(MOUSE_MIDDLE);
    }
    else if (oldMiddleButton && !middleButton) {
      Mouse.release(MOUSE_MIDDLE);
    }
    oldMiddleButton = middleButton;

  }

  delay(20);  /* twiddle */
}

