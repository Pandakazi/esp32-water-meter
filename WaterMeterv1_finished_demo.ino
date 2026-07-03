#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <math.h>
#include <string.h>

// IdeaSpark ESP32 1.14" ST7789 135x240 pins
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   15
#define TFT_DC   2
#define TFT_RST  4
#define TFT_BL   32

// Button / jumper test pin
// Touch GPIO 13 to GND to simulate a button press
#define BUTTON_PIN 13

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_MOSI, TFT_SCLK, TFT_RST);

// -------------------------
// Fake water data
// -------------------------

int waterPercent = 63;
int daysLeft = 5;
int usedToday = 8;
int avgPerDay = 12;

// Fake future scale data
float rawWeightLb = 31.42;
float fullWeightLb = 42.10;
float emptyWeightLb = 2.30;

// Fake future system data
bool sensorOK = true;
bool wifiOK = false;
int sensorNoise = 2;
int wifiSignal = 0;

// -------------------------
// Screens
// -------------------------

int currentScreen = 0;
// 0 = Main
// 1 = Details
// 2 = Help

bool devMode = false;
int devScreen = 0;
// 0 = Dev weight screen
// 1 = Dev system screen

// Fake drain is OFF for now
bool fakeDrainEnabled = false;
unsigned long lastWaterUpdate = 0;

// -------------------------
// Button tracking
// -------------------------

bool lastButtonState = HIGH;
bool buttonPressed = false;
bool holdActionTriggered = false;

unsigned long buttonPressStart = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;

// Hold ring tracking
bool holdRingVisible = false;
unsigned long lastHoldRingUpdate = 0;
int lastHoldRingBucket = -1;

// Tap / double tap tracking
bool pendingSingleTap = false;
unsigned long lastTapReleaseTime = 0;
const unsigned long doubleTapWindow = 350;

// Hold thresholds
const unsigned long tareHoldTime = 3000;
const unsigned long fullHoldTime = 5000;
const unsigned long resetHoldTime = 10000;

// Temporary message tracking
bool showingMessage = false;
unsigned long messageStartTime = 0;
const unsigned long messageDuration = 2000;

// Bubble animation
unsigned long lastBubbleUpdate = 0;
const unsigned long bubbleInterval = 300;

bool bubblesInitialized = false;
int bubble1Y = 0;
int bubble2Y = 0;
int bubble3Y = 0;
int bubble4Y = 0;
int bubble5Y = 0;

// Details page animation
unsigned long lastDetailsAnimUpdate = 0;
const unsigned long detailsAnimInterval = 220;
int detailsAnimFrame = 0;

// Jug layout
const int jugX = 24;
const int jugY = 78;
const int jugW = 86;
const int jugH = 112;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting polished water jug UI...");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  pinMode(BUTTON_PIN, INPUT_PULLUP);

  tft.init(135, 240);

  // Portrait mode
  // If upside down, change this to 2
  tft.setRotation(0);

  drawSplashScreen();
  drawCurrentScreen();

  Serial.println("Ready.");
  Serial.println("Tap GPIO 13 to GND for button press.");
  Serial.println("Double tap for DEV mode.");
}

void loop() {
  handleButton();
  handlePendingSingleTap();
  handleFakeWaterDrop();
  handleBubbleAnimation();
  handleDetailsAnimation();
  handleMessageTimeout();
}

// -------------------------
// Splash screen
// -------------------------

void drawSplashScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  uint16_t cyanGlow   = tft.color565(80, 220, 255);
  uint16_t deepBlue   = tft.color565(0, 45, 95);
  uint16_t midBlue    = tft.color565(0, 90, 180);
  uint16_t brightBlue = tft.color565(0, 170, 255);
  uint16_t softWhite  = tft.color565(220, 245, 255);
  uint16_t darkBlue   = tft.color565(0, 20, 45);
  uint16_t glassBlue  = tft.color565(35, 70, 100);
  uint16_t shadow     = tft.color565(5, 10, 20);

  tft.setTextSize(2);
  tft.setTextColor(softWhite);
  tft.setCursor(8, 18);
  tft.println("Water");

  tft.setCursor(30, 40);
  tft.println("Meter");

  tft.setTextSize(1);
  tft.setTextColor(cyanGlow);
  tft.setCursor(52, 65);
  tft.println("v 1.00");

  int bootJugX = 25;
  int bootJugY = 93;
  int bootJugW = 86;
  int bootJugH = 88;

  int innerX = bootJugX + 6;
  int innerY = bootJugY + 6;
  int innerW = bootJugW - 12;
  int innerH = bootJugH - 12;

  int bottomY = innerY + innerH - 1;
  int topY = innerY + 1;

  tft.fillRoundRect(bootJugX + 4, bootJugY + 5, bootJugW, bootJugH, 12, shadow);

  tft.fillRoundRect(bootJugX + 25, bootJugY - 17, 36, 19, 5, shadow);
  tft.drawRoundRect(bootJugX + 25, bootJugY - 17, 36, 19, 5, softWhite);
  tft.drawRoundRect(bootJugX + 26, bootJugY - 16, 34, 17, 4, glassBlue);
  tft.drawFastHLine(bootJugX + 29, bootJugY - 11, 28, glassBlue);
  tft.drawFastHLine(bootJugX + 29, bootJugY - 6, 28, glassBlue);

  tft.drawRoundRect(bootJugX, bootJugY, bootJugW, bootJugH, 12, softWhite);
  tft.drawRoundRect(bootJugX + 1, bootJugY + 1, bootJugW - 2, bootJugH - 2, 11, glassBlue);

  tft.fillRoundRect(innerX, innerY, innerW, innerH, 9, darkBlue);

  for (int y = innerY + 8; y < innerY + innerH - 5; y += 12) {
    tft.drawFastHLine(innerX + 8, y, innerW - 16, tft.color565(15, 35, 55));
  }

  // Initial side shine
  tft.drawFastVLine(bootJugX + 13, bootJugY + 18, bootJugH - 35, tft.color565(110, 180, 210));
  tft.drawFastVLine(bootJugX + 14, bootJugY + 20, bootJugH - 42, tft.color565(50, 100, 130));

  drawSplashPercentInJug(innerX, innerY, innerW, innerH, 0);

  for (int y = bottomY; y >= topY; y--) {
    int progress = map(bottomY - y, 0, bottomY - topY, 0, 100);

    uint16_t fillColor;

    if (y > bottomY - 18) {
      fillColor = deepBlue;
    } else if (y < topY + 18) {
      fillColor = brightBlue;
    } else {
      fillColor = midBlue;
    }

    int inset = 0;

    if (y < innerY + 9) {
      inset = 9 - (y - innerY);
    }

    if (y > bottomY - 9) {
      inset = 9 - (bottomY - y);
    }

    if (inset < 0) {
      inset = 0;
    }

    int lineX = innerX + inset;
    int lineW = innerW - (inset * 2);

    if (lineW > 0) {
      tft.drawFastHLine(lineX, y, lineW, fillColor);
    }

    // Keep side shine visible as water fills over it
    tft.drawFastVLine(bootJugX + 13, bootJugY + 18, bootJugH - 35, tft.color565(110, 180, 210));
    tft.drawFastVLine(bootJugX + 14, bootJugY + 20, bootJugH - 42, tft.color565(50, 100, 130));

    if (progress % 4 == 0) {
      drawSplashPercentInJug(innerX, innerY, innerW, innerH, progress);
    }

    delay(18);
  }

  // Final side shine redraw
  tft.drawFastVLine(bootJugX + 13, bootJugY + 18, bootJugH - 35, tft.color565(110, 180, 210));
  tft.drawFastVLine(bootJugX + 14, bootJugY + 20, bootJugH - 42, tft.color565(50, 100, 130));

  drawSplashPercentInJug(innerX, innerY, innerW, innerH, 100);

  delay(250);

  tft.fillRect(42, 218, 60, 10, ST77XX_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(51, 218);
  tft.println("READY");

  delay(550);
}

void drawSplashPercentInJug(int innerX, int innerY, int innerW, int innerH, int progress) {
  uint16_t textBoxBg = tft.color565(0, 20, 45);
  uint16_t textBoxBorder = tft.color565(80, 220, 255);

  int boxX = innerX + 14;
  int boxY = innerY + 26;
  int boxW = innerW - 28;
  int boxH = 24;

  tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, textBoxBg);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, textBoxBorder);

  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);

  int textX;

  if (progress >= 100) {
    textX = innerX + 18;
  } else if (progress >= 10) {
    textX = innerX + 25;
  } else {
    textX = innerX + 31;
  }

  int textY = innerY + 31;

  tft.setCursor(textX, textY);
  tft.print(progress);
  tft.print("%");
}

// -------------------------
// Optional fake water drain
// -------------------------

void handleFakeWaterDrop() {
  if (!fakeDrainEnabled) {
    return;
  }

  if (showingMessage || devMode) {
    return;
  }

  if (millis() - lastWaterUpdate > 3000) {
    lastWaterUpdate = millis();

    waterPercent--;

    if (waterPercent < 20) {
      waterPercent = 83;
      usedToday = 0;
      avgPerDay = 12;
    }

    daysLeft = max(1, waterPercent / avgPerDay);
    usedToday = 100 - waterPercent;

    drawCurrentScreen();
  }
}

// -------------------------
// Bubble-only animation
// -------------------------

void handleBubbleAnimation() {
  if (showingMessage || devMode) {
    return;
  }

  if (currentScreen != 0) {
    return;
  }

  if (millis() - lastBubbleUpdate > bubbleInterval) {
    lastBubbleUpdate = millis();
    animateBubblesOnly();
  }
}

void animateBubblesOnly() {
  int innerX = jugX + 6;
  int innerY = jugY + 6;
  int innerW = jugW - 12;
  int innerH = jugH - 12;

  int fillH = map(waterPercent, 0, 100, 0, innerH);
  int fillY = innerY + innerH - fillH;
  int waterBottom = innerY + innerH;

  if (fillH < 20) {
    return;
  }

  uint16_t brightBlue = tft.color565(0, 170, 255);
  uint16_t cyanGlow = tft.color565(80, 220, 255);
  uint16_t softBlue = tft.color565(120, 230, 255);

  int bubble1X = innerX + 18;
  int bubble2X = innerX + 48;
  int bubble3X = innerX + 60;
  int bubble4X = innerX + 32;
  int bubble5X = innerX + 70;

  if (!bubblesInitialized) {
    bubble1Y = waterBottom - 12;
    bubble2Y = waterBottom - 35;
    bubble3Y = waterBottom - 55;
    bubble4Y = waterBottom - 22;
    bubble5Y = waterBottom - 48;
    bubblesInitialized = true;
  }

  eraseBubble(bubble1X, bubble1Y, 3, fillY, fillH);
  eraseBubble(bubble2X, bubble2Y, 2, fillY, fillH);
  eraseBubble(bubble3X, bubble3Y, 3, fillY, fillH);
  eraseBubble(bubble4X, bubble4Y, 4, fillY, fillH);
  eraseBubble(bubble5X, bubble5Y, 2, fillY, fillH);

  bubble1Y -= 3;
  bubble2Y -= 2;
  bubble3Y -= 4;
  bubble4Y -= 1;
  bubble5Y -= 3;

  if (bubble1Y <= fillY + 8) {
    bubble1Y = waterBottom - 8;
  }

  if (bubble2Y <= fillY + 8) {
    bubble2Y = waterBottom - 24;
  }

  if (bubble3Y <= fillY + 8) {
    bubble3Y = waterBottom - 40;
  }

  if (bubble4Y <= fillY + 8) {
    bubble4Y = waterBottom - 16;
  }

  if (bubble5Y <= fillY + 8) {
    bubble5Y = waterBottom - 34;
  }

  tft.fillCircle(bubble1X, bubble1Y, 2, cyanGlow);
  tft.fillCircle(bubble2X, bubble2Y, 1, ST77XX_WHITE);
  tft.fillCircle(bubble3X, bubble3Y, 2, brightBlue);
  tft.fillCircle(bubble4X, bubble4Y, 3, softBlue);
  tft.fillCircle(bubble5X, bubble5Y, 1, ST77XX_WHITE);
}

void eraseBubble(int x, int y, int r, int fillY, int fillH) {
  for (int dy = -r; dy <= r; dy++) {
    int lineY = y + dy;
    int dx = sqrt((r * r) - (dy * dy));
    uint16_t bandColor = getWaterColorAtY(lineY, fillY, fillH);

    tft.drawFastHLine(x - dx, lineY, dx * 2 + 1, bandColor);
  }
}

uint16_t getWaterColorAtY(int y, int fillY, int fillH) {
  uint16_t midBlue    = tft.color565(0, 90, 180);
  uint16_t brightBlue = tft.color565(0, 170, 255);
  uint16_t darkWater  = tft.color565(0, 55, 130);

  int bandH = max(1, fillH / 4);

  if (y < fillY + bandH + 1) {
    return brightBlue;
  }

  if (y >= fillY + fillH - bandH) {
    return darkWater;
  }

  return midBlue;
}

// -------------------------
// Details page animation
// -------------------------

void handleDetailsAnimation() {
  if (showingMessage || devMode) {
    return;
  }

  if (currentScreen != 1) {
    return;
  }

  if (millis() - lastDetailsAnimUpdate > detailsAnimInterval) {
    lastDetailsAnimUpdate = millis();

    detailsAnimFrame++;
    if (detailsAnimFrame > 1000) {
      detailsAnimFrame = 0;
    }

    drawDetailsBarsOnly();
  }
}

void drawDetailsBarsOnly() {
  drawWaterBar(70, 75, 42, 8, usedToday);
  drawWaterBar(70, 134, 42, 8, avgPerDay);

  int emptyBarPercent = constrain(daysLeft * 12, 0, 100);
  drawWaterBar(70, 193, 42, 8, emptyBarPercent);
}

void drawWaterBar(int x, int y, int w, int h, int percent) {
  uint16_t darkBlue   = tft.color565(0, 20, 45);
  uint16_t midBlue    = tft.color565(0, 90, 180);
  uint16_t brightBlue = tft.color565(0, 170, 255);
  uint16_t cyanGlow   = tft.color565(80, 220, 255);
  uint16_t borderBlue = tft.color565(35, 120, 170);

  int fillW = map(constrain(percent, 0, 100), 0, 100, 0, w - 4);

  tft.drawRoundRect(x, y, w, h, 4, borderBlue);
  tft.fillRoundRect(x + 2, y + 2, w - 4, h - 4, 2, darkBlue);

  if (fillW > 0) {
    tft.fillRoundRect(x + 2, y + 2, fillW, h - 4, 2, midBlue);

    if (fillW > 4) {
      tft.drawFastHLine(x + 3, y + 2, fillW - 2, brightBlue);
    }

    if (fillW > 8) {
      int shimmerX = x + 3 + (detailsAnimFrame % fillW);
      tft.drawFastVLine(shimmerX, y + 3, h - 6, cyanGlow);
    }
  }
}

// -------------------------
// Button handling
// -------------------------

void handleButton() {
  bool reading = digitalRead(BUTTON_PIN);

  if (reading != lastButtonState) {
    lastDebounceTime = millis();
  }

  if ((millis() - lastDebounceTime) > debounceDelay) {

    if (reading == LOW && !buttonPressed) {
      buttonPressed = true;
      holdActionTriggered = false;
      holdRingVisible = false;
      lastHoldRingBucket = -1;
      lastHoldRingUpdate = 0;
      buttonPressStart = millis();
      Serial.println("Button pressed");
    }

    if (reading == LOW && buttonPressed && !holdActionTriggered) {
      unsigned long holdDuration = millis() - buttonPressStart;

      drawHoldRing(holdDuration);

      if (devMode && holdDuration >= resetHoldTime) {
        holdActionTriggered = true;
        clearHoldRingState();
        restartBoard();
      } else if (!devMode && holdDuration >= fullHoldTime) {
        holdActionTriggered = true;
        clearHoldRingState();
        setFullJug();
      }
    }

    if (reading == HIGH && buttonPressed) {
      buttonPressed = false;

      unsigned long pressDuration = millis() - buttonPressStart;

      Serial.print("Button released after ");
      Serial.print(pressDuration);
      Serial.println(" ms");

      bool hadRing = holdRingVisible;
      clearHoldRingState();

      if (holdActionTriggered) {
        holdActionTriggered = false;
        lastButtonState = reading;
        return;
      }

      if (devMode) {
        if (pressDuration < tareHoldTime) {
          registerTap();
        } else if (hadRing) {
          drawCurrentScreen();
        }

        lastButtonState = reading;
        return;
      }

      if (pressDuration >= tareHoldTime) {
        tareScale();
      } else {
        registerTap();
      }
    }
  }

  lastButtonState = reading;
}

void drawHoldRing(unsigned long holdDuration) {
  if (showingMessage) {
    return;
  }

  // Do not show ring instantly for normal taps/double taps.
  if (holdDuration < 450) {
    return;
  }

  unsigned long targetTime = devMode ? resetHoldTime : fullHoldTime;

  // Update only once per second to stop flicker.
  int secondBucket = holdDuration / 1000;

  // Force final threshold update if we hit the target.
  if (holdDuration >= targetTime) {
    secondBucket = targetTime / 1000;
  }

  if (secondBucket == lastHoldRingBucket) {
    return;
  }

  lastHoldRingBucket = secondBucket;
  holdRingVisible = true;

  int displaySeconds = constrain(secondBucket, 0, targetTime / 1000);
  int progress = map(displaySeconds, 0, targetTime / 1000, 0, 100);

  uint16_t ringDim = tft.color565(25, 45, 65);
  uint16_t ringColor;
  uint16_t textColor;
  const char* label;

  char timeLabel[5];

  if (devMode) {
    if (holdDuration >= resetHoldTime) {
      ringColor = ST77XX_RED;
      textColor = ST77XX_RED;
      label = "RST";
    } else {
      ringColor = tft.color565(170, 80, 255);
      textColor = ST77XX_WHITE;

      snprintf(timeLabel, sizeof(timeLabel), "%ds", displaySeconds);
      label = timeLabel;
    }
  } else {
    if (holdDuration >= fullHoldTime) {
      ringColor = ST77XX_RED;
      textColor = ST77XX_RED;
      label = "FULL";
    } else if (holdDuration >= tareHoldTime) {
      ringColor = ST77XX_YELLOW;
      textColor = ST77XX_YELLOW;
      label = "TARE";
    } else {
      ringColor = ST77XX_CYAN;
      textColor = ST77XX_WHITE;

      snprintf(timeLabel, sizeof(timeLabel), "%ds", displaySeconds);
      label = timeLabel;
    }
  }

  // New position: upper-right corner.
  // Keeps it off the bottom status/footer area.
  int boxX = 94;
  int boxY = 3;
  int boxW = 41;
  int boxH = 41;

  int cx = 115;
  int cy = 23;
  int r = 13;

  tft.fillRect(boxX, boxY, boxW, boxH, ST77XX_BLACK);

  tft.fillCircle(cx, cy, 11, tft.color565(3, 18, 32));
  tft.drawCircle(cx, cy, 11, tft.color565(35, 70, 100));

  int totalSegments = 20;
  int filledSegments = map(progress, 0, 100, 0, totalSegments);

  for (int i = 0; i < totalSegments; i++) {
    float angle = (-90.0 + (i * (360.0 / totalSegments))) * PI / 180.0;
    int dotX = cx + cos(angle) * r;
    int dotY = cy + sin(angle) * r;

    if (i < filledSegments) {
      tft.fillCircle(dotX, dotY, 2, ringColor);
    } else {
      tft.fillCircle(dotX, dotY, 1, ringDim);
    }
  }

  tft.setTextWrap(false);
  tft.setTextSize(1);
  tft.setTextColor(textColor);

  int labelLen = strlen(label);
  int textX = cx - (labelLen * 3);
  int textY = cy - 3;

  tft.setCursor(textX, textY);
  tft.print(label);
}

void clearHoldRingState() {
  holdRingVisible = false;
  lastHoldRingBucket = -1;
  lastHoldRingUpdate = 0;
}

void registerTap() {
  unsigned long now = millis();

  if (pendingSingleTap && (now - lastTapReleaseTime <= doubleTapWindow)) {
    pendingSingleTap = false;
    handleDoubleTap();
  } else {
    pendingSingleTap = true;
    lastTapReleaseTime = now;
  }
}

void handlePendingSingleTap() {
  if (pendingSingleTap && (millis() - lastTapReleaseTime > doubleTapWindow)) {
    pendingSingleTap = false;
    handleSingleTap();
  }
}

void handleSingleTap() {
  if (showingMessage) {
    return;
  }

  if (devMode) {
    nextDevScreen();
  } else {
    nextScreen();
  }
}

void handleDoubleTap() {
  if (showingMessage) {
    return;
  }

  if (devMode) {
    exitDevMode();
  } else {
    enterDevMode();
  }
}

void nextScreen() {
  currentScreen++;

  if (currentScreen > 2) {
    currentScreen = 0;
  }

  drawCurrentScreen();
}

void enterDevMode() {
  Serial.println("ENTER DEV MODE");

  devMode = true;
  devScreen = 0;
  pendingSingleTap = false;

  drawCurrentScreen();
}

void exitDevMode() {
  Serial.println("EXIT DEV MODE");

  devMode = false;
  currentScreen = 0;
  pendingSingleTap = false;

  drawCurrentScreen();
}

void nextDevScreen() {
  devScreen++;

  if (devScreen > 1) {
    devScreen = 0;
  }

  drawCurrentScreen();
}

// -------------------------
// Temporary message screens
// -------------------------

void handleMessageTimeout() {
  if (showingMessage && millis() - messageStartTime > messageDuration) {
    showingMessage = false;
    drawCurrentScreen();
  }
}

void tareScale() {
  Serial.println("TARE SET");

  showingMessage = true;
  messageStartTime = millis();

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(3);
  tft.setTextColor(ST77XX_CYAN);
  tft.setCursor(25, 60);
  tft.println("TARE");

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(30, 105);
  tft.println("ZERO SET");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(16, 145);
  tft.println("Empty platform saved");
}

void setFullJug() {
  Serial.println("FULL JUG SET");

  waterPercent = 100;
  daysLeft = 8;
  usedToday = 0;
  avgPerDay = 12;

  showingMessage = true;
  messageStartTime = millis();

  drawRefillAnimation();

  showingMessage = false;
  drawCurrentScreen();
}

void drawRefillAnimation() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  uint16_t cyanGlow   = tft.color565(80, 220, 255);
  uint16_t deepBlue   = tft.color565(0, 45, 95);
  uint16_t midBlue    = tft.color565(0, 90, 180);
  uint16_t brightBlue = tft.color565(0, 170, 255);
  uint16_t softWhite  = tft.color565(220, 245, 255);
  uint16_t darkBlue   = tft.color565(0, 20, 45);
  uint16_t glassBlue  = tft.color565(35, 70, 100);
  uint16_t shadow     = tft.color565(5, 10, 20);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(25, 14);
  tft.println("REFILL");

  tft.setTextSize(1);
  tft.setTextColor(cyanGlow);
  tft.setCursor(35, 38);
  tft.println("SETTING FULL");

  int refillJugX = 25;
  int refillJugY = 79;
  int refillJugW = 86;
  int refillJugH = 112;

  int innerX = refillJugX + 6;
  int innerY = refillJugY + 6;
  int innerW = refillJugW - 12;
  int innerH = refillJugH - 12;

  int bottomY = innerY + innerH - 1;
  int topY = innerY + 1;

  tft.fillRoundRect(refillJugX + 4, refillJugY + 5, refillJugW, refillJugH, 12, shadow);

  tft.fillRoundRect(refillJugX + 25, refillJugY - 18, 36, 20, 5, shadow);
  tft.drawRoundRect(refillJugX + 25, refillJugY - 18, 36, 20, 5, softWhite);
  tft.drawRoundRect(refillJugX + 26, refillJugY - 17, 34, 18, 4, glassBlue);
  tft.drawFastHLine(refillJugX + 29, refillJugY - 12, 28, glassBlue);
  tft.drawFastHLine(refillJugX + 29, refillJugY - 7, 28, glassBlue);

  tft.drawRoundRect(refillJugX, refillJugY, refillJugW, refillJugH, 12, softWhite);
  tft.drawRoundRect(refillJugX + 1, refillJugY + 1, refillJugW - 2, refillJugH - 2, 11, glassBlue);

  tft.fillRoundRect(innerX, innerY, innerW, innerH, 9, darkBlue);

  for (int y = innerY + 8; y < innerY + innerH - 5; y += 14) {
    tft.drawFastHLine(innerX + 8, y, innerW - 16, tft.color565(15, 35, 55));
  }

  tft.drawFastVLine(refillJugX + 13, refillJugY + 18, refillJugH - 35, tft.color565(110, 180, 210));
  tft.drawFastVLine(refillJugX + 14, refillJugY + 20, refillJugH - 42, tft.color565(50, 100, 130));

  drawRefillPercentInJug(innerX, innerY, innerW, innerH, 0);

  for (int y = bottomY; y >= topY; y--) {
    int progress = map(bottomY - y, 0, bottomY - topY, 0, 100);

    uint16_t fillColor;

    if (y > bottomY - 24) {
      fillColor = deepBlue;
    } else if (y < topY + 24) {
      fillColor = brightBlue;
    } else {
      fillColor = midBlue;
    }

    int inset = 0;

    if (y < innerY + 9) {
      inset = 9 - (y - innerY);
    }

    if (y > bottomY - 9) {
      inset = 9 - (bottomY - y);
    }

    if (inset < 0) {
      inset = 0;
    }

    int lineX = innerX + inset;
    int lineW = innerW - (inset * 2);

    if (lineW > 0) {
    tft.drawFastHLine(lineX, y, lineW, fillColor);
  }

    // Keep side shine visible as water fills over it
     tft.drawFastVLine(refillJugX + 13, refillJugY + 18, refillJugH - 35, tft.color565(110, 180, 210));
    tft.drawFastVLine(refillJugX + 14, refillJugY + 20, refillJugH - 42, tft.color565(50, 100, 130));

  if (progress % 4 == 0) {
  drawRefillPercentInJug(innerX, innerY, innerW, innerH, progress);
}

    delay(14);
  }

  drawRefillPercentInJug(innerX, innerY, innerW, innerH, 100);

  delay(250);

  tft.fillRoundRect(26, 204, 84, 24, 7, tft.color565(3, 18, 32));
  tft.drawRoundRect(26, 204, 84, 24, 7, ST77XX_GREEN);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_GREEN);
  tft.setCursor(37, 209);
  tft.println("SAVED");

  delay(800);
}

void drawRefillPercentInJug(int innerX, int innerY, int innerW, int innerH, int progress) {
  uint16_t textBoxBg = tft.color565(0, 20, 45);
  uint16_t textBoxBorder = tft.color565(80, 220, 255);

  int boxX = innerX + 14;
  int boxY = innerY + 39;
  int boxW = innerW - 28;
  int boxH = 24;

  tft.fillRoundRect(boxX, boxY, boxW, boxH, 6, textBoxBg);
  tft.drawRoundRect(boxX, boxY, boxW, boxH, 6, textBoxBorder);

  tft.setTextWrap(false);
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);

  int textX;

  if (progress >= 100) {
    textX = innerX + 18;
  } else if (progress >= 10) {
    textX = innerX + 25;
  } else {
    textX = innerX + 31;
  }

  int textY = innerY + 44;

  tft.setCursor(textX, textY);
  tft.print(progress);
  tft.print("%");
}

void restartBoard() {
  Serial.println("RESTARTING BOARD");

  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  tft.setTextSize(2);
  tft.setTextColor(ST77XX_RED);
  tft.setCursor(11, 70);
  tft.println("RESTARTING");

  tft.setTextSize(1);
  tft.setTextColor(ST77XX_WHITE);
  tft.setCursor(27, 112);
  tft.println("ESP32 rebooting...");

  delay(800);

  ESP.restart();
}

// -------------------------
// Screen router
// -------------------------

void drawCurrentScreen() {
  if (showingMessage) {
    return;
  }

  if (devMode) {
    if (devScreen == 0) {
      drawDevWeightScreen();
    } else {
      drawDevSystemScreen();
    }

    return;
  }

  if (currentScreen == 0) {
    drawMainScreen();
  } else if (currentScreen == 1) {
    drawDetailsScreen();
  } else if (currentScreen == 2) {
    drawHelpScreen();
  }
}

// -------------------------
// Main screen
// -------------------------

void drawMainScreen() {
  tft.fillScreen(ST77XX_BLACK);

  bubblesInitialized = false;

  drawBigPercent();
  drawJug();
  drawDaysLeft();
}

void drawBigPercent() {
  tft.setTextWrap(false);

  int textSize = 5;
  tft.setTextSize(textSize);

  int charW = 6 * textSize;
  int jugCenterX = jugX + (jugW / 2);

  if (waterPercent > 50) {
    tft.setTextColor(ST77XX_GREEN);
  } else if (waterPercent > 25) {
    tft.setTextColor(ST77XX_YELLOW);
  } else {
    tft.setTextColor(ST77XX_RED);
  }

  int xPos;

  if (waterPercent >= 100) {
    xPos = jugCenterX - (2 * charW);
  } else if (waterPercent >= 10) {
    xPos = jugCenterX - charW - (charW / 2);
  } else {
    xPos = jugCenterX - charW;
  }

  tft.setCursor(xPos, 15);
  tft.print(waterPercent);
  tft.print("%");
}

void drawJug() {
  int innerX = jugX + 6;
  int innerY = jugY + 6;
  int innerW = jugW - 12;
  int innerH = jugH - 12;

  uint16_t darkBlue   = tft.color565(0, 20, 45);
  uint16_t midBlue    = tft.color565(0, 90, 180);
  uint16_t brightBlue = tft.color565(0, 170, 255);
  uint16_t cyanGlow   = tft.color565(80, 220, 255);
  uint16_t glassBlue  = tft.color565(35, 70, 100);
  uint16_t shadow     = tft.color565(8, 8, 18);
  uint16_t dimLine    = tft.color565(45, 55, 65);

  tft.fillRoundRect(jugX + 4, jugY + 5, jugW, jugH, 12, shadow);

  tft.drawRoundRect(jugX, jugY, jugW, jugH, 12, ST77XX_WHITE);
  tft.drawRoundRect(jugX + 1, jugY + 1, jugW - 2, jugH - 2, 11, glassBlue);

  tft.fillRoundRect(jugX + 25, jugY - 18, 36, 20, 5, shadow);
  tft.drawRoundRect(jugX + 25, jugY - 18, 36, 20, 5, ST77XX_WHITE);
  tft.drawFastHLine(jugX + 29, jugY - 12, 28, glassBlue);
  tft.drawFastHLine(jugX + 29, jugY - 7, 28, glassBlue);

  tft.fillRoundRect(innerX, innerY, innerW, innerH, 9, darkBlue);

  int fillH = map(waterPercent, 0, 100, 0, innerH);
  int fillY = innerY + innerH - fillH;

  if (fillH > 0) {
    int bandH = max(1, fillH / 4);

    tft.fillRoundRect(innerX, fillY, innerW, fillH, 8, midBlue);

    tft.fillRect(innerX, fillY + fillH - bandH, innerW, bandH, tft.color565(0, 55, 130));
    tft.fillRect(innerX, fillY + bandH, innerW, bandH * 2, midBlue);
    tft.fillRect(innerX, fillY, innerW, bandH + 2, brightBlue);

    tft.drawFastHLine(innerX + 5, fillY, innerW - 10, ST77XX_WHITE);
    tft.drawFastHLine(innerX + 8, fillY + 2, innerW - 16, cyanGlow);
  }

  for (int y = innerY + 8; y < fillY; y += 14) {
    tft.drawFastHLine(innerX + 8, y, innerW - 16, dimLine);
  }

  for (int i = 0; i <= 4; i++) {
    int tickY = innerY + map(i, 0, 4, 0, innerH);
    int tickLen = (i % 2 == 0) ? 8 : 5;

    tft.drawFastHLine(jugX - tickLen - 2, tickY, tickLen, ST77XX_WHITE);
  }

  tft.drawFastVLine(jugX + 13, jugY + 18, jugH - 35, tft.color565(110, 180, 210));
  tft.drawFastVLine(jugX + 14, jugY + 20, jugH - 42, tft.color565(50, 100, 130));

  if (waterPercent > 50) {
    tft.drawFastHLine(jugX + 15, jugY + jugH + 4, jugW - 30, ST77XX_GREEN);
  } else if (waterPercent > 25) {
    tft.drawFastHLine(jugX + 15, jugY + jugH + 4, jugW - 30, ST77XX_YELLOW);
  } else {
    tft.drawFastHLine(jugX + 15, jugY + jugH + 4, jugW - 30, ST77XX_RED);
  }
}

void drawDaysLeft() {
  tft.setTextSize(2);
  tft.setTextColor(ST77XX_WHITE);

  tft.setCursor(30, 198);
  tft.print(daysLeft);
  tft.print(" DAYS");

  tft.setCursor(43, 216);
  tft.print("LEFT");
}

// -------------------------
// Details screen
// -------------------------

void drawDetailsScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  uint16_t cyanGlow = tft.color565(80, 220, 255);
  uint16_t dimBlue  = tft.color565(20, 45, 70);

  tft.setTextSize(2);
  tft.setTextColor(cyanGlow);
  tft.setCursor(9, 8);
  tft.println("WATER DATA");

  tft.drawFastHLine(10, 32, 115, dimBlue);
  tft.drawFastHLine(20, 35, 95, cyanGlow);

  drawStatCard(10, 44, 115, 48, "USED TODAY", usedToday, "%", usedToday);
  drawStatCard(10, 103, 115, 48, "AVG / DAY", avgPerDay, "%", avgPerDay);

  int emptyBarPercent = constrain(daysLeft * 12, 0, 100);
  drawStatCard(10, 162, 115, 52, "EMPTY IN", daysLeft, "DAYS", emptyBarPercent);

  drawDetailsBarsOnly();
}

void drawStatCard(int x, int y, int w, int h, const char* label, int value, const char* suffix, int barPercent) {
  uint16_t cardBorder = tft.color565(35, 120, 170);
  uint16_t cardShadow = tft.color565(5, 10, 20);
  uint16_t cardFill   = tft.color565(3, 18, 32);
  uint16_t cyanGlow   = tft.color565(80, 220, 255);
  uint16_t softWhite  = tft.color565(220, 245, 255);

  tft.fillRoundRect(x + 3, y + 3, w, h, 8, cardShadow);
  tft.fillRoundRect(x, y, w, h, 8, cardFill);
  tft.drawRoundRect(x, y, w, h, 8, cardBorder);

  tft.drawFastHLine(x + 6, y + 4, 16, cyanGlow);
  tft.drawFastHLine(x + w - 22, y + h - 5, 16, cyanGlow);

  tft.setTextSize(1);
  tft.setTextColor(cyanGlow);
  tft.setCursor(x + 8, y + 8);
  tft.print(label);

  tft.setTextSize(2);
  tft.setTextColor(softWhite);
  tft.setCursor(x + 8, y + 24);
  tft.print(value);

  if (strcmp(suffix, "DAYS") == 0) {
    tft.setTextSize(1);
    tft.setCursor(x + 36, y + 29);
    tft.print("DAYS");
  } else {
    tft.print("%");
  }
}

// -------------------------
// Help screen
// -------------------------

void drawHelpScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  uint16_t cyanGlow = tft.color565(80, 220, 255);
  uint16_t dimBlue  = tft.color565(20, 45, 70);

  tft.setTextSize(2);
  tft.setTextColor(cyanGlow);
  tft.setCursor(6, 8);
  tft.println("CONTROL");

  tft.setCursor(43, 29);
  tft.println("HELP");

  tft.drawFastHLine(10, 54, 115, dimBlue);
  tft.drawFastHLine(28, 57, 79, cyanGlow);

  drawHelpCommandCard(10, 68, 115, 37, "TAP", "NEXT SCREEN", ST77XX_GREEN, 1);
  drawHelpCommandCard(10, 113, 115, 37, "HOLD 3s", "TARE SCALE", ST77XX_YELLOW, 2);
  drawHelpCommandCard(10, 158, 115, 37, "HOLD 5s", "AUTO FULL", ST77XX_RED, 3);
  drawHelpCommandCard(10, 203, 115, 30, "2x TAP", "DEV MODE", ST77XX_CYAN, 4);
}

void drawHelpCommandCard(int x, int y, int w, int h, const char* action, const char* meaning, uint16_t accentColor, int iconType) {
  uint16_t cardBorder = tft.color565(35, 120, 170);
  uint16_t cardShadow = tft.color565(5, 10, 20);
  uint16_t cardFill   = tft.color565(3, 18, 32);
  uint16_t cyanGlow   = tft.color565(80, 220, 255);
  uint16_t softWhite  = tft.color565(220, 245, 255);

  tft.fillRoundRect(x + 3, y + 3, w, h, 8, cardShadow);
  tft.fillRoundRect(x, y, w, h, 8, cardFill);
  tft.drawRoundRect(x, y, w, h, 8, cardBorder);

  tft.fillRoundRect(x + 4, y + 6, 5, h - 12, 3, accentColor);

  int iconX = x + 22;
  int iconY = y + h / 2;

  tft.drawCircle(iconX, iconY, 8, accentColor);

  if (iconType == 1) {
    tft.fillCircle(iconX, iconY, 3, accentColor);
  } else if (iconType == 2) {
    tft.fillCircle(iconX - 4, iconY, 2, accentColor);
    tft.fillCircle(iconX, iconY, 2, accentColor);
    tft.fillCircle(iconX + 4, iconY, 2, accentColor);
  } else if (iconType == 3) {
    tft.fillCircle(iconX, iconY, 4, accentColor);
    tft.drawCircle(iconX, iconY, 11, accentColor);
  } else {
    tft.fillCircle(iconX - 3, iconY, 2, accentColor);
    tft.fillCircle(iconX + 3, iconY, 2, accentColor);
  }

  tft.setTextSize(1);
  tft.setTextColor(cyanGlow);
  tft.setCursor(x + 40, y + 7);
  tft.print(action);

  tft.setTextColor(softWhite);
  tft.setCursor(x + 40, y + 22);
  tft.print(meaning);
}

// -------------------------
// Secret DEV screens
// -------------------------

void drawDevWeightScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  uint16_t purple = tft.color565(170, 80, 255);
  uint16_t cyanGlow = tft.color565(80, 220, 255);
  uint16_t dimBlue = tft.color565(20, 45, 70);
  uint16_t softWhite = tft.color565(220, 245, 255);

  tft.setTextSize(2);
  tft.setTextColor(purple);
  tft.setCursor(18, 8);
  tft.println("DEV MODE");

  tft.setTextSize(1);
  tft.setTextColor(cyanGlow);
  tft.setCursor(31, 31);
  tft.println("WEIGHT DATA");

  tft.drawFastHLine(10, 45, 115, dimBlue);
  tft.drawFastHLine(28, 48, 79, purple);

  drawDevValueCard(10, 61, 115, 42, "RAW WT", rawWeightLb, "LB", cyanGlow);
  drawDevValueCard(10, 112, 115, 42, "FULL", fullWeightLb, "LB", ST77XX_GREEN);
  drawDevValueCard(10, 163, 115, 42, "EMPTY", emptyWeightLb, "LB", ST77XX_RED);

  tft.setTextSize(1);
  tft.setTextColor(softWhite);
tft.setCursor(14, 216);
tft.println("Tap: dev help");

tft.setCursor(18, 228);
tft.println("2x tap: exit");
}

void drawDevSystemScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(false);

  uint16_t purple = tft.color565(170, 80, 255);
  uint16_t cyanGlow = tft.color565(80, 220, 255);
  uint16_t dimBlue = tft.color565(20, 45, 70);

  tft.setTextSize(2);
  tft.setTextColor(purple);
  tft.setCursor(18, 8);
  tft.println("DEV MODE");

  tft.setTextSize(1);
  tft.setTextColor(cyanGlow);
  tft.setCursor(42, 31);
  tft.println("HELP");

  tft.drawFastHLine(10, 45, 115, dimBlue);
  tft.drawFastHLine(28, 48, 79, purple);

  drawHelpCommandCard(10, 61, 115, 37, "TAP", "WEIGHT DATA", ST77XX_GREEN, 1);
  drawHelpCommandCard(10, 106, 115, 37, "2x TAP", "EXIT DEV", ST77XX_CYAN, 4);
  drawHelpCommandCard(10, 151, 115, 37, "HOLD 10s", "RESTART", ST77XX_RED, 3);

  tft.setTextSize(1);
  tft.setTextColor(tft.color565(220, 245, 255));
  tft.setCursor(17, 214);
  tft.println("Dev tools only");

  tft.setCursor(20, 228);
  tft.println("No scale data yet");
}

void drawDevValueCard(int x, int y, int w, int h, const char* label, float value, const char* suffix, uint16_t accent) {
  uint16_t cardBorder = tft.color565(35, 120, 170);
  uint16_t cardShadow = tft.color565(5, 10, 20);
  uint16_t cardFill = tft.color565(3, 18, 32);
  uint16_t softWhite = tft.color565(220, 245, 255);

  tft.fillRoundRect(x + 3, y + 3, w, h, 8, cardShadow);
  tft.fillRoundRect(x, y, w, h, 8, cardFill);
  tft.drawRoundRect(x, y, w, h, 8, cardBorder);

  tft.fillRoundRect(x + 5, y + 7, 5, h - 14, 3, accent);

  tft.setTextSize(1);
  tft.setTextColor(accent);
  tft.setCursor(x + 17, y + 7);
  tft.print(label);

  tft.setTextSize(2);
  tft.setTextColor(softWhite);
  tft.setCursor(x + 17, y + 22);
  tft.print(value, 1);

  tft.setTextSize(1);
  tft.setCursor(x + 78, y + 27);
  tft.print(suffix);
}

void drawDevStatusCard(int x, int y, int w, int h, const char* label, const char* status, uint16_t accent) {
  uint16_t cardBorder = tft.color565(35, 120, 170);
  uint16_t cardShadow = tft.color565(5, 10, 20);
  uint16_t cardFill = tft.color565(3, 18, 32);
  uint16_t softWhite = tft.color565(220, 245, 255);

  tft.fillRoundRect(x + 3, y + 3, w, h, 8, cardShadow);
  tft.fillRoundRect(x, y, w, h, 8, cardFill);
  tft.drawRoundRect(x, y, w, h, 8, cardBorder);

  tft.drawCircle(x + 22, y + 25, 11, accent);
  tft.fillCircle(x + 22, y + 25, 5, accent);

  tft.setTextSize(1);
  tft.setTextColor(accent);
  tft.setCursor(x + 42, y + 11);
  tft.print(label);

  tft.setTextSize(2);
  tft.setTextColor(softWhite);
  tft.setCursor(x + 42, y + 26);
  tft.print(status);
}