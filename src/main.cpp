#include <predef.h>
#include <stdio.h>
#include <nbrtos.h>
#include <http.h>
#include <init.h>

#include <tcp.h>
#include <dns.h>
#include <nbtime.h>
#include <post-record-data.h>
#include <string.h>
#include <crypto/ssl.h>

#include "iothub.h"
#include "urlencoding.h"
#include "iothub_client.h"
#include "iothub_message.h"
#include "azure_c_shared_utility/platform.h"
#include "iothubtransportmqtt.h"
#include "iothub_client_options.h"

const char * AppName="microsoft-azure-final";

extern IOTHUB_CLIENT_LL_HANDLE iotHubClientHandle;

void UserMain(void * pd)
{
    init();
    WaitForActiveNetwork(TICKS_PER_SECOND * 5);   // Wait up to 5 seconds for active network activity 
    //StartHttp();

    IPADDR ntpServer;
    int dnsSuccess = GetHostByName("0.pool.ntp.org", &ntpServer, INADDR_ANY, TICKS_PER_SECOND * 5);
    if(dnsSuccess != 0 || !SetNTPTime(ntpServer))
    {
        iprintf("Failed to set time.\n");
        while(1)
         {
            OSTimeDly(TICKS_PER_SECOND);
        }
    }

    iprintf("Application %s started\n",AppName );
    if( !InitializeAzureSDK() )
    {
        iprintf("Failed to initialize the SDK.\n");
        while(1) { OSTimeDly(TICKS_PER_SECOND); }
    }

    PostRecordData();

    uint16_t count = 0;
    while (1)
    {
    	IoTHubClient_LL_DoWork(iotHubClientHandle);
        if((count++ % 5) == 0)
        {
        	iprintf("Posting record data for id: %d\r\n", count);
            PostRecordData();
        }
        OSTimeDly(TICKS_PER_SECOND);
    }
}
