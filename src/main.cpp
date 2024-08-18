#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BluetoothSerial.h>
#include "esp_sleep.h"

// Define pin numbers for LEDs, buttons, and speaker
const uint8_t buttonPins[] = {32, 33, 25, 26};
const uint8_t ledPins[] = {18, 5, 17, 2};
#define BUZZER_PIN 27
#define PAUSE_BUTTON_PIN 34

// LCD I2C setup
LiquidCrystal_I2C lcd(0x27, 16, 2);  // I2C address 0x27, 16 columns, 2 rows

#define MAX_GAME_LENGTH 100

// Bluetooth setup
BluetoothSerial SerialBT;

// Global variables - store the game state
uint8_t gameSequence[MAX_GAME_LENGTH] = {0};
uint8_t gameIndex = 0;
uint8_t score = 0;
volatile bool isPaused = false;
unsigned int delayTime = 300; // Default delay time for medium difficulty

// ISR to handle pause button
void IRAM_ATTR handlePauseButton() {
  isPaused = !isPaused;
}

/**
   Set up the Arduino board and initialize Serial communication
*/
void setup() {
  Serial.begin(9600);
  SerialBT.begin("ESP32_Simon_Dice"); // Nombre del dispositivo Bluetooth
  for (byte i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP); 
  }
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(PAUSE_BUTTON_PIN, INPUT_PULLUP);
  
  // Attach interrupt to the pause button
  attachInterrupt(digitalPinToInterrupt(PAUSE_BUTTON_PIN), handlePauseButton, FALLING);

  // Initialize LCD
  Wire.begin(21, 22); // Specify SDA and SCL pins
  lcd.init();
  lcd.backlight();
  
  // Display waiting for Bluetooth message
  lcd.setCursor(0, 0);
  lcd.print("Esperando conexion");
  lcd.setCursor(0, 1);
  lcd.print("Bluetooth...");
  Serial.println("Esperando conexion Bluetooth...");

  // Wait for Bluetooth connection
  while (!SerialBT.hasClient()) {
    delay(100); // Check for connection every 100ms
  }

  // Display connected message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Conectado");
  Serial.println("Conectado");
  delay(2000);
  
  // Display difficulty selection message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Escoja");
  lcd.setCursor(0, 1);
  lcd.print("dificultad");

  // Wait for difficulty selection
  while (!SerialBT.available()) {
    delay(100); // Check for incoming data every 100ms
  }

  String difficulty = "";
  while (SerialBT.available()) {
    char ch = (char)SerialBT.read();
    difficulty += ch;
    delay(10); // Give some time for the rest of the message to arrive
  }

  // Set delay time based on difficulty
  if (difficulty.indexOf("Easy") != -1) {
    delayTime = 600;
  } else if (difficulty.indexOf("Medium") != -1) {
    delayTime = 300;
  } else if (difficulty.indexOf("Hard") != -1) {
    delayTime = 100;
  }

  // Display selected difficulty
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dificultad:");
  lcd.setCursor(0, 1);
  lcd.print(difficulty);
  Serial.println("Dificultad seleccionada: " + difficulty);
  delay(2000);
  
  // Prepare game start message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Simon Dice");
  
  // Seed the random number generator with millis() and analogRead()
  randomSeed(millis() + analogRead(4));
}


/**
   Lights the given LED and makes the buzzer vibrate
*/
void lightLedAndVibrate(byte ledIndex) {
  digitalWrite(ledPins[ledIndex], HIGH);
  digitalWrite(BUZZER_PIN, HIGH);
  delay(delayTime);
  digitalWrite(ledPins[ledIndex], LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

/**
   Plays the current sequence of lights that the user has to repeat
*/
void playSequence() {
  for (int i = 0; i < gameIndex; i++) {
    if (isPaused) return;  // Check for pause
    byte currentLed = gameSequence[i];
    lightLedAndVibrate(currentLed);
    delay(500);
  }
}

/**
    Waits until the user pressed one of the buttons,
    and returns the index of that button
*/
byte readButtons() {
  while (true) {
    if (isPaused) return 255;  // Special value to indicate pause
    for (byte i = 0; i < 4; i++) {
      byte buttonPin = buttonPins[i];
      if (digitalRead(buttonPin) == LOW) { 
        return i;
      }
    }
    delay(1);
  }
}

/**
  Play the game over sequence, and report the game score
*/
void gameOver() {

  
  // Enviar la puntuación al dispositivo Bluetooth

  SerialBT.println(score);

  gameIndex = 0;
  delay(200);

  // Play a Wah-Wah-Wah-Wah sequence
  for (byte i = 0; i < 4; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(100);
    digitalWrite(BUZZER_PIN, LOW);
    delay(100);
  }

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Perdiste :(");
  lcd.setCursor(0, 1);
  lcd.print("Puntuacion: ");
  lcd.print(score);
  delay(5000);
  lcd.clear();
  score = 0; // Reset score after displaying it
  
  // Ask for new difficulty level
  lcd.setCursor(0, 0);
  lcd.print("Escoja");
  lcd.setCursor(0, 1);
  lcd.print("dificultad");

  // Wait for difficulty selection
  while (!SerialBT.available()) {
    delay(100); // Check for incoming data every 100ms
  }

  String difficulty = "";
  while (SerialBT.available()) {
    char ch = (char)SerialBT.read();
    difficulty += ch;
    delay(10); // Give some time for the rest of the message to arrive
  }

  // Set delay time based on difficulty
  if (difficulty.indexOf("Easy") != -1) {
    delayTime = 600;
  } else if (difficulty.indexOf("Medium") != -1) {
    delayTime = 300;
  } else if (difficulty.indexOf("Hard") != -1) {
    delayTime = 100;
  }

  // Display selected difficulty
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dificultad:");
  lcd.setCursor(0, 1);
  lcd.print(difficulty);
  Serial.println("Dificultad seleccionada: " + difficulty);
  delay(2000);

  // Prepare game start message
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Simon Dice");
}

/**
   Get the user's input and compare it with the expected sequence.
*/
bool checkUserSequence() {
  for (int i = 0; i < gameIndex; i++) {
    byte expectedButton = gameSequence[i];
    byte actualButton = readButtons();
    if (actualButton == 255) return false;  // Paused
    lightLedAndVibrate(actualButton);
    if (expectedButton != actualButton) {
      return false;
    }
  }
  return true;
}

/**
   Plays a level-up sequence whenever the user finishes a level
*/
void playLevelUpSequence() {
  for (byte i = 0; i < 3; i++) {
    if (isPaused) return;  // Check for pause
    digitalWrite(BUZZER_PIN, HIGH);
    delay(150);
    digitalWrite(BUZZER_PIN, LOW);
    delay(150);
  }
}

/**
   The main game loop
*/
void loop() {
  if (isPaused) {
    // Display pause message
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Juego Pausado");

    // Wait until pause state is toggled
    while (isPaused) {
      delay(100);  // Wait until pause state is toggled
    }

    // Clear pause message and resume game
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Simon Dice");
    lcd.setCursor(0, 1);
    lcd.print("Puntuacion: ");
    lcd.print(score);
    delay(2000);
    lcd.clear();
  }

  // Add a random LED to the end of the sequence
  gameSequence[gameIndex] = random(0, 4);
  gameIndex++;
  if (gameIndex >= MAX_GAME_LENGTH) {
    gameIndex = MAX_GAME_LENGTH - 1;
  }

  // Display the sequence
  playSequence();
  if (isPaused) return;  // Check for pause
  if (!checkUserSequence()) {
    gameOver();
  } else {
    score++; // Increment score
    lcd.setCursor(0, 1);
    lcd.print("Puntuacion: ");
    lcd.print(score);
  }

  delay(300);
  if (isPaused) return;  // Check for pause

  if (gameIndex > 0) {
    playLevelUpSequence();
    delay(300);
  }
}
