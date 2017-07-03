#include <Adafruit_PN532.h>
#include <EEPROM.h>
#include "Arduino.h" 

#define SCK  (10)
#define MISO (11)
#define MOSI (12)
#define SS   (13)

#define RELAY_ON (0)
#define RELAY_OFF (1)

Adafruit_PN532 nfc(SCK, MISO, MOSI, SS);

#define LED_RED (2)
#define LED_GREEN (3)
#define BUZZER (4)
#define DOOR_RELAY (5)

//---------------------------------------------
// EEPROM
//---------------------------------------------

const uint32_t EEPROMSize = 512; //BE CAREFULL NOT TO EXCEED EEPROM SIZE OF YOUR ARDUINO 
                            //TO AVOID DAMAGING YOUR EEPROM!!!!!!!
const uint32_t memBase = 0; // Start the storage of RFID from this address

//---------------------------------------------
// RFID MASTER, HARDCODED
//---------------------------------------------
uint32_t uid_master = 902167349;

uint32_t master_mode = 0;  
uint32_t master_mode_counter = 0;

//---------------------------------------------
// DEBUG
//---------------------------------------------

//boolean debug = true;
boolean debug = false;


/***********************************************************************************************************************************************/
// Functions
/***********************************************************************************************************************************************/
//---------------------------------------------
//get RFID as int
//---------------------------------------------

uint64_t getCardIdAsInt(uint8_t uid[],uint8_t uidLength){
  
  if (uidLength == 4) {
    // We probably have a Mifare Classic card ... 
    uint32_t cardid = uid[0];
    cardid <<= 8;
    cardid |= uid[1];
    cardid <<= 8;
    cardid |= uid[2];  
    cardid <<= 8;
    cardid |= uid[3]; 
    if(debug) {
      Serial.print("Seems to be a Mifare Classic card #");
      Serial.println(cardid);
    }
    return cardid;
  }
  
  else if (uidLength == 7)
  {
    // We probably have a Mifare Ultralight card ... 
    uint64_t cardid = 0;
    memcpy(&cardid, uid, sizeof(uid)); 
    
    if(debug) {  
      Serial.println("Seems to be a Mifare Ultralight card #");
      // Print function does not support 64 bit
    }
    if(debug) {
      for(uint8_t i = 0; i < uidLength; i++) {
        Serial.print(" byte ");Serial.print(i);Serial.print(" = ");
        Serial.print(uid[i],HEX);
        Serial.println(" ");
      }  
    }
    return cardid;
  }  
}

//-------------------------------------------------------------------------------------------------------
// EEPROM Functions
//-------------------------------------------------------------------------------------------------------

//---------------------------------------------------------------
// erase EEPROM
//---------------------------------------------------------------
void initializeEeprom() { 
  Serial.println("------------------------------------------------------");     
  Serial.println("Initializing EEPROM by erasing all RFIDs              ");
  Serial.println("Setting values of EEPROM addresses to 0               "); 
  Serial.println("EEPROM max memory size:                               ");
  Serial.println(EEPROMSize);  
  Serial.println("------------------------------------------------------");    
  
  byte zero  = 0;
  
  for(uint32_t adr = memBase; adr <= EEPROMSize; adr++) {
    EEPROM.write(adr, zero);
  }
}

//---------------------------------------------------------------
// printEeprom()
// Print the EEPROM to serial output
// for debugging
//---------------------------------------------------------------

void printEeprom(){
  uint32_t ads = memBase;
  while(ads <= EEPROMSize) {   
    byte output = EEPROM.read(ads);
    if((ads % 10) == 0  ) Serial.println(" "); 
    Serial.print(ads);Serial.print(" => ");Serial.print(output,HEX); Serial.print("   "); 
    ads++;
  }
  Serial.println(" ");
}

//---------------------------------------------------------------
// findRfidInEeprom(uidLength, uid)
// looks for matching RFID
// returns the address of the first byte of RFID (length indication) if found
// returns -1 if NOT found
// returns -2 if error
//---------------------------------------------------------------

int32_t findRfidInEeprom(uint8_t uidLength, uint8_t uid[]) {
  byte key = memBase;
  byte val = EEPROM.read(key);
  boolean match = false; 
  
  if(val == 0 && key == 0) {
    if(debug) Serial.println("EEPROM is empty ");
  }
  else {
    while(val != 0)
    {
      if(key >= EEPROMSize) 
      {
        if(debug) Serial.println("ERROR: EEPROM STORAGE MESSED UP! Return -2");//this should not happen! If so initialize EEPROM
        return -2; 
      }
      if(val == uidLength) {
        // check if uid match the uid in EEPROM   
        byte uidAddress = key +1;
        match = true;     
        //compare uid bytes  
        for(byte i = 0; i < uidLength; i++) {
          byte uidVal = EEPROM.read(( uidAddress + i));
          //the first byte of uidVal is the next address
          if(uidVal != uid[i]) {
            //got to next key
            match = false;
      // in case no break => all bytes same
            break;
          }
        }
      
        if(match) {
          if(debug) {Serial.println("RFID uid matching in Address = "); Serial.println(key);}
          return key;
        }
      }
      
      key = key + val +1; 
      val = EEPROM.read(key);
    }
  }  
  if(debug) { Serial.println("No RFID match in EEPROM, returning -1"); }
  return -1;
}

//---------------------------------------------------------------
// deleteRfidfromEeprom(address, uidLength)
// delete the RFID from EEPROM
// sets the UID values to 0
// in the EEPROM structure there will be a "hole" with zeroes
// we are not shifting the addresses to avoid unnecessary writes
// to the EEPROM. The 'hole' will be filled with next RFID 
// storage that has the same uidLength
//
//
//  BUG if rfid was last rfid in eeprom, length is not set to 0 => not needed because by formatting eeprom all is set to 0
//---------------------------------------------------------------
void deleteRfidfromEeprom(uint32_t address, uint8_t uidLength) {
  byte zero = 0; 
  if(debug) Serial.println("Erasing RFID");
  for(uint8_t m = 1; m <= uidLength; m++) {
    uint32_t adr = address + m;
    if(debug) {
      Serial.print("Address: ");
      Serial.print(adr);
      Serial.println(" ");
    }
    EEPROM.write(adr, zero);
  }
  if(debug) { printEeprom(); }
}

//---------------------------------------------------------------
// getEndOfRfidsChainInEeprom(uidLength)
// returns the address of the end of the Rfids chain stored in the EEPROM
// returns -1 if no space left
// returns -2 if unexpected error
//---------------------------------------------------------------
int32_t getEndOfRfidsChainInEeprom(uint8_t uidLength) {
  byte key = memBase;
  byte val = EEPROM.read(key);
  
  if(val == 0 && key == 0) {
    if(debug) Serial.println("EEPROM is empty ");
    return key;
  }
  else {
    // if length byte indicator is 0 it means it is the end of the RFIDs stored, last RFID stored
    while(val != 0) {
      //this should not happen! If so initialize EEPROM
      if(key > EEPROMSize) {
        Serial.println("ERROR: EEPROM STORAGE MESSED UP! EXITING STORAGE READ");
        return -2;
      }
      key = key + val +1; 
      val = EEPROM.read(key);
    }
    if((key + uidLength) > EEPROMSize) {
      Serial.println("Not enough space left in EEPROM to store RFID");//the RFID to be appended at the end of the storage chain exeeds the EEPROM length
      return -1;
    }
    else return key; 
  }  
}


//---------------------------------------------------------------
// getFreeEepromStorageFragment(uint8_t uidLength)
// return the address where to store the RFID 
// with the rfidLength specified.
// Instead of just appending the RFID to the end of 
// the storage we look for an erased RFID space and 
// fill this
// return address
// return -2 if error
// return -1 if no free storage address found
//---------------------------------------------------------------
int32_t getFreeEepromStorageFragment(uint8_t uidLength) {
  uint32_t key = memBase;
  byte val = EEPROM.read(key); //holds the uidLength stored in EEPROM
  boolean free = false; 

  if(val == 0 && key == 0) {
    // EEPROM empty, use the address at key = memBase
    return key;
  }
  else {
    //loop till the end of storage chain indicated by a zero value in the key position
    while(val != 0) {
      //this should not happen! If so initialize EEPROM
      if(key > EEPROMSize) {
        Serial.println("ERROR: EEPROM STORAGE MESSED UP! EXITING STORAGE READ");
        return -2; 
      }
      // check if uidLength  match the uidLength in EEPROM
      if(val == uidLength) {     
        uint32_t uidAddress = key +1;
        free = true;
        //check if uid bytes are all zero => free storage fragment
        for(uint8_t i = 0; i < uidLength; i++) {
          byte uidVal = EEPROM.read(( uidAddress + i));         
          if(uidVal != 0) {
            //got to next key
            free = false;
            break;
          }
        }
        // in case no break => all bytes have zero value => free fragment
        if(free) {
          return key;
        }
      }    
      key = key + val +1; 
      val = EEPROM.read(key);
    } 
    return -1;    
  }
}

//---------------------------------------------------------------
// getEepromStorageAddress(uint8_t uidLength)
// combination of getFreeEepromStorageFragment
// and getEndOfRfidsChainInEeprom
// return address
// return -1 if no free storage address found
// return -2 if error
//---------------------------------------------------------------
int32_t getEepromStorageAddress(uint8_t uidLength) {
  int32_t fragment = getFreeEepromStorageFragment(uidLength);
  if(debug) {
    Serial.print("getFreeEepromStorageFragment returned ");Serial.print(fragment);
    Serial.println(" ");
  }  
  // free fragment found
  if(fragment >= 0) {
    return fragment;
  }
  // error returned
  else if (fragment == -2) {
    return fragment;
  }
  // no free fragment available
  // check if space available at end of rfid storage chain
  else if (fragment == -1) {
    int32_t append = getEndOfRfidsChainInEeprom(uidLength);
    if(debug) {
      Serial.print("getEndOfRfidsChainInEeprom returned ");Serial.print(append);
      Serial.println(" ");
    }     
      return append;
    }
  // should never occur, return error
  else {
    return -2;
  }  
}

//---------------------------------------------------------------
// writeRfidToEeprom(addrees,uidlength,uid)
// write RFID to EEPROM
//---------------------------------------------------------------
void writeRfidToEeprom(uint32_t StoragePositionAddress, uint8_t uidLength, uint8_t uid[]) {
  // Writing into first free address the length of the RFID uid
  EEPROM.write(StoragePositionAddress, uidLength); 
  // Writing into the following addresses the RFID uid values (byte per byte)
  uint32_t uidBytePosition = StoragePositionAddress +1; //next position after addressByte which contains the uidLength
  for(uint8_t r=0; r < uidLength; r++) {
    EEPROM.write(uidBytePosition, uid[r]);   
    uidBytePosition++;
  } 
  if(debug) { printEeprom(); }
}



/***********************************************************************************************************************************************/
// SETUP
/***********************************************************************************************************************************************/
void setup(void) {

  digitalWrite(DOOR_RELAY, RELAY_OFF);
  
  pinMode(BUZZER, OUTPUT);
  pinMode(DOOR_RELAY, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT); 
 
  Serial.begin(115200);
  if(debug) Serial.println("ProjectBuzzV1.2 says Hello!");

  nfc.begin();

  uint32_t versiondata = nfc.getFirmwareVersion();
  if (! versiondata) {
    indications(10, 2500, 2500);
    if(debug) Serial.print("Didn't find PN53x board");
    while (1);
  }
  if(debug) {
    Serial.print("Found chip PN5"); Serial.println((versiondata>>24) & 0xFF, HEX); 
    Serial.print("Firmware ver. "); Serial.print((versiondata>>16) & 0xFF, DEC); 
    Serial.print('.'); Serial.println((versiondata>>8) & 0xFF, DEC);
  }
  
  /********************************************************
  // Befor you run this sketch the first time
  // uncoment the following initializeEeprom(); 
  // to clear the EEPROM neccessary to be shure
  // that all EEPROM values used for the RFID 
  // storage are initialized with 0.
  // Connect to the serial to check if all values are 0,
  // then comment the functions again and start using
  // the Rfid access control system
  /********************************************************/
  //initializeEeprom();
  if(debug) { printEeprom(); }
  
  // configure board to read RFID tags
  nfc.SAMConfig();
 
  if(debug) Serial.println("Waiting for an ISO14443A Card ...");
}




/***********************************************************************************************************************************************/
//MAIN
/***********************************************************************************************************************************************/
void loop(void) {
   
  uint8_t success;
  uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };  // Buffer to store the returned UID, 7 bit max
  uint8_t uidLength;                        // Length of the UID (4 or 7 bytes depending on ISO14443A card type)
  uint64_t cardid;                          // UID as int
    
  // Wait for an ISO14443A type cards (Mifare, etc.).  When one is found
  // 'uid' will be populated with the UID, and uidLength will indicate
  // if the uid is 4 bytes (Mifare Classic) or 7 bytes (Mifare Ultralight)
  success = nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength);
  
  
   
  // MASTER MODE?  
  if(master_mode == 1) {
    //stay in master mode for max 20 seconds
    master_mode_counter++;
    if(master_mode_counter >= 20) {//reset master mode after 20 sec no RFID was inserted
      master_mode= 0; 
      master_mode_counter = 0;
    }
    if(debug) {
      Serial.println("MASTER MODE");
    }
    delay(500);   
  }

  //got rfid?
  if (success) {
      // Display some basic information about the card
      if(debug) {
        Serial.println("Found an ISO14443A card");
        Serial.print("  UID Length: ");Serial.print(uidLength, DEC);Serial.println(" bytes");
        Serial.print("  UID Value: ");nfc.PrintHex(uid, uidLength);
      }
      
      cardid = getCardIdAsInt(uid,uidLength);
      
      //is MASTER

      if (master_mode == 1 && cardid == uid_master) {
        initializeEeprom();
        indications(10, 100, 100);
        master_mode= 0; 
        master_mode_counter = 0;
      } else if (cardid == uid_master) { 
        master_mode = 1;
        master_mode_counter = 0;
        indications(3, 100, 100);
        if(debug) Serial.println("MASTER detected, going into MASTER MODE");
      } 
      // is not MASTER
      else {       
        int32_t findUid = findRfidInEeprom( uidLength, uid);
        
        // is MASTER MODE, include or exclude RFID from storage
        if(master_mode == 1) {
          // card rfid already exists so exlude it from storage    
          if( findUid != -1) {
            if(debug) { 
              Serial.println("removing card from eeprom"); Serial.println(" ");
            }
            deleteRfidfromEeprom(findUid, uidLength);
            indications(10, 20, 20);
            delay(250);
            digitalWrite(LED_RED, HIGH);
            delay(750);
            digitalWrite(LED_RED, LOW);
          }
          // card rfid not found in storage so include it
          else if (findUid == -1) {
            // check if space to store rfid available
            int32_t storageAddress = getEepromStorageAddress(uidLength);
          
            // storage available
            if(storageAddress >= 0) {
              if(debug) {
                Serial.print("storing card in position = ");Serial.print(storageAddress);Serial.println(" ");       
              }
              // storing card
              writeRfidToEeprom( storageAddress, uidLength, uid);  
              indications(10, 20, 20);
              delay(250);
              digitalWrite(LED_GREEN, HIGH);
              delay(750);
              digitalWrite(LED_GREEN, LOW);
            }         
            else { // no storage space available or error
              if(debug) Serial.println("memory full or error");
              indications(3, 1000, 1000);
            }
          }
          master_mode = 0;
        }
        // no MASTER MODE, authorise or deny door access
        else {
          // card authorised
          if( findUid != -1) {
           // open door      
           if(debug) Serial.println("Card authorised, open door");
            digitalWrite(LED_GREEN, HIGH);
            Serial.println("ACCESS GRANTED");
            delay(100);
            digitalWrite(DOOR_RELAY, RELAY_ON);
            delay(50);
            Serial.println("DOOR UNLOCKED!\n");
            digitalWrite(DOOR_RELAY, RELAY_OFF);
            delay(850);
            digitalWrite(LED_GREEN, LOW);
          }
          else {
            // deny access
            if(debug) Serial.println("Card not authorised, access denied");
            Serial.println("NO MATCH FOUND");
            Serial.println("ACCESS DENIED\n");
            digitalWrite(LED_RED, HIGH);
            digitalWrite(BUZZER, HIGH);
            delay(1000);
            digitalWrite(LED_RED, LOW);
            digitalWrite(BUZZER, LOW);
          }          
        }
      }
      delay(500);  
    }     
//did not get RFID, looping till RFID inserted    
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


