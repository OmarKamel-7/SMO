void showSDFiles()
{
    tft.fillScreen(COLOR_BG);
    drawHeader("SD CARD");

    File root = SD.open("/");

    if(!root)
    {
        tft.setCursor(10,60);
        tft.setTextColor(COLOR_WHITE);
        tft.setTextSize(2);
        tft.print("SD Error");
        return;
    }

    int y = 55;

    File file = root.openNextFile();

    while(file)
    {
        tft.setCursor(10,y);
        tft.setTextColor(COLOR_WHITE);
        tft.setTextSize(1);

        if(file.isDirectory())
        {
            tft.print("[DIR] ");
        }

        tft.print(file.name());

        y += 15;

        if(y > 220)
        {
            break;
        }

        file = root.openNextFile();
    }

    root.close();
}