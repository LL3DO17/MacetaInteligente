/*
  batteryControl_JFL.cpp - Battery control logic for SmartPot project
  Created by Javier F. Lledó, July 27th, 2023.
*/

#include <BQ24195.h>
#include "Arduino.h"
#include "batteryControl_JFL.h"

float rawADC;
float voltADC;
float voltBat;
int max_Source_voltage;
bool isBatteryDisabled;

const int R1 =  330000;
const int R2 = 1000000;
const float batteryFullVoltage = 4.0;
const float batteryEmptyVoltage = 3.3;
const float batteryCapacity = 2.000;
const float batteryMaxInputCurrent = 2.000;
const float batteryMaxInputVoltage = 3.88;

void batterySetup() {

  //PMIC.enableBoostMode();

  max_Source_voltage = (3.3 * (R1 + R2))/R2;
  isBatteryDisabled = false;

  if (!PMIC.begin()) {
    Serial.println("Failed to initialize PMIC!");
    while (1);
  }

  // Set the input current limit to 2 A and the overload input voltage to 3.88 V
  if (!PMIC.setInputCurrentLimit(batteryMaxInputCurrent)) {
    Serial.println("Error in set input current limit");
  }

  if (!PMIC.setInputVoltageLimit(batteryMaxInputVoltage)) {
    Serial.println("Error in set input voltage limit");
  }

  // set the minimum voltage used to feeding the module embed on Board
  if (!PMIC.setMinimumSystemVoltage(batteryEmptyVoltage)) {
    Serial.println("Error in set minimum system volage");
  }

  // Set the desired charge voltage to 4.11 V
  if (!PMIC.setChargeVoltage(batteryFullVoltage)) {
    Serial.println("Error in set charge volage");
  }

  // The charge current should be defined as maximum at (C for hour)/2h
  // to avoid battery explosion (for example for a 750 mAh battery set to 0.375 A)
  if (!PMIC.setChargeCurrent(batteryCapacity/2)) {
    Serial.println("Error in set charge current");
  }

  analogReference(AR_DEFAULT);
  //analogReadResolution(12);


  Serial.println("Battery initialization done!");

}

String getChargeStatusMessage() {
  switch (PMIC.chargeStatus()){
    case(CHARGE_TERMINATION_DONE):
      return "CHARGE_TERMINATION_DONE";
    case(NOT_CHARGING):
      return "NOT_CHARGING";
    case(PRE_CHARGING):
      return "PRE_CHARGING";
    case(FAST_CHARGING):
      return "FAST_CHARGING";
  }
    
}

int getBatteryPercentage() {

  analogReadResolution(12);
  delay(100);
    // put your main code here, to run repeatedly:
  rawADC = analogRead(ADC_BATTERY);                     //the value obtained directly at the PB09 input pin
  voltADC = rawADC * (3.3/4095.0);                      //convert ADC value to the voltage read at the pin
  voltBat = voltADC * (max_Source_voltage/3.3);         //we cannot use map since it requires int inputs/outputs
  
  int new_batt = (voltBat - batteryEmptyVoltage) * (100) / (batteryFullVoltage - batteryEmptyVoltage);    //custom float friendly map function

  //report information over Serial
  Serial.print("The ADC on PB09 reads a value of ");
  Serial.print(rawADC);
  Serial.print(" which is equivialent to ");
  Serial.print(voltADC);
  Serial.print("V. This means the battery voltage is ");
  Serial.print(voltBat);
  Serial.print("V. Which is equivalent to a charge level of ");
  Serial.print(new_batt);
  Serial.println("%.");

  analogReadResolution(10);

  return new_batt;
}

int batteryCheck() {

  // FirebaseData MacetaInteligenteDefinitiva;
  // String ruta = "MacetaInteligente";

  int batPercentage = getBatteryPercentage();
  String statusMessage = getChargeStatusMessage();
  Serial.println(statusMessage);

  // Firebase.setString(MacetaInteligenteDefinitiva, ruta + "/Estado Bateria", getChargeStatusMessage());

  if (statusMessage != "NOT_CHARGING") {
    if (isBatteryDisabled) {
      isBatteryDisabled = false;
      PMIC.enableBATFET();
    } 

    if (getChargeStatusMessage() == "CHARGE_TERMINATION_DONE" or batPercentage > 90) {
      PMIC.disableCharge();
      Serial.println("Charge disabled!");
    }
  } else if (PMIC.isPowerGood() and batPercentage < 60){
    PMIC.enableCharge();

  } else if (batPercentage < 10) {
    PMIC.disableBATFET();
    isBatteryDisabled = true;
  }

  // Mandar "batPercentage" y "statusMessage" a Firebase
  
  return batPercentage;
}


 