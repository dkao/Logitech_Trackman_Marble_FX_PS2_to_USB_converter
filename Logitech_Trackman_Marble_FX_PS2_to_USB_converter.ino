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

/*
 * Use AdvMouse library.
 * Install library/AdvMouse to your Arduino library directory
 */
#define ADVANCE_MOUSE

#ifdef ADVANCE_MOUSE
  #include <AdvMouse.h>
  #define MOUSE_BEGIN       AdvMouse.begin()
  #define MOUSE_PRESS(x)    AdvMouse.press_(x)
  #define MOUSE_RELEASE(x)  AdvMouse.release_(x)
  #define MOUSE_MOVE(...)                                                     \
    do {                                                                      \
      if (AdvMouse.needSendReport() || mx != 0 || my != 0) {                  \
        AdvMouse.move(__VA_ARGS__);                                           \
      }                                                                       \
    } while (0)
#else
  #include <Mouse.h>
  #define MOUSE_BEGIN       Mouse.begin()
  #define MOUSE_PRESS(x)    Mouse.press(x)
  #define MOUSE_RELEASE(x)  Mouse.release(x)
  #define MOUSE_MOVE(...)   Mouse.move(__VA_ARGS__)
#endif

/* Comment this line to use original remote mode. */
#define STREAM_MODE

/*
 * Set sample rate.
 * PS/2 default sample rate is 100Hz.
 * Valid sample rates: 10, 20, 40, 60, 80, 100, 200
 */
#define SAMPLE_RATE 200

/*
 * Use red button as back button instead of firmware scroll.
 * Scrolling can be done with software wheel emulation
 */
#define RED_BUTTON_AS_BACK

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
#ifdef SAMPLE_RATE
  //  Serial.print("Setting sample rate: ");
  //  Serial.println(SAMPLE_RATE);
  mouse_write(0xf3);  /* sample rate */
  mouse_read();  /* ack */
  mouse_write(SAMPLE_RATE);
  mouse_read();  /* ack */
#endif
#ifndef STREAM_MODE
  //  Serial.print("Sending remote mode code\n");
  mouse_write(0xf0);  /* remote mode */
  mouse_read();  /* ack */
  //  Serial.print("Read ack byte2\n");
  delayMicroseconds(100);
#endif
}

void mouse_enable_report()
{
  mouse_write(0xf4); /* enable report */
  mouse_read(); /* ack */
  delayMicroseconds(100);
}

int isPs2pp4ThButtonDown = false;
int isPs2pp5ThButtonDown = false;
bool oldLeftButton = false;
bool oldRightButton = false;
bool oldMiddleButton = false;
bool old4thButton = false;
bool old5thButton = false;

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
#ifdef STREAM_MODE
  mouse_enable_report();
#endif
  MOUSE_BEGIN;
}

bool ps2pp_decode(char r0, char r1, char r2)
{
  if ((r0 & 0x48) != 0x48) {
    return false;
  }
  int t = ((((r0 & 0x30) << 2) | (r1 & 0x30)) >> 4) & 0xF;
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
    isPs2pp4ThButtonDown = !!(data & 0x10);
    isPs2pp5ThButtonDown = !!(data & 0x20);
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

#ifndef STREAM_MODE
  mouse_write(0xeb);  /* give me data! */
  mouse_read();      /* ignore ack */
#endif
  mstat = mouse_read();
  mx = mouse_read();
  my = mouse_read();

  bool isPs2pp = ps2pp_decode(mstat, mx, my);
  if (!isPs2pp) {
    // handle left button
    bool leftButton = mstat & 0x01;
    if (!oldLeftButton && leftButton) {
      MOUSE_PRESS(MOUSE_LEFT);
    }
    else if (oldLeftButton && !leftButton) {
      MOUSE_RELEASE(MOUSE_LEFT);
    }
    oldLeftButton = leftButton;

    // handle right button
    bool rightButton = mstat & 0x02;
    if (!oldRightButton && rightButton) {
      MOUSE_PRESS(MOUSE_RIGHT);
    }
    else if (oldRightButton && !rightButton) {
      MOUSE_RELEASE(MOUSE_RIGHT);
    }
    oldRightButton = rightButton;

    // handle middle button
    bool middleButton = mstat & 0x04;
    if (!oldMiddleButton && middleButton) {
      MOUSE_PRESS(MOUSE_MIDDLE);
    }
    else if (oldMiddleButton && !middleButton) {
      MOUSE_RELEASE(MOUSE_MIDDLE);
    }
    oldMiddleButton = middleButton;

#if defined(RED_BUTTON_AS_BACK) && defined(ADVANCE_MOUSE)
    if (!old4thButton && isPs2pp4ThButtonDown) {
      MOUSE_PRESS(MOUSE_BACK);
    } else if (old4thButton && !isPs2pp4ThButtonDown) {
      MOUSE_RELEASE(MOUSE_BACK);
    }
    old4thButton = isPs2pp4ThButtonDown;

    if (!old5thButton && isPs2pp5ThButtonDown) {
      MOUSE_PRESS(MOUSE_FORWARD);
    } else if (old5thButton && !isPs2pp5ThButtonDown) {
      MOUSE_RELEASE(MOUSE_FORWARD);
    }
    old5thButton = isPs2pp5ThButtonDown;
#else
    if (isPs2pp4ThButtonDown) {
      // translate y scroll into wheel-scroll
      MOUSE_MOVE(0, 0, my);
    }
    else
#endif
    {
      MOUSE_MOVE(mx, Y_MULT * my, 0);
    }
  }

#ifndef STREAM_MODE
  delay(20);  /* twiddle */
#endif
}
