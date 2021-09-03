#include "network.h"

WifiLib wifilib;
weather wtr;
int8_t wifi_num=-1;//记录已保存的wifi名序号
void Network::init(void)
{
	Serial.println("scan start");
	int n = WiFi.scanNetworks();
	Serial.println("scan done");
	
	bool WifiScanFlag=0;//找到或没找到的flag
	if (n == 0)
	{
		Serial.println("no networks found");
	}
	else
	{
		Serial.print(n);
		Serial.println(" networks found");
		for (int i = 0; i < n; ++i)//输出所有找到的wifi号
		{
			Serial.print(i + 1);
			Serial.print(": ");
			Serial.print(WiFi.SSID(i));
			Serial.print(" (");
			Serial.print(WiFi.RSSI(i));
			Serial.print(")");
			Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
			delay(10);
		}
		//两次遍历，找出保存的wifi
		Serial.print("\nfound saved wifi:  ");
		for(int i=0;i<n;i++)//在找到的wifi目录中遍历
		{
			for(uint8_t j=0;j<10;j++)//在wifilib中遍历
			{
				String s(wifilib.ssid[j]);
				if(s==WiFi.SSID(i))
				{
					Serial.print("\n"+WiFi.SSID(i));
					if(WifiScanFlag==0)wifi_num=j;
					WifiScanFlag=1;
				}
			}
		}
		if(wifi_num!=-1)//如果在列表中找到已保存的wifi，则输出该wifi的ssid
		{
			// Serial.print("\nfound saved wifi:  ");
			// Serial.print(wifilib.ssid[wifi_num]);
			Serial.println("\n");
			Serial.print("Connecting: ");
			Serial.print(wifilib.ssid[wifi_num]);
			Serial.print(" @");
			Serial.print(wifilib.password[wifi_num]);
			String ssid(wifilib.ssid[wifi_num]);
    		String password(wifilib.password[wifi_num]);
			if(wifi_num==5)
			{
				WiFi.begin(ssid.c_str(), password.c_str());
			}
			else
			{
				WiFi.begin(ssid.c_str(), password.c_str());
			}
			while (WiFi.status() != WL_CONNECTED)
			{
				delay(500);
				Serial.print(".");
			}
			Serial.println("");
			Serial.println("WiFi connected");
			Serial.println("IP address: ");
			Serial.println(WiFi.localIP());
		}
		else		//如果没找到，则输出没找到
		{
			Serial.print("\ncan't found any saved wifi !");
		}
	}
}

unsigned int Network::getBilibiliFans(String uid)
{
	String fansCount = "";
	HTTPClient http;
	http.begin("http://api.bilibili.com/x/relation/stat?vmid=" + uid);

	// start connection and send HTTP headerFFF
	int httpCode = http.GET();

	// httpCode will be negative on error
	if (httpCode > 0)
	{
		// file found at server
		if (httpCode == HTTP_CODE_OK)
		{
			String payload = http.getString();
			int pos = (payload.indexOf("follower"));
			fansCount = payload.substring(pos + 10, payload.length() - 2);
		}
	}
	else
	{
		Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
	}
	http.end();

	return atol(fansCount.c_str());
}

weather Network::getWeather(int city)
{
	String WtrSta;
	HTTPClient http;
	if(city==1)//南京
	http.begin("https://api.seniverse.com/v3/weather/now.json?key=SUM14Zsp92jXoaT2y&location=nanjing&language=en&unit=c");
	else if(city==2)//郑州
	http.begin("https://api.seniverse.com/v3/weather/now.json?key=SUM14Zsp92jXoaT2y&location=zhengzhou&language=en&unit=c");
	// start connection and send HTTP headerFFF
	int httpCode = http.GET();

	// httpCode will be negative on error
	if (httpCode > 0)
	{
		// file found at server
		if (httpCode == HTTP_CODE_OK)
		{
			String payload = http.getString();
			int pos = (payload.indexOf("text"));
			int codepos=payload.indexOf("code");
			int WtrLen=0;
			int CodLen=0;
			uint8_t i=pos+7;
			while(payload[i]!=0x22)
			{
				WtrLen++;
				i++;
			}
			i=codepos+7;
			while(payload[i]!=0x22)
			{
				CodLen++;
				i++;
			}
			String WtrCode=payload.substring(codepos+7,codepos+7+CodLen);
			sscanf(WtrCode.c_str(),"%d",&wtr.code);
			
			wtr.text = payload.substring(pos + 7, pos+7+WtrLen);
			// Serial.println(wtr.code);
			// Serial.println(wtr.text);
		}
	}
	else
	{
		Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
	}
	http.end();
	return wtr;
}