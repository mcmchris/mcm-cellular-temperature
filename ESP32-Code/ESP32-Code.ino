/*
   MCM - Cellular Temperature Sensor Node
   @Author: Christopher Mendez | MCMCHRIS
   @Date: 05/04/2023 (mm/dd/yy)
   @Brief:
   This firmware runs on a ESP32C3 MCU to measure temperature with a DS18B20 sensor using One Wire protocol and upload it to the cloud
   once you request it with an API call using the Notehub platform and a Cellular Notecard. Also you are able to control a relay from anywhere using cellular
   connectivity. 
*/

// Include the libraries we need
#include <OneWire.h>            //http://librarymanager/All#OneWire
#include <DallasTemperature.h>  //http://librarymanager/All#DallasTemperature
#include <Notecard.h>           //http://librarymanager/All#Notecard
#include <Wire.h>

// Peripherals wiring pinout
#define ONE_WIRE_BUS D3
#define LED D0
#define RELAY D9

#define PRODUCT_UID "com.my-company.my-name:my-project"  // create a project on Notehub to define yours

#define myProductID PRODUCT_UID

#define usbSerial Serial

Notecard notecard;

// Parameters for this project
#define INBOUND_QUEUE_POLL_SECS 10
#define INBOUND_QUEUE_NOTEFILE "data.qi"
#define INBOUND_QUEUE_COMMAND_FIELD "send"

#define myLiveDemo true

// Setup a oneWire instance to communicate with any OneWire devices
OneWire oneWire(ONE_WIRE_BUS);

// Pass our oneWire reference to Dallas Temperature.
DallasTemperature sensors(&oneWire);

float tempC;
bool control = false;

/*
 * The setup function.
 */
void setup(void) {
  // start serial port
  Serial.begin(115200);
 
  pinMode(LED, OUTPUT);
  pinMode(RELAY, OUTPUT);
  pinMode(D6, INPUT);
  // Start up the library
  sensors.begin();
  notecard.setDebugOutputStream(Serial);

  Wire.begin();
  notecard.begin();

  J *req = notecard.newRequest("hub.set");
  if (myProductID[0]) {
    JAddStringToObject(req, "product", myProductID);
  }
#if myLiveDemo
  JAddStringToObject(req, "mode", "continuous");
  JAddBoolToObject(req, "sync", true);  // Automatically sync when changes are made on notehub
#else
  JAddStringToObject(req, "mode", "periodic");
  JAddNumberToObject(req, "inbound", 60);
#endif
  notecard.sendRequest(req);
}

/*
 * Main function, get and show the temperature
 */
void loop(void) {
  // call sensors.requestTemperatures() to issue a global temperature
  // request to all devices on the bus
  Serial.print("Requesting temperatures...");
  sensors.requestTemperatures();  // Send the command to get temperatures
  Serial.println("DONE");

  // We use the function ByIndex, and as an example get the temperature from the first sensor only.
  tempC = sensors.getTempCByIndex(0);

  // Check if reading was successful
  if (tempC != DEVICE_DISCONNECTED_C) {
    Serial.print("Temperature for the device 1 (index 0) is: ");
    Serial.println(tempC);
  } else {
    Serial.println("Error: Could not read temperature data");
  }

  if (tempC > 100 && !control) {
    digitalWrite(LED, HIGH);
    delay(1000);
    Serial.println("Temperature alert!");
    digitalWrite(LED, LOW);
    sendNote();
    control = true;
  }

  if (tempC < 100 && control) {
    Serial.println("Temperature below 100ºC");
    sendNote();
    control = false;
  }

  // On a periodic basis, check the inbound queue for messages.  In a real-world application,
  // this would be checked using a frequency commensurate with the required inbound responsiveness.
  // For the most common "periodic" mode applications, this might be daily or weekly.  In this
  // example, where we are using "continuous" mode, we check quite often for demonstratio purposes.
  static unsigned long nextPollMs = 0;
  if (millis() > nextPollMs) {
    nextPollMs = millis() + (INBOUND_QUEUE_POLL_SECS * 1000);

    getNote();
  }
}

void getNote() {
  // Process all pending inbound requests
  while (true) {

    // Get the next available note from our inbound queue notefile, deleting it
    J *req = notecard.newRequest("note.get");
    JAddStringToObject(req, "file", INBOUND_QUEUE_NOTEFILE);
    JAddBoolToObject(req, "delete", true);
    J *rsp = notecard.requestAndResponse(req);
    if (rsp != NULL) {

      // If an error is returned, this means that no response is pending.  Note
      // that it's expected that this might return either a "note does not exist"
      // error if there are no pending inbound notes, or a "file does not exist" error
      // if the inbound queue hasn't yet been created on the service.
      if (notecard.responseError(rsp)) {
        notecard.deleteResponse(rsp);
        break;
      }

      // Get the note's body
      J *body = JGetObject(rsp, "body");
      if (body != NULL) {

        char *RelayStat = JGetString(body, "Relay");

        if (RelayStat != "") {
          int status = atoi(RelayStat);
          digitalWrite(RELAY, status);
        }

        char *TempReq = JGetString(body, "GetTemp");
        int status2 = atoi(TempReq);
        if (status2 == 1) {
          sendNote();
        }

        for (int i = 0; i < 4; i++) {
          digitalWrite(LED, HIGH);
          delay(100);
          digitalWrite(LED, LOW);
          delay(100);
        }
      }
    }
    notecard.deleteResponse(rsp);
  }
}

void sendNote() {

  J *req = notecard.newRequest("note.add");
  if (req != NULL) {
    JAddStringToObject(req, "file", "sensors.qo");
    JAddBoolToObject(req, "sync", true);

    J *body = JCreateObject();
    if (body != NULL) {
      JAddNumberToObject(body, "temp", tempC);
      JAddItemToObject(req, "body", body);
    }

    notecard.sendRequest(req);
  }
}