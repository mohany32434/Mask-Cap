/*  Program controls two stepper motors that operate the mask mechanisim. 
 *   Both continously advertises over BLE and Scans. if another maskcap is detected in bluetooth range 
 *   both masks will auto-close. 
 *   
 *   Project by: 
 *    Matthew Bellafaire
 *    Kurtis Rhein
 *    Steven DeCoste 
 */
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEServer.h>
#include <BLEAdvertisedDevice.h>
#include <Stepper.h>

#define SERVICE_UUID        "ca575296-1972-43c1-8475-e4bb29c7b3f5"

boolean retracted = false;
int scanTime = 1; //In seconds
BLEScan* pBLEScan;
boolean detected = false;


// Number of steps per output rotation
const int stepsPerRevolution = 600;

// Create Instance of Stepper library
//Code fix
Stepper rightStepper(stepsPerRevolution, 32, 33, 25, 26);
Stepper leftStepper(stepsPerRevolution, 22, 23, 21, 19);


class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    /**
        Called for each advertising BLE server.
    */
    void onResult(BLEAdvertisedDevice advertisedDevice) {
      //      Serial.print("BLE Advertised Device found: ");
      //      Serial.println(advertisedDevice.toString().c_str());

      // We have found a device, let us now see if it contains the service we are looking for.
      if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
        BLEDevice::getScan()->stop();
        detected = true;
      }
    }
};


void setup() {
  Serial.begin(115200);
  Serial.println("Scanning...");
  pinMode(4, OUTPUT);
  digitalWrite(4, LOW);
  rightStepper.setSpeed(25);
  leftStepper.setSpeed(25);

  //Create BLE advertisement and begin advertising. We're using the server here so that it can show up as 
  //"CapMask" on a BLE Scanner (easier to debug than memorizing the UUID)
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(false); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);  // less or equal setInterval value


  BLEDevice::init("CapMask");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pService->start();
  // BLEAdvertising *pAdvertising = pServer->getAdvertising();  // this still is working for backward compatibility
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);  // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();
  Serial.println("Characteristic defined! Now you can read it in your phone!");
}

//To move the motors properly we use a FreeRTOS task
void raiseLeft(void * pvParameters) {
  leftStepper.setSpeed(20);
  leftStepper.step(-1500);
  vTaskDelete(NULL);
}

void lowerLeft(void * pvParameters) {
  leftStepper.setSpeed(35);
  leftStepper.step(1000);
  vTaskDelete(NULL);
}
void raiseRight(void * pvParameters) {
  rightStepper.setSpeed(20);
  rightStepper.step(-1000);
  vTaskDelete(NULL);
}

void lowerRight(void * pvParameters) {
  rightStepper.setSpeed(35);
  rightStepper.step(1500);
  vTaskDelete(NULL);
}

//raises mask
void raiseMask() {

  TaskHandle_t xRaiseLeft = NULL;
  TaskHandle_t xRaiseRight = NULL;
  xTaskCreatePinnedToCore( raiseRight, "raise_right", 4096, (void *) 1 , tskIDLE_PRIORITY, &xRaiseRight, 0 );
  xTaskCreatePinnedToCore( raiseLeft, "raise_left", 4096, (void *) 1 , tskIDLE_PRIORITY, &xRaiseLeft, 0 );

  delay(15000);
}

//closes mask
void lowerMask() {
  TaskHandle_t xLowerLeft = NULL;
  TaskHandle_t xLowerRight = NULL;
  xTaskCreatePinnedToCore( lowerRight, "raise_right", 4096, (void *) 1 , tskIDLE_PRIORITY, &xLowerRight, 0 );
  xTaskCreatePinnedToCore( lowerLeft, "raise_left", 4096, (void *) 1 , tskIDLE_PRIORITY, &xLowerLeft, 0 );

  delay(15000);
}

//not much happening here
void loop() {

  //set detected to false, the callback will set it to true if a device with the same UUID is found (another mask)
  detected = false;
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
  Serial.print("Devices found: ");
  Serial.println(foundDevices.getCount());
  Serial.println("Scan done!");
  pBLEScan->clearResults();   // delete results fromBLEScan buffer to release memory
  delay(1000);

  //if we detected another mask and the mask is open then we need to close the mask
    if (retracted && detected) {
      lowerMask();
      retracted = false;

     //if it's closed and there's no other masks around then we can open the mask safely. 
    } else if (!retracted && !detected) {
      raiseMask(); 
      retracted = true;
    }

}
