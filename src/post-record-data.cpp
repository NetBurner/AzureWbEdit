/*
 * post-temperature-data.cpp
 *
 *  Created on: Aug 28, 2018
 *      Author: jonca
 */

#include <stdio.h>
#include <string.h>
#include <nbtime.h>
#include "urlencoding.h"
#include <crypto/ssl.h>
#include <base64.h>
#include <nettypes.h>
#include <config_obj.h>
#include <json_lexer.h>
#include <random.h>
#include <limits.h>
#include <record-data.h>
#include "iothub.h"
#include "urlencoding.h"
#include "iothub_client.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/platform.h"
#include "iothubtransportmqtt.h"
#include "iothub_client_options.h"

#include "iothubtransportmqtt.h"

#define BUFFER_LENGTH 200

bool gIsCollecting = true;

IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;
typedef struct EVENT_INSTANCE_TAG
{
    IOTHUB_MESSAGE_HANDLE messageHandle;
    size_t messageTrackingId;
} EVENT_INSTANCE;

const char connectionString[] = "HostName=<REPLACE WITH YOUR STRING>"; // Make sure to replace this with your specific connection string


static int DeviceMethodCallback(const char* method_name, const unsigned char* payload, size_t size, unsigned char** response, size_t* response_size, void* userContextCallback)
{

    // TODO: Check the method name
    ParsedJsonDataSet payloadObj((const char *)payload, size);
    json_primative_type type = payloadObj.FindGlobalElementAfterName("command");
    if (type != NUMBER)
    {
        iprintf("Did not find a command in the payload.\n");
    }
    else
    {
        CommandType command = (CommandType)payloadObj.CurrentNumber();
        switch (command)
        {
        case SET_IS_COLLECTING:
            type = payloadObj.FindGlobalElementAfterName("data");
            if (type != STRING)
            {
                iprintf("Did not find the data in the payload.\n");
                break;
            }
            gIsCollecting = atoi(payloadObj.CurrentString());
            iprintf("Setting collecting to %d.\n", gIsCollecting);
            break;
        case NONE:
        default:
            break;
        }
    }

    *response = 0;
    *response_size = 0;

    return IOTHUBMESSAGE_ACCEPTED;
}

static void ConnectionStatusCallback(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
	(void)reason;
	(void)user_context;
	// This sample DOES NOT take into consideration network outages.
	if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
	{
		(void)printf("The device client is connected to iothub\r\n");
	}
	else
	{
		(void)printf("The device client has been disconnected\r\n");
	}
}

typedef struct CLIENT_SAMPLE_INFO_TAG
{
    unsigned int sleep_time;
    char* iothub_uri;
    char* access_key_name;
    char* device_key;
    char* device_id;
    int registration_complete;
} CLIENT_SAMPLE_INFO;

typedef struct IOTHUB_CLIENT_SAMPLE_INFO_TAG
{
    int connected;
    int stop_running;
} IOTHUB_CLIENT_SAMPLE_INFO;

static IOTHUBMESSAGE_DISPOSITION_RESULT receive_msg_callback(IOTHUB_MESSAGE_HANDLE message, void* user_context)
{
    (void)message;
    IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
    (void)printf("Stop message received from IoTHub\r\n");
    iothub_info->stop_running = 1;
    return IOTHUBMESSAGE_ACCEPTED;
}

static void iothub_connection_status(IOTHUB_CLIENT_CONNECTION_STATUS result, IOTHUB_CLIENT_CONNECTION_STATUS_REASON reason, void* user_context)
{
    (void)reason;
    if (user_context == NULL)
    {
        printf("iothub_connection_status user_context is NULL\r\n");
    }
    else
    {
        IOTHUB_CLIENT_SAMPLE_INFO* iothub_info = (IOTHUB_CLIENT_SAMPLE_INFO*)user_context;
        if (result == IOTHUB_CLIENT_CONNECTION_AUTHENTICATED)
        {
            iothub_info->connected = 1;
        }
        else
        {
            iothub_info->connected = 0;
            iothub_info->stop_running = 1;
        }
    }
}


static IOTHUBMESSAGE_DISPOSITION_RESULT ReceiveMessageCallback(IOTHUB_MESSAGE_HANDLE message, void *userContextCallback)
{
    return IOTHUBMESSAGE_ACCEPTED;
}

bool InitializeAzureSDK()
{
	int res = IoTHub_Init();
	iprintf("IoT Init Result: %d\r\n", res);

    iotHubClientHandle = IoTHubClient_LL_CreateFromConnectionString(connectionString, MQTT_Protocol);

    if (iotHubClientHandle == nullptr)
    {
        iprintf("Did not create client handle.\n");
        return false;
    }
    else
    {
    	iprintf("IoTHubClientHandle creation successful.\r\n");
    }

    // Setting connection status callback to get indication of connection to IoT Central
    (void)IoTHubClient_LL_SetConnectionStatusCallback(iotHubClientHandle, ConnectionStatusCallback, nullptr);

    bool traceOn = true;
    IoTHubClient_LL_SetOption(iotHubClientHandle, OPTION_LOG_TRACE, &traceOn);

    int deviceMethodContext = 0;
    if (IoTHubClient_LL_SetDeviceMethodCallback(iotHubClientHandle, DeviceMethodCallback, &deviceMethodContext) != IOTHUB_CLIENT_OK)
    {
        iprintf("ERROR: IoTHubClient_LL_SetDeviceMethodCallback...FAILED!\n");
        return false;
    }
    else
    {
    	iprintf("IoT HubClient Method Callback Setting Successful.\r\n");
    }

    return true;

}


void CreateOutMessage(ParsedJsonDataSet& jsonOutObject) {
    static uint16_t postId = 0;
    time_t seconds = 0;
    char dateString[50] = { 0 };

    time(&seconds);
    struct tm tm_struct;

    localtime_r( &seconds, &tm_struct);
    sniprintf( dateString, 50, "%d/%d/%d %02d:%02d:%02d",
               tm_struct.tm_mon+1,
               tm_struct.tm_mday,
               tm_struct.tm_year+1900,
               tm_struct.tm_hour,
               tm_struct.tm_min,
               tm_struct.tm_sec );

    PostRecord record = { dateString, postId++ };

    SerializeRecordJson(record, jsonOutObject);
}

void PostRecordData()
{
    char buffer[BUFFER_LENGTH];
    static int iterator = 0;
    ParsedJsonDataSet jsonOut;
    CreateOutMessage(jsonOut);
    jsonOut.PrintObjectToBuffer(buffer, BUFFER_LENGTH, false);

    iprintf("Printing buffer: %s\r\n", buffer);

    EVENT_INSTANCE azureInstance;
    iprintf("Calling create from byte array\r\n");
    OSTimeDly(2);
    azureInstance.messageHandle = IoTHubMessage_CreateFromByteArray((const unsigned char*)buffer, strlen(buffer));
    iprintf("Done\r\n");
    OSTimeDly(2);
    if (azureInstance.messageHandle == NULL)
    {
        iprintf("ERROR: iotHubMessageHandle is NULL\n");
    }
    else
    {
        // TODO: There's no topic name involved here.
        // TODO: Check out the propmap business. That's weird.
    	azureInstance.messageTrackingId = iterator;

        IOTHUB_CLIENT_RESULT result = IoTHubClient_LL_SendEventAsync(iotHubClientHandle, azureInstance.messageHandle, NULL, NULL);
        if (result != IOTHUB_CLIENT_OK)
        {
            iprintf("Failed to send.\n");
        }
        else
        {
            iprintf("Success\n");
        }
        iterator++;
        OSTimeDly(2);
    }

}
