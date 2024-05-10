#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <DHT.h>
#include <Stepper.h>
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#define SS_PIN 5
#define RST_PIN 0
#define DHT_B_PIN 32
#define DHT_R_PIN 2
#define DHTTYPE DHT22
#define EM_SENSOR 39
#define BUZZER 1 // on the pwm driver
#define MOTION_S 34
#define B_FAN 2 // on the pwm driver
#define BUTTON 35
#define PHOTO_R 25
#define S_IN1 26
#define S_IN2 33
#define S_IN3 14
#define S_IN4 12
#define R_LIGHT 0 // on the pwm driver
#define B_LIGHT 3 // on the pwm driver
#define L_LIGHT 4 // on the pwm driver
#define L_BUTTON 36
#define HEAT_FAN 5 // on the pwm driver
#define HEAT_1 6   // on the pwm driver
#define HEAT_2 7   // on the pwm driver

// Stepper motors steps per revolution
const int stepsPerRevolution = 3700;

// Create instance of the Stepper class
Stepper blinder(stepsPerRevolution, S_IN1, S_IN3, S_IN2, S_IN4);

// Create instance of the Adafruit_PWMServoDriver class
Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

// Create instance of the MFRC522 class
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

// Create instance of the LiquidCrystal class
LiquidCrystal lcd(27, 13, 4, 17, 16, 15);

// Create instance of the DHT class
DHT dht(DHT_B_PIN, DHTTYPE);
DHT dht2(DHT_R_PIN, DHTTYPE);

// Define authorized RFID tag
int My_RFID_Tag[4] = {0xE3, 0xA2, 0xF2, 0x0F};
// Define variables
boolean My_Card = false;
boolean house_locked = false;
int door_locked;
boolean isAlarm = false;
boolean roomTemp = true;
boolean bFan = false;
int unlockTry = 0;
boolean blinderUp = false;
int blinderTime = 0;
boolean lightState = false;

int pinStateCurrent = LOW; // current state of pin

// Temp control
boolean tempCState = false;
int coolingTime = 0;

void setup()
{
  Wire.begin();
  Serial.begin(9600);
  dht.begin();
  dht2.begin();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.print("Welcome home!");
  SPI.begin();     // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522
  pinMode(EM_SENSOR, INPUT_PULLUP);
  pinMode(MOTION_S, INPUT);
  pinMode(BUTTON, INPUT);
  pinMode(PHOTO_R, INPUT);
  pinMode(L_BUTTON, INPUT);
  blinder.setSpeed(5);
  pwm.begin();
  pwm.setPWMFreq(1600); // This is the maximum PWM frequency
  pwm.setPWM(B_FAN, 0, 4096);
  pwm.setPWM(HEAT_FAN, 0, 4096);
  pwm.setPWM(HEAT_1, 0, 4096);
  pwm.setPWM(HEAT_2, 0, 4096);
}

void loop()
{

  displayTemp();

  lightBasedActions();

  securityLock();

  lightManual();

  readingRFID();

  delay(500);
}

// Function to control the light manually
void lightManual()
{
  if (digitalRead(L_BUTTON) == HIGH)
  {
    lightState = !lightState;
    lightSwitching();
  }
}

// Function to control the light and blinder based on the light sensor
void lightBasedActions()
{
  int lightVal = analogRead(PHOTO_R);
  if (lightVal > 800 && !blinderUp && blinderTime > 20)
  {
    blinder.step(stepsPerRevolution);
    blinderUp = true;
    lightState = false;
    lightSwitching();
    blinderTime = 0;
  }
  else if (lightVal < 800 && blinderUp && blinderTime > 20)
  {
    blinder.step(-stepsPerRevolution);
    blinderUp = false;
    blinderTime = 0;
    lightState = true;
    lightSwitching();
  }
  blinderTime++;
}

// Function to switch the light on and off
void lightSwitching()
{
  if (lightState)
  {
    pwm.setPWM(L_LIGHT, 4096, 0);
    pwm.setPWM(B_LIGHT, 4096, 0);
    pwm.setPWM(R_LIGHT, 4096, 0);
  }
  else
  {
    pwm.setPWM(L_LIGHT, 0, 4096);
    pwm.setPWM(B_LIGHT, 0, 4096);
    pwm.setPWM(R_LIGHT, 0, 4096);
  }
}

// Function to read the RFID card
void readingRFID()
{
  My_Card = true;

  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if (!rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if (!rfid.PICC_ReadCardSerial())
    return;

  Serial.println("Card readed");
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  if (piccType == MFRC522::PICC_TYPE_MIFARE_MINI ||
      piccType == MFRC522::PICC_TYPE_MIFARE_1K ||
      piccType == MFRC522::PICC_TYPE_MIFARE_4K)
  {
    Serial.println("Card type is good");
    displayCardInfo();        // Display card's unique ID
    checkCardAuthorization(); // Check if the card is authorized
  }

  // Halt PICC
  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

// Function to check the magnetic door switch sensor
void magneticSensor()
{
  if (isAlarm == false && house_locked)
  {
    door_locked = digitalRead(EM_SENSOR);

    if (door_locked == HIGH)
    {
      securityAlarm();
    }
  }
}

// Function to check the motion sensor
void motionDetection()
{
  if (isAlarm == false && house_locked)
  {
    pinStateCurrent = digitalRead(MOTION_S); // read new state
    if (pinStateCurrent == HIGH)
    { // pin state change: LOW -> HIGH
      securityAlarm();
    }
  }
}

// Function to lock the house
void securityLock()
{
  if (!isAlarm && house_locked)
  {
    pinStateCurrent = digitalRead(MOTION_S); // read new state
    door_locked = digitalRead(EM_SENSOR);
    if (pinStateCurrent == HIGH)
    {
      securityAlarm();
    }
    if (door_locked == HIGH && unlockTry <= 5)
    {
      pwm.setPWM(BUZZER, 4096, 0);
      delay(500);
      pwm.setPWM(BUZZER, 0, 4096);
      unlockTry++;
    }
    if (door_locked == HIGH && unlockTry > 5)
    {
      securityAlarm();
    }
  }
}

// Function to activate the alarm
void securityAlarm()
{
  isAlarm = true;
  lcd.clear();
  lcd.print("ALARM ON");
  pwm.setPWM(BUZZER, 4096, 0);
}

// Function to display the temperature and humidity on the LCD and control the temperature
void displayTemp()
{
  if (digitalRead(BUTTON) == HIGH)
  {
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
  if (bH >= 70 && !bFan)
  {
    bFan = true;
    pwm.setPWM(B_FAN, 4096, 0);
    delay(500);
  }
  else if (bH < 70 && bFan)
  {
    bFan = false;
    pwm.setPWM(B_FAN, 0, 4096);
    delay(500);
  }
  if (roomTemp)
  {
    lcd.print("R: H:");
    lcd.print(rH);
    lcd.print("% T:");
    lcd.print(rT, 1);
    lcd.print("C");
  }
  else
  {
    lcd.print("B: H:");
    lcd.print(bH);
    lcd.print("% T:");
    lcd.print(bT, 1);
    lcd.print("C");
  }
  if (rT > 28 && !tempCState)
  {
    tempControl(2);
    tempCState = true;
  }
  else if (rT < 19 && !tempCState)
  {
    tempControl(1);
    tempCState = true;
  }
  else if (rT <= 28 && rT >= 19 && tempCState)
  {
    tempControl(0);
  }
}

// Function to control the temperature
void tempControl(int heat)
{
  if (heat == 0)
  {
    pwm.setPWM(HEAT_1, 0, 4096);
    pwm.setPWM(HEAT_2, 0, 4096);
    if (coolingTime > 20)
    {
      pwm.setPWM(HEAT_FAN, 0, 4096);
      tempCState = false;
      coolingTime = 0;
    }
    else
    {
      coolingTime++;
    }
  }
  else if (heat == 1)
  {
    pwm.setPWM(HEAT_FAN, 4096, 0);
    delay(1000);
    pwm.setPWM(HEAT_1, 4096, 0);
    pwm.setPWM(HEAT_2, 0, 4096);
  }
  else if (heat == 2)
  {
    pwm.setPWM(HEAT_FAN, 4096, 0);
    delay(1000);
    pwm.setPWM(HEAT_1, 0, 4096);
    pwm.setPWM(HEAT_2, 4096, 0);
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
  My_Card ? grantAccess() : denyAccess(); // Grant or deny access based on card authorization
}

// Function to grant access if the card is authorized
void grantAccess()
{
  if (house_locked)
  {
    Serial.println("\nRFID accepted");
    lcd.clear();
    lcd.print("Welcome home!");
    isAlarm = false;
    pwm.setPWM(BUZZER, 0, 4096);
    house_locked = false;
    unlockTry = 0;
    delay(2000);
  }
  else
  {
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
  if (house_locked)
  {
    lcd.clear();
    lcd.print("House is locked!");
  }
  else
  {
    lcd.clear();
    lcd.print("Welcome home!");
  }
}

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void printHex(byte *buffer, byte bufferSize)
{
  for (byte i = 0; i < bufferSize; i++)
  {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}