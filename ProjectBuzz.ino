#include <SD.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_PN532.h>

#define PN532_SCK  (2)
#define PN532_SS   (3)
#define PN532_MOSI (4)
#define PN532_MISO (5)
#define SDPIN (10)

#define RELAY_ON (0)
#define RELAY_OFF (1)

#define LED_RED (A1)
#define LED_GREEN (A0)
#define BUZZER (A3)
#define DOOR_RELAY (A2)

Adafruit_PN532 nfc(PN532_SCK, PN532_MISO, PN532_MOSI, PN532_SS);

File codesFile;

String uidString;

void setup(void) {

  Serial.begin(115200);
  Serial.println("\nProjectBuzz :)\n");
  
  digitalWrite(DOOR_RELAY, RELAY_OFF);

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(DOOR_RELAY, OUTPUT);

  Serial.println("Initializing SD card...");

  // For SDCard to work
  pinMode(SDPIN, OUTPUT);
  digitalWrite(SDPIN, HIGH);

  if (!SD.begin(10)) {
    Serial.println("Initialization failed!");
    errorSDAccess();     // Indicate SD initialization error
  }

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (!versiondata) {
    Serial.print("Didn't find NFC board");    
    while (1) {
      digitalWrite(LED_RED, HIGH);
      delay(150);
      digitalWrite(LED_RED, LOW);
      delay(5000);
    }
  }
  // NFC chip found, print the info
  Serial.print("Found chip PN5"); Serial.println((versiondata >> 24) & 0xFF, HEX);
  Serial.print("Firmware ver. "); Serial.print((versiondata >> 16) & 0xFF, DEC);
  Serial.print('.'); Serial.println((versiondata >> 8) & 0xFF, DEC);
  
  nfc.SAMConfig();    // Configure the board to read NFC cards
}

void loop(void) {
  
  uint8_t readSuccess;
  uint8_t unregisteredReadSuccess;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)

  Serial.println("Waiting for a Card...\n");
  readSuccess = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);  

  if (readSuccess) {
    for ( int i = 0; i < 4; i++ ) {
      uidString +=  uid[i];
    }
    Serial.println("ID: " + uidString);    
    indications(4, 50, 50);    // Card successfully read indication.

    delay(250);
        
    if (uidString == "5319725153") {    // Put a card ID here to make it a Mastercard
      Serial.println("Mastercard found");
      digitalWrite(LED_GREEN, HIGH);
      digitalWrite(LED_RED, HIGH);
      digitalWrite(BUZZER, HIGH);
      delay(2000);
      Serial.println("Waiting for a master command...");
      uint8_t uidTwo[] = { 0, 0, 0, 0, 0, 0, 0 };
      uint8_t uidLengthTwo;
      unregisteredReadSuccess = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uidTwo, &uidLengthTwo);

      if (unregisteredReadSuccess) {
        digitalWrite(LED_GREEN, LOW);
        digitalWrite(LED_RED, LOW);
        digitalWrite(BUZZER, LOW);
        String uidStringTwo = "";
        for ( int i = 0; i < 4; i++ ) {
          uidStringTwo +=  uidTwo[i];
        }
        Serial.println("ID: " + uidStringTwo);
        if (uidStringTwo == "5319725153") {    // Mastercard tapped again
          
          SD.remove("codes.txt");    // Remove the codes.txt file if it exists
          
          indications(6, 100, 100);    // Master-Reset Indication
          delay(500);
          
          digitalWrite(LED_GREEN, HIGH);
          digitalWrite(LED_RED, HIGH);
          digitalWrite(BUZZER, HIGH);
          delay(250);
          digitalWrite(LED_GREEN, LOW);
          digitalWrite(LED_RED, LOW);
          digitalWrite(BUZZER, LOW);
          Serial.println("MASTER-RESET Complete!\n");
        } else {    // Anyother card is tapped
          delay(500);
          digitalWrite(LED_GREEN, HIGH);
          digitalWrite(LED_RED, HIGH);
          digitalWrite(BUZZER, HIGH);
          delay(250);
          digitalWrite(LED_GREEN, LOW);
          digitalWrite(LED_RED, LOW);
          digitalWrite(BUZZER, LOW);
          delay(250);
          
          codesFile = SD.open("codes.txt", FILE_WRITE);    // If the file is available, open it. Else, create it
          
          // If the file is open, write to it.
          if (codesFile) {
            codesFile.println(uidStringTwo);    // Writing the new UID String to SD
            codesFile.close();            
            Serial.println("New card registered!\n");
            // print to the serial port too:
            Serial.println("ID written to SD card: " + uidStringTwo);
          } else {
            errorSDAccess();    // File opening error indication
          }
          
          indications(10, 25, 25);    // New ID registered indication     
          delay(250);          
          digitalWrite(LED_GREEN, HIGH);
          digitalWrite(BUZZER, HIGH);
          delay(500);
          digitalWrite(LED_GREEN, LOW);
          digitalWrite(BUZZER, LOW);
        }
      }
    } else {
      
      if (codeFound()) {
        digitalWrite(LED_GREEN, HIGH);
        Serial.println("ACCESS GRANTED");
        delay(300);
        digitalWrite(DOOR_RELAY, RELAY_ON);
        delay(150);
        Serial.println("DOOR UNLOCKED!\n");
        digitalWrite(DOOR_RELAY, RELAY_OFF);
        delay(550);
        digitalWrite(LED_GREEN, LOW);
      } else {
        Serial.println("NO MATCH FOUND");
        Serial.println("ACCESS DENIED\n");
        digitalWrite(LED_RED, HIGH);
        digitalWrite(BUZZER, HIGH);
        delay(1000);
        digitalWrite(LED_RED, LOW);
        digitalWrite(BUZZER, LOW);
      }      
    }
      delay(2500);    // Delay till the next read          
      uidString = "";   
      codesFile.close();
  }             
}

  bool codeFound() {
  String codesFoundOnSD;
  Serial.println("Retrieving registered codes...");
  codesFile = SD.open("codes.txt");

  // if the file is open, read from it.
  if (codesFile){
    while (codesFile.available()) { 
            codesFoundOnSD = codesFile.readStringUntil('\n');
            codesFoundOnSD.trim();
        Serial.println("Comparing with: " + codesFoundOnSD);
        if(uidString == codesFoundOnSD) {            
        Serial.println("MATCH FOUND!");
            return true; 
        }
    }
  } else {
    errorSDAccess();    // File opening error indication
  }
  return false;
}

void errorSDAccess() {  
  Serial.println("Error. SD access failed.");
  indications(5, 1000, 1000);
}

void indications(int count, int delayOne, int delayTwo) {
  for (int i = 0; i < count; i = i + 1) {
    digitalWrite(LED_GREEN, HIGH);
    digitalWrite(LED_RED, HIGH);
    digitalWrite(BUZZER, HIGH);
    delay(delayOne);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_RED, LOW);
    digitalWrite(BUZZER, LOW);
    delay(delayTwo);
  }
}
