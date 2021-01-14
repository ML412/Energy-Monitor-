/* For the senior research project in the NSCC Electronic Engineering Technology program, I designed a prototype energy monitoring device in partnership with the
NSCC Applied Energy Research Lab and a fellow classmate. This energy monitor is capable of measuring two voltages, five currents, temperature, and the flow of water leaving a 
residential hotwater tank. The purpose of this project was to design a fairly inexpensive energy monitor that could be installed in a home and measure a variety of quantities
to allow the homeowner to make informed decisions about the energy usage of their home based on the data provided by the device.

The device does a pretty good job of measuring the desired quantities, for a student prototype. There are however many improvements that could be made if the project were
to ever leave the prototype stage. Our voltage measurements were very accurate (within 1% or the line voltage as measured with a DMM), but our measurements were very prone to noise
which caused the voltage measurements to jump an alarming +/- 5V from the line voltage. We did not have enough time in the school year to eliminate the noise, but we suspect that the 
noise may have been the result of several factors (stray charges in the air, the length of the wires from the voltage sensor to the mcu, etc). We did design a PCB for the 
device but it was never assembled and tested due to COVID-19 shutting everything down. We believe that getting the energy monitor off the breadboards and onto a PCB would help
counter the noise on our voltage measurements by reducing the lengths of conductor paths from the voltage sensors to the mcu and by enclosing the PCB in a container to help counter
stray charges in the air. We made heavy use of statistical sampling to try to make it so that the final voltage measurements at the end of a given loop were the average of many
measurements, this of course slows the device down significantly. A PCB with proper shielding and conductors could alleviate some of the stress on the processor by reducing noise
and allowing us to remove the code which implemented the sampling.

Another improvement to be made is to how the device measures current. We used simple resistor voltage dividers to provide a 1.65V DC offset to the output of the current sensors
which ensures that the output voltage of the current sensors never drops below zero, which could potentially damage the mcu. Instead of using resistors to create the DC offset, it
would be better to use opamp voltage dividers due to the opamps high impedance and temperature coefficients. Our device never got the chance to measure current beyond 3.5-4A due to
difficulties generating higher currents at the school. If the device ever leaves the prototype stage, opamp voltage dividers would need to be implemented, as well as creating new
transfer functions to measure higher currents. 

This device runs off an ESP32 but was programmed using the Arduino editor and ESP32 Arduino library functions. Beyond the prototype stage, it would be best to program in the
ESP32 IDE itself for efficiency. The device uses the ESP32 to send the data over wifi to a server in the lab where it is stored. */

#include "driver/adc.h"
#include "esp_adc_cal.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "EmonLib.h" 

#define VOLT_CAL 110
#define DEFAULT_VREF 1100 
#define NO_OF_SAMPLES 200 
#define size 150   

const int ONE_WIRE_A = 23;
OneWire oneWireA(ONE_WIRE_A);
DallasTemperature sensorsA(&oneWireA);
DeviceAddress tempDeviceAddressA;
EnergyMonitor emon1, emon2;
WiFiClient client;

const char* ssid = "*****"; //insert homeowner wifi and password
const char* pass = "*****";
const char* host = "192.168.1.151"; 
String url = "/data/post.php";

static const adc_atten_t atten = ADC_ATTEN_DB_11; 
static const adc_unit_t unit = ADC_UNIT_1;
static esp_adc_cal_characteristics_t *adc_chars; //for current gpio 34
static const adc_channel_t channel = ADC_CHANNEL_6; 
static esp_adc_cal_characteristics_t *adc_chars4; //for voltage1 gpio 32
static const adc_channel_t channel4 = ADC_CHANNEL_4; 
static esp_adc_cal_characteristics_t *adc_chars5; //for voltage2 gpio 33
static const adc_channel_t channel5 = ADC_CHANNEL_5; 

const int s0 = 14;
const int s1 = 27;
const int s2 = 26;
const int s3 = 25;
const int d1 = 19;
const int d2 = 5;
const int vp1 = 32;
const int vp2 = 33;

int numA;
float flow1;
float V1, V2;
float C1, C2, C3, C4, C5;
char buffer[size];

void initADC1CH6()//current
{
  //Configure ADC
  if (unit == ADC_UNIT_1) {
    adc1_config_width(ADC_WIDTH_BIT_10);
    adc1_config_channel_atten((adc1_channel_t) channel, atten);
  } else {
    adc2_config_channel_atten((adc2_channel_t)channel, atten);
  }
  //Characterize ADC
  adc_chars = (esp_adc_cal_characteristics_t *) calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_10, DEFAULT_VREF, adc_chars);
}

void setup()  
{
  pinMode(s0, OUTPUT); 
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
  pinMode(s3, OUTPUT);
  pinMode(d1, OUTPUT);
  pinMode(d2, OUTPUT);
  initADC1CH6();
  initADC1CH4();
  initADC1CH5();
  emon1.voltage(vp1, VOLT_CAL, 1.7); 
  emon2.voltage(vp2, VOLT_CAL, 1.7);
  Serial.begin(115200);
  WiFi.begin("*****", "*****");
  WIFI_CHECK();
   while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.println("Connecting to network...");
  }
  Serial.print("Connected to network with local IP: ");
  Serial.println(WiFi.localIP());
  
  sensorsA.begin();
  numA = sensorsA.getDeviceCount();
  printf("numA = %d\n", numA);
  for (int i = 0; i < 8; i++) //should be < numA....but there seems to be a problem
  {
    if (sensorsA.getAddress(tempDeviceAddressA, i))
   {
      Serial.print("Found device on ONE_WIRE_A: ");
      Serial.print(i, DEC);
      Serial.print(" with address: ");
      printAddress(tempDeviceAddressA);
      Serial.println();
    }
  }
  delay(2000);
}

/* We make use of the delay() function throughout the code. We found that it helps ensure that the functions are implemented properly before the mcu attempts to jump to the next
statments which control the sensors. */
void loop() 
{
  WIFI_CHECK();
  delay(1000);
  emon1.calcVI(20,2000); 
  emon2.calcVI(20,2000);
  V1 = emon1.Vrms;
  V2 = emon2.Vrms;
  printf("V1 = %.1f V\n", V1);
  printf("V2 = %.1f V\n", V2);
  CH15();
  delay(250);
  C1 = Current();
  printf("Current1 = %.1f A\n", C1);
  CH14();
  delay(250);
  C2 = Current(); 
  printf("Current2 = %.1f A\n", C2);
  CH13();
  delay(250);
  C3 = Current();
  printf("Current3 = %.1f A\n", C3);
  CH12();
  delay(250);
  C4 = Current();
  printf("Current4 = %.1f A\n", C4);
  CH11();
  delay(250);
  C5 = Current();
  printf("Current5 = %.1f A\n", C5);
  delay(250);
  sensorsA.requestTemperatures();
  float t0 = sensorsA.getTempCByIndex(0);
  delay(250);
  float t1 = sensorsA.getTempCByIndex(1);
  delay(250);
  float t2 = sensorsA.getTempCByIndex(2);
  printf("temp0 = %.1f C\n", t0);
  printf("temp1 = %.1f C\n", t1);
  printf("temp2 = %.1f C\n", t2);

 flow1 = 25; //placeholder value to be sent to the server while the actual flow sensor is not connected to running water
 sprintf(buffer, "Voltage1=%.1f&Voltage2=%.1f&Current1=%.1f&Current2=%.1f&Current3=%.1f&Current4=%.1f&Current5=%.1f&Temp1=%.1f&Temp2=%.1f&Temp3=%.1f&Flow=%.1f\r\n\r\n", V1, V2, C1, C2, C3, C4, C5, t0, t1, t2, flow1); 
 printf("%s\n", buffer);
 post(buffer);
 delay(1000);
}

void post(String PostData) 
{
  Serial.println("Connecting to Server");
  if (client.connect("192.168.1.151", 80)) 
  {
    Serial.println("connected");
    client.println("POST /data/post.php HTTP/1.1");
    client.println("Host:  192.168.1.151");
    client.println("User-Agent: ESP32");
    client.println("Connection: close");
    client.println("Content-Type: application/x-www-form-urlencoded");
    client.print("Content-Length: ");
    client.println(PostData.length());
    client.println();
    client.println(PostData);
  }
  else 
  {
    Serial.println("Connection Failed");
  }
}
float Current()
{
  float Period = 16667;
  float average = 0, current = 0, start = 0, sum = 0, mini = 0, maxi = 0;
  int value;
  average = 0;
  current = 0;

  for (int i = 0; i < NO_OF_SAMPLES; i++)
  {
    sum = 0;
    maxi = 0;
    mini = 1024;
    start = micros();
    while ( (micros() - start) < Period)
    {
      value = adc1_get_raw((adc1_channel_t)channel);
      if ( value > maxi)
        maxi = value;
      if ( value < mini)
        mini = value;
    }
    sum = sum + (maxi - mini);
  }
  average = sum / NO_OF_SAMPLES;
  //current = (3.0753 * average) + 0.1431;
  current = (0.6887 * average) + 0.2108;
  return current;
}

float Voltage()
{
  float Period = 16667;
  float average = 0, voltage = 0, start = 0, sum = 0, minv = 0, maxv = 0;
  int value;

  average = 0;
  voltage = 0;

  for (int i = 0; i < NO_OF_SAMPLES; i++)
  {
    sum = 0;
    maxv = 0;
    minv = 1024;
    start = micros();
    while ( (micros() - start) < Period)
    {
      value = adc1_get_raw((adc1_channel_t)channel4);
      if ( value > maxv)
        maxv = value;
      if ( value < minv)
        minv = value;
    }
    sum = sum + (maxv - minv);
  }
  average = sum / NO_OF_SAMPLES;
  voltage = (215.96 * average) - 132.99; //non-ideal 100k pulldown
  return voltage;
}

float Flow()
{
  float frequency = 0, avgperiod = 0, flow = 0;
  long high = 0, low = 0, period = 0, sum = 0;
  const int flow_pin = 21;

  for (int i = 0; i < 100; i++)
  {
    high = pulseIn(flow_pin, HIGH, 250000);
    low = pulseIn(flow_pin, LOW, 250000);
    period = (high + low);
    sum = sum + period;
  }

  avgperiod = sum / 100;
  frequency = 1000000 / avgperiod;
  flow = (frequency * 0.1151) + 0.9628;
  return flow;
}

void CH15() 
{
  digitalWrite(s0, HIGH);
  digitalWrite(s1, HIGH);
  digitalWrite(s2, HIGH);
  digitalWrite(s3, HIGH);
}
void CH14()
{
  digitalWrite(s0, LOW);
  digitalWrite(s1, HIGH);
  digitalWrite(s2, HIGH);
  digitalWrite(s3, HIGH);
}
void CH13()
{
  digitalWrite(s0, HIGH);
  digitalWrite(s1, LOW);
  digitalWrite(s2, HIGH);
  digitalWrite(s3, HIGH);
}
void CH12()
{
  digitalWrite(s0, LOW);
  digitalWrite(s1, LOW);
  digitalWrite(s2, HIGH);
  digitalWrite(s3, HIGH);
}
void CH11()
{
  digitalWrite(s0, HIGH);
  digitalWrite(s1, HIGH);
  digitalWrite(s2, LOW);
  digitalWrite(s3, HIGH);
}
void CH0()
{
  digitalWrite(s0, LOW);
  digitalWrite(s1, LOW);
  digitalWrite(s2, LOW);
  digitalWrite(s3, LOW);
}

void BlueLED()
{
  digitalWrite(d1, HIGH);
  digitalWrite(d2, LOW);
}
void RedLED()
{
  digitalWrite(d1, LOW);
  digitalWrite(d2, HIGH);
}
void WIFI_CHECK()
{
  if(WiFi.status() != WL_CONNECTED)
  {
    RedLED();
  }
  else BlueLED();
}

void printAddress(DeviceAddress deviceAddress)
{
  for (uint8_t i = 0; i < 8; i++)
  {
    if (deviceAddress[i] < 16) Serial.print("0");
    Serial.print(deviceAddress[i], HEX);
    
  }
}

void initADC1CH4() //voltage1 
{
  //Configure ADC
  if (unit == ADC_UNIT_1) {
    adc1_config_width(ADC_WIDTH_BIT_10);
    adc1_config_channel_atten((adc1_channel_t) channel4, atten);
  } else {
    adc2_config_channel_atten((adc2_channel_t)channel, atten);
  }
  //Characterize ADC
  adc_chars4 = (esp_adc_cal_characteristics_t *) calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_10, DEFAULT_VREF, adc_chars4);
}

void initADC1CH5() //voltage2
{
  //Configure ADC
  if (unit == ADC_UNIT_1) {
    adc1_config_width(ADC_WIDTH_BIT_10);
    adc1_config_channel_atten((adc1_channel_t) channel5, atten);
  } else {
    adc2_config_channel_atten((adc2_channel_t)channel, atten);
  }

  //Characterize ADC
  adc_chars5 = (esp_adc_cal_characteristics_t *) calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(unit, atten, ADC_WIDTH_BIT_10, DEFAULT_VREF, adc_chars5);
  
}

