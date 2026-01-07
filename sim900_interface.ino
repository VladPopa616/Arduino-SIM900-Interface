#include <SoftwareSerial.h>

// Pins: RX is 7 (SIM900 TX), TX is 8 (SIM900 RX)
SoftwareSerial mySerial(7, 8); 

void setup() {
  Serial.begin(115200); 
  mySerial.begin(9600); 
  
  Serial.println(F("--- [SYSTEM STARTUP] ---"));
  delay(3000); 

  // Silent Initialization
  mySerial.println(F("AT")); 
  delay(200);
  mySerial.println(F("AT+CPIN=0000")); // Change 0000 to your SIM PIN if needed
  delay(200);
  mySerial.println(F("ATE0"));         // Echo OFF (Very important for clean logs)
  delay(200);
  mySerial.println(F("AT+CMGF=1"));    // Text Mode
  delay(200);
  mySerial.println(F("AT+CLIP=1"));    // Caller ID ON
  delay(200);
  mySerial.println(F("AT+CNMI=2,2,0,0,0")); // Stream SMS to Serial

  Serial.println(F("READY: GSM Module Online."));
  printMenu();
}

void loop() {
  // 1. Background Monitor for Incoming Events (Calls/SMS)
  if (mySerial.available()) {
    String line = mySerial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      parseGSMEvent(line);
    }
  }

  // 2. User Menu Input
  if (Serial.available()) {
    char option = Serial.read();
    clearSerialBuffer(); 
    handleMenu(option);
  }
}

// ----------------- The "Translator" Function ----------------- //

void parseGSMEvent(String data) {
  // Translate Incoming Calls
  if (data.indexOf("RING") != -1) {
    Serial.println(F("\n[!] ALERT: Phone is ringing..."));
  } 
  else if (data.indexOf("+CLIP:") != -1) {
    int firstQuote = data.indexOf('"');
    int secondQuote = data.indexOf('"', firstQuote + 1);
    String callerNum = data.substring(firstQuote + 1, secondQuote);
    Serial.print(F("[ID]: Incoming call from: "));
    Serial.println(callerNum);
  }
  // Translate Signal Strength
  else if (data.indexOf("+CSQ:") != -1) {
    int colon = data.indexOf(':');
    int comma = data.indexOf(',');
    int rssi = data.substring(colon + 2, comma).toInt();
    
    int percent = (rssi == 99) ? 0 : map(rssi, 0, 31, 0, 100);
    String quality = (rssi > 20) ? "Excellent" : (rssi > 15) ? "Good" : (rssi > 10) ? "Fair" : "Poor";

    Serial.print(F("[STATUS]: Signal: "));
    Serial.print(percent);
    Serial.print(F("% ("));
    Serial.print(quality);
    Serial.println(F(")"));
  }
  // Handle new incoming SMS
  else if (data.indexOf("+CMT:") != -1) {
    Serial.println(F("\n--- NEW MESSAGE RECEIVED ---"));
    // The message text usually follows on the next line
  }
  // Filter out the noise (OK/ERROR/Prompts)
  else if (data != "OK" && data != "ERROR" && data != ">") {
    Serial.print(F("[GSM]: "));
    Serial.println(data);
  }
}

// ----------------- Action Logic ----------------- //

void handleMenu(char choice) {
  switch (choice) {
    case '1': 
      performSmartSMS();
      break;
    
    case '2': {
      Serial.print(F("\nEnter Number to Dial: "));
      String num = getCleanInput();
      mySerial.print(F("ATD"));
      mySerial.print(num);
      mySerial.println(F(";"));
      Serial.println(F(">> Dialing..."));
      break;
    }
    
    case '3': 
      mySerial.println(F("ATH")); 
      Serial.println(F(">> Hanging up..."));
      break;
    
    case '4': 
      mySerial.println(F("AT+CSQ")); 
      break;
      
    case 'm': 
      printMenu(); 
      break;
  }
}

void performSmartSMS() {
  Serial.println(F("\n--- SEND SMS ---"));
  Serial.print(F("Enter Recipient: "));
  String num = getCleanInput();
  
  Serial.print(F("Enter Message: "));
  String msg = getCleanInput();
  
  // Step 1: Initiate SMS
  mySerial.print(F("AT+CMGS=\""));
  mySerial.print(num);
  mySerial.println(F("\""));
  
  // Step 2: Wait for the '>' prompt
  unsigned long startWait = millis();
  bool ready = false;
  while (millis() - startWait < 2000) {
    if (mySerial.available() && mySerial.read() == '>') {
      ready = true;
      break;
    }
  }

  if (ready) {
    // Step 3: Send Body
    mySerial.print(msg);
    mySerial.write(26); // Ctrl+Z
    Serial.println(F("Status: Sending to network..."));
    
    // Step 4: Intercept response for success/fail
    String feedback = "";
    unsigned long startFinal = millis();
    bool success = false;
    
    while (millis() - startFinal < 10000) { // 10s timeout for network delay
      if (mySerial.available()) {
        char c = mySerial.read();
        feedback += c;
        if (feedback.indexOf("OK") != -1) { success = true; break; }
        if (feedback.indexOf("ERROR") != -1) { success = false; break; }
      }
    }

    if (success) {
      Serial.println(F(">>> SUCCESS: Message sent successfully!"));
    } else {
      Serial.println(F(">>> FAILED: Message failed. Check signal or balance."));
    }
  } else {
    Serial.println(F(">>> ERROR: Module did not accept command."));
  }
  printMenu();
}

// ----------------- Stability Helpers ----------------- //

String getCleanInput() {
  while (Serial.available() == 0) delay(10);
  String input = Serial.readStringUntil('\n');
  input.trim();
  if (input.length() == 0) return getCleanInput(); // Ignore empty accidental enters
  return input;
}

void clearSerialBuffer() {
  delay(100);
  while (Serial.available() > 0) Serial.read();
}

void printMenu() {
  Serial.println(F("\n=== MAIN MENU ==="));
  Serial.println(F("1: Send SMS   | 2: Make Call"));
  Serial.println(F("3: Hang Up    | 4: Check Signal"));
  Serial.println(F("m: Show Menu"));
  Serial.println(F("=================="));
}