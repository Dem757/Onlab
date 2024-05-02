#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <DHT.h>

#define SS_PIN 5
#define RST_PIN 0
#define DHT_B_PIN 32
#define DHT_R_PIN 2
#define DHTTYPE DHT22
#define EM_SENSOR 39
#define BUZZER 33
#define MOTION_S 34
#define B_FAN 26
#define BUTTON 35


MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

LiquidCrystal lcd(21, 22, 4, 17, 16, 15);

DHT dht(DHT_B_PIN, DHTTYPE);
DHT dht2(DHT_R_PIN, DHTTYPE);

// Define authorized RFID tag
int My_RFID_Tag[4] = {0xE3, 0xA2, 0xF2, 0x0F};
boolean My_Card = false;
boolean house_locked = false;
int door_locked; 
boolean isAlarm = false;
boolean roomTemp = true;
boolean bFan = false;
int unlockTry = 0;

int pinStateCurrent   = LOW;  // current state of pin

void setup() { 
  Serial.begin(9600);
  dht.begin();
  dht2.begin();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.print("Welcome home!");
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 
  pinMode(EM_SENSOR, INPUT_PULLUP);
  pinMode(BUZZER, OUTPUT);
  pinMode(MOTION_S, INPUT);
  pinMode(B_FAN, OUTPUT);
  digitalWrite(B_FAN, LOW);
  pinMode(BUTTON, INPUT);

}

void loop() {

  displayTemp();

  securityLock();

  readingRFID();

  delay(500);
}

void readingRFID() {
  My_Card = true;

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

    // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  Serial.println("Card readed");
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI ||  
    piccType == MFRC522::PICC_TYPE_MIFARE_1K ||
    piccType == MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println("Card type is good");
    displayCardInfo();    // Display card's unique ID
    checkCardAuthorization();  // Check if the card is authorized
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

void magneticSensor() {
  if (isAlarm == false && house_locked) {
    door_locked = digitalRead(EM_SENSOR);

    if(door_locked == HIGH) {
      securityAlarm();
    }
  }
}

void motionDetection() {
  if (isAlarm == false && house_locked) {
    pinStateCurrent = digitalRead(MOTION_S);   // read new state
    if (pinStateCurrent == HIGH) {   // pin state change: LOW -> HIGH
      securityAlarm();
    }
  }
}

void securityLock() {
  if (!isAlarm && house_locked) {
    pinStateCurrent = digitalRead(MOTION_S);   // read new state
    door_locked = digitalRead(EM_SENSOR);
    if (pinStateCurrent == HIGH) {
      securityAlarm();
    }
    if (door_locked == HIGH && unlockTry <= 5) {
      digitalWrite(BUZZER, HIGH);
      delay(500);
      digitalWrite(BUZZER, LOW);
      unlockTry++;
    }
    if (door_locked == HIGH && unlockTry > 5) {
      securityAlarm();
    }
  }
}

void securityAlarm() {
  isAlarm = true;
  lcd.clear();
  lcd.print("ALARM ON");
  digitalWrite(BUZZER, HIGH);
}

void displayTemp()
{
  if (digitalRead(BUTTON) == HIGH) {
    roomTemp = !roomTemp;
    delay(500);
  }
  lcd.setCursor(0, 1);
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  int bH = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float bT = dht.readTemperature();
  bT = round(bT * 10) / 10;
  int rH = dht2.readHumidity();
  float rT = dht2.readTemperature();
  rT = round(rT * 10) / 10;
  if (bH >= 70 && !bFan) {
    bFan = true;
    digitalWrite(B_FAN, HIGH);
    delay(500);
  }
  else
  if (bH < 70 && bFan) {
    bFan = false;
    digitalWrite(B_FAN, LOW);
    delay(500);
  }
  if (roomTemp) {
    lcd.print("R: H:");
    lcd.print(rH);
    lcd.print("% T:");
    lcd.print(rT, 1);
    lcd.print("C");
  } else {
    lcd.print("B: H:");
    lcd.print(bH);
    lcd.print("% T:");
    lcd.print(bT, 1);
    lcd.print("C");
  }
}

// Function to display the RFID card's unique ID on LCD and Serial monitor
void displayCardInfo()
{
  printHex(rfid.uid.uidByte, rfid.uid.size);
  delay(500);
}

// Function to check if the read RFID card is authorized
void checkCardAuthorization()
{
  for (int i = 0; i < 4; i++)
  {
    if (My_RFID_Tag[i] != rfid.uid.uidByte[i])
    {
      My_Card = false;
      break;
    }
  }
  Serial.println();
  delay(1000);
  My_Card ? grantAccess() : denyAccess();  // Grant or deny access based on card authorization
}

// Function to grant access if the card is authorized
void grantAccess()
{
  if(house_locked) {
    Serial.println("\nRFID accepted");
    lcd.clear();
    lcd.print("Welcome home!");
    isAlarm = false;
    digitalWrite(BUZZER, LOW);
    house_locked = false;
    unlockTry = 0;
    delay(2000);
  } else {
    lcd.clear();
    lcd.print("House is locking!");
    delay(5000);
    house_locked = true;
    lcd.clear();
    lcd.print("House is locked!");
  }

}

// Function to deny access if the card is unauthorized
void denyAccess()
{
  Serial.println("\nRFID declined");
  lcd.clear();
  lcd.print("Access denied!");
  delay(1000);
  if(house_locked) {
    lcd.clear();
    lcd.print("House is locked!");
  } else {
    lcd.clear();
    lcd.print("Welcome home!");
  }

}

/**
 * Helper routine to dump a byte array as hex values to Serial. 
 */
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}