extern char __StackLimit;
extern char __bss_end__;

int getFreeRAM()
{
    char top;
    return &top - &__bss_end__;
}



 
void runSystemInfo()
{
    pico_unique_board_id_t id;

    while(true)
    {
        pico_get_unique_board_id(&id);

        tft.fillScreen(COLOR_BG);
        drawHeader("SYSTEM INFO");

        tft.setTextColor(COLOR_WHITE);
        tft.setTextSize(1);

        int y = 42;

        tft.setCursor(5,y);
        tft.print("Board : Pico 2");
        y += 12;

        tft.setCursor(5,y);
        tft.print("MCU   : RP2350");
        y += 12;

        tft.setCursor(5,y);
        tft.print("Cores : 2");
        y += 12;

        tft.setCursor(5,y);
        tft.print("CPU   : ");
        tft.print(clock_get_hz(clk_sys)/1000000);
        tft.print(" MHz");
        y += 12;

        tft.setCursor(5,y);
        tft.print("Flash : ");
        tft.print(PICO_FLASH_SIZE_BYTES / 1024 / 1024);
        tft.print(" MB");
        y += 12;

        int freeRam = getFreeRAM();

        tft.setCursor(5,y);
        tft.print("Free RAM : ");
        tft.print(freeRam);
        tft.print(" B");
        y += 12;

        tft.setCursor(5,y);
        tft.print("Used RAM : ");
        tft.print((520*1024)-freeRam);
        tft.print(" B");
        y += 12;

        tft.setCursor(5,y);
        tft.print("Total RAM: 520 KB");
        y += 12;

        unsigned long sec = millis()/1000;

        int h = sec/3600;
        int m = (sec%3600)/60;
        int s = sec%60;

        tft.setCursor(5,y);
        tft.print("Uptime: ");

        if(h<10) tft.print('0');
        tft.print(h);
        tft.print(':');

        if(m<10) tft.print('0');
        tft.print(m);
        tft.print(':');

        if(s<10) tft.print('0');
        tft.print(s);
        y += 12;

        tft.setCursor(5,y);
        tft.print("Build:");
        tft.print(__DATE__);
        y += 12;

        tft.setCursor(5,y);
        tft.print(__TIME__);
        y += 12;

        tft.setCursor(5,y);
        tft.print("UID:");

        for(int i=0;i<8;i++)
        {
            if(id.id[i] < 16)
                tft.print('0');

            tft.print(id.id[i], HEX);
        }

        y += 15;

        sensors_event_t a,g,temp;

        tft.setCursor(5,y);

        if(mpu.getEvent(&a,&g,&temp))
            tft.print("MPU6050 : OK");
        else
            tft.print("MPU6050 : FAIL");

        if(digitalRead(BTN_BACK)==LOW)
        {
          break;
        }

        delay(3000);
    }
}