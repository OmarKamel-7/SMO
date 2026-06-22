#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <Wire.h>
#include <SD.h>



// ST7789 INIT

#define TFT_CS   17
#define TFT_DC   21
#define TFT_RST  20

Adafruit_ST7789 tft = Adafruit_ST7789(
  TFT_CS,
  TFT_DC,
  TFT_RST
);


// BUTTONS

#define BTN_UP      2
#define BTN_DOWN    3
#define BTN_SELECT  4
#define BTN_BACK    5



//// COLOR PALLETTE FOR THE MAIN MENU 

#define COLOR_BG        0x2108   // Dark gray
#define COLOR_HEADER    0xF81F   // Pink/Magenta header
#define COLOR_ACCENT    0xFFE0   // Yellow selection
#define COLOR_WHITE     0xFFFF
#define COLOR_SELECTED  0x0000
// ================= MENU =================

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
  "SD Card"
};


const uint8_t menuCount =
sizeof(menuItems) / sizeof(menuItems[0]);


int selectedItem = 0;
int previousSelected = 0;

int menuTop = 0;
int previousTop = -1;


bool inApp = false;



// ================= BUTTON MEMORY =================

bool lastUp = HIGH;
bool lastDown = HIGH;
bool lastSelect = HIGH;
bool lastBack = HIGH;



// =====================================================
// DRAW FUNCTIONS
// =====================================================


void drawHeader(const char* title)
{

  tft.fillRect(
    0,
    0,
    240,
    35,
    COLOR_HEADER
  );


  tft.setTextColor(
    COLOR_WHITE
  );


  tft.setTextSize(2);


  tft.setCursor(
    95,
    9
  );


  tft.print(title);

}




void drawSplash()
{

  tft.fillScreen(
    COLOR_BG
  );


  tft.setTextColor(
    COLOR_ACCENT
  );


  tft.setTextSize(4);


  tft.setCursor(
    75,
    45
  );


  tft.print("SMO");



  tft.setTextSize(2);


  tft.setTextColor(
    COLOR_WHITE
  );


  tft.setCursor(
    45,
    105
  );


  tft.print("by Omar Kamel");



  tft.setCursor(
    60,
    140
  );


  tft.print("Dev Version");



  tft.setCursor(
    75,
    175
  );


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

  }

  else
  {

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

  }



  tft.setTextSize(2);


  tft.setCursor(
    28,
    y + 12
  );


  tft.print(
    menuItems[index]
  );

}




void drawScrollIndicator()
{

  tft.fillRect(
    220,
    45,
    10,
    170,
    COLOR_BG
  );



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

  tft.fillScreen(
    COLOR_BG
  );


  drawHeader(
    name
  );



  tft.drawRoundRect(
    20,
    65,
    200,
    90,
    10,
    COLOR_ACCENT
  );



  tft.setTextColor(
    COLOR_WHITE
  );


  tft.setTextSize(2);



  tft.setCursor(
    45,
    95
  );


  tft.print(
    "Feature"
  );



  tft.setCursor(
    45,
    125
  );


  tft.print(
    "Coming Soon"
  );



  tft.setTextSize(1);



  tft.setCursor(
    70,
    190
  );


  tft.print(
    "BACK TO EXIT"
  );

}




void openApp()
{


  switch(selectedItem)
  {

    case 0:
      drawPlaceholder("Oscilloscope");
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
      drawPlaceholder("Games");
      break;


    case 5:
      drawPlaceholder("Apps");
      break;


    case 6:
      drawPlaceholder("Settings");
      break;


    case 7:
      drawPlaceholder("System Info");
      break;


    case 8:
      drawPlaceholder("Gyroscope");
      break;


    case 9:
      drawPlaceholder("SD Card");
      break;

  }



  inApp = true;

}



// =====================================================
// BUTTONS
// =====================================================


void updateButtons()
{


  bool up =
  digitalRead(BTN_UP);


  bool down =
  digitalRead(BTN_DOWN);


  bool select =
  digitalRead(BTN_SELECT);


  bool back =
  digitalRead(BTN_BACK);




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



// =====================================================
// SETUP
// =====================================================


void setup()
{

  pinMode(
    BTN_UP,
    INPUT_PULLUP
  );


  pinMode(
    BTN_DOWN,
    INPUT_PULLUP
  );


  pinMode(
    BTN_SELECT,
    INPUT_PULLUP
  );


  pinMode(
    BTN_BACK,
    INPUT_PULLUP
  );



  SPI.begin();



  tft.init(
    240,
    240
  );


  // 180 degree rotation
  tft.setRotation(2);



  drawSplash();



  drawMenuFull();


}




// =====================================================
// LOOP
// =====================================================


void loop()
{

  updateButtons();


  delay(25);

}