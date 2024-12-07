#include <Wire.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include "pitches.h"

// OLED display dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

// TCA9548A I2C Multiplexer Address
#define TCA_ADDR 0x70

// Joystick Pins
#define VRX_PIN A0 // Joystick X-axis
#define VRY_PIN A1 // Joystick Y-axis
#define SWITCH_PIN 8 // Joystick button (click)

// Ultrasonic Sensor Pins
const int trigPins[3] = {2, 4, A3};  // Trigger pins for ultrasonic sensors
const int echoPins[3] = {3, 5, A2};  // Echo pins for ultrasonic sensors

// NRF24L01 Setup
RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001"; // Shared address for both sender and receiver

// Buzzer Pin
#define BUZZER_PIN 6

// Deadzone for Joystick
#define DEADZONE 100

// Variables for Joystick and Display
int selectedScreen = 0; // Initialize on Screen 1 (AirPods)
int displayMode = 0;    // 0 = Default Screen, 1 = Alarm Screen
unsigned long buttonPressStart = 0;
bool buttonHeld = false;

// Variables for Dock and Alarm
bool dockStatus[3] = {false, false, false}; // Dock statuses for 3 sensors
bool alarmStatus[3] = {false, false, false}; // Alarm statuses for 3 sensors
long remoteDistance = -1; // Default value for no signal

// Melody and Durations
int melody[] = {
  NOTE_D5, NOTE_D5, NOTE_D5, 
  NOTE_D5, NOTE_F5, NOTE_G5, NOTE_F5, 
  NOTE_D5, NOTE_F5, NOTE_G5, NOTE_F5, 
  NOTE_D5, NOTE_D5, NOTE_D5,
  NOTE_D5, NOTE_F5, NOTE_G5, NOTE_F5, 
  NOTE_D5, NOTE_F5, NOTE_G5, NOTE_F5
};

int durations[] = {
  450, 450, 450, 
  200, 200, 200, 200, 
  200, 200, 200, 200,
  450, 450, 450,
  200, 200, 200, 200, 
  200, 200, 200, 200
};

// Select TCA9548A Channel
void selectChannel(uint8_t channel) {
  Wire.beginTransmission(TCA_ADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
  delay(10); // Allow stabilization
}

// Measure Distance Using an Ultrasonic Sensor
long measureDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  return pulseIn(echoPin, HIGH) * 0.034 / 2;
}

// Update Dock Statuses for Local Sensors
void updateDockStatuses() {
  for (int i = 0; i < 3; i++) { // Only update statuses for 3 sensors
    dockStatus[i] = measureDistance(trigPins[i], echoPins[i]) < 10;
  }
}

// Play Melody on Buzzer
void playMelody() {
  for (int i = 0; i < sizeof(melody) / sizeof(melody[0]); i++) {
    tone(BUZZER_PIN, melody[i], durations[i]);
    delay(durations[i] * 1.1); // Pause between notes
  }
  noTone(BUZZER_PIN); // Ensure buzzer stops after melody
}

// Display Content on OLED
void displayContent(const char* title, bool dock, bool alarm, bool isSelected) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 1);
  display.println(title);

  if (isSelected && displayMode == 1) {
    display.setCursor(0, 24);
    display.println("Hold: On");
    display.setCursor(0, 47);
    display.println("Click: Off");
  } else {
    display.setCursor(0, 24);
    display.print("Dock: ");
    display.println(dock ? "Full" : "Open");
    display.setCursor(0, 47);
    display.print("Alarm: ");
    display.println(alarm ? "On" : "Off");
  }

  display.display();
}

// Display Current Screen Info on OLED 4
void displayScreenInfo(int screen) {
  selectChannel(2); // OLED 4 is on Channel 2
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.println("Screen:");
  
  display.setTextSize(2);
  display.setCursor(0, 20);

  const char* screenName = (screen == 0) ? "AirPods" : 
                           (screen == 1) ? "Wallet" : 
                           (screen == 2) ? "Phone" : "Unknown";

  display.println(screenName);

  display.setTextSize(2);
  display.setCursor(0, 45);
  char screenNumber[10];
  sprintf(screenNumber, "Screen %d", screen + 1); // Screen numbers are 1-based
  display.println(screenNumber);

  display.display();
}

// Handle Joystick Input
void handleJoystickInput() {
  int xValue = analogRead(VRX_PIN);
  int yValue = analogRead(VRY_PIN);
  int buttonState = digitalRead(SWITCH_PIN);

  // Navigate screens (skip OLED 4)
  if (xValue < 512 - DEADZONE && selectedScreen < 2) selectedScreen++; // Move right
  if (xValue > 512 + DEADZONE && selectedScreen > 0) selectedScreen--; // Move left

  // Navigate modes up and down
  if (yValue > 512 + DEADZONE) displayMode = 1; // Alarm Screen
  if (yValue < 512 - DEADZONE) displayMode = 0; // Default Screen

  // Handle button press and hold
  if (buttonState == LOW && !buttonHeld) {
    buttonPressStart = millis();
    buttonHeld = true;
  }

  if (buttonHeld) {
    if (millis() - buttonPressStart >= 3000 && displayMode == 1) {
      alarmStatus[selectedScreen] = !alarmStatus[selectedScreen]; // Toggle alarm status
      displayMode = 0; // Automatically return to default screen
    }
  }

  if (buttonState == HIGH) buttonHeld = false; // Reset hold state
}

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // Initialize ultrasonic sensor pins
  for (int i = 0; i < 3; i++) { // Only initialize pins for 3 sensors
    pinMode(trigPins[i], OUTPUT);
    pinMode(echoPins[i], INPUT);
  }
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(SWITCH_PIN, INPUT_PULLUP);

  // Initialize OLED screens
  for (int i = 0; i < 4; i++) {
    selectChannel(2 + i);
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
      Serial.print("OLED ");
      Serial.print(i + 1);
      Serial.println(" initialization failed!");
      while (true);
    }
    if (i == 0) displayScreenInfo(selectedScreen); // OLED 4
    else displayContent("OLED Ready", false, false, false);
  }

  // Initialize NRF24L01
  radio.begin();
  radio.openReadingPipe(0, address);
  radio.setPALevel(RF24_PA_LOW);
  radio.startListening(); // Set to receive mode
}

void loop() {
  updateDockStatuses(); // Update dock statuses for sensors

  // Check for data from sender
  if (radio.available()) {
    radio.read(&remoteDistance, sizeof(remoteDistance));
    Serial.print("Received Distance: ");
    Serial.println(remoteDistance);

    // Check if the door is open (distance > 10) and handle alarms
    if (remoteDistance > 100) { // Door is open
      for (int i = 0; i < 3; i++) {
        if (alarmStatus[i] && dockStatus[i]) {
          playMelody();
          break;
        }
      }
    }
  }

  // Process joystick input
  handleJoystickInput();

  // Update OLED displays
  for (int i = 0; i < 4; i++) {
    selectChannel(2 + i);
    if (i == 0) {
      displayScreenInfo(selectedScreen); // OLED 4 dynamically updates
    } else if (displayMode == 1 && selectedScreen == i - 1) {
      char alarmTitle[10];
      sprintf(alarmTitle, "Alarm %d", i);
      displayContent(alarmTitle, dockStatus[i - 1], alarmStatus[i - 1], true);
    } else {
      const char* defaultTitle = (i == 1) ? "AirPods" : (i == 2) ? "Wallet" : "Phone";
      displayContent(defaultTitle, dockStatus[i - 1], alarmStatus[i - 1], false);
    }
  }

  delay(100); // Stabilize loop
}
