// Example sketch which shows how to display a 64x32 animated GIF image stored in FLASH memory
// on a 64x32 LED matrix
//
// Credits: https://github.com/bitbank2/AnimatedGIF/tree/master/examples/ESP32_LEDMatrix_I2S
//

/* INSTRUCTIONS

   1. First Run the 'ESP32 Sketch Data Upload Tool' in Arduino from the 'Tools' Menu.
      - If you don't know what this is or see it as an option, then read this:
        https://github.com/me-no-dev/arduino-esp32fs-plugin
      - This tool will upload the contents of the data/ directory in the sketch folder onto
        the ESP32 itself.
        Step one is neccessary if you want to use use a GIF image on the clock display, although this function is somewhat commented out in the code

   2. You can drop any animated GIF you want in there, but keep it to the resolution of the
      MATRIX you're displaying to. To resize a gif, use this online website: https://ezgif.com/

   3. Have fun.
*/


//DHT
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>
#define DHTPIN 23
#define DHTTYPE    DHT22
DHT_Unified dht(DHTPIN, DHTTYPE);

uint32_t delayMS;
////////////////////////////////////////////////////


#define FILESYSTEM SPIFFS
#include "FS.h"
#include "SPIFFS.h"
#include <AnimatedGIF.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "image.h"
#include "image2.h" 
#include "humidity.h"
#include "humidity2.h"
#include "background.h"
#include "sun.h"
#include "sun2.h"
#include "sun3.h"
#include "morning.h"
#include "Moon.h"
#include <RTClib.h>
RTC_DS3231 rtc;
// ----------------------------
//RTOS Definition
#if CONFIG_FREERTOS_UNICORE
static const BaseType_t app_cpu = 0;
#else
static const BaseType_t app_cpu = 1;
#endif

/*
   Below is an is the 'legacy' way of initialising the MatrixPanel_I2S_DMA class.
   i.e. MATRIX_WIDTH and MATRIX_HEIGHT are modified by compile-time directives.
   By default the library assumes a single 64x32 pixel panel is connected.

   Refer to the example '2_PatternPlasma' on the new / correct way to setup this library
   for different resolutions / panel chain lengths within the sketch 'setup()'.

*/

//----------------------------------------Defines the connected PIN between P5 and ESP32.
#define R1_PIN 19
#define G1_PIN 13
#define B1_PIN 18
#define R2_PIN 5
#define G2_PIN 12
#define B2_PIN 17

#define A_PIN 16
#define B_PIN 27
#define C_PIN 4
#define D_PIN 26
#define E_PIN 14 //-1  //--> required for 1/32 scan panels, like 64x64px. Any available pin would do, i.e. IO32.

#define LAT_PIN 25
#define OE_PIN 15
#define CLK_PIN 2

#define PANEL_RES_X 64      // Number of pixels wide of each INDIVIDUAL panel module. 
#define PANEL_RES_Y 32     // Number of pixels tall of each INDIVIDUAL panel module.
#define PANEL_CHAIN 1      // Total number of panels chained one to another
//----------------------------------------
int Hours;
int H;
String Period;
int count1 = 0;
int count2 = 0;
int count3 = 0;
boolean increasing = true;

//MatrixPanel_I2S_DMA dma_display;
MatrixPanel_I2S_DMA *dma_display = nullptr;

uint16_t myRED = dma_display->color565(255, 0, 0);
uint16_t myGREEN = dma_display->color565(0, 255, 0);
uint16_t myBLUE = dma_display->color565(0, 0, 255);
uint16_t myWHITE = dma_display->color565(255, 255, 255);
uint16_t myYELLOW = dma_display->color565(255, 255, 0);
uint16_t myCYAN = dma_display->color565(0, 255, 255);
uint16_t myMAGENTA = dma_display->color565(255, 0, 255);
uint16_t myBLACK = dma_display->color565(0, 0, 0);

uint16_t myCOLORS[7] = {myRED, myGREEN, myBLUE, myYELLOW, myCYAN, myMAGENTA};
uint8_t c = 0;
int8_t scrollPos = 64;
unsigned long printStart;
unsigned long printTime = 0;
const uint8_t printPeriod = 50;

unsigned long serialStart;
unsigned long serialTime = 0;
const uint16_t serialPeriod = 1000;

String daysOfTheWeek[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
String Month[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

void drawImage(int x, int y, int imageWidth, int imageHeight, const unsigned short *image)
{
  int counter = 0;
  for (int yy = 0; yy < imageHeight; yy++)
  {
    for (int xx = 0; xx < imageWidth; xx++)
    {
      dma_display->drawPixel(xx + x , yy + y, image[counter]);
      counter++;
    }
  }
}
//-------------


AnimatedGIF gif;
File f;
int x_offset, y_offset;

//int delay_time=1000;

// Draw a line of image directly on the LED Matrix
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > MATRIX_WIDTH)
    iWidth = MATRIX_WIDTH;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < pDraw->iWidth)
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      } // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        for (int xOffset = 0; xOffset < iCount; xOffset++ ) {
          dma_display->drawPixel(x + xOffset, y, usTemp[xOffset]); // 565 Color Format
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount)
      {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  }
  else // does not have transparency
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++)
    {
      dma_display->drawPixel(x, y, usPalette[*s++]); // color 565
    }
  }
} /* GIFDraw() */


void * GIFOpenFile(const char *fname, int32_t *pSize)
{
  Serial.print("Playing gif: ");
  Serial.println(fname);
  f = FILESYSTEM.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  //  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name)
{
  start_tick = millis();

  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth()) / 2;
    if (x_offset < 0) x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight()) / 2;
    if (y_offset < 0) y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {
      if ( (millis() - start_tick) > 8000) { // we'll get bored after about 8 seconds of the same looping gif
        break;
      }
    }
    gif.close();
  }

} /* ShowGIF() */
///////////////////////////////////////////////////////////////////////////
/*
  ///////////////////////////////////////////////////////////////////TASK1/////////////////////////////////////
  void firstTask(void*parameter) {
  while (1) {
    //DHT


  }
  }



  ///////////////////////////////////////////////////////TASK2//////////////////////////////////////////////
  void secondTask(void*parameter) {
  while (1) {

  }
  }
  ///////////////////////////////////////////////////////////////////////////////////////////////////////
*/
/************************* Arduino Sketch Setup and Loop() *******************************/
void setup() {



  Serial.begin(115200);
  pinMode(33, INPUT);
  //DHT
  dht.begin();
  Serial.println(F("DHTxx Unified Sensor Example"));
  // Print temperature sensor details.
  sensor_t sensor;

  dht.temperature().getSensor(&sensor);
  Serial.println(F("------------------------------------"));
  Serial.println(F("Temperature Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("°C"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("°C"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("°C"));
  Serial.println(F("------------------------------------"));
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  Serial.println(F("Humidity Sensor"));
  Serial.print  (F("Sensor Type: ")); Serial.println(sensor.name);
  Serial.print  (F("Driver Ver:  ")); Serial.println(sensor.version);
  Serial.print  (F("Unique ID:   ")); Serial.println(sensor.sensor_id);
  Serial.print  (F("Max Value:   ")); Serial.print(sensor.max_value); Serial.println(F("%"));
  Serial.print  (F("Min Value:   ")); Serial.print(sensor.min_value); Serial.println(F("%"));
  Serial.print  (F("Resolution:  ")); Serial.print(sensor.resolution); Serial.println(F("%"));
  Serial.println(F("------------------------------------"));
  // Set delay between sensor readings based on sensor details.

  delayMS = sensor.min_delay / 1000;
  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


  delay(1000);
  //Initialize the connected PIN between Panel P5 and ESP32.
  HUB75_I2S_CFG::i2s_pins _pins = {R1_PIN, G1_PIN, B1_PIN, R2_PIN, G2_PIN, B2_PIN, A_PIN, B_PIN, C_PIN, D_PIN, E_PIN, LAT_PIN, OE_PIN, CLK_PIN};
  delay(10);

  HUB75_I2S_CFG mxconfig(
    PANEL_RES_X,   // module width
    PANEL_RES_Y,   // module height
    PANEL_CHAIN,    // Chain length
    _pins          //--> pin mapping.
  );



  // Set I2S clock speed.
  mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;  // I2S clock speed, better leave as-is unless you want to experiment.
  delay(10);


  // mxconfig.gpio.e = 18;
  // mxconfig.clkphase = false;
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;

  // Display Setup
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);
  dma_display->begin();
  dma_display->setBrightness8(150); //0-255
  dma_display->clearScreen();
  dma_display->setRotation(0);
  dma_display->fillScreen(myBLACK);
  delay(1000);
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }

  drawImage(0, 0, 64, 32, image1); //draw an image

  delay(5000);
  drawImage(0, 0, 64, 32, image22); //draw an image
  delay(3000);
  Serial.println("Starting AnimatedGIFs Sketch");

  // Start filesystem
  Serial.println(" * Loading SPIFFS");
  if (!SPIFFS.begin()) {
    Serial.println("SPIFFS Mount Failed");
  }

  // dma_display->begin();

  /* all other pixel drawing functions can only be called after .begin() */
  dma_display->fillScreen(dma_display->color565(0, 0, 0));
  gif.begin(LITTLE_ENDIAN_PIXELS);

  ///////////////////////////////////////////////////////
  dma_display->clearScreen();
  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);
  dma_display->setCursor(2, 4);
  dma_display->setTextColor(dma_display->color565(255, 0, 0));
  //dma_display->print(F("OWURA'S"));
  delay(3000);

  //rtc.adjust(DateTime(__DATE__, __TIME__)); uncomment when uploading to update date and time

  /////////////////////////////////////////////////////////////////
  dma_display->fillScreen(myBLACK);
  dma_display->setTextSize(1);      // Normal 1:1 pixel scale
  dma_display->setTextColor(dma_display->color565(255, 255, 255));  // White color

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
  /*
    xTaskCreatePinnedToCore(
      firstTask,
      "First Task",
      1024,
      NULL,
      1,
      NULL,
      app_cpu);


    xTaskCreatePinnedToCore(
      secondTask,
      "Second Task",
      1024,
      NULL,
      2,
      NULL,
      app_cpu);
  */
  /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


}

//String gifDir = "/gifs"; // play all GIFs in this directory on the SD card
char filePath[] = { "/gifs/do not disturb.gif"};
//File root, gifFile;



void loop()
{
  // delay(delayMS);
  // Get temperature event and print its value.
  sensors_event_t event;
  dht.temperature().getEvent(&event);
  if (isnan(event.temperature)) {
    Serial.println(F("Error reading temperature!"));
  }
  else {
    Serial.print(F("Temperature: "));
    Serial.print(event.temperature);
    Serial.println(F("°C"));
  }
  // Get humidity event and print its value.
  dht.humidity().getEvent(&event);
  if (isnan(event.relative_humidity)) {
    Serial.println(F("Error reading humidity!"));
  }
  else {
    Serial.print(F("Humidity: "));
    Serial.print(event.relative_humidity);
    Serial.println(F("%"));
  }

  ///////////////////////////////////////////////
  dht.temperature().getEvent(&event);
  int Temp = event.temperature;
  dht.humidity().getEvent(&event);
  int Hum = event.relative_humidity;
  //int button = digitalRead(33);
  //Serial.println(button);

  /*
    while (1) // run forever
    {

    root = FILESYSTEM.open(gifDir);
    if (root)
    {
      gifFile = root.openNextFile();
      while (gifFile)
      {
        if (!gifFile.isDirectory()) // play it
        {

          // C-strings... urghh...
          memset(filePath, 0x0, sizeof(filePath));
          strcpy(filePath, gifFile.path());

          // Show it.
          ShowGIF(filePath);

        }
        gifFile.close();
       gifFile = root.openNextFile();

      }
      root.close();
    } // root

    delay(1000); // pause before restarting

    } // while
  */
  //memset(filePath, 0x0, sizeof(filePath));
  //strcpy(filePath, gifFile.path());

  // Show it.
  /*
    if (button == 1) {
    ShowGIF(filePath);
    }
    if (button == 0) {
    for (uint8_t i = 0; i < 10; i++) {
      uint8_t next_digit = (i + 1) % 10;
      morphDigits(i, next_digit, 24, 16, 10, 100);
      delay(1000);
    }

    }

  */

  // yield();
  //ShowGIF(filePath);

  DateTime now = rtc.now();

  serialStart = millis();

  if (serialStart - serialTime >= serialPeriod) {
    serialTime = serialStart;
    Serial.print("Time:- ");
    // Serial.printf("%02d", now.hour(), DEC);
    Serial.print(H);
    Serial.print(": ");
    Serial.printf("%02d", now.minute(), DEC);
    Serial.print(": ");
    Serial.printf("%02d", now.second(), DEC);
    Serial.println();

    Serial.print("Date:- ");
    Serial.print(now.day(), DEC);
    Serial.print(".");
    Serial.printf("%02d", now.month(), DEC);
    Serial.print(".");
    Serial.println(now.year(), DEC);

    Serial.print("Today is ");
    Serial.println(daysOfTheWeek [now.dayOfTheWeek()]);

    Serial.println();
    //  delay(50);


  }



  ///24Hours to 12 Hours calculation
  H = now.hour();

  if (H == 0) {
    Hours = 12;
    Period = "AM";
  }

  else if (H >= 1 && H <= 11) {
    Period = "AM";
    Hours = H;
  }

  else if (H == 12) {
    Hours = 12;
    Period = "PM";
  }

  if (H >= 13 && H <= 23) {
    Period = "PM";
    Hours = (H - 12);
  }

  //////////////////////////////////////////////////////////////////////////////////////////////






  //////////////////////////////////////////////////////////////////////////////////////////////////
  // dma_display->fillScreen(dma_display->color565(0, 0, 0));  // Clear screen
  dma_display->clearScreen();
  dma_display->setTextWrap(false);
  dma_display->setTextSize(1);


  dma_display->setCursor(0, 0);
  dma_display->print(Hours);

  //dma_display->setCursor(11, 0);
  dma_display->print(":");

  //dma_display->setCursor(15, 0);
  dma_display->print(now.minute());


  //dma_display->setCursor(26, 0);
  dma_display->print(":");
  //dma_display->setCursor(30, 0);
  dma_display->print( now.second());

  //dma_display->setCursor(42, 7);
  dma_display->print(Period);////AM or PM

  dma_display->setCursor(0, 8);
  dma_display->print(daysOfTheWeek [now.dayOfTheWeek()]);

  dma_display->setTextColor(dma_display->color565(0, 0, 0));  // White color

  //////////////////////////////////////////////////////////////////////////////////////////////////////////

  /*
    if (Hours == 12 || 1 >= Hours <= 4 && Period == "PM") {
      dma_display->setBrightness8(210); //0-255
      drawImage(54, 10, 10, 22, sunIcon); //draw an image
    }

     if (Hours > 4 && Hours < 7 && Period == "PM") {
      dma_display->setBrightness8(150); //0-255
      drawImage(54, 10, 10, 22, sunIcon3); //draw an image

    }

    if (Hours >= 7 && Hours <= 11 && Period == "PM") {
      dma_display->setBrightness8(100); //0-255
      drawImage(54, 8, 10, 22, moonIcon); //draw an image
    }


     if (Hours == 12 || Hours >= 1 && Hours <= 6  && Period == "AM") {
      dma_display->setBrightness8(50); //0-255
      drawImage(54, 10, 10, 22, morningIcon); //draw an image
    }

     if (Hours > 6 && Hours <= 11 && Period == "AM") {
      dma_display->setBrightness8(170); //0-255
      drawImage(54, 10, 10, 22, sunIcon2); //draw an image
    }

  */



  if (H >= 13 && H <= 16) {
    dma_display->setBrightness8(210); //0-255
    drawImage(54, 10, 10, 22, sunIcon); //draw an image
  }

  if (H >= 17 && H <= 19) {
    dma_display->setBrightness8(150); //0-255
    drawImage(54, 10, 10, 22, sunIcon3); //draw an image

  }

  if (H >= 20 && H <= 23) {
    dma_display->setBrightness8(100); //0-255
    drawImage(54, 8, 10, 22, moonIcon); //draw an image
  }


  if (H >= 6 && H <= 12) {
    dma_display->setBrightness8(50); //0-255
    drawImage(54, 10, 10, 22, morningIcon); //draw an image
  }

  if (H == 0 || H > 1 && H < 6) {
    dma_display->setBrightness8(170); //0-255
    drawImage(54, 10, 10, 22, sunIcon2); //draw an image
  }


  //drawImage(54, 10, 10, 22, sunIcon); //draw an image

  ////////////////////////////////////////////////////////////////////////////////////////////////////////////

  if (now.day() >= 10) {
    drawImage(37, 12, 20, 20, humIcon); //draw an image
  }
  else if (now.day() < 10) {
    drawImage(37, 12, 20, 20, humIcon2); //draw an image
  }


  dma_display->setCursor(39, 20);
  dma_display->print(Hum);
  dma_display->print("%");



  // dma_display->setTextColor(dma_display->color565(0, 0, 0));  // black color
  drawImage((count2 - 1), (16 - count3), 33, 18, back); //draw background image
  dma_display->setTextSize(2);
  dma_display->setCursor((count2 + 1), 17);
  dma_display->print(Temp);

  dma_display->setTextSize(1);
  dma_display->setCursor((26 + count2), 22);
  dma_display->print("C");

  dma_display->setCursor((22 + count2), 15);
  dma_display->print(".");

  dma_display->setTextColor(dma_display->color565(255, 255, 255));  // White color
  dma_display->setCursor(19, 8);
  dma_display->print(now.day());
  //dma_display->print("/");
  dma_display->print((Month - 1)[now.month()]);

  //dma_display->drawLine(0, 0, 64, 2, dma_display->color565(150, 0, 0));
  //drawImage(54, 10, 10, 22, morningIcon); //draw an image

  vTaskDelay(1000 / portTICK_PERIOD_MS);

  count1++;
  count3++;

  if (count3 > 1) {
    count3 = 0;
  }

  if (count2 <= 0) {
    increasing = true;

  }
  if (increasing == true) {
    count2++;
  }
  if (count2 > 5) {
    increasing = false; // Switch to counting down
  }
  if (increasing == false) {
    count2--;
  }

  Serial.println(count2);


  if (count1 == 200) {
    drawImage(0, 0, 64, 32, image1); //draw an image
    vTaskDelay(6000 / portTICK_PERIOD_MS);

  }

  if (count1 >= 400) {
    dma_display->clearScreen();
    drawImage(0, 0, 64, 32, image22); //draw an image
    vTaskDelay(6000 / portTICK_PERIOD_MS);
    count1 = 0;
  }


  dma_display->setTextColor(dma_display->color565(255, 255, 255));  // White color

}
/*
  void drawDigit(uint8_t digit, int x, int y) {
  dma_display->setTextSize(1);      // Normal 1:1 pixel scale
  dma_display->setTextColor(dma_display->color565(255, 255, 255));  // White color
  dma_display->setCursor(0, 0);
  dma_display->print("digit");
  }

  void morphDigits(uint8_t start_digit, uint8_t end_digit, int x, int y, uint8_t steps, uint16_t delay_time) {
  for (uint8_t i = 0; i < steps; i++) {
    float progress = (float)i / (float)(steps - 1);
    int intermediate_digit = start_digit + progress * (end_digit - start_digit);
    dma_display->fillScreen(dma_display->color565(0, 0, 0));  // Clear screen
    drawDigit(intermediate_digit, x, y);
    delay(delay_time);
  }
  dma_display->fillScreen(dma_display->color565(0, 0, 0));  // Clear screen
  drawDigit(end_digit, x, y);
  }
*/



/*
  void loop() {
  for (uint8_t i = 0; i < 10; i++) {
    uint8_t next_digit = (i + 1) % 10;
    morphDigits(i, next_digit, 24, 16, 10, 100);
    delay(1000);
  }
  }

  void drawDigit(uint8_t digit, int x, int y) {
  display->setTextSize(1);      // Normal 1:1 pixel scale
  display->setTextColor(display->color565(255, 255, 255));  // White color
  display->setCursor(x, y);
  display->print(digit);
  }

  void morphDigits(uint8_t start_digit, uint8_t end_digit, int x, int y, uint8_t steps, uint16_t delay_time) {
  for (uint8_t i = 0; i < steps; i++) {
    float progress = (float)i / (float)(steps - 1);
    int intermediate_digit = start_digit + progress * (end_digit - start_digit);
    display->fillScreen(display->color565(0, 0, 0));  // Clear screen
    drawDigit(intermediate_digit, x, y);
    delay(delay_time);
  }
  display->fillScreen(display->color565(0, 0, 0));  // Clear screen
  drawDigit(end_digit, x, y);
  }
*/
