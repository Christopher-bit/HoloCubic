#ifndef NETWORK_H
#define NETWORK_H

#include <WiFi.h>
#include <HTTPClient.h>

class weather{
	public:
	String text;
	int code;
};
class Network
{
private:
	 
public:
	void init(void);
	unsigned int getBilibiliFans(String url);
	weather getWeather(int city);
};
class WifiLib{
	public:
    char ssid[10][30]={
		"Christopher",
		"FAST_322E",
		"TP-LINK_EA16",
		"@PHICOMM_C8_5G",
		"玩游戏的小破为"
		// "nuaa.wifi6",
		// "nuaa.portal"
	};
    char password[10][30]={
		"wsywsywsy",
		"20000606",
		"20000606",
		"mima69521530",
		"wyx03060106"
	};
};

#endif