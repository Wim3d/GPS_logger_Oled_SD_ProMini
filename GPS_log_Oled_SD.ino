/*
 * This logger uses an Arduino Pro mini 3.3V 328
 * Written by W. Hoogervorst
 */
#include <SSD1306_text.h>

#include <SPI.h>
#include <SD.h>
#include "TinyGPS++.h"
#include "SoftwareSerial.h"

// SD card
#define SD_CS     10

static uint8_t Byte1[16] = {128, 0, 128, 136, 28, 54, 97, 195, 230, 252, 248, 248, 120, 192, 128, 0};
static uint8_t Byte2[16] = {7, 28, 48, 99, 78, 200, 152, 145, 177, 3, 6, 12, 24, 12, 5, 3};

char filename[] = "00000000.CSV";

uint8_t speed = 0;
uint8_t sat = 0;
uint16_t  write_interval = 10000;
uint32_t  last_write = 0;

uint8_t prev_speed = 255; // quite impossible value so the speed is updated if starting to drive
uint8_t prev_sat = 255; // quite impossible value so the speed is updated if starting to drive

//variables for interrupt ans switching
volatile int state = LOW;
uint8_t interruptPin = 2;
uint8_t switchcount = 1;
uint8_t counts = 2;

// variables for average speed
uint16_t time_interval_ms;
uint32_t time_avg_speed;
uint8_t avg_speed;
uint16_t dist_interval;
float dist_total_m;
uint32_t PrevTimeMeas = 0;
uint8_t prev_avgspeed = 255;  // quite impossible value so the avg speed is updated if starting to drive

// define Oled on I2C
SSD1306_text oled(5);

//define serial connection for GPS
SoftwareSerial serial_connection(4, 3);  // RX (to TX op GPS) , TX (to RX of GPS)

//define GPS variable
TinyGPSPlus gps;

File dataFile;

void setup(void) {
  attachInterrupt(digitalPinToInterrupt(interruptPin), count, FALLING); //pin is pulled up in normal state
  // Serial.begin(9600);

  // Initialize, optionally clear the screen
  oled.init();
  oled.clear();                 // clear screen

  oled.setTextSize(2, 2);
  oled.setCursor(1, 3);       // move cursor to row #, pixel column #
  oled.write("GPS logger");
  /* oled.setCursor(2, 27);
    oled.write("Starting up...");
  */
  oled.setCursor(5, 57);
  for (int i = 0; i < 16; i++)
  {
    oled.sendData(Byte1[i]);
  }
  oled.setCursor(6, 57);
  for (int j = 0; j < 16; j++)
  {
    oled.sendData(Byte2[j]);
  }

  delay (2000);

  //setup SD card
  // Serial.println("Initializing SD card...");
  // see if the SD card is present and can be initialized:
  if (!SD.begin(SD_CS)) {
    //    Serial.println("Card failed, or not present");
    oled.clear();                 // clear screen
    oled.setTextSize(1, 1);
    oled.setCursor(2, 25);
    oled.write("No valid SD card");
    //    oled.setCursor(4, 35);
    //   oled.write("No logging");
    // don't do anything more in setup:
    return;
  }

  oled.clear();                 // clear screen
  oled.setTextSize(2, 2);
  oled.setCursor(0, 3);
  oled.write("SD card OK");
  serial_connection.begin(9600); //This opens up communications to the GPS

  smartDelay(1000);

  oled.clear();                 // clear screen
  oled.setCursor(0, 21);
  oled.write("Waiting");
  oled.setCursor(3, 45);
  oled.write("for");
  oled.setCursor(6, 21);
  oled.write("GPS fix");

  while (gps.satellites.value() == 0)
  {
    smartDelay(1000);
  }
  // create filename
  smartDelay(1000);
  getFilename(filename);
  //Serial.println (filename);
  //  Serial.println ("file printed");
  //create csv file
  //dataFile = SD.open("GPSlog2.csv", FILE_WRITE);
  dataFile = SD.open(filename, FILE_WRITE);
  dataFile.println(" ");
  dataFile.println("Latitude;Longitude;Date;Time;Speed (KM/H); Heigt(m);Satellite count");
  dataFile.close();

  smartDelay(100);

  //print main characters
  oled.clear();                 // clear screen
  oled.setTextSize(1, 1);
  // draw picture of satellite
  oled.setCursor(0, 111);
  for (int i = 0; i < 16; i++)
  {
    oled.sendData(Byte1[i]);
  }
  oled.setCursor(1, 111);
  for (int j = 0; j < 16; j++)
  {
    oled.sendData(Byte2[j]);
  }

  oled.setCursor(6, 90);
  oled.write("km/h");
  oled.setTextSize(1, 1);
  oled.setCursor(1, 95);
  oled.print(sat);
}

void loop()
{
  //  first handle the interrupt request
  if (state == HIGH)
  {
    //refresh screen
    oled.clear();
    // draw picture of satellite
    oled.setCursor(0, 111);
    for (int i = 0; i < 16; i++)
    {
      oled.sendData(Byte1[i]);
    }
    oled.setCursor(1, 111);
    for (int j = 0; j < 16; j++)
    {
      oled.sendData(Byte2[j]);
    }

    oled.setTextSize(1, 1);
    oled.setCursor(6, 90);
    oled.write("km/h");
    oled.setCursor(1, 95);
    oled.print(sat);


    if (switchcount == 1) // if we are switching to average speed mode, average numbers are resetted for avg speed calculation, and average format is printed
    {
      dist_total_m = 0;
      time_avg_speed = millis();
      PrevTimeMeas = millis();

      oled.setTextSize(1, 1);
      oled.setCursor(2, 0);
      oled.write("avg:");
      oled.setCursor(2, 62);
      oled.write("km/h");
      prev_avgspeed = 255;  // quite impossible value so the avg speed is updated if starting to drive
    }
    switchcount = switchcount + 1;
    state = LOW;
    prev_speed = 255;  // quite impossible value so the avg speed is updated if starting to drive
  }
  if (switchcount > counts)
  {
    switchcount = 1;
  }

  //  print data on the screen every second
  Updatescreen();
  //smartDelay(1000);

  // write to file every write_interval
  if (millis() > last_write + write_interval)
  {
    dataFile = SD.open(filename, FILE_WRITE);
    //dataFile = SD.open("GPSlog2.csv", FILE_WRITE);
    //Latitude      Longitude     Date     Time    Speed    Satellites
    dataFile.print(gps.location.lat(), 6);
    dataFile.print(" ;");
    dataFile.print(gps.location.lng(), 6);
    dataFile.print(";");
    dataFile.print(gps.date.value());
    dataFile.print(";");
    dataFile.print(gps.time.value());
    dataFile.print(";");
    dataFile.print(gps.speed.kmph(), 0);
    dataFile.print(";");
    dataFile.print(gps.altitude.meters(), 0);
    dataFile.print(";");
    dataFile.println(gps.satellites.value());
    dataFile.close();
    last_write = millis();
  }
  smartDelay(500);

}

// separate functions
void Updatescreen(void)
{
  // print number of satellites
  sat = (uint8_t)gps.satellites.value();
  if (sat != prev_sat) // only update the value on screen if the number of satellites is changed
  {
    erase(1, 95, 1, 2);
    oled.setTextSize(1, 1);
    oled.setCursor(1, 95);
    oled.print(sat);
    prev_sat = sat;
  }

  speed = (uint8_t)round(gps.speed.kmph());
  if (gps.satellites.value() == 0)
  {
    speed = 0;
  }

  if (speed != prev_speed) // only update the value on screen if the speed is changed
  {
    oled.setTextSize(3, 3);
    if (speed < 10)
    {
      if (prev_speed >= 10)
      {
        erase(5, 14, 3, 3);
      }
      oled.setCursor(5, 50);
      oled.print(speed);
    }
    else
    {
      if (speed < 100)
      {
        if (prev_speed >= 100)
        {
          erase(5, 14, 3, 3);
        }

        oled.setCursor(5, 32);
        oled.print(speed);
      }
      else
      {
        oled.setCursor(5, 14);
        oled.print(speed);
      }
    }
    prev_speed = speed;
  }

  if (switchcount == 2) // switchcount determines lay out of screen
  {
    // calculate average speed
    // calculate distance for calculating average speed
    //time in ms
    time_interval_ms = millis() - PrevTimeMeas;
    dist_interval = time_interval_ms * gps.speed.kmph() / 3.6; // travelled distance in m in interval
    dist_total_m = dist_total_m + (float)dist_interval / 1000;
    PrevTimeMeas = millis();

    float duration_avg_speed = ((float)millis() - (float)time_avg_speed ) / 1000; // we are measuring time in seconds

    avg_speed = (uint8_t) round(3.6 * dist_total_m / duration_avg_speed); //average speed in m/s

    if (avg_speed != prev_avgspeed) // only update the value on screen if the avg speed is changed
    {
      oled.setTextSize(2, 2);
      if (avg_speed < 10)
      {
        if (prev_avgspeed >= 10)
        {
          erase(1, 24, 2, 3);
        }
        oled.setCursor(1, 48);
        oled.print(avg_speed);
      }
      else
      {
        if (avg_speed < 100)
        {
          if (prev_avgspeed >= 100)
          {
            erase(1, 24, 2, 3);
          }

          oled.setCursor(1, 36);
          oled.print(avg_speed);
        }
        else
        {
          oled.setCursor(1, 24);
          oled.print(avg_speed);
        }
      }
    }
    prev_avgspeed = avg_speed;
  }
}

static void smartDelay(unsigned long ms)
{
  unsigned long start = millis();
  do
  {
    // If data has come in from the GPS module
    while (serial_connection.available())
      gps.encode(serial_connection.read()); // Send it to the encode function
    // tinyGPS.encode(char) continues to "load" the tinGPS object with new
    // data coming in from the GPS module. As full NMEA strings begin to come in
    // the tinyGPS library will be able to start parsing them for pertinent info
    //yield(); // feed the watchdog
  } while (millis() - start < ms);
}

void getFilename(char *filename) {
  filename[0] = '2';
  filename[1] = '0';
  filename[2] = (gps.date.year() - 2000) / 10 + '0';
  filename[3] = gps.date.year() % 10 + '0';
  filename[4] = gps.date.month() / 10 + '0';
  filename[5] = gps.date.month() % 10 + '0';
  filename[6] = gps.date.day() / 10 + '0';
  filename[7] = gps.date.day() % 10 + '0';
  filename[8] = '.';
  filename[9] = 'C';
  filename[10] = 'S';
  filename[11] = 'V';
  return;
}

void erase(int row, int pos, int sizes, int chars)
{
  for (int i = 0; i < sizes; i++)
  {
    oled.setCursor(row + i, pos); //set cursor at row and continu for sizes
    for (int j = 0; j < 6 * sizes * chars ; j++)   //width character is 6 x sizes, length of string is number of chars
    {
      oled.sendData(0);
    }
  }
}

// interrupt function
void count() {
  state = HIGH;
}
