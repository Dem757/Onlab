#include <SPI.h>
#include <MFRC522.h>
#include <LiquidCrystal.h>
#include <DHT.h>

#define SS_PIN 5
#define RST_PIN 0
#define DHTPIN 32
#define DHTTYPE DHT22

MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class

LiquidCrystal lcd(21, 22, 4, 17, 16, 15);


// Define authorized RFID tag
int My_RFID_Tag[4] = {0xE3, 0xA2, 0xF2, 0x0F};
boolean My_Card = false;
boolean house_locked = true;

void setup() { 
  Serial.begin(9600);
  dht.begin();
  // set up the LCD's number of columns and rows:
  lcd.begin(16, 2);
  lcd.print("House is locked!");
  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522 

}

void loop() {
    // Assume card is authorized initially
  My_Card = true;

  displayTemp();

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
  delay(50);
}

void displayTemp()
{
  lcd.setCursor(0, 1);
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  int h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  t = round(t * 10) / 10;
  lcd.print("H:");
  lcd.print(h);
  lcd.print("% T:");
  lcd.print(t, 1);
  lcd.print("C");
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
  Serial.println("\nRFID accepted");
  lcd.clear();
  lcd.print("Welcome home!");
  house_locked = false;
  delay(2000);
  house_locked = true;
  lcd.clear();
  lcd.print("House is locked!");
}

// Function to deny access if the card is unauthorized
void denyAccess()
{
  Serial.println("\nRFID declined");
  lcd.clear();
  lcd.print("Access denied!");
  delay(1000);
  lcd.clear();
  lcd.print("House is locked!");
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