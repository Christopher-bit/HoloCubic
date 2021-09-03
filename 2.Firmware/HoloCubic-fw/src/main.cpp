#include <Arduino.h>
#include "display.h"
#include "imu.h"
#include "ambient.h"
#include "network.h"
#include "sd_card.h"
#include "rgb_led.h"
#include "lv_port_indev.h"
#include "lv_port_fatfs.h"
#include "lv_cubic_gui.h"
#include "gui_guider.h"
#include "TFT_eSPI.h"
#include "lv_examples/lv_examples.h"
// #include "images.h"

#include <HTTPClient.h>
#include <NTPClient.h>
#include <ArduinoJson.h>


/*** Component objects ***/
Display screen;
IMU mpu;
Pixel rgb;
SdCard tf;
Network wifi;
WiFiUDP Udp;

lv_ui guider_ui;

int imu_data[6]={0};
uint8_t SerialBuf[5]={0};
short DlyTim=1;
char DlyFlag=0;
CRGB leds[1];
extern WifiLib wifilib;
extern TFT_eSPI tft;
extern const lv_img_dsc_t logo;
int frame_id = 0;
char buf[100];
tm timeinfo;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 4*3600; //不知道为何是4*60*60
const int   daylightOffset_sec = 4*3600;//不知道为何是4*60*60
WiFiUDP ntpUDP;
bool LedTogFlg=0;
hw_timer_t * timer = NULL;//定时器中断
int img_change_sec =0;
int img_change_flag=0;
extern lv_obj_t* img;
uint8_t packetBuffer[1030]; //buffer to hold incoming packet
uint8_t  ReplyBuffer[] = "acknowledged";       // a string to send back
uint8_t  WriteBuffer[] = {'e','s','p',0x0d,0x0a};
uint8_t okbfr[]="svrok";
IPAddress ylj;
unsigned int remote_port=8080;
uint8_t ledsta=4;
uint8_t webledsta;//监听到新包的标志位

char ImgPat[19]={'/','i','m','a','g','e','/','i','m','a','g','e','0','0','0','.','b','i','n'};
const uint8_t binrply[]="ready for file transfer";
const uint8_t txtrply[]="ready for file transfer:txt";
uint8_t filetype;
String imgsum;
String txtsum;
int imgnum;
int txtnum;

uint8_t SvrSucFlg=0;//服务器连接成功标志位
uint8_t ScnOpnFlg=0;//息屏标志位
uint32_t TimRcd=0;//时间记录
uint8_t TimSpdTgtr=0;//纪念日计算标志位
uint8_t JinianFlg=0;//纪念日背景图片切换标志位
uint8_t XiangceFlg=0;//相册背景图片切换标志位
uint8_t NJFlg=0;//南京切换标志位
uint8_t ZZFlg=0;//郑州切换标志位
File root;//根目录
uint8_t rootflg=0;//根目录打开标志位
extern weather wtr;//外部变量 天气类
int svrtransflg=0;//传输完成标志位
uint8_t wifi_ok=0;//wifi连接成功标志位
uint8_t filemngflg=0;//文件管理标志位
uint8_t imgtransflg=0;//图片传输标志位

/*0为纪念日，1为天气，2为相册*/
uint8_t ScrnFlg=0;//屏幕功能标志位
int FcnShftTim=0;//功能切换用时

void IRAM_ATTR onTimer();
void WebLed(String webcmd);
void LedProc(uint8_t ledsta);

void setup()
{
    Serial.begin(115200);

    /*** Init screen ***/
    screen.init();
    screen.setBackLight(0.2);

    /*** Init IMU as input device ***/
    lv_port_indev_init();
    mpu.init();

    /*** Init on-board RGB ***/
    // rgb.init();
    // rgb.setBrightness(0.1).setRGB(0, 0, 122, 204).setRGB(1, 0, 122, 204);
    FastLED.addLeds<WS2812B, RGB_LED_PIN, GRB>(leds, 1);
    leds[0] = CRGB::Black;

    /*  1/(80MHZ/80) = 1us  */
    timer = timerBegin(1, 120, true);
    /* 将onTimer函数附加到我们的计时器 */
    timerAttachInterrupt(timer, &onTimer, true);
    /* *设置闹钟每秒调用onTimer函数1 tick为1us   => 1秒为1000000us * / 
    / *重复闹钟（第三个参数）*/
    timerAlarmWrite(timer, 20000, true);
    /* 启动警报*/
    timerAlarmEnable(timer);

    /*** Init micro SD-Card ***/
    tf.init();
    

    // lv文件系统初始化
    lv_fs_if_init();
    // tf.listDir("/",1);
    /*** Inflate GUI objects ***/
    lv_holo_cubic_gui();
    extern lv_obj_t* scr;
    extern lv_obj_t* img;
    scr = lv_scr_act();
	img = lv_img_create(lv_scr_act(), NULL);
    String initfile=tf.readFileLine("/filename.txt",1);
    Serial.println(initfile.c_str());
	lv_img_set_src(img, initfile.c_str());
	lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
    screen.routine();
    /*** Read WiFi info from network.h, then scan & connect WiFi ***/
    wifi.init();
    if(WiFi.isConnected())
    {
        Udp.begin(12340);
        ylj.fromString("101.34.153.102");

        uint8_t waitsec=0;
        while(waitsec<5)
        {
            waitsec++;
            if(!SvrSucFlg)
            {
                Udp.beginPacket(ylj, remote_port);
                Udp.write(WriteBuffer,3);//sizeof(ReplyBuffer)
                Udp.endPacket();
                delay(50);
                int packetSize = Udp.parsePacket();
                if (packetSize) 
                {
                    IPAddress remoteIp = Udp.remoteIP();// read the packet into packetBufffer
                    Udp.read((unsigned char*)packetBuffer, 1030);
                    String bfr=(char*)packetBuffer;
                    String cmp=bfr.substring(0,5);
                    Serial.print(cmp+"\n");
                    if(!strcmp(cmp.c_str(),"espok"))//espok发过来，说明svr也ok了
                    {
                        Serial.print("client connected!\n");
                        Udp.beginPacket(ylj, remote_port);
                        Udp.write(okbfr,5);//sizeof(ReplyBuffer)
                        Udp.endPacket();
                        delay(50);
                        SvrSucFlg=1;//表明服务器有东西要传给分光棱镜
                        packetSize=0;
                        break;
                    }
                }
            }
        }
    }

    // lv_demo_widgets();
}


char filename[30]={0};
char abspath[30]={0};
void loop()
{
    /**********************
     * update IMU data
     **********************/
    mpu.update(200);//200 means update IMU data every 200ms
    imu_data[0]=mpu.getAccelX();
    imu_data[1]=mpu.getAccelY();
    imu_data[2]=mpu.getAccelZ();
    imu_data[3]=mpu.getGyroX();
    imu_data[4]=mpu.getGyroY();
    imu_data[5]=mpu.getGyroZ();
    
    if(TimRcd>30000)//满5分钟息屏
    {
        screen.setBackLight(0);
        Udp.stop();
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        WiFi.setSleep(true);
        int mode=WiFi.getMode();
        Serial.println(mode);
        ScnOpnFlg=1;//息屏标志位置一
        TimRcd=0;
    }
    if((imu_data[0]>8000||imu_data[0]<-8000)&&ScnOpnFlg==1)//息屏状态下唤醒屏幕
    {
        screen.setBackLight(0.2);
        wifi.init();//重连wifi
        if(WiFi.isConnected())
        {
            Udp.begin(12340);
            uint8_t waitsec=0;
            while(waitsec<5)
            {
                waitsec++;
                if(!SvrSucFlg)
                {
                    Udp.beginPacket(ylj, remote_port);
                    Udp.write(WriteBuffer,3);//sizeof(ReplyBuffer)
                    Udp.endPacket();
                    delay(50);
                    int packetSize = Udp.parsePacket();
                    if (packetSize) 
                    {
                        IPAddress remoteIp = Udp.remoteIP();// read the packet into packetBufffer
                        Udp.read((unsigned char*)packetBuffer, 1030);
                        String bfr=(char*)packetBuffer;
                        String cmp=bfr.substring(0,5);
                        Serial.print(cmp+"\n");
                        if(!strcmp(cmp.c_str(),"espok"))//espok发过来，说明svr也ok了
                        {
                            Serial.print("client connected!\n");
                            Udp.beginPacket(ylj, remote_port);
                            Udp.write(okbfr,5);//sizeof(ReplyBuffer)
                            Udp.endPacket();
                            delay(50);
                            SvrSucFlg=1;//表明服务器有东西要传给分光棱镜
                            packetSize=0;
                            break;
                        }
                    }
                }
            }
        }
        ScnOpnFlg=0;//息屏标志位置零
        TimSpdTgtr=0;//重算纪念日
    }


    if(SvrSucFlg==1&&WiFi.isConnected()==1)//若服务器有东西传过来
    {
        while(1)
        {
            int packetSize = Udp.parsePacket();
            IPAddress remoteIp = Udp.remoteIP();
            Udp.read((unsigned char*)packetBuffer, 1030);// read the packet into packetBufffer
            if (packetSize>10) 
            {
                int PacSum=packetBuffer[0];
                PacSum=(PacSum<<8)+packetBuffer[1];

                int PacSeq=packetBuffer[2];
                PacSeq=(PacSeq<<8)+packetBuffer[3];

                int PacLen=packetBuffer[4];
                PacLen=(PacLen<<8)+packetBuffer[5];
                
                if(PacSeq==0)
                {
                    for(int i=0;i<30;i++)
                    {
                        filename[i]=0;
                        abspath[i]=0;
                    }
                    filename[0]='/';
                    filename[1]='i';
                    filename[2]='m';
                    filename[3]='a';
                    filename[4]='g';
                    filename[5]='e';
                    filename[6]='/';
                    memcpy(filename+7,(char*)packetBuffer+6,PacLen);
                    abspath[0]='S';
                    abspath[1]=':';
                    memcpy(abspath+2,filename,PacLen+1+6);
                }
                else if(PacSeq==1)
                {
                    tf.writeBinToSd(filename,packetBuffer+6,PacLen);
                }
                else if(PacSeq>1)
                {
                    tf.appendBinToSd(filename,packetBuffer+6,PacLen);
                    WebLed("BLN green");
                }
                Serial.print(PacSum);Serial.print('\t');
                Serial.print(PacSeq);Serial.print('\t');
                Serial.print(PacLen);
                Serial.print("\n");
                if(PacSeq==PacSum)//传输完成
                {
                    Serial.print("write complete\n");
                    Serial.print("total image:");Serial.print(imgnum);Serial.print("\n");
                    Serial.print("total text:");Serial.print(txtnum);Serial.print("\n");
                    imgnum++;
                    imgsum[6]=imgnum/100+0x30;
                    imgsum[7]=(imgnum/10)%10+0x30;
                    imgsum[8]=imgnum%10+0x30;
                    svrtransflg=1;
                    // tf.deleteFile("/shiftbcgd.txt");
                    // tf.writeFile("/shiftbcgd.txt","1\n");
                    tf.deleteFile("/filename.txt");
                    tf.writeFile("/filename.txt",abspath);
                    tf.appendFile("/filename.txt","\n");
                    WebLed("open led");
                    break;
                }
            }
        }
        SvrSucFlg=0;
    }


    if(ScnOpnFlg==0&&imu_data[5]>12000)//转动屏幕切换功能标志位
    {
        delay(400);
        if(imu_data[5]>12000)
        {
            ScnOpnFlg=0;//息屏标志位置零
            ScrnFlg++;
            ScrnFlg%=6;
            TimRcd=0;
        }
    }

    if(ScrnFlg==0)//纪念日模式
    {
        WebLed("BLN green");
        XiangceFlg=0;//相册标志位置零
        NJFlg=0;//天气标志位置零
        ZZFlg=0;//天气标志位置零
        rootflg=0;//根目录标志位置零
        filemngflg=0;//文件管理标志位置零
        imgtransflg=0;//图片传输标志位置零
        if(JinianFlg==0)
        {
            String shiftfilename=tf.readFileLine("/filename.txt",1);
            Serial.println(shiftfilename);
            if(svrtransflg==1)//接收到新图片
            {
                lv_img_set_src(img,shiftfilename.c_str());
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
                svrtransflg=0;
                JinianFlg=1;
            }
            else
            {
                lv_img_set_src(img, shiftfilename.c_str());
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
                JinianFlg=1;
            }
        }
        /**********************
         * get ntp time, date
         **********************/
        if(TimSpdTgtr<2&&WiFi.getMode()==WIFI_STA&&WiFi.isConnected()==1)
        {
            NTPClient timeClient(ntpUDP, "ntp1.aliyun.com", 60 * 60 * 0, 30 * 60 * 1000);
            timeClient.begin();
            if(timeClient.update())
            {
                String time_str = timeClient.getFormattedTime();
                int day=timeClient.getDay();
                unsigned long epochTime=timeClient.getEpochTime();
                struct tm *ptm = gmtime ((time_t *)&epochTime);
                int monthDay = ptm->tm_mday;
                int currentMonth = ptm->tm_mon+1;
                int currentYear = ptm->tm_year+1900;
                unsigned long SecondSpentTogether=epochTime-1614960000;//1614960000是2020.03.06 00：00的时间戳
                unsigned long DaySpentTogether=SecondSpentTogether/86400;
                //Print complete date:
                String currentDate = String(currentYear) + "-" + String(currentMonth) + "-" + String(monthDay)+ "\t" + "XingQi " + String(day);
                tft.drawNumber(DaySpentTogether,28,185,LOAD_FONT6);//print commemoration day
                extern int8_t wifi_num;
                if(wifi_num==4)
                {
                    tft.drawString("wyx's hotspot",80,20,LOAD_FONT2);
                }
                else
                tft.drawString(wifilib.ssid[wifi_num],80,20,LOAD_FONT2);
                String daystr=tf.readFileLine("/day.txt",1);
                Serial.println("day:"+daystr);
                int daynum=0;
                sscanf(daystr.c_str(),"%d",&daynum);
                if((DaySpentTogether-daynum)>1)
                {
                    tf.deleteFile("/filename.txt");
                    tf.writeFile("/filename.txt","S:/jinian(4).bin\n");
                }
                tf.deleteFile("/day.txt");
                tf.writeFile("/day.txt",String(DaySpentTogether).c_str());
                tf.appendFile("/day.txt","\n");
                // int nowhour=((epochTime%86400)/3600+8)%24;//当前小时
                // int nowmin=((epochTime%86400)%3600)/60;//当前分钟
                // Serial.print(String(nowhour)+":"+String(nowmin)+"\n");
                TimSpdTgtr++;
            }
        }
        else 
        {
            TimSpdTgtr=3;
        }
    }


    else if(ScrnFlg==1)//相册模式
    {
        WebLed("BLN blue");
        JinianFlg=0;//纪念日标志位置零
        TimSpdTgtr=0;//时间戳获取标志位置零
        NJFlg=0;//天气标志位置零
        ZZFlg=0;//天气标志位置零
        filemngflg=0;//文件管理标志位置零
        imgtransflg=0;//图片传输标志位置零
        /**********************
         * photo frame switching
         **********************/
        if(img_change_flag==1)
        {
            if(rootflg==0)   
            {
                root=SD.open("/image");
                rootflg=1;
            }
            File image=root.openNextFile();
            if(image==NULL)
            {
                root=SD.open("/image");
                image=root.openNextFile();
            }
            String imagename=image.name();
            String imagefile="S:"+imagename;
            Serial.println(imagefile);
            lv_img_set_src(img, imagefile.c_str());
            lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
            img_change_flag=0;
        }
        screen.routine();
    }

    else if(ScrnFlg==2)//天气预报模式，南京
    {
        WebLed("BLN gold");
        XiangceFlg=0;//相册标志位置零
        JinianFlg=0;//纪念标志位置零
        TimSpdTgtr=0;//时间戳获取标志位置零
        rootflg=0;//根目录标志位置零
        ZZFlg=0;//天气标志位置零
        filemngflg=0;//文件管理标志位置零
        imgtransflg=0;//图片传输标志位置零
        /**********************
         * get weather
         **********************/
        if(NJFlg==0&&WiFi.isConnected()==1)
        {
            LedProc(6);
            tft.fillScreen(0x0);
            wifi.getWeather(1);
            if(wtr.code>=0&&wtr.code<=3)
            {
                lv_img_set_src(img, "S:/weather/sun.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code>=4&&wtr.code<=8)
            {
                lv_img_set_src(img, "S:/image/cloudy.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code==9)
            {
                lv_img_set_src(img, "S:/weather/cloud.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if((wtr.code>=13&&wtr.code<=19)||wtr.code==10)
            {
                lv_img_set_src(img, "S:/weather/rain.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code==11||wtr.code==12)
            {
                lv_img_set_src(img, "S:/weather/light.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code>=11&&wtr.code<=25)
            {
                lv_img_set_src(img, "S:/weather/snow.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else
            {
                lv_img_set_src(img, "S:/weather/qie.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            tft.drawString(wtr.text,80,210,LOAD_FONT4); 
            tft.drawString("Nanjing",0,0,LOAD_FONT4); 
            NJFlg=1;
        }
        else if(WiFi.isConnected()==0)
        {
            tft.drawString("connect lost...",40,0,LOAD_FONT4);
            lv_img_set_src(img, "S:/image/penguin.bin");
            lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
            screen.routine();
        }
    }


    else if(ScrnFlg==3)//天气预报模式，郑州
    {
        WebLed("BLN purple");
        XiangceFlg=0;//相册标志位置零
        JinianFlg=0;//纪念标志位置零
        TimSpdTgtr=0;//时间戳获取标志位置零
        rootflg=0;//根目录标志位置零
        NJFlg=0;//天气标志位置零
        filemngflg=0;//文件管理标志位置零
        imgtransflg=0;//图片传输标志位置零
        /**********************
         * get weather
         **********************/
        if(ZZFlg==0&&WiFi.isConnected()==1)
        {
            LedProc(6);
            tft.fillScreen(0x0);
            wifi.getWeather(2);
            if(wtr.code>=0&&wtr.code<=3)
            {
                lv_img_set_src(img, "S:/weather/sun.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code>=4&&wtr.code<=8)
            {
                lv_img_set_src(img, "S:/image/cloudy.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code==9)
            {
                lv_img_set_src(img, "S:/weather/cloud.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if((wtr.code>=13&&wtr.code<=19)||wtr.code==10)
            {
                lv_img_set_src(img, "S:/weather/rain.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code==11||wtr.code==12)
            {
                lv_img_set_src(img, "S:/weather/light.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else if(wtr.code>=11&&wtr.code<=25)
            {
                lv_img_set_src(img, "S:/weather/snow.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            else
            {
                lv_img_set_src(img, "S:/weather/qie.bin");
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
            }
            tft.drawString(wtr.text,80,210,LOAD_FONT4); 
            tft.drawString("Zhengzhou",0,0,LOAD_FONT4); 
            ZZFlg=1;
        }
        else if(WiFi.isConnected()==0)
        {
            tft.drawString("connect lost...",40,0,LOAD_FONT4);
            lv_img_set_src(img, "S:/image/penguin.bin");
            lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
            screen.routine();
        }
    }

    /**********************
     * file management
     **********************/
    else if(ScrnFlg==4)
    {
        imgtransflg=0;
        if(filemngflg==0)
        {
            lv_img_set_src(img, "S:/image/black.bin");
            lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
            screen.routine();
            tft.drawString("file management",20,0,LOAD_FONT4);
            filemngflg=1;//文件管理标志位置一
        }
        WebLed("BLN salmon");
        char buf[30];
        for(int i=0;i<30;i++)
        {
            buf[i]=0;
        }
        Serial.read(buf,30);
        if(buf[0]=='l'&&buf[1]=='i'&&buf[2]=='s')
        {
            if(buf[4]=='0')
            tf.listDir("/",0);
            else if(buf[4]=='1')
            tf.listDir("/",1);
        }
        else if(buf[0]=='d'&&buf[1]=='e'&&buf[2]=='l')
        {
            String fn(buf);
            String fp=fn.substring(4);
            int length=fp.length();
            char fpath[30];
            for(int i=0;i<30;i++)
            {
                fpath[i]=0;
            }
            memcpy(fpath,buf+4,length);
            tf.deleteFile(fpath);
        }
    }

    else if(ScrnFlg==5)
    {
        if(imgtransflg==0)
        {
            lv_img_set_src(img, "S:/image/black.bin");
            lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
            screen.routine();
            tft.drawString("image trans",45,0,LOAD_FONT4);
            imgtransflg=1;
        }
        int packetSize = Udp.parsePacket();
        IPAddress remoteIp = Udp.remoteIP();
        Udp.read((unsigned char*)packetBuffer, 1030);// read the packet into packetBufffer
        if (packetSize>10) 
        {
            int PacSum=packetBuffer[0];
            PacSum=(PacSum<<8)+packetBuffer[1];

            int PacSeq=packetBuffer[2];
            PacSeq=(PacSeq<<8)+packetBuffer[3];

            int PacLen=packetBuffer[4];
            PacLen=(PacLen<<8)+packetBuffer[5];
            
            if(PacSeq==0)
            {
                for(int i=0;i<30;i++)
                {
                    filename[i]=0;
                    abspath[i]=0;
                }
                filename[0]='/';
                filename[1]='i';
                filename[2]='m';
                filename[3]='a';
                filename[4]='g';
                filename[5]='e';
                filename[6]='/';
                memcpy(filename+7,(char*)packetBuffer+6,PacLen);
                abspath[0]='S';
                abspath[1]=':';
                memcpy(abspath+2,filename,PacLen+1+6);
            }
            else if(PacSeq==1)
            {
                tf.writeBinToSd(filename,packetBuffer+6,PacLen);
            }
            else if(PacSeq>1)
            {
                tf.appendBinToSd(filename,packetBuffer+6,PacLen);
                WebLed("BLN green");
                LedProc(ledsta);
            }
            Serial.print(PacSum);Serial.print('\t');
            Serial.print(PacSeq);Serial.print('\t');
            Serial.print(PacLen);
            Serial.print("\n");
            if(PacSeq==PacSum)//传输完成
            {
                Serial.print("write complete\n");
                Serial.print("total image:");Serial.print(imgnum);Serial.print("\n");
                Serial.print("total text:");Serial.print(txtnum);Serial.print("\n");
                imgnum++;
                imgsum[6]=imgnum/100+0x30;
                imgsum[7]=(imgnum/10)%10+0x30;
                imgsum[8]=imgnum%10+0x30;
                WebLed("open led");
                lv_img_set_src(img, abspath);
                lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
                screen.routine();
                delay(1000);
            }
        }
    }
    /**********************
     * led process
     **********************/
    LedProc(ledsta);



    /**********************
     * udp packet sending test
     **********************/
    // if(!SvrSucFlg)
    // {
    //     Udp.beginPacket(ylj, remote_port);
    //     Udp.write(WriteBuffer,3);//sizeof(ReplyBuffer)
    //     Udp.endPacket();
    //     delay(100);
    //     int packetSize = Udp.parsePacket();
    //     if (packetSize) 
    //     {
    //         IPAddress remoteIp = Udp.remoteIP();// read the packet into packetBufffer
    //         Udp.read((unsigned char*)packetBuffer, 1030);
    //         String bfr=(char*)packetBuffer;
    //         String cmp=bfr.substring(0,5);
    //         Serial.print(cmp+"\n");
    //         if(!strcmp(cmp.c_str(),"espok"))//espok发过来，说明svr也ok了
    //         {
    //             Serial.print("client connected!\n");
    //             Udp.beginPacket(ylj, remote_port);
    //             Udp.write(okbfr,5);//sizeof(ReplyBuffer)
    //             Udp.endPacket();
    //             delay(100);
    //             SvrSucFlg=1;
    //             packetSize=0;
    //         }
    //     }
    // }
    
    /**********************
     * udp packet parsing
     **********************/
    // else if(SvrSucFlg==1)
    // {
    //     int packetSize = Udp.parsePacket();
    //     IPAddress remoteIp = Udp.remoteIP();
    //     Udp.read((unsigned char*)packetBuffer, 1030);// read the packet into packetBufffer
    //     if (packetSize>10) 
    //     {
    //         int PacSum=packetBuffer[0];
    //         PacSum=(PacSum<<8)+packetBuffer[1];

    //         int PacSeq=packetBuffer[2];
    //         PacSeq=(PacSeq<<8)+packetBuffer[3];

    //         int PacLen=packetBuffer[4];
    //         PacLen=(PacLen<<8)+packetBuffer[5];
            
    //         if(PacSeq==0)
    //         {
    //             for(int i=0;i<30;i++)
    //             {
    //                 filename[i]=0;
    //                 abspath[i]=0;
    //             }
    //             filename[0]='/';
    //             filename[1]='w';
    //             filename[2]='e';
    //             filename[3]='a';
    //             filename[4]='t';
    //             filename[5]='h';
    //             filename[6]='e';
    //             filename[7]='r';
    //             filename[8]='/';
    //             memcpy(filename+9,(char*)packetBuffer+6,PacLen);
    //             abspath[0]='S';
    //             abspath[1]=':';
    //             memcpy(abspath+2,filename,PacLen+1+8);
    //         }
    //         else if(PacSeq==1)
    //         {
    //             tf.writeBinToSd(filename,packetBuffer+6,PacLen);
    //         }
    //         else if(PacSeq>1)
    //         {
    //             tf.appendBinToSd(filename,packetBuffer+6,PacLen);
    //             WebLed("BLN green");
    //         }
    //         Serial.print(PacSum);Serial.print('\t');
    //         Serial.print(PacSeq);Serial.print('\t');
    //         Serial.print(PacLen);
    //         Serial.print("\n");
    //         if(PacSeq==PacSum)//传输完成
    //         {
    //             Serial.print("write complete\n");
    //             Serial.print("total image:");Serial.print(imgnum);Serial.print("\n");
    //             Serial.print("total text:");Serial.print(txtnum);Serial.print("\n");
    //             imgnum++;
    //             imgsum[6]=imgnum/100+0x30;
    //             imgsum[7]=(imgnum/10)%10+0x30;
    //             imgsum[8]=imgnum%10+0x30;
    //             img_change_flag=1;
    //             WebLed("open led");
    //         }
    //         // send a reply, to the IP address and port that sent us the packet we received
    //         uint8_t rep[8]={'y','e','s','_','_'};
    //         rep[5]=(PacSeq/100)+0x30;
    //         rep[6]=(PacSeq/10)%10+0x30;
    //         rep[7]=PacSeq%10+0x30;
    //         Udp.beginPacket(Udp.remoteIP(), 2333);
    //         Udp.write(rep,8);
    //         Udp.endPacket();
    //         delay(10);
            
    //     }

    //     if(img_change_flag==1)
    //     {
    //         Serial.print(abspath);
    //         lv_img_set_src(img,abspath);
    //         lv_obj_align(img, NULL, LV_ALIGN_CENTER, 0, 0);
    //         img_change_flag=0;
    //     }
    //     screen.routine();

    // }

    


    








    

    /**********************
     * serial print test
     **********************/
    // Serial.println("hello");





    /**********************
     * LED BLN Control
     **********************/
    // leds[0] = CRGB::Blue;
    // FastLED.setBrightness(DlyTim);
    // FastLED.show();
    // delay(10);
    // if(DlyFlag==0)
    // {
    //     DlyTim++;
    //     if(DlyTim==64)
    //     {
    //     DlyFlag=1;
    //     }
    // }
    // else if(DlyFlag==1)
    // {
    //     DlyTim--;
    //     if(DlyTim==0)
    //     {
    //     DlyFlag=0;
    //     }
    // }




    




//    int len = sprintf(buf, "S:/Scenes/Holo3D/frame%03d.bin", frame_id++);
//    int len = sprintf(buf, "S:/wsy.bin", frame_id++);
//    buf[len] = 0;
//    lv_img_set_src(guider_ui.scenes_canvas, buf);
//    Serial.println(buf);
//1
//    if (frame_id == 138) frame_id = 0;

    // delay(10);
}

void WebLed(String webcmd)
{
    if(webcmd=="open led")ledsta=1;
    if(webcmd=="close led")ledsta=2;
    if(webcmd=="toggle led")ledsta=3;
    if(webcmd=="BLN blue")ledsta=4;
    if(webcmd=="BLN red")ledsta=5;
    if(webcmd=="BLN gold")ledsta=6;
    if(webcmd=="BLN green")ledsta=7;
    if(webcmd=="BLN purple")ledsta=8;
    if(webcmd=="BLN salmon")ledsta=9;
    switch(ledsta)
    {
        case 1:
        {
            leds[0] = CRGB::Blue;
            FastLED.setBrightness(64);
            FastLED.show();
            Serial.println("led opened");
            break;
        }
        case 2:
        {
            leds[0] = CRGB::Black;
            FastLED.show();
            Serial.println("led closed");
            break;
        }
        case 3:
        {
            if(LedTogFlg==0)
            {
                leds[0] = CRGB::Blue;
                FastLED.setBrightness(64);
                FastLED.show();
                LedTogFlg=1;
                Serial.println("led opened");
            }
            else if(LedTogFlg==1)
            {
                leds[0] = CRGB::Black;
                FastLED.show();
                LedTogFlg=0;
                Serial.println("led closed");
            }
            break;
        }
    }
}

void IRAM_ATTR onTimer()
{
    if(DlyFlag==0)
    {
        DlyTim++;
        if(DlyTim==64)
        {
        DlyFlag=1;
        }
    }
    else if(DlyFlag==1)
    {
        DlyTim--;
        if(DlyTim==0)
        {
        DlyFlag=0;
        }
    }
    if(ScnOpnFlg==0)//如果亮屏，则进行时间记录
    {
        TimRcd++;
        img_change_sec++;
        if(img_change_sec==100)
        {
            img_change_flag=1;
            img_change_sec=0;
        }
    }
}

void LedProc(uint8_t ledsta)
{
    switch(ledsta)
    {
        case 4://Blue BLN control
        {
            leds[0] = CRGB::Blue;
            FastLED.setBrightness(DlyTim);
            FastLED.show();
            break;
        }
        case 5://Red BLN control
        {
            leds[0] = CRGB::Red;
            FastLED.setBrightness(DlyTim);
            FastLED.show();
            break;
        }
        case 6://Gold BLN control
        {
            leds[0] = CRGB::Gold;
            FastLED.setBrightness(DlyTim);
            FastLED.show();
            break;
        }
        case 7:
        {
            leds[0] = CRGB::Green;
            FastLED.setBrightness(DlyTim);
            FastLED.show();
            break;
        }
        case 8:
        {
            leds[0] = CRGB::MediumPurple;
            FastLED.setBrightness(DlyTim);
            FastLED.show();
            break;
        }
        case 9:
        {
            leds[0] = CRGB::LightSalmon;
            FastLED.setBrightness(DlyTim);
            FastLED.show();
            break;
        }
    }
    delay(10);
}