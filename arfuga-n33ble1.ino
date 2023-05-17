#include <ArduinoBLE.h>

////////////////////////////////////////////////////////////////////////////////
// Pin Management                                                             //
// pin assignments and state tracking                                         //
////////////////////////////////////////////////////////////////////////////////

// Pin assignments
#define PinButLeft 6
#define PinLedLeft 4
#define PinButRight 10
#define PinLedRight 8

#define PinLedBoard LED_BUILTIN
#define PinLedRed LEDR
#define PinLedGreen LEDG
#define PinLedBlue LEDB

// Pin state tracking
bool pinsUpdated = false;
byte lastPinLedLeft;
byte lastPinLedRight;
byte lastPinLedBoard;
byte lastPinLedRed;
byte lastPinLedGreen;
byte lastPinLedBlue;

#define digitalWriteHistory(pin, value, pinLast) do { \
  digitalWrite((pin), (value)); \
  pinsUpdated |= ((pinLast) != (value)); \
  (pinLast) = (value); \ 
} while(false)

#define analogWriteHistory(pin, value, pinLast) do { \
  analogWrite((pin), (value)); \
  pinsUpdated |= ((pinLast) != (value)); \
  (pinLast) = (value); \
} while(false)

////////////////////////////////////////////////////////////////////////////////
// Button Function                                                            //
// called when checking the button, checks for debounces and holds            //
////////////////////////////////////////////////////////////////////////////////

// Byte Button Characteristics:
#define ButtonMaskPress       0b11000000  // Type of button press that has occurred.
#define ButtonMaskIncremental 0b00111111  // Incrementing version upon each button press. Starts at 0 and wraps at 64.

#define ButtonPressOnce  0b00000000  // Pressed for no more than 500ms and released for at least 500ms.
#define ButtonPressTwice 0b01000000  // Pressed for no more than 500ms and released twice, no more than 500ms between presses, no less than 500ms after last release.
#define ButtonHoldShort  0b10000000  // Held down for at least 500ms and no more than 4000ms.
#define ButtonHoldLong   0b11000000  // Held down for at least 3000ms.

#define ButtonDebounceDelay 20        // How long to wait for a LOW->HIGH or HIGH->LOW transition to become permanent.
#define ButtonHoldThreshold 500       // How long until a press is considered a short hold.
#define ButtonRepressPatience 500     // How long until a single press release is registered and no double press is registered.
#define ButtonHoldLongThreshold 3000  // How long until a short hold becomes a long hold.

#define ButtonPressedLedSpan 500  // How long does the led blink take (one state) when being pressed

// Button definitions state for checkButton:
enum ButtonState { bReleased, bPressDebounce, bPressed, bReleaseDebounce };
enum ButtonPressType { bpNone, bpPressOnce, bpPressTwice, bpHoldShort, bpHoldLong };
typedef struct {
  byte pin;
  ButtonState state;
  unsigned long holdTimer;
  unsigned long debounceTimer;
  ButtonPressType suspectedPressType;
} Button;
#define initButton(pin) {(pin), bReleased, 0, 0, bpNone}

ButtonPressType checkButton(Button* const button) {
  // Note that since we're using INPUT_PULLUP wiring post-switch to GND, we need to read LOW as DOWN.
  bool const isDown = (digitalRead(button->pin) == LOW);
  switch ( button->state ) {
    case bReleased:
      if ( isDown ) { // if pressed, wait for debounce to stabilize
        button->debounceTimer = millis();
        button->state = bPressDebounce;
      } else if ((( button->suspectedPressType == bpPressOnce ) || ( button->suspectedPressType == bpPressTwice )) && ( millis() - button->holdTimer >= ButtonRepressPatience )) {
        // if still suspecting a press type and patience has run out, report the suspected press type
        ButtonPressType const pressType = button->suspectedPressType;
        button->suspectedPressType = bpNone;
        return pressType;
      }
      break;

    case bPressDebounce:
      if ( !isDown ) { // if released before debounce delay, not a press - return to bReleased
        button->state = bReleased;
      } else if ( millis() - button->debounceTimer >= ButtonDebounceDelay ) {  // if debounce delay is up, change to bPressed timing
        button->state = bPressed;
        button->holdTimer = millis();
      }
      break;

    case bPressed:
      if ( !isDown ) { // if released, wait for debounce to stabilize
        button->debounceTimer = millis();
        button->state = bReleaseDebounce;
      } else if (( button->suspectedPressType != bpHoldLong ) && ( millis() - button->holdTimer >= ButtonHoldLongThreshold )) {
        // if held long enough for holdLong and haven't already reported holdLong, immediately report holdLong
        button->suspectedPressType = bpHoldLong;
        return bpHoldLong;
      }
      break;

    case bReleaseDebounce:
      if ( isDown ) { // if pressed before debounce delay, not a release - return to bPressed
        button->state = bPressed;
      } else if ( millis() - button->debounceTimer >= ButtonDebounceDelay ) {  // if debounce delay is up...
        if ( button->suspectedPressType == bpHoldLong ) { // if holdLong was already reported, move to bReleased with no suspects
          button->suspectedPressType = bpNone;
          button->state = bReleased;
          return bpNone;
        }

        if ( millis() - button->holdTimer >= ButtonHoldThreshold ) { // if held long enough for holdShort, report holdShort and move to bReleased with no suspects
          button->suspectedPressType = bpNone;
          button->state = bReleased;
          return bpHoldShort;
        }

        if (( button->suspectedPressType == bpPressOnce ) || ( button->suspectedPressType == bpPressTwice )) {
          // if already suspecting pressOnce or pressTwice, move to bReleased with suspects on pressTwice
          button->suspectedPressType = bpPressTwice;
        } else {  // move to bReleased with suspects on pressOnce
          button->suspectedPressType = bpPressOnce;
        }
        button->state = bReleased;
      }
      break;
  }

  return bpNone;
}

void setAndIncrementButtonCharacter(byte const buttonPressType, BLEByteCharacteristic* const character) {
  byte const newIncremental = (character->value() + 1) & ButtonMaskIncremental;
  character->writeValue(buttonPressType | newIncremental);
}

void checkButtonAndSetChar(Button* const button, BLEByteCharacteristic* const character) {
  switch (checkButton(button)) {
    case bpNone:
      break;
    case bpPressOnce:
      setAndIncrementButtonCharacter(ButtonPressOnce, character);
      break;
    case bpPressTwice:
      setAndIncrementButtonCharacter(ButtonPressTwice, character);
      break;
    case bpHoldShort:
      setAndIncrementButtonCharacter(ButtonHoldShort, character);
      break;
    case bpHoldLong:
      setAndIncrementButtonCharacter(ButtonHoldLong, character);
      break;
  }
}

Button butLeft = initButton(PinButLeft);
Button butRight = initButton(PinButRight);


////////////////////////////////////////////////////////////////////////////////
// UI Timers                                                                  //
// sources unified timers for blinking and other ui patterns                  //
////////////////////////////////////////////////////////////////////////////////

#define MsPerSec 1000

// All UI elements must "loop" within this range, no operations longer than this.
#define UiTimerSpan (4 * MsPerSec)

// *6 here is to allow all 4 timing settings to exist evenly in this repeating timer
// 1000, 2000, 3000, and 4000 all fit evenly in 24000 (4000 * 6)
#define UiTimerLoop (6 * UiTimerSpan)

// Follows millis(). Jumps UiTimerLoop when difference between millis() and this
// is at least UiTimerLoop.
unsigned long uiTimer = 0;

// Difference between uiTimer and millis(). Never larger than UiTimerLoop. 
// Use with isTimerInSpan.
unsigned int uiTimerDiff = 0;

// Returns true if `(timer % (spanWidth*2)) < spanWidth)`.
// Used for blinking leds or timing behaviors.
// E.g. returns true for 500ms for every 1000ms that `timer` passes.
bool isTimerInSpan(unsigned long* const timer, unsigned int const spanWidth) {
  while (*timer >= spanWidth * 2) {
    *timer -= (spanWidth * 2);
  }
  return *timer < spanWidth;
}


////////////////////////////////////////////////////////////////////////////////
// Bluetooth Low-Energy                                                       //
// characteristic value macros and structure definition                       //
////////////////////////////////////////////////////////////////////////////////

// Unsigned Long Characteristics:
// long is 4 bytes: byte separation is as follows
// b0: blink sequence
// b1: 0-255 output for onboard multicolor LEDR
// b2: 0-255 output for onboard multicolor LEDG
// b3: 0-255 output for onboard multicolor LEDB
#define ledBlinkSequenceFromUL(x) ((byte)((x) & 0xFF))
#define ledRedFromUL(x) ((byte)((~x >> 8) & 0xFF))
#define ledGreenFromUL(x) ((byte)((~x >> 16) & 0xFF))
#define ledBlueFromUL(x) ((byte)((~x >> 24) & 0xFF))

// Byte LedBlink Characteristics:
// Sequences: number of blinks in a timing span
// Timing: how long to (a) display X blinks, and (b) wait between repeated sequences
// Follow loopLedBlinkSequence() for more info.
#define ledBlinkSeqFirst(x) ((x) & 0b00000111)
#define ledBlinkSeqSecond(x) (((x) >> 3) & 0b00000111)
#define ledBlinkTiming(x) (((x) >> 6) & 0b00000011)

BLEService cbService                      ("d2e4b177-0dbe-4f21-b54a-2a81e4d23c4e");
BLEUnsignedLongCharacteristic charBoardLed("1e7edaf6-ef50-44d4-8368-8becd241b5be", BLERead | BLEWrite);
BLEByteCharacteristic charButLeft         ("2472170e-84b1-46aa-bdd7-f2861c021c2a", BLERead | BLENotify);
BLEByteCharacteristic charButRight        ("4bcf9ccf-2e59-4b61-a3b4-2cdc8f005d3b", BLERead | BLENotify);
BLEByteCharacteristic charButHandLeft     ("1f01d834-af5e-4811-b4a6-95213ec9b193", BLERead | BLEWrite);
BLEByteCharacteristic charButHandRight    ("0fb8f0e2-3e24-49ab-a1e1-6f5494f92a0e", BLERead | BLEWrite);
BLEByteCharacteristic charLedLeft         ("72c46fe5-2c30-47fb-994b-0f044b2fec4f", BLERead | BLEWrite);
BLEByteCharacteristic charLedRight        ("13d19bef-4512-4fef-84dc-7f251da6510b", BLERead | BLEWrite);

char serialCommand = 0;

void setup() {
  pinMode(PinButLeft,  INPUT_PULLUP);
  pinMode(PinButRight, INPUT_PULLUP);
  pinMode(PinLedLeft,  OUTPUT);
  pinMode(PinLedRight, OUTPUT);

  pinMode(PinLedRed,   OUTPUT);
  pinMode(PinLedGreen, OUTPUT);
  pinMode(PinLedBlue,  OUTPUT);
  pinMode(PinLedBoard, OUTPUT);

  analogWriteHistory(PinLedRed,   255, lastPinLedRed);
  analogWriteHistory(PinLedGreen, 255, lastPinLedGreen);
  analogWriteHistory(PinLedBlue,  255, lastPinLedBlue);

  Serial.begin(9600);

  digitalWriteHistory(PinLedBoard, HIGH, lastPinLedBoard);

  if (!BLE.begin()) {
    digitalWriteHistory(PinLedBoard, LOW, lastPinLedBoard);
    while (1);
  }

  cbService.addCharacteristic(charBoardLed);
  cbService.addCharacteristic(charButLeft);
  cbService.addCharacteristic(charButRight);
  cbService.addCharacteristic(charButHandLeft);
  cbService.addCharacteristic(charButHandRight);
  cbService.addCharacteristic(charLedLeft);
  cbService.addCharacteristic(charLedRight);

  BLE.setLocalName("DLZP-N33BLE-1");  // Visible to central devices.
  BLE.setAdvertisedServiceUuid("5202f40a-2c8c-4636-8f63-23c468df0c55");
  BLE.addService(cbService);

  charBoardLed.writeValue((unsigned long)0);  // Start on, and rgb(255, 255, 255)
  charButLeft.writeValue((byte)0);
  charButRight.writeValue((byte)0);
  charButHandLeft.writeValue((byte)0);
  charButHandRight.writeValue((byte)0);
  charLedLeft.writeValue((byte)0b00111000);  // Start off
  charLedRight.writeValue((byte)0);          // Start on

  if (!BLE.advertise()) {
    digitalWriteHistory(PinLedBoard, LOW, lastPinLedBoard);
    while (1);
  }
}

void loopLedBlinkSequence(byte ledPin, byte blinkSeq, byte* lastLedPin) {
  // Led operation is broken into four parts, two blinking sequences, and two off periods.
  // Blinking sequences each can be different. Timing of all four parts can be adjusted.
  // Example: 1/4 of the loop consists of 15 chars (representing time):
  // <XXX---XXX---XXX---------------XXXXX-----XXXXX--------------->
  //  ^firstSeq      ^firstOff      ^secondSeq     ^secondOff     ^ loop back to firstSeq

  byte const firstSeq = ledBlinkSeqFirst(blinkSeq);
  byte const secondSeq = ledBlinkSeqSecond(blinkSeq);
  byte const timing = ledBlinkTiming(blinkSeq);

  if (firstSeq == 0 || secondSeq == 0) {
    digitalWriteHistory(ledPin, secondSeq == 0 ? HIGH : LOW, *lastLedPin);
    return;
  }

  unsigned long localTimerDiff = uiTimerDiff;
  unsigned int const halfSequenceSpan = (UiTimerSpan / 8) * (timing + 1); // ((4000 / 2) / 4) * (1 + (0_3)): 500(0), 1000(1), 1500(2), 2000(3) 
  unsigned int const sequencePartSpan = halfSequenceSpan / 2;

  bool const isFirstHalf = isTimerInSpan(&localTimerDiff, halfSequenceSpan);
  if (!isTimerInSpan(&localTimerDiff, sequencePartSpan)) {
    digitalWriteHistory(ledPin, LOW, *lastLedPin);
    return;
  }

  // 000: 0: Always on (above)
  // 001: 1: 1 toggle (1 on)
  // 010: 2: 3 toggle (2 on, 1 off)
  // 011: 3: 5 toggle (3 on, 2 off)
  // 100: 4: 7 toggle (4 on, 3 off)
  // ...
  byte const togglesPerSequence = ((isFirstHalf ? firstSeq : secondSeq) * 2) - 1;
  unsigned int const sequenceToggleSpan = sequencePartSpan / togglesPerSequence;

  bool const isToggleOn = isTimerInSpan(&localTimerDiff, sequenceToggleSpan);
  digitalWriteHistory(ledPin, (isToggleOn ? HIGH : LOW), *lastLedPin);
}

void loop() {
  BLE.poll();


  // Update uiTimer and uiTimerDiff.
  while ((millis() - uiTimer) >= UiTimerLoop) {
    uiTimer += UiTimerLoop;
  }
  uiTimerDiff = (millis() - uiTimer);


  // Handle the board led (both basic onboard and RGB)
  unsigned long const charBoardLedValue = charBoardLed.value();
  loopLedBlinkSequence(PinLedBoard, ledBlinkSequenceFromUL(charBoardLedValue), &lastPinLedBoard);
  if (charBoardLed.written()) {
    analogWriteHistory(PinLedRed,   ledRedFromUL(charBoardLedValue), lastPinLedRed);
    analogWriteHistory(PinLedGreen, ledGreenFromUL(charBoardLedValue), lastPinLedGreen);
    analogWriteHistory(PinLedBlue,  ledBlueFromUL(charBoardLedValue), lastPinLedBlue);
  }


  // Check button inputs
  checkButtonAndSetChar(&butLeft, &charButLeft);
  checkButtonAndSetChar(&butRight, &charButRight);

  // If buttons are pressed, override leds with a fixed blink sequence, or follow sequence if not pressed.
  if(butLeft.state == bPressed) {
    unsigned long adjustedTimerDiff = (millis() - butLeft.holdTimer);
    digitalWriteHistory(PinLedLeft, isTimerInSpan(&adjustedTimerDiff, ButtonPressedLedSpan) ? HIGH : LOW, lastPinLedLeft);
  } else {
    loopLedBlinkSequence(PinLedLeft, charLedLeft.value(), &lastPinLedLeft);
  }

  if(butRight.state == bPressed) {
    unsigned long adjustedTimerDiff = (millis() - butRight.holdTimer);
    digitalWriteHistory(PinLedRight, isTimerInSpan(&adjustedTimerDiff, ButtonPressedLedSpan) ? HIGH : LOW, lastPinLedRight);
  } else {
    loopLedBlinkSequence(PinLedRight, charLedRight.value(), &lastPinLedRight);
  }
  
  
  // Check Serial I/O
  if(Serial && pinsUpdated) {
    pinsUpdated = false;
    char buffer[18];
    snprintf(buffer, 19, "%d,%d,%d,%3d,%3d,%3d", 
             lastPinLedBoard, 
             lastPinLedLeft,
             lastPinLedRight,
             lastPinLedRed,
             lastPinLedGreen,
             lastPinLedBlue);
    Serial.println(buffer);
  }

  while(Serial && Serial.available()) {
    if(!serialCommand) {
      serialCommand = Serial.read();
      switch(serialCommand) {
        case 'l':
        case 'r':
          break;

        case 'p':
          pinsUpdated = true;
          serialCommand = 0;
          break;

        default:
          serialCommand = 0;
          break;
      }
      
      continue;
    }

    switch(serialCommand) {
      case 'l':
      case 'r': {
        // '0' is 48, minus one more for '1' to match ButtonPressOnce (0x00______)
        byte const pressType = (Serial.read() - 49) << 6;
        Serial.println(pressType);
        setAndIncrementButtonCharacter(pressType, ((serialCommand == 'l') ? &charButLeft : &charButRight));
        serialCommand = 0;
        break;
      }
    }
  }
}
