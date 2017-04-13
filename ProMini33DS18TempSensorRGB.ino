#include <SoftwareSerial.h>
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Wire.h>
#include <t5403.h>
#include <EEPROM.h>
#include <LiquidCrystal.h>

// Initialize the library with the pins we're using.
// (Note that you can use different pins if needed.)
// See http://arduino.cc/en/Reference/LiquidCrystal
// for more information:

LiquidCrystal lcd(12,11,5,4,3,2);

#define ONE_WIRE_BUS A0

SoftwareSerial mySerial(0, 1); // RX, TX


OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
T5403 barometer(MODE_I2C);

// RGB LED Pins
const int RED_PIN   = 6; // Common Anode Pinout
const int GREEN_PIN = 5; // Common Anode Pinout
const int BLUE_PIN  = 3; // Common Anode Pinout

// Temp Variables
float degF_Out, maxDegF_Out, minDegF_Out;

// Baro Variables
double relPress, relPressMax, relPressMin, absPress;
float baroTemp, baroTempMax, baroTempMin;
double altMin, altMax;
//double baseAltitude_m = 2591.11; //8501 feet Woodland Park
double baseAltitude_m;// = 2028.00; //6654 feet Colorado Springs
double calFactor = 0.07;

byte eepromByte;


void setup() 
{
  // put your setup code here, to run once:
  mySerial.begin(9600);

  // Initialize the Temp Sensor Library
  sensors.begin();
  barometer.begin();

  // The LiquidCrystal library can be used with many different
  // LCD sizes. We're using one that's 2 lines of 16 characters,
  // so we'll inform the library of that:
  
  lcd.begin(8, 2);  // The smaller one is actually 2 lines of 8 chars

  // Data sent to the display will stay there until it's
  // overwritten or power is removed. This can be a problem
  // when you upload a new sketch to the Arduino but old data
  // remains on the display. Let's clear the LCD using the
  // clear() command from the LiquidCrystal library:

  lcd.clear();

  // When the display powers up, the invisible cursor starts 
  // on the top row and first column.
  
  lcd.print("PWS");
  lcd.setCursor(0,1);
  lcd.print("V1.0");
  delay(5000);
  
  // configure RGB LED Pins
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);

  // Test RGB LED using Temp scale
  for (float i=-1.0; i<= 101.0; i+= 0.5)
  {
      showTempRGB(i);//, 0.0, 0.0);
      delay(5);
  }

  resetStatistics();
  
  // Initialize the Temp Sensor Library
  //sensors.begin();
  //barometer.begin();
  
  baseAltitude_m = 2028.00; //6654 feet Colorado Springs

  eepromByte = EEPROM.read(0);
  if (eepromByte == 0xFF)
  {
    EEPROM.put(0,baseAltitude_m);
  }
  else
  {
    baseAltitude_m = EEPROM.get(0,baseAltitude_m);
  }
}

void loop(void) 
{
    // put your main code here, to run repeatedly:
    int ledIntensity;
    int attempts = 0;
    int maxAttempts = 25;        

    //Need to display to LCD here
    // Here we'll set the invisible cursor to the first column
    // (column 0) of the first line (line 0):

    // Take a temperature every time
    // Take a temperature reading from the DS18B20 Sensor
    sensors.requestTemperatures(); // Send the command to get temperatures
    //degF_Out = 70.12; // Simulated Value
    degF_Out = sensors.getTempFByIndex(0);  // Device 1 is index 0
    showTempRGB(degF_Out);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("TO F");
    lcd.setCursor(0,1);
    lcd.print(degF_Out);
    delay(3000);

    // Get the pressure and temperature from the barometer
    baroTemp = barometer.getTemperature(FAHRENHEIT)/100.00;
    absPress = barometer.getPressure(MODE_ULTRA);

    baseAltitude_m = altitude(absPress,101325.000);
    
    // Sometime this may return negative values so we only want good values
    relPress = sealevel_inhg(absPress, baseAltitude_m) - calFactor;
    //relPress = 30.01; // Simulated Value
     // Bumped this from 28:32 to 26:34 since travelling down elevation caused way high unajusted pressure
    while (relPress < 26.00 )
    {
        mySerial.print(F("Pressure measurement too low (range 26.00 to 34.00): "));
        mySerial.print(relPress);
        mySerial.println(F(" in-hg"));
        relPress = sealevel_inhg(absPress, baseAltitude_m) - calFactor;
    }
    while (relPress > 34 && attempts < maxAttempts)
    {
        mySerial.print(F("Pressure measurement too high (range 26.00 to 34.00): "));
        mySerial.print(relPress);
        mySerial.print(F(" in-hg; Retry "));
        mySerial.print(attempts);
        mySerial.print(F(" of "));
        mySerial.println(maxAttempts);
        relPress = sealevel_inhg(absPress, baseAltitude_m) - calFactor;
        attempts += 1;
    }

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("TB F");
    lcd.setCursor(0,1);
    lcd.print(baroTemp);
    delay(3000);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("P inHg");
    lcd.setCursor(0,1);
    lcd.print(relPress);
    delay(3000);

    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Alt Ft");
    lcd.setCursor(0,1);
    lcd.print(baseAltitude_m*3.28);
    delay(3000);

    // Update the maxs
    if (degF_Out > maxDegF_Out)
    {
        maxDegF_Out = degF_Out;
    }
    if (baroTemp > baroTempMax)
    {
        baroTempMax = baroTemp;
    }
    if (relPress > relPressMax)
    {
        relPressMax = relPress;
    }
           
    // Update the mins
    if (degF_Out < minDegF_Out)
    {
        minDegF_Out = degF_Out;
    }
    if (baroTemp < baroTempMin)
    {
        baroTempMin = baroTemp;
    }
    if (relPress < relPressMin)
    {
        relPressMin = relPress;
    }
    // Print to Serial Port
    sendToSerial();
    
}// End of Main


void showTempRGB(float currentTemp)
{
  int redIntensity;
  int greenIntensity;
  int blueIntensity;

  float slopeRed;
  float slopeBlue;
  float slopeGreen;
  
  // Python equation: RedColorValue = slopeRed1*temp + (256 - slopeRed1*(t=0.0))
  // Python equation: BlueColorValue = 255;
  if (currentTemp <= 32)          // zone 1
  {
    slopeRed   = (0.00-255.00)/(32.00-0.00);
    slopeBlue  = 0;
    slopeGreen = 0;
    redIntensity = (int) (slopeRed*currentTemp + (256 - slopeRed*0));    // As Temp increases, Red Decreases
    blueIntensity = 255;           // blue is always on
    greenIntensity = 0;        // green is always off
  }
  
  // Python equation: RedColorValue = Off
  // y1.append(mb2*t[i] + (256.00 - mb2*t[t.index(32.0)]))
  // y2.append(mg1*t[i] + (0.0 - mg1*t[t.index(32.0)]))
  
  else if (currentTemp > 32.0 && currentTemp <= 70.0)          // zone 2
  {
    slopeRed   = 0;
    slopeBlue  = (0.00-255.00)/(70.00-32.00);
    slopeGreen = (255.00-0.00)/(70.00-32.00);
    redIntensity = 0;                                                       // As Temp increases, Keep Zero
    blueIntensity = (int) (slopeBlue*currentTemp + (256 - slopeBlue*32.0));           // As Temp increases, blue fades out
    greenIntensity = (int) (slopeGreen*currentTemp + (0.0 - slopeGreen*32.0));        // As Temp decreases, green fades in
  }

  // mr2*t[i] +(0.0 - mr2*t[t.index(70.0)])
  // Blue OFF
  // mg2*t[i] + (256.00 - mg2*t[t.index(70.0)])
  
  else if (currentTemp > 70.0 && currentTemp <= 90.0)          // zone 2
  {
    slopeRed   = (255.00-0.00)/(90.00-70.00);
    slopeBlue  = 0;
    slopeGreen = (0.00-255.00)/(90.00-70.00);
    redIntensity = (int) (slopeRed*currentTemp + (0.0 - slopeRed*70.0));              // As Temp increases, red fades in
    blueIntensity = 0;                                                        // As Temp increases, blue stays off
    greenIntensity = (int) (slopeGreen*currentTemp + (256 - slopeGreen*70.0));        // As Temp decreases, green fades out
  }

  
  else if (currentTemp > 90.0 && currentTemp <= 100.0)          // zone 2
  {
    slopeRed   = 0;
    slopeBlue  = (255.00-0.00)/(100.00-90.00);
    slopeGreen = (255.00-0.00)/(100.00-90.00);
    redIntensity = 255;              // As Temp increases, red fades in
    blueIntensity = (int) (slopeBlue*currentTemp + (0.0 - slopeBlue*90.0));                                                        // As Temp increases, blue stays off
    greenIntensity = (int) (slopeGreen*currentTemp + (0.0 - slopeGreen*90.0));        // As Temp decreases, green fades out
  }

  else if (currentTemp > 100.0)
  {
    redIntensity = 255;
    blueIntensity = 255;
    greenIntensity = 255;  
  }

  // Now that the brightness values have been set, command the LED
  // to those values

  mySerial.print(F("R="));
  mySerial.print(redIntensity);
  mySerial.print(F("; "));
  mySerial.print(F("G="));
  mySerial.print(greenIntensity);
  mySerial.print(F("; "));
  mySerial.print(F("B="));
  mySerial.print(blueIntensity);
  mySerial.println(F(";"));
  
  analogWrite(RED_PIN, redIntensity);
  analogWrite(BLUE_PIN, blueIntensity);
  analogWrite(GREEN_PIN, greenIntensity);
}

void sendToSerial()
{
    mySerial.print(F("T1_Min="));
    mySerial.print(minDegF_Out);
    mySerial.print(F("; "));
    mySerial.print(F("T1_Max="));
    mySerial.print(maxDegF_Out);
    mySerial.print(F("; "));
    mySerial.print(F("T1="));
    mySerial.print(degF_Out);
    mySerial.print(F("; "));
    mySerial.print(F("TBr_Min="));
    mySerial.print(baroTempMin);
    mySerial.print(F("; "));
    mySerial.print(F("TBr_Max="));
    mySerial.print(baroTempMax);
    mySerial.print(F("; "));
    mySerial.print(F("TBr="));
    mySerial.print(baroTemp);
    mySerial.print(F("; "));
    mySerial.print(F("P="));
    mySerial.print(relPress);
    mySerial.print(F("; "));
    mySerial.print(F("P_Min="));
    mySerial.print(relPressMin);
    mySerial.print(F("; "));
    mySerial.print(F("P_Max="));
    mySerial.print(relPressMax);
    mySerial.print(F("; "));
    mySerial.print(F("Alt="));
    mySerial.print(baseAltitude_m*3.28);
    mySerial.println(F(";"));
}

void resetStatistics()
{
    maxDegF_Out = -20;
    minDegF_Out = 130;
    baroTempMax = -20;
    baroTempMin = 130;
    relPressMax = 28.00;
    relPressMin = 31.00;
}

double sealevel_mb(double P_Pa, double A_m)
// Given Pressure P (Pa)taken at altitude A (meters)
// return equivalent pressure (mb or hPa) at sea level
{
  return ( (P_Pa/100)/pow(1-(A_m/44330.0),5.255));
}

double sealevel_inhg(double P_Pa, double A_m)
// Same as above, but return in inhg units
{
  double pressVar_mb;
  pressVar_mb = sealevel_mb(P_Pa, A_m);
  return ( pressVar_mb * 0.02953); // 1 mb = 0.02953 inhg
}

double altitude(double P, double P0)
// Given a pressure measurement P (Pa) and the pressure at a baseline P0 (Pa),
// return altitude (meters) above baseline.
{
  return(44330.0*(1-pow(P/P0,1/5.255)));
}
