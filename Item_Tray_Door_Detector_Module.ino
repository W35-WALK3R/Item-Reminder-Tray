#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>

// NRF24L01 setup
RF24 radio(9, 10); // CE, CSN
const byte address[6] = "00001"; // Shared address for both sender and receiver

// Ultrasonic sensor pins
#define TRIG_PIN 2
#define ECHO_PIN 3

// Function to measure distance using the ultrasonic sensor
long measureDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Calculate the distance (in cm)
  long duration = pulseIn(ECHO_PIN, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

void setup() {
  Serial.begin(9600);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Initialize NRF24L01
  radio.begin();
  radio.openWritingPipe(address); // Set up the pipe
  radio.setPALevel(RF24_PA_LOW);
  radio.stopListening(); // Set to transmit mode

  Serial.println("Ultrasonic Sensor Sender Initialized");
}

void loop() {
  // Measure distance
  long distance = measureDistance();

  // Send the distance to the receiver
  bool success = radio.write(&distance, sizeof(distance));
  
  // Print the result to the Serial Monitor
  if (success) {
    Serial.print("Distance sent: ");
    Serial.print(distance);
    Serial.println(" cm");
  } else {
    Serial.println("Failed to send distance!");
  }

  delay(500); // Send every 500 ms
}
