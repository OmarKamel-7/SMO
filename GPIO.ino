

// I2C SCANNER USING THE CUSTOM I2C OB pins 6 sda , scl 7
bool gpioScreenDrawn = false;


void runI2CScanner()
{

    tft.fillScreen(COLOR_BG);

    drawHeader("I2C SCANNER");


    tft.setTextSize(1);
    tft.setTextColor(COLOR_WHITE);


    tft.setCursor(10,50);
    tft.print("Scanning...");


    int y = 70;

    int found = 0;


    for(byte address = 0x08; address <= 0x77; address++)
    {

        I2C_SENSORS.beginTransmission(address);

        byte error = I2C_SENSORS.endTransmission();


        if(error == 0)
        {

            tft.setCursor(20,y);

            tft.print("DEVICE: 0x");

            if(address < 16)
                tft.print("0");


            tft.println(address,HEX);


            y += 15;

            found++;

        }

    }



    if(found == 0)
    {

        tft.setCursor(20,90);

        tft.print("NO DEVICE");

    }


    tft.setCursor(20,200);

    tft.print("BACK EXIT");


}

void RunI2cScanner()
{

    static bool first = true;


    if(first)
    {

        runI2CScanner();

        first = false;

    }


}



// MAIN GPIO MENU HANDLING

const char* gpioItems[] = {"I2C Scanner","GPIO Read","GPIO Write"};

const int gpioCount = sizeof(gpioItems) / sizeof(gpioItems[0]);

int gpioSelected = 0;
int gpioTop = 0;

void handleGPIOMenu()
{
    static bool firstDraw = true;

    if(firstDraw)
    {
        tft.fillScreen(COLOR_BG);
        drawHeader("GPIO");

        for(int i = 0; i < gpioCount; i++)
        {
            int y = 50 + i * 55;

            if(i == gpioSelected)
            {
                tft.fillRoundRect(15, y, 190, 42, 8, COLOR_ACCENT);
                tft.setTextColor(COLOR_SELECTED);
            }
            else
            {
                tft.drawRoundRect(15, y, 190, 42, 8, COLOR_ACCENT);
                tft.setTextColor(COLOR_WHITE);
            }

            tft.setTextSize(2);
            tft.setCursor(28, y + 12);
            tft.print(gpioItems[i]);
        }

        firstDraw = false;
    }

    if(digitalRead(BTN_UP) == LOW)
    {
        waitRelease(BTN_UP);
        gpioSelected--;
        if(gpioSelected < 0) gpioSelected = gpioCount - 1;
        firstDraw = true;
    }

    if(digitalRead(BTN_DOWN) == LOW)
    {
        waitRelease(BTN_DOWN);
        gpioSelected++;
        if(gpioSelected >= gpioCount) gpioSelected = 0;
        firstDraw = true;
    }

    if(digitalRead(BTN_SELECT) == LOW)
    {
        waitRelease(BTN_SELECT);

        switch(gpioSelected)
        {
            case 0:
                runLoop(RunI2cScanner);
                firstDraw = true;
                break;
        }
    }
}