






///////////smo.ino (main)


#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <math.h>
#include <string.h>

#include <pico/stdlib.h>
#include <hardware/clocks.h>
#include <pico/unique_id.h>
#include <hardware/adc.h>


// ST7789 INIT

#define TFT_CS   17
#define TFT_DC   21
#define TFT_RST  20
#define TFT_BL   15


#define SDA 7
#define SCL 6

// BUTTONS

#define BTN_R       1
#define BTN_L       0
#define BTN_UP      2
#define BTN_DOWN    3
#define BTN_SELECT  4
#define BTN_BACK    5

#define PWM_OUT_PIN 13

#define TFT_CS   17
#define TFT_DC   21
#define TFT_RST  20

#define SD_CS    22

//// COLOR PALLETTE FOR THE MAIN MENU 

#define COLOR_BG        0x1082   // dark navy
#define COLOR_HEADER    0xF800   // doom red
#define COLOR_ACCENT    0xFD20   // orange
#define COLOR_WHITE     0xFFFF
#define COLOR_SELECTED  0x0000



// objects

Adafruit_ST7789 tft = Adafruit_ST7789(
  TFT_CS,
  TFT_DC,
  TFT_RST
);

TwoWire I2C_SENSORS(i2c1, SDA, SCL);

Adafruit_MPU6050 mpu;




////////////globals
int gameSelected = 0;
int gameTop = 0;

int previousGameSelected = -1;
int previousGameTop = -1;


bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastSelect = HIGH;
bool lastBack = HIGH;

// Screensaver

unsigned long lastInputTime = 0;

const uint32_t SCREEN_SAVER_DELAY = 7000;




int selectedItem = 0;
int previousSelected = 0;

int menuTop = 0;
int previousTop = -1;


bool inApp = false;


// ================= SCREEN SAVER LINK =================

extern bool screenSaverActive;

extern void startSaver();

extern void updateSaver();



//// function handler in runtime and helpers
void waitRelease(uint8_t pin)
{
    while(digitalRead(pin) == LOW)
    {
        delay(1);
    }
}


void runLoop(void (*func)())
{

    while(true)
    {

        if(digitalRead(BTN_BACK) == LOW)
        {

            break;
        }


        func();

        delay(25);
    }

}


bool anyButtonPressed()
{
    return digitalRead(BTN_UP) == LOW ||
           digitalRead(BTN_DOWN) == LOW ||
           digitalRead(BTN_L) == LOW ||
           digitalRead(BTN_R) == LOW ||
           digitalRead(BTN_SELECT) == LOW ||
           digitalRead(BTN_BACK) == LOW;
}

// menu stuff

const char* menuItems[] =
{
  "Oscilloscope",
  "Logic Analyzer",
  "LCR Meter",
  "Signal Gen.",
  "Games",
  "Apps",
  "Settings",
  "System Info",
  "Gyroscope",
  "SD Card",
  "GPIO"
};
const uint8_t menuCount = sizeof(menuItems) / sizeof(menuItems[0]);






// =====================================================
// DRAW FUNCTIONS
// =====================================================


void drawHeader(const char* title)
{
  tft.fillRect(0,0,240,35,COLOR_HEADER);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(2);
  tft.setCursor(75,9);
  tft.print(title);
}




void drawSplash()
{

  tft.fillScreen(COLOR_BG);
  tft.setTextColor(COLOR_ACCENT);
  tft.setTextSize(4);
  tft.setCursor(75,45);
  tft.print("SMO");
  tft.setTextSize(2);
  tft.setTextColor(COLOR_WHITE);
  tft.setCursor(30,105);
  tft.print("by Omarkameldev");
  tft.setCursor(60,140);
  tft.print("Dev Version");
  tft.setCursor(75,175);
  tft.print("Beta v1.0");
  delay(2000);

}



// =====================================================
// MENU DRAW
// =====================================================


void drawMenuItem(
  int index,
  bool selected
)
{

  int y =
  50 + ((index - menuTop) * 55);



  if(selected)
  {

    tft.fillRoundRect(15,y,190,42,8,COLOR_ACCENT);
    tft.setTextColor(COLOR_SELECTED);

  }

  else
  {

    tft.drawRoundRect(15,y,190,42,8,COLOR_ACCENT);
    tft.setTextColor(COLOR_WHITE);

  }

  tft.setTextSize(2);
  tft.setCursor(28,y + 12);
  tft.print(menuItems[index]);

}




void drawScrollIndicator()
{

  tft.fillRect(
    220,
    45,
    10,170,COLOR_BG);



  int height =
  170 / menuCount;



  int pos =
  45 + (selectedItem * height);



  tft.fillRoundRect(
    230,
    pos,
    8,
    height,
    4,
    COLOR_ACCENT
  );

}





void drawMenuFull()
{

  tft.fillScreen(
    COLOR_BG
  );


  drawHeader(
    "SMO MENU"
  );



  for(int i=0;i<3;i++)
  {

    int item =
    menuTop + i;


    if(item < menuCount)
    {

      drawMenuItem(
        item,
        item == selectedItem
      );

    }

  }


  drawScrollIndicator();

}



// =====================================================
// FIXED PARTIAL REDRAW
// =====================================================


void redrawChangedItem()
{


  if(menuTop != previousTop)
  {

    drawMenuFull();


    previousTop =
    menuTop;


    previousSelected =
    selectedItem;


    return;

  }




  if(
    previousSelected >= menuTop &&
    previousSelected < menuTop + 3
  )
  {


    int y =
    50 + ((previousSelected-menuTop)*55);



    tft.fillRoundRect(
      15,
      y,
      190,
      42,
      8,
      COLOR_BG
    );



    tft.drawRoundRect(
      15,
      y,
      190,
      42,
      8,
      COLOR_ACCENT
    );



    tft.setTextColor(
      COLOR_WHITE
    );


    tft.setTextSize(2);


    tft.setCursor(
      28,
      y+12
    );


    tft.print(
      menuItems[previousSelected]
    );

  }





  if(
    selectedItem >= menuTop &&
    selectedItem < menuTop+3
  )
  {


    int y =
    50 + ((selectedItem-menuTop)*55);



    tft.fillRoundRect(
      15,
      y,
      190,
      42,
      8,
      COLOR_ACCENT
    );



    tft.setTextColor(
      COLOR_SELECTED
    );


    tft.setTextSize(2);


    tft.setCursor(
      28,
      y+12
    );


    tft.print(
      menuItems[selectedItem]
    );

  }




  drawScrollIndicator();



  previousSelected =
  selectedItem;

}




// =====================================================
// APP PLACEHOLDER
// =====================================================


void drawPlaceholder(
const char* name
)
{
  tft.fillScreen(COLOR_BG);
  drawHeader(name);
  tft.drawRoundRect(20,65,200,90,10,COLOR_ACCENT);
  tft.setTextColor(COLOR_WHITE);
  tft.setTextSize(2);
  tft.setCursor(45,95);
  tft.print("Feature");
  tft.setCursor(45,125);
  tft.print("Coming Soon");
  tft.setTextSize(1);
  tft.setCursor(70,190);
  tft.print("BACK TO EXIT");

}




void openApp()
{


  switch(selectedItem)
  {

    case 0:
      waitRelease(BTN_SELECT);
      scopeEnter();
      runLoop(runOscilloscope);
      break;


    case 1:
      drawPlaceholder("Logic Analyzer");
      break;


    case 2:
      drawPlaceholder("LCR Meter");
      break;


    case 3:
      drawPlaceholder("Signal Generator");
      break;


    case 4:
      waitRelease(BTN_SELECT);
      runLoop(handleGamesMenu);

      break;


    case 5:
      waitRelease(BTN_SELECT);
      drawAppsMenu();
      runLoop(handleAppsMenu);
    
      break;


    case 6:
      drawPlaceholder("Settings");
      break;


    case 7:
      waitRelease(BTN_SELECT);
      runLoop(runSystemInfo);
      break;


    case 8:
      waitRelease(BTN_SELECT);

      runLoop(sensorMenu);
      break;


    case 9:
      runLoop(showSDFiles);
      break;

    case 10:
      waitRelease(BTN_SELECT);
      runLoop(handleGPIOMenu);
      break;  

  }



  inApp = true;

}



// =====================================================
// BUTTONS
// =====================================================


void updateButtons()
{


  bool up = digitalRead(BTN_UP);

  bool down = digitalRead(BTN_DOWN);


  bool select = digitalRead(BTN_SELECT);

  bool back = digitalRead(BTN_BACK);




  if(!inApp)
  {


    if(lastUp == HIGH && up == LOW)
    {

      selectedItem--;



      if(selectedItem < 0)
      selectedItem = menuCount-1;



      if(selectedItem < menuTop)
      menuTop = selectedItem;



      if(selectedItem >= menuTop+3)
      menuTop = selectedItem-2;



      redrawChangedItem();

    }





    if(lastDown == HIGH && down == LOW)
    {


      selectedItem++;



      if(selectedItem >= menuCount)
      selectedItem = 0;



      if(selectedItem < menuTop)
      menuTop = selectedItem;



      if(selectedItem >= menuTop+3)
      menuTop = selectedItem-2;



      redrawChangedItem();

    }





    if(lastSelect == HIGH && select == LOW)
    {

      openApp();

    }


  }



  else
  {


    if(lastBack == HIGH && back == LOW)
    {

      inApp=false;


      drawMenuFull();

    }


  }




  lastUp = up;
  lastDown = down;
  lastSelect = select;
  lastBack = back;


}



// SETUP

void setup()
{
  digitalWrite(TFT_BL, HIGH); 
  pinMode(BTN_R,INPUT_PULLUP);
  pinMode(BTN_L,INPUT_PULLUP);
  pinMode(BTN_UP,INPUT_PULLUP);
  pinMode(BTN_DOWN,INPUT_PULLUP);
  pinMode(BTN_SELECT,INPUT_PULLUP);
  pinMode(BTN_BACK,INPUT_PULLUP);
  pinMode(PWM_OUT_PIN, OUTPUT);
  analogWriteFreq(1000);     
  analogWriteRange(255);     
  analogWrite(PWM_OUT_PIN, 128);
  adc_init();
  adc_set_temp_sensor_enabled(true);
  SPI.begin();
  I2C_SENSORS.begin();
  mpu.begin(0x68, &I2C_SENSORS);
  randomSeed(analogRead(26));
  if(!SD.begin(SD_CS))
  {
    Serial.println("SD Failed");
  }
  else
  {
    Serial.println("SD OK");
  }
  sensorInit();
  tft.init(240,240);
  tft.setRotation(2);

  drawSplash();
  drawScreen_1();
  delay(4000);
  drawMenuFull();

}




// LOOP

void loop()
{
  if(anyButtonPressed())
  {
  lastInputTime = millis();
  }

  if(!screenSaverActive)
  {

    updateButtons();


    if(millis() - lastInputTime > SCREEN_SAVER_DELAY)
    {
      startSaver();
    }

  }


  updateSaver();


  delay(25);

}



// sample menu to use 

// // =====================================================
// // ADD YOUR FEATURE HERE
// // =====================================================


// // 1) ADD APP FUNCTION HERE
// // example:
// // void runWifiAnalyzer(){}

// void runYourFeature()
// {

//     // your feature code here


// }







// // =====================================================
// // MENU DATA
// // ADD / REMOVE ITEMS HERE
// // =====================================================


// const char* featureItems[] =
// {

//     "Feature 1",
//     "Feature 2",
//     "Feature 3",
//     "Back"          // keep this

// };


// const int featureCount =
// sizeof(featureItems) / sizeof(featureItems[0]);




// // =====================================================
// // MENU STATE
// // DO NOT TOUCH
// // =====================================================


// int featureSelected = 0;
// int featureTop = 0;

// int previousFeatureSelected = -1;
// int previousFeatureTop = -1;







// // =====================================================
// // FEATURE MENU LOGIC
// // DO NOT ADD DRAW CODE HERE
// // =====================================================


// void handleFeatureMenu()
// {


//     bool changed = false;



//     // ================= BUTTONS =================


//     if(digitalRead(BTN_UP) == LOW)
//     {

//         waitRelease(BTN_UP);


//         featureSelected--;


//         if(featureSelected < 0)
//             featureSelected = featureCount-1;



//         if(featureSelected < featureTop)
//             featureTop = featureSelected;



//         if(featureSelected >= featureTop+3)
//             featureTop = featureSelected-2;



//         changed = true;

//     }





//     if(digitalRead(BTN_DOWN) == LOW)
//     {

//         waitRelease(BTN_DOWN);


//         featureSelected++;


//         if(featureSelected >= featureCount)
//             featureSelected = 0;



//         if(featureSelected < featureTop)
//             featureTop = featureSelected;



//         if(featureSelected >= featureTop+3)
//             featureTop = featureSelected-2;



//         changed = true;

//     }





//     // ================= SELECT =================


//     if(digitalRead(BTN_SELECT) == LOW)
//     {

//         waitRelease(BTN_SELECT);



//         switch(featureSelected)
//         {


//             // ==================================
//             // ADD YOUR APP OPENING HERE
//             // ==================================


//             case 0:

//                 runYourFeature();

//                 break;



//             case 1:

//                 // run another feature

//                 break;



//             case 2:

//                 // run another feature

//                 break;




//             // BACK

//             case featureCount-1:

//                 return;


//         }


//     }





//     // ================= DRAW UPDATE =================


//     if(
//         changed ||
//         featureSelected != previousFeatureSelected ||
//         featureTop != previousFeatureTop
//     )
//     {

//         drawFeatureMenu();


//         previousFeatureSelected =
//         featureSelected;


//         previousFeatureTop =
//         featureTop;


//     }


// }









// // =====================================================
// // DRAW MENU
// // DO NOT PUT LOGIC HERE
// // =====================================================


// void drawFeatureMenu()
// {


//     // full redraw only when scrolling


//     if(featureTop != previousFeatureTop)
//     {


//         tft.fillScreen(
//             COLOR_BG
//         );


//         drawHeader(
//             "FEATURES"
//         );



//         for(int i=0;i<3;i++)
//         {


//             int index =
//             featureTop+i;



//             if(index >= featureCount)
//                 break;



//             drawFeatureItem(
//                 index,
//                 index == featureSelected,
//                 i
//             );


//         }


//     }


//     else
//     {


//         // redraw old item


//         if(previousFeatureSelected >= featureTop &&
//            previousFeatureSelected < featureTop+3)
//         {

//             drawFeatureItem(
//                 previousFeatureSelected,
//                 false,
//                 previousFeatureSelected-featureTop
//             );

//         }



//         // redraw new item


//         drawFeatureItem(
//             featureSelected,
//             true,
//             featureSelected-featureTop
//         );

//     }





//     // ================= SCROLL BAR =================


//     tft.fillRect(
//         220,
//         45,
//         10,
//         170,
//         COLOR_BG
//     );


//     int barHeight =
//     170 / featureCount;



//     int barPos =
//     45 + featureSelected*barHeight;



//     tft.fillRoundRect(
//         230,
//         barPos,
//         8,
//         barHeight,
//         4,
//         COLOR_ACCENT
//     );

// }








// // =====================================================
// // DRAW ONE ITEM
// // DO NOT TOUCH
// // =====================================================


// void drawFeatureItem(
// int index,
// bool selected,
// int position
// )
// {


//     int y =
//     50 + position*55;




//     if(selected)
//     {


//         tft.fillRoundRect(
//             15,
//             y,
//             190,
//             42,
//             8,
//             COLOR_ACCENT
//         );


//         tft.setTextColor(
//             COLOR_SELECTED
//         );


//     }

//     else
//     {


//         tft.fillRoundRect(
//             15,
//             y,
//             190,
//             42,
//             8,
//             COLOR_BG
//         );


//         tft.drawRoundRect(
//             15,
//             y,
//             190,
//             42,
//             8,
//             COLOR_ACCENT
//         );


//         tft.setTextColor(
//             COLOR_WHITE
//         );


//     }




//     tft.setTextSize(2);


//     tft.setCursor(
//         28,
//         y+12
//     );


//     tft.print(
//         featureItems[index]
//     );


// }
