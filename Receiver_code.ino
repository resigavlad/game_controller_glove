#include <SoftwareSerial.h>

#define tx 2
#define rx 3
SoftwareSerial configBt(rx, tx);

int resultVector[] = {0, 5, 5};
int resultVectorCopy[] = {0, 5, 5}; //used for testing if there is a change in received data
int resultVectorLength = sizeof(resultVector) / sizeof(int) - 1;

void setup() {
  configBt.begin(115200); // Default communication rate of the Bluetooth module
  
  pinMode(tx, OUTPUT);
  pinMode(rx, INPUT);
  
  Serial.begin(115200);
}

int detectChange() {
  for(int i = 0; i < resultVectorLength; i++) {
    if(resultVector[1] != resultVectorCopy[1]) {
      return 1;
    }
  }

  return 0;
}

void loop() {
  if(configBt.available() > 0)
  {
    int aux = configBt.read();

    resultVector[aux / 10 % 10] = aux % 10 + aux / 100 * 10; 
  }

  if(detectChange() == 1) {

    //send data to pc via usb in format %number,%number,%number\n
    for(int i = 0; i < resultVectorLength; i++) {
      Serial.print(resultVector[i]);
      Serial.flush();
      Serial.print(",");
      Serial.flush();

      resultVectorCopy[1] = resultVector[1];
    }
    Serial.println(resultVector[resultVectorLength]);
    Serial.flush();
  }
}
