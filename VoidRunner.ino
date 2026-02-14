/*
* ARDUINO PROJECT: VOID RUNNER - Arduino LCD Obstacle Game
*
* ZACHARY SWAIN
*
* DESCRIPTION: 
* This is a side scrolling arcade game rendered on a standard 16x2 LCD.
* The player controls a character on the left side of the screen and must
* dodge incoming enemies scrolling from the right by switching rows.
*/


#include <LiquidCrystal.h>
#include <IRremote.h>

// --- CONFIGURATION & CONSTANTS ---
const int PIN_RS = 2, PIN_EN = 3, PIN_D4 = 4, PIN_D5 = 5, PIN_D6 = 6, PIN_D7 = 7;
const int PIN_JOY_Y = A0;
const int PIN_JOY_SW = 8;
const int PIN_TRIG = 9;
const int PIN_ECHO = 10;
const int PIN_IR = 11;
const int PIN_BUZZER = 12;
const int PIN_RED = A2, PIN_GRN = A3, PIN_BLU = A4;
const int LCD_COLS = 16;
const int LCD_ROWS = 2;
const int JOY_UP_THRESHOLD = 300;
const int JOY_DOWN_THRESHOLD = 700;

const int SHIELD_THRESHOLD_CM = 12;
const unsigned long SHIELD_DURATION_MS = 2000;
const unsigned long SHIELD_COOLDOWN_MS = 5000;
const unsigned long SHIELD_CHIME_INTERVAL = 400;
const unsigned long IR_DEBOUNCE_MS = 500;
const unsigned long POST_GAMEOVER_IR_GUARD_MS = 1500;

byte charRunner[8] = {0b01110, 0b01110, 0b00100, 0b01110, 0b10101, 0b00100, 0b01010, 0b10001};
byte charEnemy[8]  = {0b00000, 0b01110, 0b11111, 0b10101, 0b11111, 0b01110, 0b01010, 0b10001};

LiquidCrystal lcd(PIN_RS, PIN_EN, PIN_D4, PIN_D5, PIN_D6, PIN_D7);

enum GameState { LOCKED, STANDBY, MENU, PLAYING, GAMEOVER };
enum SoundType {
  SND_POWER_ON = 0,
  SND_CLICK = 1,
  SND_DIE = 2,
  SND_SHIELD_ACTIVE = 3,
  SND_SHIELD_END = 4,
  SND_SHIELD_READY = 5
};

struct Enemy {
  int col;
  int row;
};

// --- GLOBAL VARIABLES ---
GameState currentState = LOCKED;
unsigned long lastIrCode = 0;
unsigned long ignoreIrUntil = 0;

int playerRow = 0;
int score = 0;
int gameSpeed = 300;
unsigned long lastMoveTime = 0;
int lastSpeedUpScore = 0;

Enemy enemies[2]; // Two enemies

bool isShieldActive = false;
bool isShieldReady = true;
unsigned long shieldTimerEnd = 0;     // When the active shield turns off
unsigned long shieldCooldownEnd = 0;  // When the shield becomes ready again
unsigned long lastShieldChime = 0;
// Edge-detect joystick press so the loop never blocks on a held button.
bool wasJoyPressed = false;

// --- HELPERS ---
void setLed(int r, int g, int b) {
  digitalWrite(PIN_RED, r ? HIGH : LOW);
  digitalWrite(PIN_GRN, g ? HIGH : LOW);
  digitalWrite(PIN_BLU, b ? HIGH : LOW);
}

bool isCellOnLcd(int col, int row) {
  return col >= 0 && col < LCD_COLS && row >= 0 && row < LCD_ROWS;
}

void playSound(SoundType type) {
  switch (type) {
    case SND_POWER_ON: tone(PIN_BUZZER, 600, 80); delay(90); tone(PIN_BUZZER, 800, 140); break;
    case SND_CLICK: tone(PIN_BUZZER, 200, 120); break;
    case SND_DIE: tone(PIN_BUZZER, 150, 450); break;
    case SND_SHIELD_ACTIVE: tone(PIN_BUZZER, 1200, 60); break;
    case SND_SHIELD_END: tone(PIN_BUZZER, 400, 180); break;
    case SND_SHIELD_READY: tone(PIN_BUZZER, 900, 80);  break;
  }
}

int getDistance() {
  digitalWrite(PIN_TRIG, LOW); delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH); delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  long duration = pulseIn(PIN_ECHO, HIGH, 5000); 
  if (duration == 0) return 100; 
  return (int)(duration * 0.034 / 2.0);
}

// Reset all per-run gameplay variables before entering PLAYING.
void resetGameForPlay() {
  score = 0;
  gameSpeed = 300;
  lastSpeedUpScore = 0;
  lastMoveTime = millis();
  playerRow = 0;
  enemies[0] = {15, random(0, 2)};
  enemies[1] = {20, random(0, 2)};
  isShieldReady = true;
  isShieldActive = false;
  shieldTimerEnd = 0;
  lastShieldChime = 0;
}

// --- STATE MANAGEMENT ---
void changeState(GameState newState) {
  currentState = newState;
  lcd.clear();
  
  switch (currentState) {
    case LOCKED:
      lcd.print("SYSTEM LOCKED");
      lcd.setCursor(0, 1); lcd.print("Type START...");
      setLed(255, 0, 0); 
      break;
    case STANDBY:
      lcd.print("STANDBY MODE");
      lcd.setCursor(0, 1); lcd.print("Use Remote...");
      setLed(0, 0, 255); 
      playSound(SND_CLICK);
      break;
    case MENU:
      lcd.print("VOID RUNNER");
      lcd.setCursor(0, 1); lcd.print("Press Stick...");
      setLed(0, 255, 0); 
      playSound(SND_CLICK);
      break;
    case PLAYING:
      lcd.print("GO!");
      resetGameForPlay();
      playSound(SND_POWER_ON);
      delay(200);
      lcd.clear();
      break;
    case GAMEOVER:
      lcd.print("GAME OVER");
      lcd.setCursor(0, 1); lcd.print("Score: "); lcd.print(score);
      setLed(255, 0, 0); 
      playSound(SND_DIE);
      delay(2000);
      // Ignore repeated IR frames right after game over to avoid instant menu flip.
      ignoreIrUntil = millis() + POST_GAMEOVER_IR_GUARD_MS;
      lastIrCode = 0;
      changeState(MENU);
      break;
  }
}

// --- LOGIC HANDLERS ---
void handleSerial() {
  if (currentState == LOCKED && Serial.available() > 0) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.indexOf("START") > -1) {
      Serial.println(F("ACCESS GRANTED"));
      changeState(STANDBY);
      playSound(SND_POWER_ON);
    }
  }
}

void handleRemote(unsigned long now) {
  if (currentState == PLAYING || currentState == LOCKED) return;

  if (IrReceiver.decode()) {
    unsigned long code = IrReceiver.decodedIRData.decodedRawData;
    // Ignore IR repeat frames; only react to new, non-zero codes.
    bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT);
    if (!isRepeat && code != 0 && now > ignoreIrUntil && code != lastIrCode) {
      // Allow explicit two-way navigation between standby and menu.
      if (currentState == STANDBY) changeState(MENU);
      else if (currentState == MENU) changeState(STANDBY);
      lastIrCode = code;
      ignoreIrUntil = now + IR_DEBOUNCE_MS;
    }
    IrReceiver.resume();
  }
}

void handleShield(unsigned long now) {
  // Activation
  if (isShieldReady && !isShieldActive) {
    int dist = getDistance();
    if (dist <= SHIELD_THRESHOLD_CM) {
      isShieldActive = true;
      isShieldReady = false;
      shieldTimerEnd = now + SHIELD_DURATION_MS;
      shieldCooldownEnd = shieldTimerEnd + SHIELD_COOLDOWN_MS;
      lastShieldChime = now;
      setLed(255, 255, 0); // Yellow
      playSound(SND_SHIELD_ACTIVE);
    }
  }

  // Active State
  if (isShieldActive) {
    if (now >= shieldTimerEnd) {
      isShieldActive = false;
      setLed(255, 0, 0); // Red
      playSound(SND_SHIELD_END);
    } else {
      if (now - lastShieldChime >= SHIELD_CHIME_INTERVAL) {
        playSound(SND_SHIELD_ACTIVE);
        lastShieldChime = now;
      }
    }
  }

  // Cooldown State
  if (!isShieldActive && !isShieldReady) {
    if (now >= shieldCooldownEnd) {
      isShieldReady = true;
      setLed(0, 255, 0); // Green
      playSound(SND_SHIELD_READY);
    } else {
      if (currentState == PLAYING) setLed(0, 0, 255); // Blue indicator
    }
  }
}

void updateGamePhysics(unsigned long now) {
  // Joystick
  int joy = analogRead(PIN_JOY_Y);
  if (joy < JOY_UP_THRESHOLD) playerRow = 0;
  else if (joy > JOY_DOWN_THRESHOLD) playerRow = 1;

  // Movement Timer
  if (now - lastMoveTime > gameSpeed) {
    lastMoveTime = now;
    
    // Erase old positions
    if (isCellOnLcd(enemies[0].col, enemies[0].row)) {
      lcd.setCursor(enemies[0].col, enemies[0].row); lcd.print(" ");
    }
    if (isCellOnLcd(enemies[1].col, enemies[1].row)) {
      lcd.setCursor(enemies[1].col, enemies[1].row); lcd.print(" ");
    }

    // Update Logic
    for (int i = 0; i < 2; i++) {
      enemies[i].col--;
      
      // Respawn
      if (enemies[i].col < 0) {
        enemies[i].col = 15; 
        if (i == 1) enemies[i].col += random(2, 6); 
        enemies[i].row = random(0, 2);
        score++;
        
        if (score % 5 == 0 && score != lastSpeedUpScore) {
          gameSpeed = max(80, gameSpeed - 20);
          lastSpeedUpScore = score;
        }
      }
    }

    // Collision Check
    bool hit = false;
    for (int i = 0; i < 2; i++) {
      if (enemies[i].col == 0 && enemies[i].row == playerRow) {
        hit = true;
      }
    }

    if (hit && !isShieldActive) {
      changeState(GAMEOVER);
    } else {
      // Draw New
      lcd.setCursor(0, playerRow); lcd.write(byte(0)); 
      lcd.setCursor(0, (playerRow == 0 ? 1 : 0)); lcd.print(" "); 

      if (isCellOnLcd(enemies[0].col, enemies[0].row)) {
        lcd.setCursor(enemies[0].col, enemies[0].row); lcd.write(byte(1));
      }
      if (isCellOnLcd(enemies[1].col, enemies[1].row)) {
        lcd.setCursor(enemies[1].col, enemies[1].row); lcd.write(byte(1));
      }
    }
  }
}

// --- SETUP & LOOP ---
void setup() {
  Serial.begin(9600);
  pinMode(PIN_JOY_SW, INPUT_PULLUP);
  pinMode(PIN_TRIG, OUTPUT); pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_RED, OUTPUT); pinMode(PIN_GRN, OUTPUT); pinMode(PIN_BLU, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);

  lcd.begin(16, 2);
  lcd.createChar(0, charRunner); 
  lcd.createChar(1, charEnemy);
  
  randomSeed(analogRead(A1));
  IrReceiver.begin(PIN_IR, ENABLE_LED_FEEDBACK);
  
  changeState(LOCKED);
  Serial.println(F("BOOT COMPLETE"));
}

void loop() {
  unsigned long now = millis();

  // Global handlers run in every loop iteration.
  handleSerial();
  handleRemote(now);

  // State machine: process only the logic that belongs to the active state.
  if (currentState == MENU) {
    bool joyPressed = (digitalRead(PIN_JOY_SW) == LOW);
    if (joyPressed && !wasJoyPressed) {
      changeState(PLAYING);
    }
    wasJoyPressed = joyPressed;
  }
  else if (currentState == PLAYING) {
    wasJoyPressed = false;
    handleShield(now);
    updateGamePhysics(now);
  }
  else {
    wasJoyPressed = false;
  }
}
