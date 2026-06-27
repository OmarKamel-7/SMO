
/////////family slide show

// ================= SCREEN SAVER =================

bool screenSaverActive = false;

unsigned long lastSlideTime = 0;

int currentSlide = -1;


void (*slides[])() =
{
  omarkamel,
  kamelomar,
  kamel,
  alikamel,
  lolo,
  drawScreen_1,
  flipper1,
  drawScreen_3,
  drawScreen_4,
  drawScreen_14,
  drawScreen_17,
  drawScreen992,
  drawScreen_33,
  drawScreen_13,
  drawScreen_15,
  drawScreen163,
  drawScreen_20,
  drawScreen93,
  drawScreen_111
};


int slideCount = sizeof(slides) / sizeof(slides[0]);

void startSaver()
{
  screenSaverActive = true;

  tft.fillScreen(ST77XX_BLACK);

  lastSlideTime = 0;
}


void updateSaver()
{

  if(!screenSaverActive)
    return;



  // exit screensaver on ANY button
  if(anyButtonPressed())
  {

    while(anyButtonPressed())
      delay(1);


    screenSaverActive = false;

    lastInputTime = millis();

    drawMenuFull();

    return;
  }



  // change animation every 1-2 sec
  if(millis() - lastSlideTime > random(1000,2000))
  {

    int next;


    do
    {
      next = random(0, slideCount);

    }
    while(next == currentSlide);



    currentSlide = next;


    slides[currentSlide]();


    lastSlideTime = millis();

  }

}
// =====================================================
// APP FUNCTIONS
// =====================================================

void runCalculator(){}
void runPaint(){}
void runNotes(){}
void runClock(){}
void runFileManager(){}
void runWiFi(){}
void runBluetooth(){}

////////tempreature function using the sensor inside the pico2 
void runTemperatureApp(){

    tft.fillScreen(COLOR_BG);
    drawHeader("TEMP SENSOR");

    int lastTemp10=-10000;

    while(true){

        if(digitalRead(BTN_BACK)==LOW){
            waitRelease(BTN_BACK);
            return;
        }

        adc_select_input(4);

        uint16_t raw=adc_read();

        float voltage=raw*3.3f/4095.0f;
        float c=27.0f-(voltage-0.706f)/0.001721f;
        float f=c*9.0f/5.0f+32.0f;
        float k=c+273.15f;

        int temp10=(int)(c*10);

        if(temp10!=lastTemp10){

            tft.fillRect(0,38,240,170,COLOR_BG);

            tft.setTextSize(2);

            tft.setTextColor(COLOR_ACCENT);
            tft.setCursor(10,45);
            tft.print("Temperature");

            tft.setTextColor(COLOR_WHITE);

            tft.setCursor(10,70);
            tft.print("C : ");
            tft.print(c,1);

            tft.setCursor(10,92);
            tft.print("F : ");
            tft.print(f,1);

            tft.setCursor(10,114);
            tft.print("K : ");
            tft.print(k,1);

            tft.setCursor(10,136);
            tft.print("Voltage : ");
            tft.print(voltage,3);
            tft.print("V");

            tft.setCursor(10,158);
            tft.print("ADC Raw : ");
            tft.print(raw);

            tft.setCursor(10,180);
            tft.print("Channel : ADC4");

            tft.setCursor(10,202);

            if(c<20)
                tft.print("Status : Cold");
            else if(c<27)
                tft.print("Status : Normal");
            else if(c<29)
                tft.print("Status : Warm");
            else
                tft.print("Status : HOT!");

            lastTemp10=temp10;
        }

        delay(100);
    }
}

// =====================================================
// APPS
// =====================================================

const char* appItems[]={
    "Calculator",
    "Paint",
    "Notes",
    "Clock",
    "File Manager",
    "WiFi",
    "Bluetooth",
    "tempreature",
    "saver",
    "slideshow",
    "Back"
};

const int appCount=sizeof(appItems)/sizeof(appItems[0]);

int appSelected=0;
int appTop=0;

int previousAppSelected=-1;
int previousAppTop=-1;

// =====================================================
// APPS MENU
// =====================================================

void handleAppsMenu(){

    bool changed=false;

    if(digitalRead(BTN_UP)==LOW){

        waitRelease(BTN_UP);

        appSelected--;

        if(appSelected<0)
            appSelected=appCount-1;

        if(appSelected<appTop)
            appTop=appSelected;

        if(appSelected>=appTop+3)
            appTop=appSelected-2;

        changed=true;
    }

    if(digitalRead(BTN_DOWN)==LOW){

        waitRelease(BTN_DOWN);

        appSelected++;

        if(appSelected>=appCount)
            appSelected=0;

        if(appSelected<appTop)
            appTop=appSelected;

        if(appSelected>=appTop+3)
            appTop=appSelected-2;

        changed=true;
    }

    if(digitalRead(BTN_SELECT)==LOW){

        waitRelease(BTN_SELECT);

        switch(appSelected){

            case 0:
                runCalculator();
                break;

            case 1:
                runPaint();
                break;

            case 2:
                runNotes();
                break;

            case 3:
                runClock();
                break;

            case 4:
                runFileManager();
                break;

            case 5:
                runWiFi();
                break;

            case 6:
                runBluetooth();
                break;

            case 7:
                waitRelease(BTN_SELECT);
                runLoop(runTemperatureApp);
                break;

            case 8:
              runLoop(runScreenSaver);
              break;

            case 9:
                waitRelease(BTN_SELECT);
                runLoop(startSaver);
            case 10:
                return;    
        }

        changed=true;
    }

    if(changed ||
       appSelected!=previousAppSelected ||
       appTop!=previousAppTop){

        drawAppsMenu();

        previousAppSelected=appSelected;
        previousAppTop=appTop;
    }

}

// =====================================================
// DRAW APPS MENU
// =====================================================

void drawAppsMenu(){

    if(appTop!=previousAppTop){

        tft.fillScreen(COLOR_BG);

        drawHeader("APPS");

        for(int i=0;i<3;i++){

            int index=appTop+i;

            if(index>=appCount)
                break;

            drawAppItem(index,index==appSelected,i);
        }

    }else{

        if(previousAppSelected>=appTop &&
           previousAppSelected<appTop+3){

            drawAppItem(previousAppSelected,false,
                        previousAppSelected-appTop);
        }

        if(appSelected>=appTop &&
           appSelected<appTop+3){

            drawAppItem(appSelected,true,
                        appSelected-appTop);
        }

    }

    tft.fillRect(220,45,10,170,COLOR_BG);

    int barHeight=170/appCount;
    int barPos=45+appSelected*barHeight;

    tft.fillRoundRect(230,barPos,8,barHeight,4,COLOR_ACCENT);
}

// =====================================================
// DRAW ONE APP
// =====================================================

void drawAppItem(int index,bool selected,int position){

    int y=50+position*55;

    if(selected){

        tft.fillRoundRect(15,y,190,42,8,COLOR_ACCENT);
        tft.setTextColor(COLOR_SELECTED);

    }else{

        tft.fillRoundRect(15,y,190,42,8,COLOR_BG);
        tft.drawRoundRect(15,y,190,42,8,COLOR_ACCENT);
        tft.setTextColor(COLOR_WHITE);

    }

    tft.setTextSize(2);
    tft.setCursor(28,y+12);
    tft.print(appItems[index]);
}