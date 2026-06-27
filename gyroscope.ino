

// ================= MENU =================

const char* sensorItems[] =
{
    "Gyroscope",
    "Accelerometer",
    "Temperature",
    "Level"
};


int sensorSelected = 0;
int sensorCount = 4;



// ================= INIT =================

void sensorInit()
{
    if(!mpu.begin(0x68, &I2C_SENSORS))
    {
        Serial.println("MPU FAIL");
    }
    else
    {
        Serial.println("MPU OK");
    }


    mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
    mpu.setGyroRange(MPU6050_RANGE_500_DEG);
}



// ================= SENSOR MENU =================

void sensorMenu()
{

    while(true)
    {

        tft.fillScreen(COLOR_BG);

        drawHeader("SENSORS");


        for(int i = 0; i < sensorCount; i++)
        {

            int y = 55 + (i * 40);


            if(i == sensorSelected)
            {
                tft.fillRoundRect(15,y,210,32,5,COLOR_ACCENT);
                tft.setTextColor(COLOR_SELECTED);
            }
            else
            {
                tft.setTextColor(COLOR_WHITE);
            }


            tft.setTextSize(2);
            tft.setCursor(30,y+8);
            tft.print(sensorItems[i]);

        }



        if(digitalRead(BTN_UP) == LOW)
        {
            waitRelease(BTN_UP);

            sensorSelected--;

            if(sensorSelected < 0)
                sensorSelected = sensorCount - 1;
        }



        if(digitalRead(BTN_DOWN) == LOW)
        {
            waitRelease(BTN_DOWN);

            sensorSelected++;

            if(sensorSelected >= sensorCount)
                sensorSelected = 0;
        }



        if(digitalRead(BTN_SELECT) == LOW)
        {
            waitRelease(BTN_SELECT);


            if(sensorSelected == 0)
                gyroScreen();


            if(sensorSelected == 1)
                accelScreen();


            if(sensorSelected == 2)
                tempScreen();


            if(sensorSelected == 3)
                levelScreen();

        }



        if(digitalRead(BTN_BACK) == LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }


        delay(50);

    }

}



// ================= GYRO =================

void gyroScreen()
{

    tft.fillScreen(COLOR_BG);
    drawHeader("GYRO");


    while(true)
    {

        if(digitalRead(BTN_BACK)==LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }


        sensors_event_t a,g,temp;

        mpu.getEvent(&a,&g,&temp);



        tft.fillRect(10,60,220,130,COLOR_BG);


        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE);


        tft.setCursor(20,70);
        tft.print("X: ");
        tft.print(g.gyro.x,2);


        tft.setCursor(20,110);
        tft.print("Y: ");
        tft.print(g.gyro.y,2);


        tft.setCursor(20,150);
        tft.print("Z: ");
        tft.print(g.gyro.z,2);



        delay(80);

    }

}




// ================= ACCEL =================


void accelScreen()
{

    tft.fillScreen(COLOR_BG);
    drawHeader("ACCEL");


    while(true)
    {

        if(digitalRead(BTN_BACK)==LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }



        sensors_event_t a,g,temp;

        mpu.getEvent(&a,&g,&temp);



        tft.fillRect(10,60,220,130,COLOR_BG);


        tft.setTextSize(2);
        tft.setTextColor(COLOR_WHITE);



        tft.setCursor(20,70);
        tft.print("X: ");
        tft.print(a.acceleration.x,2);



        tft.setCursor(20,110);
        tft.print("Y: ");
        tft.print(a.acceleration.y,2);



        tft.setCursor(20,150);
        tft.print("Z: ");
        tft.print(a.acceleration.z,2);



        delay(80);

    }

}




// ================= TEMP =================


void tempScreen()
{

    tft.fillScreen(COLOR_BG);
    drawHeader("TEMP");


    while(true)
    {

        if(digitalRead(BTN_BACK)==LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }



        sensors_event_t a,g,temp;

        mpu.getEvent(&a,&g,&temp);



        tft.fillRect(10,70,220,80,COLOR_BG);


        tft.setTextSize(3);
        tft.setTextColor(COLOR_WHITE);


        tft.setCursor(40,100);

        tft.print(temp.temperature,1);

        tft.print(" C");



        delay(200);

    }

}




// ================= LEVEL =================


void levelScreen()
{

    tft.fillScreen(COLOR_BG);
    drawHeader("LEVEL");


    while(true)
    {

        if(digitalRead(BTN_BACK)==LOW)
        {
            waitRelease(BTN_BACK);
            return;
        }



        sensors_event_t a,g,temp;

        mpu.getEvent(&a,&g,&temp);



        int x = 120 + (a.acceleration.x * 10);

        int y = 120 + (a.acceleration.y * 10);



        tft.fillCircle(120,120,45,COLOR_BG);

        tft.drawCircle(120,120,45,COLOR_ACCENT);

        tft.fillCircle(x,y,8,COLOR_WHITE);



        delay(60);

    }

}