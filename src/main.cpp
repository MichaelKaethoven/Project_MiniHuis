#include "bitmaps.h"
#include <Adafruit_SSD1306.h>
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <Bounce2.h>
#include <DHT_U.h>
#include <Servo.h>
#include <Wire.h>

#pragma region Sources
/*
- Bounce.2h documentation | https://github.com/thomasfredericks/Bounce2 |
01/10/2025
- C++ documentation | https://www.w3schools.com/cpp | 10/10/2025
AdaFruit DISPLAY_BJ documentation |
https://adafruit.github.io/Adafruit_SSD1306/html/class_adafruit___s_s_d1306.html|
10/10/2025
- BlackJack logic | https://www.chatgpt.com | 1/10/2025
- Bitmap creation | https://javl.github.io/image2cpp | 10/10/2025
- DHT sensor | https://github.com/adafruit/Adafruit_Sensor/blob/master/
|23/10/2025
*/
#pragma endregion

#pragma region Constants
// ---- OLED ----
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define BJ_ADDRESS 0x3C
#define DHT_ADDRESS 0x3D
Adafruit_SSD1306 DISPLAY_BJ(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
Adafruit_SSD1306 DISPLAY_DHT(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- PIN DEFINITIONS ----
#define LED_RED 10
#define LED_GREEN 8
#define LED_BLUE 9
#define PIR_SENSOR 3
#define DHT_SENSOR 7
#define SERVO_PIN 5

#define BUTTON_RED 13   // also functions as the "HIT" button
#define BUTTON_WHITE 12 // also functions as the "STAND" button
#define BUTTON_YELLOW 11

Servo doorServo;
#define DHTTYPE DHT22
DHT_Unified dht(DHT_SENSOR, DHTTYPE);

#pragma endregion

#pragma region Debouncers
// ---- DEBOUNCERS ----
// using <Bounce2.h>
Bounce debouncerRed = Bounce();
Bounce debouncerWhite = Bounce();
Bounce debouncerYellow = Bounce();

#pragma endregion

#pragma region Enums
// ---- ENUMs ----
enum LedColor { CLR_RED, CLR_WHITE, CLR_YELLOW, CLR_NONE };
enum DoorState { OPEN, CLOSE };
enum DISPLAY_BJState { CARD, WIN, BUST, LOSE, TIE };

DISPLAY_BJState currentState = CARD;

// char array for printing the colors easier, just in case I need it
const char *colorNames[] = {"RED", "WHITE", "YELLOW", "NONE"};

#pragma endregion

#pragma region BlackJack
// ---- CARD STRUCT ----
// basic struct to make 2 value objects
struct Card {
  const char *value;
  const unsigned char *suit;
};

// ---- DECK ----
// every single card in the deck
Card deck[] = {{"A", spade},   {"2", spade},   {"3", spade},   {"4", spade},
               {"5", spade},   {"6", spade},   {"7", spade},   {"8", spade},
               {"9", spade},   {"T", spade},   {"J", spade},   {"Q", spade},
               {"K", spade},

               {"A", heart},   {"2", heart},   {"3", heart},   {"4", heart},
               {"5", heart},   {"6", heart},   {"7", heart},   {"8", heart},
               {"9", heart},   {"T", heart},   {"J", heart},   {"Q", heart},
               {"K", heart},

               {"A", diamond}, {"2", diamond}, {"3", diamond}, {"4", diamond},
               {"5", diamond}, {"6", diamond}, {"7", diamond}, {"8", diamond},
               {"9", diamond}, {"T", diamond}, {"J", diamond}, {"Q", diamond},
               {"K", diamond},

               {"A", club},    {"2", club},    {"3", club},    {"4", club},
               {"5", club},    {"6", club},    {"7", club},    {"8", club},
               {"9", club},    {"T", club},    {"J", club},    {"Q", club},
               {"K", club}};

#pragma endregion

#pragma region Functions
// ---- FUNCTION DECLARATIONS ----
//
// basics / helpers
void setupButtons();
void setupLeds();
void setupOLED();
void setupSensors();
void handleButtonPress();
void turnButton(LedColor color);

// OLED functions
void printText(Adafruit_SSD1306 &display, String text, int x, int y, int size);
void drawCard(const char *value, const unsigned char *suitBitmap, int x, int y);
void drawHand(int hand[], int numCards, int startY);
void drawBitmapImage(const unsigned char *bitmap);
void drawBitmapImage(const unsigned char *bitmap, int x, int y, int width,
                     int height);
void clearArea(int x, int y, int w, int h);
void updateScoreDisplay(int playerValue, int dealerValue);
void displayDHTToOled(float temp, float humid);

// Blackjack functions
void updateBlackJack();
void hit();
void stand();
void resetGame();
void startDealerHitting();
void finishDealerHitting();
void handleGameEnd();
int calculateHandValue(int *handToCalculate, int drawnCards);
int determineDealerHand();

// sensor functions
void updateDHTSensor();

// servo functions
void moveDoor(DoorState state);
void setupServo();
void updateDoor();

// DHT sensor functions
void getTempFromSensor();
void getHumidFromSensor();

#pragma endregion

#pragma region Variables
// timing sensors
static unsigned long lastReadMs = 0;
const unsigned long readIntervalMs =
    5000UL; // UL is just a cast so the calculations get forced to use UL

// timing blackjack
unsigned long lastDealerHitTime = 0;
bool dealerHitting = false;
int dealerHitInterval = 700;

// Blackjack
int playerHand[10];     // store indexes of drawn cards
int numDrawnPlayer = 0; // number of cards drawn
int deckSize = 52;      // total deck size
int dealerHand[10];
int numDrawnDealer = 0;

// Servo
int doorPos = 0;
bool isDoorOpen = false;

// leds
bool redOn = false;
bool whiteOn = false;
bool yellowOn = false;

#pragma endregion

#pragma region Setup
// ---- SETUP ----
void setup() {
  Serial.begin(9600);
  Serial.println("System ready. RGB LED is OFF.");
  Wire.begin();

  setupButtons();
  setupLeds();
  setupOLED();
  setupServo();
  setupSensors();
}

void setupLeds() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);

  // RGB is connected with 5V
  // => HIGH means led is OFF
  digitalWrite(LED_RED, HIGH);
  digitalWrite(LED_GREEN, HIGH);
  digitalWrite(LED_BLUE, HIGH);
}

// ---- SETUP FUNCTIONS ----
void setupButtons() {
  pinMode(BUTTON_RED, INPUT);
  pinMode(BUTTON_WHITE, INPUT);
  pinMode(BUTTON_YELLOW, INPUT);

  debouncerRed.attach(BUTTON_RED);
  debouncerRed.interval(50);

  debouncerWhite.attach(BUTTON_WHITE);
  debouncerWhite.interval(50);

  debouncerYellow.attach(BUTTON_YELLOW);
  debouncerYellow.interval(50);
}

void setupOLED() {
  // Gets called in void setup
  if (!DISPLAY_BJ.begin(SSD1306_SWITCHCAPVCC, BJ_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed FOR BJ"));
    while (true) {
      // "Infinite loop" to wait until the OLED screen is initialised
      // otherwise there might be weird behaviour with the OLED screen
    }
  }
  DISPLAY_BJ.clearDisplay();
  Serial.println("DISPLAY BJ READY");
  if (!DISPLAY_DHT.begin(SSD1306_SWITCHCAPVCC, DHT_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed FIR DHT"));
    while (true) {
      // "Infinite loop" to wait until the OLED screen is initialised
      // otherwise there might be weird behaviour with the OLED screen
    }
  }
  DISPLAY_DHT.clearDisplay();
  Serial.println("DISPLAY DHT READY");
  DISPLAY_BJ.display();
  DISPLAY_DHT.display();
}

void setupServo() { doorServo.attach(SERVO_PIN); }

void setupSensors() {
  pinMode(PIR_SENSOR, INPUT);
  dht.begin();
}

#pragma endregion

#pragma region Loop
// ---- LOOP ----
void loop() {
  handleButtonPress();
  updateBlackJack();
  updateDoor();
  updateDHTSensor();
}
#pragma endregion

#pragma region Door Logic

void updateDoor() {
  if (digitalRead(PIR_SENSOR)) {
    moveDoor(OPEN);
  } else {
    moveDoor(CLOSE);
  }
}

void moveDoor(DoorState state) {
  switch (state) {
  case OPEN:
    if (isDoorOpen)
      break;
    for (doorPos = 0; doorPos <= 95; doorPos++) {
      doorServo.write(doorPos);
      isDoorOpen = true;
    }
    break;
  case CLOSE:
    if (!isDoorOpen)
      break;
    for (doorPos = 95; doorPos >= 0; doorPos--) {
      doorServo.write(doorPos);
      isDoorOpen = false;
    }

    break;
  }
}

#pragma endregion

#pragma region DHT Logic

void updateDHTSensor() {
  sensors_event_t event;
  float temp = 0;
  float humid = 0;

  unsigned long now = millis();
  if ((unsigned long)(now - lastReadMs) < readIntervalMs)
    return;

  // Temperature
  dht.temperature().getEvent(&event);
  if (!isnan(event.temperature)) {
    Serial.print(F("Temperature: "));
    temp = event.temperature;
    Serial.print(temp);

    Serial.println(F("°C"));
  }

  // Humidity
  dht.humidity().getEvent(&event);
  if (!isnan(event.relative_humidity)) {
    Serial.print(F("Humidity: "));
    humid = event.relative_humidity;
    Serial.print(humid);
    Serial.println(F("%"));
  }

  // Mark read time after successful sensor update
  lastReadMs = now;
  displayDHTToOled(temp, humid);
}

#pragma endregion

#pragma region Button Logic (BJ & RGB)
// ---- LOGIC ----
void handleButtonPress() {
  // Update debouncers every loop
  debouncerRed.update();
  debouncerWhite.update();
  debouncerYellow.update();

  // --- HIT ---
  if (debouncerRed.fell()) {
    turnButton(CLR_RED);
    hit();
  }

  // --- STAND ---
  if (debouncerWhite.fell()) {
    turnButton(CLR_WHITE);
    stand();
  }

  // --- RESET ---
  if (debouncerYellow.fell()) {
    turnButton(CLR_YELLOW);
    resetGame();
  }
}

// ---- RGB CONTROL ----
void turnButton(LedColor color) {
  switch (color) {
  case CLR_RED:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    break;

  case CLR_WHITE:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    break;

  case CLR_YELLOW:
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, HIGH);
    break;

  case CLR_NONE:
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_BLUE, HIGH);
    break;
  }

  Serial.print("LED set to: ");
  Serial.println(colorNames[color]);
}
#pragma endregion

#pragma region BlackJack Code

void updateBlackJack() { finishDealerHitting(); }

void hit() {
  if (numDrawnPlayer >= 10)
    return; // limit on-screen cards

  // Pick a random card index from deck
  int cardIndex;
  bool ok = false;
  while (!ok) {
    cardIndex = random(deckSize);
    ok = true;
    for (int i = 0; i < numDrawnPlayer; i++) {
      if (playerHand[i] == cardIndex) {
        ok = false;
        break;
      }
    }
  }
  playerHand[numDrawnPlayer++] = cardIndex;

  if (numDrawnDealer == 0) {
    determineDealerHand();
  }

  // ---- DRAW ALL CARDS ----
  DISPLAY_BJ.clearDisplay();

  // Calculate hand value
  int playerValue = calculateHandValue(playerHand, numDrawnPlayer);
  int dealerValue = calculateHandValue(dealerHand, numDrawnDealer);

  // DISPLAY_BJ hand value at top-left (margin 5 px)
  updateScoreDisplay(playerValue, dealerValue);

  drawHand(playerHand, numDrawnPlayer, 20);
  if (playerValue > 21) {
    Serial.println("Hand is over 21");
    drawBitmapImage(bust);
  }

  Serial.print("Hit! Drew card index: ");
  Serial.println(cardIndex);
  Serial.print("Current hand value: ");
  Serial.println(playerValue);
  DISPLAY_BJ.display();
}

void stand() {
  Serial.println("Action: STAND");
  printText(DISPLAY_BJ, "STAND!", 0, 0, 1);
  startDealerHitting();
}

void resetGame() {
  Serial.println("Action: RESET");
  numDrawnPlayer = 0;
  numDrawnDealer = 0;
  DISPLAY_BJ.clearDisplay();
  DISPLAY_BJ.display();
}

// handToCalculate is an array of indexes to look up what the drawn cards are
int calculateHandValue(int *handToCalculate, int drawnCards) {
  int total = 0;
  int aceCount = 0;

  for (int i = 0; i < drawnCards; i++) {
    const char *val = deck[handToCalculate[i]].value;

    if (strcmp(val, "A") == 0) {
      total += 11;
      aceCount++;
    } else if (strcmp(val, "K") == 0 || strcmp(val, "Q") == 0 ||
               strcmp(val, "J") == 0 || strcmp(val, "T") == 0) {
      total += 10;
    } else {
      total += atoi(val); // convert char* array to
    }
  }

  // Adjust for aces if total > 21
  while (total > 21 && aceCount > 0) {
    total -= 10;
    aceCount--;
  }

  return total;
}

int determineDealerHand() {
  int index = random(52);
  Serial.print("Dealer index: ");
  Serial.println(index);
  dealerHand[numDrawnDealer++] = index;
  int val = calculateHandValue(dealerHand, numDrawnDealer);
  Serial.print("Dealer val: ");
  Serial.println(val);
  return val;
}

void startDealerHitting() {
  dealerHitting = true;
  lastDealerHitTime = millis();
  Serial.println("Dealer starts hitting...");
  drawHand(dealerHand, numDrawnDealer, 20);
}

void finishDealerHitting() {
  if (!dealerHitting)
    return;

  unsigned long now = millis();

  // Wait between hits for animation
  if (now - lastDealerHitTime < dealerHitInterval)
    return;

  int dealerValue = calculateHandValue(dealerHand, numDrawnDealer);

  // Dealer stops hitting once 17 or higher
  if (dealerValue >= 17 && dealerValue <= 21) {
    dealerHitting = false;
    Serial.println("Dealer stands.");
    printText(DISPLAY_BJ, "Dealer stands", 5, 55, 1);
    handleGameEnd();
    return;
  } else if (dealerValue > 21) {
    dealerHitting = false;
    Serial.println("Dealer busts");
    printText(DISPLAY_BJ, "Dealer bust", 5, 55, 1);
    handleGameEnd();
    return;
  }

  int cardIndex;
  bool ok = false;
  while (!ok) {
    cardIndex = random(deckSize);
    ok = true;
    for (int i = 0; i < numDrawnDealer; i++) {
      if (dealerHand[i] == cardIndex) {
        ok = false;
        break;
      }
    }
  }
  dealerHand[numDrawnDealer++] = cardIndex;

  // Recalculate and show dealer + player values
  int playerValue = calculateHandValue(playerHand, numDrawnPlayer);
  dealerValue = calculateHandValue(dealerHand, numDrawnDealer);
  updateScoreDisplay(playerValue, dealerValue);

  // ---- DRAW DEALER CARDS ----
  drawHand(dealerHand, numDrawnDealer, 20);

  lastDealerHitTime = now;
}

void handleGameEnd() {
  int playerVal = calculateHandValue(playerHand, numDrawnPlayer);
  int dealerVal = calculateHandValue(dealerHand, numDrawnDealer);

  // 0 = player lose, 1 = push, 2 = player win, 3 = dealer bust

  if (playerVal > 21) {
    currentState = LOSE; // Player busts
  } else if (dealerVal > 21) {
    currentState = WIN; // Dealer busts
  } else if (playerVal > dealerVal) {
    currentState = WIN; // Player higher
  } else if (playerVal < dealerVal) {
    currentState = LOSE; // Dealer higher
  } else {
    currentState = TIE; // Equal value
  }

  // Optional: print result for debugging
  Serial.print("Player: ");
  Serial.print(playerVal);
  Serial.print(" | Dealer: ");
  Serial.print(dealerVal);
  Serial.print(" => State: ");
  delay(700);
  switch (currentState) {
  case WIN:
    Serial.println("WIN");
    drawBitmapImage(win);
    break;
  case LOSE:
    Serial.println("LOSE");
    drawBitmapImage(lost);
    break;
  case TIE:
    Serial.println("TIE");
    break;
  }
}

#pragma endregion

#pragma region Display helpers
// ---- DISPLAY_BJ HELPERS ----
void drawBitmapImage(const unsigned char *bitmap) {
  drawBitmapImage(bitmap, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void drawBitmapImage(const unsigned char *bitmap, int x, int y, int width,
                     int height) {
  DISPLAY_BJ.clearDisplay();
  DISPLAY_BJ.drawBitmap(x, y, bitmap, width, height, SSD1306_WHITE);
  DISPLAY_BJ.display();
}

// Draw a hand of cards at a given vertical position
void drawHand(int hand[], int numCards, int startY) {
  // Clear the area for this hand
  int handHeight = 32; // adjust to fit your cards
  DISPLAY_BJ.fillRect(0, startY, SCREEN_WIDTH, handHeight, BLACK);

  int startX = 5;          // left margin
  int spacing = 35;        // space between cards
  int cardWidth = 16 + 18; // 16 for suit + ~18 for text area
  int maxWidth = SCREEN_WIDTH;
  int rowHeight = 16;

  int x = startX;
  int y = startY;

  for (int i = 0; i < numCards; i++) {
    if (x + cardWidth > maxWidth) {
      x = startX;
      y += rowHeight;
    }

    drawCard(deck[hand[i]].value, deck[hand[i]].suit, x, y);
    x += spacing;
  }
}

void drawCard(const char *value, const unsigned char *suitBitmap, int x,
              int y) {
  DISPLAY_BJ.drawBitmap(x, y, suitBitmap, 16, 16, SSD1306_WHITE);
  DISPLAY_BJ.setTextSize(2);
  DISPLAY_BJ.setTextColor(SSD1306_WHITE);
  DISPLAY_BJ.setCursor(x + 18, y);
  DISPLAY_BJ.print(value);
  DISPLAY_BJ.display();
}

void clearArea(int x, int y, int w, int h) {
  DISPLAY_BJ.fillRect(x, y, w, h, BLACK);
  DISPLAY_BJ.display();
}

void updateScoreDisplay(int playerValue, int dealerValue) {
  clearArea(0, 0, SCREEN_WIDTH, 12);
  printText(DISPLAY_BJ,
            "P: " + String(playerValue) + "  |  D: " + String(dealerValue), 5,
            5, 1);
}

void printText(Adafruit_SSD1306 &display, String text, int x, int y, int size) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.print(text);
  display.display();
}

void displayDHTToOled(float temp, float humid) {
  DISPLAY_DHT.clearDisplay();
  String text =
      "Temp: " + String(temp) + "C\n" + " Humid: " + String(humid) + "%";
  printText(DISPLAY_DHT, text, 5, 20, 1);
}
#pragma endregion
