/*
 *  Contains code for the math stuff by
    DroneBot Workshop 2019
    https://dronebotworkshop.com
*/

#include <SoftwareSerial.h>
#include <Wire.h>

//inidicates which value to send through bluetooth
//sending all data at once in a single loop will cause input ping
int dataToSend = 0;

const int flexSensorPin = A1;
int flexSensorReadValue;
int selectedAction = 0;

//pins used to communicate with bluetooth device
#define tx 4
#define rx 5
SoftwareSerial bt(rx, tx);

//activation values for flex sensor
//may require modifications based on resistance value used
const int minFlexSensorTreshold = 260;
const int maxFlexSensorTreshold = 300;

char pitchVarForTransmission;
char rollVarForTransmission;

const int MPU_6050_IC2_ADDRESS = 0x68;

//Variables for Gyroscope
int gyro_x, gyro_y, gyro_z;
long gyro_x_cal, gyro_y_cal, gyro_z_cal;
boolean set_gyro_angles;
 
long acc_x, acc_y, acc_z, acc_total_vector;
float angle_roll_acc, angle_pitch_acc;
 
float angle_pitch, angle_roll;
int angle_pitch_buffer, angle_roll_buffer;
float angle_pitch_output, angle_roll_output;
 
// Setup timers and temp variables
long loop_timer;
int temp;

void setup() {
  
  Wire.begin();

  pinMode(flexSensorPin, INPUT_PULLUP);

  //Start and setup the registers of the MPU-6050
  start_mpu_6050();

  //Calibrate the MPU-6050 to it's resting position
  //This process requires 7-8 seconds when it is recommended
  //to leave the device resting on the table
  calibrate_sensors();

  //Configuring pins used to communicate with the bluetooth unit
  pinMode(tx, OUTPUT);
  pinMode(rx, INPUT);
  bt.begin(115200);

  Serial.begin(115200); // Default communication rate of the Bluetooth module
  
  delay(20);

  // Init Timer 
  loop_timer = micros();    
}

void loop() {
  read_mpu_6050_data();

  //Subtract the offset values from the raw gyro values
  gyro_x -= gyro_x_cal;                                                
  gyro_y -= gyro_y_cal;                                                
  gyro_z -= gyro_z_cal;                                                
         
  //Gyro angle calculations . Note 0.0000611 = 1 / (250Hz x 65.5)
  
  //Calculate the traveled pitch angle and add this to the angle_pitch variable
  angle_pitch += gyro_x * 0.0000611;  
  //Calculate the traveled roll angle and add this to the angle_roll variable
  //0.000001066 = 0.0000611 * (3.142(PI) / 180degr) The Arduino sin function is in radians                                
  angle_roll += gyro_y * 0.0000611; 
                                     
  //If the IMU has yawed transfer the roll angle to the pitch angle
  angle_pitch += angle_roll * sin(gyro_z * 0.000001066);
  //If the IMU has yawed transfer the pitch angle to the roll angle               
  angle_roll -= angle_pitch * sin(gyro_z * 0.000001066);               
  
  //Accelerometer angle calculations
  
  //Calculate the total accelerometer vector
  acc_total_vector = sqrt((acc_x*acc_x)+(acc_y*acc_y)+(acc_z*acc_z)); 
   
  //57.296 = 1 / (3.142 / 180) The Arduino asin function is in radians
  //Calculate the pitch angle
  angle_pitch_acc = asin((float)acc_y/acc_total_vector)* 57.296; 
  //Calculate the roll angle      
  angle_roll_acc = asin((float)acc_x/acc_total_vector)* -57.296;       
  
  //Accelerometer calibration value for pitch
  angle_pitch_acc -= 0.0;
  //Accelerometer calibration value for roll                                              
  angle_roll_acc -= 0.0;                                               
 
  if(set_gyro_angles){ 
  
  //If the IMU has been running 
  //Correct the drift of the gyro pitch angle with the accelerometer pitch angle                      
    angle_pitch = angle_pitch * 0.9996 + angle_pitch_acc * 0.0004; 
    //Correct the drift of the gyro roll angle with the accelerometer roll angle    
    angle_roll = angle_roll * 0.9996 + angle_roll_acc * 0.0004;        
  }
  else{ 
    //IMU has just started  
    //Set the gyro pitch angle equal to the accelerometer pitch angle                                                           
    angle_pitch = angle_pitch_acc;
    //Set the gyro roll angle equal to the accelerometer roll angle                                       
    angle_roll = angle_roll_acc;
    //Set the IMU started flag                                       
    set_gyro_angles = true;                                            
  }
  
  //To dampen the pitch and roll angles a complementary filter is used
  //Take 90% of the output pitch value and add 10% of the raw pitch value
  angle_pitch_output = angle_pitch_output * 0.9 + angle_pitch * 0.1; 
  //Take 90% of the output roll value and add 10% of the raw roll value 
  angle_roll_output = angle_roll_output * 0.9 + angle_roll * 0.1; 
  //Wait until the loop_timer reaches 4000us (250Hz) before starting the next loop  

  flexSensorReadValue = analogRead(flexSensorPin);
  if(flexSensorReadValue <= minFlexSensorTreshold) {
    selectedAction = 1;
  }
  else if(flexSensorReadValue >= maxFlexSensorTreshold) {
    selectedAction = 2;
  }
  else {
    selectedAction = 0;
  }
  //bt.write(selectedAction);

  //limit sensitivity for easerier transmission
  if(angle_pitch_output <= -45) {
    angle_pitch_output = -45;
  }
  
  if(angle_pitch_output >= 45) {
    angle_pitch_output = 45;
  }

  if(angle_roll_output <= -45) {
    angle_roll_output = -45;
  }

  
  if(angle_roll_output >= 45) {
    angle_roll_output = 45;
  }

  pitchVarForTransmission = map(angle_pitch_output, -45, 45, 0, 10);
  rollVarForTransmission = map(angle_roll_output, -45, 45, 0, 10);

  switch(dataToSend) {
    case 0:
      bt.write(selectedAction);
      break;
    case 1:
      bt.write(pitchVarForTransmission % 10 + 10 + (pitchVarForTransmission / 10) * 100);
      break;
    case 2:
      bt.write(rollVarForTransmission % 10 + 20 + (rollVarForTransmission / 10) * 100);
      break;
  }

  //data is sent in the follosinw format : abc where a is the first digit of read data
                                                   //b is the position in the result vector
                                                   //c is the first digit of read data

  dataToSend++;
  dataToSend = dataToSend % 3;
  
  while(micros() - loop_timer < 4000); 
  //Reset the loop timer                                
 loop_timer = micros();
}

void start_mpu_6050() {
  //Activate with MPU-6050
  Wire.beginTransmission(MPU_6050_IC2_ADDRESS);                                        
  Wire.write(0x6B);                                                 
  Wire.write(0x00);                                                    
  Wire.endTransmission();

  //Configure accelerometer sensitivity (+/-8g)
  Wire.beginTransmission(MPU_6050_IC2_ADDRESS);                                        
  Wire.write(0x1C);                                                   
  Wire.write(0x10);                                                   
  Wire.endTransmission(); 

  //Configure the gyroscope sensitivity (500dps)
  Wire.beginTransmission(MPU_6050_IC2_ADDRESS);                                        
  Wire.write(0x1B);                                                    
  Wire.write(0x08);                                                   
  Wire.endTransmission(); 
}

void calibrate_sensors() {
  //Read the raw acc and gyro data from the MPU-6050 1000 times                                          
  for (int cal_int = 0; cal_int < 1000 ; cal_int ++){                  
    read_mpu_6050_data();                                        
    gyro_x_cal += gyro_x;                                              
    gyro_y_cal += gyro_y;                                            
    gyro_z_cal += gyro_z; 
    //Delay 3us to have 250Hz for-loop                                             
    delay(3);                                                          
  }
 
  // Divide all results by 1000 to get average offset
  gyro_x_cal /= 1000;                                                 
  gyro_y_cal /= 1000;                                                 
  gyro_z_cal /= 1000;
}

void read_mpu_6050_data() {
  //Read the raw gyro and accelerometer data
                                         
  Wire.beginTransmission(0x68);  
  //Send the requested starting register                                      
  Wire.write(0x3B);                                                  
  Wire.endTransmission(); 
  //Request 14 bytes from the MPU-6050                                  
  Wire.requestFrom(0x68,14);                                          
  while(Wire.available() < 14);
   
  //Turns two 8-bit values into one 16-bit value                                       
  acc_x = Wire.read()<<8|Wire.read();                                  
  acc_y = Wire.read()<<8|Wire.read();                                  
  acc_z = Wire.read()<<8|Wire.read();                                  
  temp = Wire.read()<<8|Wire.read();                                   
  gyro_x = Wire.read()<<8|Wire.read();                                 
  gyro_y = Wire.read()<<8|Wire.read();                                 
  gyro_z = Wire.read()<<8|Wire.read();  
}
