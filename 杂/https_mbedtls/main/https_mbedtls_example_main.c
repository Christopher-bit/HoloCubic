
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include "mbedtls/certs.h"

#include "os.h"
#include "cJSON.h"

           

#define EXAMPLE_WIFI_SSID "RI-PIPE-3-2G"         //账号
#define EXAMPLE_WIFI_PASS "hyk11611344"       //密码

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
//"www.howsmyssl.com"
#define WEB_SERVER "api.seniverse.com" 
#define WEB_PORT "443"
#define WEB_URL "https://www.howsmyssl.com/a/check"

static const char *TAG = "example";

// static const char *REQUEST = "GET " WEB_URL " HTTP/1.0\r\n"
    // "Host: "WEB_SERVER"\r\n"
    // "User-Agent: esp-idf/1.0 esp32\r\n"
    // "\r\n";


extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");

static esp_err_t event_handler(void *ctx, system_event_t *event)
{
    switch(event->event_id) {
    case SYSTEM_EVENT_STA_START:
        esp_wifi_connect();
        break;
    case SYSTEM_EVENT_STA_GOT_IP:
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        /* This is a workaround as ESP32 WiFi libs don't currently
           auto-reassociate. */
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void initialise_wifi(void)
{
    tcpip_adapter_init();
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK( esp_event_loop_init(event_handler, NULL) );
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
        },
    };
    ESP_LOGI(TAG, "Setting WiFi configuration SSID %s...", wifi_config.sta.ssid);
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
    ESP_ERROR_CHECK( esp_wifi_start() );
}
 static char host[32] = "api.seniverse.com";
 static char filename[1024] = "v3/weather/daily.json?key=SNzrtSbzTn3jtdgkS&location=shenzhen&language=zh-Hans&unit=c&start=0&days=5";
/* GET请求 */
#define GET                     "GET /%s HTTP/1.1\r\nAccept:*/*\r\nHost:%s\r\n\r\n"
/* POST请求,此章节暂时不讲 */
#define POST                    "POST /%s HTTP/1.1\r\nAccept: */*\r\nContent-Length: %d\r\nContent-Type: application/x-www-form-urlencoded; charset=utf-8\r\nHost: %s\r\nConnection: Keep-Alive\r\n\r\n%s"
/* 存放get请求的相关数据 */
static uint8_t https_get_buffer[1024];
static uint8_t  recv_buff[1024*2];

static void https_get_task(void *pvParameters)
{
    char buf[512];
    int ret, flags, len;

    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ssl_context ssl;
    //mbedtls_x509_crt cacert;/* 当需要进行CA证书认证时才需要初始化 */
    mbedtls_ssl_config conf;
    mbedtls_net_context server_fd;

    mbedtls_ssl_init(&ssl);
    //mbedtls_x509_crt_init(&cacert);/* 当需要进行CA证书认证时才需要初始化 */
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ESP_LOGI(TAG, "Seeding the random number generator");//播种随机数生成器

    mbedtls_ssl_config_init(&conf);

    mbedtls_entropy_init(&entropy);
    //This function seeds and sets up the CTR_DRBG entropy source for future reseeds. 
    if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                                    NULL, 0)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned %d", ret);
        abort();
    }

    ESP_LOGI(TAG, "Loading the CA root certificate...");
	/* 当需要进行CA证书认证时才需要初始化 */
    // ret = mbedtls_x509_crt_parse(&cacert, server_root_cert_pem_start,
                                 // server_root_cert_pem_end-server_root_cert_pem_start);

    // if(ret < 0)
    // {
        // ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x\n\n", -ret);
        // abort();
    // }

    ESP_LOGI(TAG, "Setting hostname for TLS session...");//正在设置TLS会话的主机名

     /* Hostname set here should match CN in server certificate 此处设置的主机名应与服务器证书中的cn匹配*/
    if((ret = mbedtls_ssl_set_hostname(&ssl, WEB_SERVER)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
        abort();
    }

    ESP_LOGI(TAG, "Setting up the SSL/TLS structure...");//设置SSL/TLS结构

    if((ret = mbedtls_ssl_config_defaults(&conf,
                                          MBEDTLS_SSL_IS_CLIENT,
                                          MBEDTLS_SSL_TRANSPORT_STREAM,
                                          MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned %d", ret);
        goto exit;
    }

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);//证书不同也照样运行下去
    //mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);/* 当需要进行CA证书认证时才需要初始化 */
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg); /* 设置随机数生成的函数及方法 */
#ifdef CONFIG_MBEDTLS_DEBUG
    mbedtls_esp_enable_debug_log(&conf, 4);
#endif

    if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0)
    {
        ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x\n\n", -ret);
        goto exit;
    }

    while(1) {
        /* Wait for the callback to set the CONNECTED_BIT in the
           event group.
        */
        xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                            false, true, portMAX_DELAY);
        ESP_LOGI(TAG, "Connected to AP");
		/* 服务器连接的句柄初始化*/
        mbedtls_net_init(&server_fd);
//api.seniverse.com/v3/weather/daily.json?key=SNzrtSbzTn3jtdgkS&location=beijing&language=zh-Hans&unit=c&start=0&days=5
        ESP_LOGI(TAG, "Connecting to %s:%s...", host, WEB_PORT);
		/* 连接WEB服务器,端口，TCP连接*/
        if ((ret = mbedtls_net_connect(&server_fd, host,
                                      WEB_PORT, MBEDTLS_NET_PROTO_TCP)) != 0)
        {
            ESP_LOGE(TAG, "mbedtls_net_connect returned -%x", -ret);
            goto exit;
        }

        ESP_LOGI(TAG, "Connected.");
		/* 设置发送以及接收的时候,调用的内部函数 */
        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");
		/* 执行SSL/TLS握手*/
        while ((ret = mbedtls_ssl_handshake(&ssl)) != 0)
        {
            if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
            {
                ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
                goto exit;
            }
        }

        ESP_LOGI(TAG, "Verifying peer X.509 certificate...");//验证对等X.509证书
		/* 校验证书*/
        if ((flags = mbedtls_ssl_get_verify_result(&ssl)) != 0)
        {
            ESP_LOGW(TAG, "Failed to verify peer certificate!");
            bzero(buf, sizeof(buf));
            mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
            ESP_LOGW(TAG, "verification info: %s", buf);
        }
        else {
            ESP_LOGI(TAG, "Certificate verified.");
        }

        ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(&ssl));//密码套件是

        ESP_LOGI(TAG, "Writing HTTP request...");

        
		size_t written_bytes = 0;
		 written_bytes = sprintf((char*)https_get_buffer, GET, filename, host);
		 ESP_LOGI(TAG, "  > get:%s",https_get_buffer);
		while ((ret = mbedtls_ssl_write(&ssl, https_get_buffer, written_bytes)) <= 0)
		{
			if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
			{
				ESP_LOGI(TAG, " failed\n  ! mbedtls_ssl_write returned %d\n\n", ret);
				goto exit;
			}
		}
        ESP_LOGI(TAG, "Reading HTTP response...");

	  do
	  {
		len = sizeof(recv_buff) - 1;
		memset(recv_buff, 0, sizeof(recv_buff));
		ret = mbedtls_ssl_read(&ssl, recv_buff, len);
		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE)
		{
		  continue;
		}

		if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY)
		{
		  ESP_LOGI(TAG, "MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY\n");
		  ret = 0;
		  goto exit;
		}

		if (ret < 0)
		{
		  ESP_LOGI(TAG, "failed\n  ! mbedtls_ssl_read returned %d\n\n", ret);
		  break;
		}
		if (ret == 0)
		{
		  ESP_LOGI(TAG, "\n\nEOF\n\n");      
		  break;
		}
		else
		{
			/* 存放接收到的josn数据 */
			cJSON *json_data;
			len = ret;
			//ESP_LOGI(TAG, "-----> %d bytes read\n\n%s", len, recv_buff);
			json_data = cJSON_Parse(os_strchr((char*)recv_buff, '{'));  
			
			cJSON *results = cJSON_GetObjectItem(json_data,"results");
			cJSON *location = cJSON_GetObjectItem(cJSON_GetArrayItem(results,0),"location");
			cJSON *daily = cJSON_GetObjectItem(cJSON_GetArrayItem(results,0),"daily");
			ESP_LOGI("", "city: %s\n%s\n%s\n%s\n",
			cJSON_Print(cJSON_GetObjectItem(location, "name")),
			cJSON_Print(cJSON_GetArrayItem(daily, 0)),
			cJSON_Print(cJSON_GetArrayItem(daily, 1)),
			cJSON_Print(cJSON_GetArrayItem(daily, 2)));
			
			
		}
	  } while (1);
		
        mbedtls_ssl_close_notify(&ssl);

    exit:
        mbedtls_ssl_session_reset(&ssl);
        mbedtls_net_free(&server_fd);

        if(ret != 0)
        {
            mbedtls_strerror(ret, buf, 100);
            ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
        }

        //putchar('\n'); // JSON output doesn't have a newline at end

        static int request_count;
        ESP_LOGI(TAG, "Completed %d requests", ++request_count);

        for(int countdown = 20; countdown >= 0; countdown--) {
            ESP_LOGI(TAG, "%d...", countdown);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        ESP_LOGI(TAG, "Starting again!");
    }
}

void app_main()
{
    ESP_ERROR_CHECK( nvs_flash_init() );
    initialise_wifi();
    xTaskCreate(&https_get_task, "https_get_task", 8192, NULL, 5, NULL);
}
