/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include "aos/kernel.h"
#include "ulog/ulog.h"
#include "aos/hal/gpio.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "linkkit/infra/infra_types.h"
#include "linkkit/infra/infra_defs.h"
#include "linkkit/infra/infra_compat.h"
#include "linkkit/dev_model_api.h"
#include "linkkit/infra/infra_config.h"
#include "linkkit/wrappers/wrappers.h"

#ifdef INFRA_MEM_STATS
    #include "linkkit/infra/infra_mem_stats.h"
#endif

#include "cJSON.h"

#ifdef ATM_ENABLED
    #include "at_api.h"
#endif
#include "app_entry.h"

#define GPIO_LED_IO     2

// 换成自己的相关信息
#define PRODUCT_KEY      "a14nZc42cBL"
#define PRODUCT_SECRET   "eZymfgEMF5dp91QO"
#define DEVICE_NAME      "rgblight"
#define DEVICE_SECRET    "j79mGQPPxpfqFbnnjlsMfOK9QgbmHT6h"

#define EXAMPLE_TRACE(...)                                          \
    do {                                                            \
        HAL_Printf("\033[1;32;40m%s.%d: ", __func__, __LINE__);     \
        HAL_Printf(__VA_ARGS__);                                    \
        HAL_Printf("\033[0m\r\n");                                  \
    } while (0)

#define EXAMPLE_MASTER_DEVID            (0)
#define EXAMPLE_YIELD_TIMEOUT_MS        (200)

gpio_dev_t led;

typedef struct {
    int master_devid;
    int cloud_connected;
    int master_initialized;
} user_example_ctx_t;

static user_example_ctx_t g_user_example_ctx;

/** cloud connected event callback */
static int user_connected_event_handler(void)
{
    EXAMPLE_TRACE("Cloud Connected");
    g_user_example_ctx.cloud_connected = 1;

    return 0;
}

/* device initialized event callback */
static int user_initialized(const int devid)
{
    EXAMPLE_TRACE("Device Initialized");
    g_user_example_ctx.master_initialized = 1;

    return 0;
}

/** recv event post response message from cloud **/
static int user_property_set_event_handler(const int devid, const char *request, const int request_len)
{
    int res = 0;
	uint8_t lightswitch = 0;
    cJSON *root = NULL;	
	led.port = GPIO_LED_IO;
    led.config = OUTPUT_PUSH_PULL;
    hal_gpio_init(&led);
    printf("Property Set Received, Devid: %d, Request: %s\r\n", devid, request);
    if ((root = cJSON_Parse(request)) == NULL)
    {
        printf("json parse error\r\n");
        return -1;
    }
	lightswitch = cJSON_GetObjectItem(root, "LightSwitch")->valueint;
	printf("zaizhe: %d\r\n", lightswitch);
	if (lightswitch == 0)
		{
			hal_gpio_output_low(&led); 
			printf("close light\r\n");
        return 0;
		}
        else if (lightswitch == 1)
        {
            hal_gpio_output_high(&led); 
			printf("open light\r\n");
        }
		
	res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)request, request_len);
    aos_free(root);
    return 0;
   
}


static int user_service_request_event_handler(const int devid, const char *serviceid, const int serviceid_len,
        const char *request, const int request_len,
        char **response, int *response_len)
{
    int add_result = 0;
    int ret = -1;
    cJSON *root = NULL, *item_number_a = NULL, *item_number_b = NULL;
    const char *response_fmt = "{\"Result\": %d}";
    EXAMPLE_TRACE("Service Request Received, Service ID: %.*s, Payload: %s", serviceid_len, serviceid, request);

    /* Parse Root */
    root = cJSON_Parse(request);
    if (root == NULL || !cJSON_IsObject(root)) {
        EXAMPLE_TRACE("JSON Parse Error");
        return -1;
    }
    do{
        if (strlen("Operation_Service") == serviceid_len && memcmp("Operation_Service", serviceid, serviceid_len) == 0) {
            /* Parse NumberA */
            item_number_a = cJSON_GetObjectItem(root, "NumberA");
            if (item_number_a == NULL || !cJSON_IsNumber(item_number_a)) {
                break;
            }
            EXAMPLE_TRACE("NumberA = %d", item_number_a->valueint);

            /* Parse NumberB */
            item_number_b = cJSON_GetObjectItem(root, "NumberB");
            if (item_number_b == NULL || !cJSON_IsNumber(item_number_b)) {
                break;
            }
            EXAMPLE_TRACE("NumberB = %d", item_number_b->valueint);
            add_result = item_number_a->valueint + item_number_b->valueint;
            ret = 0;
            /* Send Service Response To Cloud */
        }
    }while(0);

    *response_len = strlen(response_fmt) + 10 + 1;
    *response = (char *)HAL_Malloc(*response_len);
    if (*response != NULL) {
        memset(*response, 0, *response_len);
        HAL_Snprintf(*response, *response_len, response_fmt, add_result);
        *response_len = strlen(*response);
    }

    cJSON_Delete(root);
    return ret;
}

void user_post_property(void)
{
    static int cnt = 0;
    int res = 0;

    char property_payload[30] = {0};
    HAL_Snprintf(property_payload, sizeof(property_payload), "{\"Counter\": %d}", cnt++);

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_POST_PROPERTY,
                             (unsigned char *)property_payload, strlen(property_payload));

    EXAMPLE_TRACE("Post Property Message ID: %d", res);
}

void user_post_event(void)
{
    int res = 0;
    char *event_id = "HardwareError";
    char *event_payload = "{\"ErrorCode\": 0}";

    res = IOT_Linkkit_TriggerEvent(EXAMPLE_MASTER_DEVID, event_id, strlen(event_id),
                                   event_payload, strlen(event_payload));
    EXAMPLE_TRACE("Post Event Message ID: %d", res);
}

void user_deviceinfo_update(void)
{
    int res = 0;
    char *device_info_update = "[{\"attrKey\":\"abc\",\"attrValue\":\"hello,world\"}]";

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_DEVICEINFO_UPDATE,
                             (unsigned char *)device_info_update, strlen(device_info_update));
    EXAMPLE_TRACE("Device Info Update Message ID: %d", res);
}

void user_deviceinfo_delete(void)
{
    int res = 0;
    char *device_info_delete = "[{\"attrKey\":\"abc\"}]";

    res = IOT_Linkkit_Report(EXAMPLE_MASTER_DEVID, ITM_MSG_DEVICEINFO_DELETE,
                             (unsigned char *)device_info_delete, strlen(device_info_delete));
    EXAMPLE_TRACE("Device Info Delete Message ID: %d", res);
}

static int user_cloud_error_handler(const int code, const char *data, const char *detail)
{
    EXAMPLE_TRACE("code =%d ,data=%s, detail=%s", code, data, detail);
    return 0;
}

void set_iotx_info()
{
    char _device_name[IOTX_DEVICE_NAME_LEN + 1] = {0};
    HAL_GetDeviceName(_device_name);
    if (strlen(_device_name) == 0) {
        HAL_SetProductKey(PRODUCT_KEY);
        HAL_SetProductSecret(PRODUCT_SECRET);
        HAL_SetDeviceName(DEVICE_NAME);
        HAL_SetDeviceSecret(DEVICE_SECRET);
    }
}

int linkkit_main(void *paras)
{
    int res = 0;
    int cnt = 0;
    int auto_quit = 0;
    iotx_linkkit_dev_meta_info_t master_meta_info;
    int domain_type = 0, dynamic_register = 0, post_reply_need = 0, fota_timeout = 30;
    int   argc = 0;
    char  **argv = NULL;

    if (paras != NULL) {
        argc = ((app_main_paras_t *)paras)->argc;
        argv = ((app_main_paras_t *)paras)->argv;
    }
#ifdef ATM_ENABLED
    if (IOT_ATM_Init() < 0) {
        EXAMPLE_TRACE("IOT ATM init failed!\n");
        return -1;
    }
#endif


    if (argc >= 2 && !strcmp("auto_quit", argv[1])) {
        auto_quit = 1;
        cnt = 0;
    }
    memset(&g_user_example_ctx, 0, sizeof(user_example_ctx_t));

    memset(&master_meta_info, 0, sizeof(iotx_linkkit_dev_meta_info_t));
    HAL_GetProductKey(master_meta_info.product_key);
    HAL_GetDeviceName(master_meta_info.device_name);
    HAL_GetProductSecret(master_meta_info.product_secret);
    HAL_GetDeviceSecret(master_meta_info.device_secret);

    IOT_SetLogLevel(IOT_LOG_INFO);

    /* Register Callback */
    IOT_RegisterCallback(ITE_CONNECT_SUCC, user_connected_event_handler);
    IOT_RegisterCallback(ITE_PROPERTY_SET, user_property_set_event_handler);

    domain_type = IOTX_CLOUD_REGION_SHANGHAI;
    IOT_Ioctl(IOTX_IOCTL_SET_DOMAIN, (void *)&domain_type);

    /* Choose Login Method */
    dynamic_register = 0;
    IOT_Ioctl(IOTX_IOCTL_SET_DYNAMIC_REGISTER, (void *)&dynamic_register);

    /* post reply doesn't need */
    post_reply_need = 1;
    IOT_Ioctl(IOTX_IOCTL_RECV_EVENT_REPLY, (void *)&post_reply_need);

    IOT_Ioctl(IOTX_IOCTL_FOTA_TIMEOUT_MS, (void *)&fota_timeout);
#if defined(USE_ITLS)
    {
        char url[128] = {0};
        int port = 1883;
        snprintf(url, 128, "%s.itls.cn-shanghai.aliyuncs.com",master_meta_info.product_key);
        IOT_Ioctl(IOTX_IOCTL_SET_MQTT_DOMAIN, (void *)url);
        IOT_Ioctl(IOTX_IOCTL_SET_CUSTOMIZE_INFO, (void *)"authtype=id2");
        IOT_Ioctl(IOTX_IOCTL_SET_MQTT_PORT, &port);
    } 
#endif
    /* Create Master Device Resources */
    do {
        g_user_example_ctx.master_devid = IOT_Linkkit_Open(IOTX_LINKKIT_DEV_TYPE_MASTER, &master_meta_info);
        if (g_user_example_ctx.master_devid >= 0) {
            break;
        }
        EXAMPLE_TRACE("IOT_Linkkit_Open failed! retry after %d ms\n", 2000);
        HAL_SleepMs(2000);
    } while (1);
    /* Start Connect Aliyun Server */
    do {
        res = IOT_Linkkit_Connect(g_user_example_ctx.master_devid);
        if (res >= 0) {
            break;
        }
        EXAMPLE_TRACE("IOT_Linkkit_Connect failed! retry after %d ms\n", 5000);
        HAL_SleepMs(5000);
    } while (1);

    while (1) {
        IOT_Linkkit_Yield(EXAMPLE_YIELD_TIMEOUT_MS);

    }

    IOT_Linkkit_Close(g_user_example_ctx.master_devid);

    IOT_DumpMemoryStats(IOT_LOG_DEBUG);

    return 0;
}
