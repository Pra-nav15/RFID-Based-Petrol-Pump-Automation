#include <Arduino.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h> // Include Keypad library

// Pin Declarations
#define MFRC522_SDA_PIN         32
#define MFRC522_RST_PIN         27
#define FLAME_SENSOR_SIGNAL_PIN 36
#define RELAY_IN_PIN            5
#define BUZZER_PIN              14
#define TRIGGER_PIN             13
#define ECHO_PIN                12

// Keypad Pins
const byte ROWS = 4; //four rows
const byte COLS = 3; //three columns
char keys[ROWS][COLS] = {
  {'1','2','3'},
  {'4','5','6'},
  {'7','8','9'},
  {'*','0','#'}
};
byte rowPins[ROWS] = {35, 33, 25, 34}; //connect to the row pinouts of the keypad
byte colPins[COLS] = {15, 2, 4};        //connect to the column pinouts of the keypad

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// LCD Dimensions
#define SCREEN_WIDTH  16
#define SCREEN_HEIGHT 2

// Global Variables
char password[]     = "1234";
int  balanceAmount = 50000;

LiquidCrystal_I2C lcd(0x27, SCREEN_WIDTH, SCREEN_HEIGHT);
MFRC522 mfrc522(MFRC522_SDA_PIN, MFRC522_RST_PIN);

void setup() {
    pinMode(RELAY_IN_PIN, OUTPUT);
    digitalWrite(RELAY_IN_PIN, HIGH); // Turn relay on initially
    pinMode(FLAME_SENSOR_SIGNAL_PIN, INPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(TRIGGER_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    lcd.init();
    lcd.backlight();
    SPI.begin();
    mfrc522.PCD_Init();

       // Check flame sensor during entire program
    checkFlameSensor();
    // Check petrol level
    checkPetrolLevel();

    Serial.begin(9600);
    while (!Serial); // Wait for Serial to initialize

    lcd.clear();
    lcd.print("Welcome, scan");
    lcd.setCursor(0, 1);
    lcd.print("your Petro-card");
}

void loop() {
    char key = keypad.getKey(); // Check for keypad input
    if (key != NO_KEY) { // If key is pressed
        if (key == '#') { // '#' to confirm entry
        } else if (key == '*') { // '*' to clear input
            lcd.clear();
        } else {
            lcd.print('*'); // Print '*' instead of actual characters to LCD
        }
    }

    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Authenticating...");
        delay(2000);

        String rfidData = "";
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            rfidData += String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
            rfidData += String(mfrc522.uid.uidByte[i], HEX);
        }
        if (authenticateRFID(rfidData)) {
            lcd.clear();
            lcd.print("Access Granted");
            delay(2000);
            lcd.clear();
            lcd.print("Enter Password:");
            Serial.println("Enter Password:");

            String enteredPassword = readSerialUntilHash();
            if (enteredPassword == password) {
                lcd.clear();
                lcd.print("Password Correct");
                delay(2000);
                Serial.println("Enter Price :");
                lcd.clear();
                lcd.print("Enter the Price:");

                String price = readSerialUntilHash();
                float petrolPrice = price.toFloat();
                if (petrolPrice > balanceAmount) {
                    lcd.clear();
                    lcd.print("Not sufficient balance");
                } else {
                    executePumpingSequence(petrolPrice);
                }
                clearSerialBuffer(); // Clear Serial Monitor buffer
            } else {
                lcd.clear();
                lcd.print("Wrong Password");
                buzz(); // Buzz 3 times for wrong password
            }
        } else {
            lcd.clear();
            lcd.print("Invalid Card");
            buzz(); // Buzz 3 times for invalid card
        }
        delay(2000);
        lcd.clear();
        lcd.print("Welcome, scan");
        lcd.setCursor(0, 1);
        lcd.print("your Petro-card");
    }

    // Check flame sensor during entire program
    checkFlameSensor();
    // Check petrol level
    checkPetrolLevel();
}

String readSerialUntilHash() {
    String input = "";
    while (!Serial.available()); // Wait for input
    while (Serial.available()) {
        char c = Serial.read();
        if (c == ' ') break; // '#' to confirm input
        if (c == '.') { // '*' to clear input
            lcd.clear();
            continue;
        }
        if (c >= 32 && c <= 126) { // Printable characters
            input += c;
            lcd.print('*'); // Print '*' instead of actual characters to LCD
        }
        delay(10); // Debounce delay
    }
    return input;
}

bool authenticateRFID(String rfidData) {
    // Valid card UID: F31037DD
    return (rfidData.equalsIgnoreCase("F31037DD"));
}

void executePumpingSequence(float petrolPrice) {
    float petrolAmount = petrolPrice * 9.218;
    lcd.clear();
    lcd.print("Flow Rate:");
    lcd.setCursor(0, 1);
    lcd.print(petrolAmount);
    lcd.print(" ml");
    delay(2000);
    lcd.clear();
    lcd.print("Processing...");
    delay(2000);
    updateBalance(petrolPrice);

    // Display updated balance
    lcd.clear();
    lcd.print("Balance: ");
    lcd.print(balanceAmount);
    delay(2000);

    // Control relay based on petrol price
    controlRelay(petrolPrice);
}

void controlRelay(float price) {
    // Calculate time delays based on petrol price
    int onDelay  = int(price * 10); // Example: On delay in milliseconds is 10 times the price
    int offDelay = int(price * 20); // Example: Off delay in milliseconds is 20 times the price

    // Turn on the relay
    digitalWrite(RELAY_IN_PIN, HIGH);
    // Turn off the relay
    delay(500);
    digitalWrite(RELAY_IN_PIN, LOW);
    delay(offDelay);
    digitalWrite(RELAY_IN_PIN, HIGH);
    lcd.clear();
    lcd.print("Thank you for");
    lcd.setCursor(0, 1);
    lcd.print("Filling Fuel");
}

void buzz() {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(500);
    digitalWrite(BUZZER_PIN, LOW);
    delay(500);
}

void checkPetrolLevel() {
    // Simulated logic for checking petrol level
    float petrolLevel = measurePetrolLevel(); // Measure petrol level using ultrasonic sensor
    if (petrolLevel > 12) { // If petrol level is below 12 cm
        lcd.clear();
        lcd.print("Low Petrol Level");
        buzz(); // Buzz 3 times for low petrol level
    }
    else {
        // If petrol level is not low, display the welcome message
        lcd.clear();
        lcd.print("Welcome, scan");
        lcd.setCursor(0, 1);
        lcd.print("your Petro-card");
    }
}

float measurePetrolLevel() {
    // Triggering ultrasonic sensor to measure distance
    digitalWrite(TRIGGER_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIGGER_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIGGER_PIN, LOW);
    // Measuring the duration of the pulse from ECHO_PIN
    float duration = pulseIn(ECHO_PIN, HIGH);
    // Calculating distance based on duration
    float distance = duration * 0.034 / 2; // Speed of sound is 0.034 cm/µs
    return distance;
}

void checkFlameSensor() {
    int flameValue = analogRead(FLAME_SENSOR_SIGNAL_PIN);

    if (flameValue < 4095) {
        digitalWrite(BUZZER_PIN, HIGH);
        lcd.clear();
        lcd.print("Fire detected!");
    } else {
        digitalWrite(BUZZER_PIN, LOW);
    }
}

void updateBalance(float petrolPrice) {
    balanceAmount -= petrolPrice;
}

void clearSerialBuffer() {
    while (Serial.available()) {
        Serial.read(); // Clearing each character in the buffer
    }
}
