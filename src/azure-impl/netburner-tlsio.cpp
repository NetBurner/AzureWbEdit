/*
 * netburner-xio.cpp
 *
 * Based heavily on the Arduino PAL implementation found here: https://github.com/Azure/azure-iot-pal-arduino/blob/master/pal/src/tlsio_arduino.c
 *
 * However, this implementation does not take into consideration the OPENING or CLOSING states. The underlying SSL
 * implementation does not expose those states, blocking until the connections are either opened or closed. Thus, the
 * "dowork" function is essentially downgraded to the equivalent of "read" in the AWS or Paho platform implementations.
 *
 * I have also taken the liberty of changing the strict cyclomatic complexity-compliant formulation of nested ifs to
 * separate out input validation, because I like that style better, and to pair the state sections with switch
 * statements to highlight uncovered states/branches.
 */

#include <stdio.h>
#include <stdlib.h>
#include <azure_c_shared_utility/tlsio.h>
#include <azure_c_shared_utility/tlsio_options.h>
#include <tcp.h>
#include <dns.h>
#include <crypto/ssl.h>
#include <webclient/http_funcs.h>
#include <iosys.h>

#define MAX_TLS_OPENING_RETRY   10
#define MAX_TLS_CLOSING_RETRY   10
#define RECEIVE_BUFFER_SIZE     128
#define CONNECT_TIMEOUT 10 * TICKS_PER_SECOND
#define RECEIVE_TIMEOUT .1 * TICKS_PER_SECOND

#define __FAILURE__ -1

#define CallErrorCallback() do { if (tlsioInstance->onIoError != NULL) (void)tlsioInstance->onIoError(tlsioInstance->onIoErrorContext); } while((void)0, 0)
#define CallOpenCallback(status) do { if (tlsioInstance->onIoOpenComplete != NULL) (void)tlsioInstance->onIoOpenComplete(tlsioInstance->onIoOpenCompleteContext, status); } while((void)0, 0)
#define CallCloseCallback() do { if (tlsioInstance->onIoCloseComplete != NULL) (void)tlsioInstance->onIoCloseComplete(tlsioInstance->onIoCloseCompleteContext); } while((void)0, 0)

enum TLSIO_NETBURNER_STATE {
    TLSIO_NETBURNER_STATE_CLOSED,
    TLSIO_NETBURNER_STATE_OPENING,
    TLSIO_NETBURNER_STATE_OPEN,
    TLSIO_NETBURNER_STATE_CLOSING,
    TLSIO_NETBURNER_STATE_ERROR,
    TLSIO_NETBURNER_STATE_NULL
};

typedef struct NetBurnerTLS_tag
{
    ON_IO_OPEN_COMPLETE onIoOpenComplete;
    void* onIoOpenCompleteContext;

    ON_BYTES_RECEIVED onBytesReceived;
    void* onBytesReceivedContext;

    ON_IO_ERROR onIoError;
    void* onIoErrorContext;

    ON_IO_CLOSE_COMPLETE onIoCloseComplete;
    void* onIoCloseCompleteContext;

    char * remoteHost;
    int remotePort;
    IPADDR remoteAddr;
    int fds;
    TLSIO_NETBURNER_STATE state;
    TLSIO_OPTIONS options;
} NetBurnerTLS;

CONCRETE_IO_HANDLE tlsioNetburnerCreate(void * io_create_parameters);
void tlsioNetburnerDestroy(CONCRETE_IO_HANDLE concrete_io);
int tlsioNetburnerOpen(CONCRETE_IO_HANDLE concreteIo, ON_IO_OPEN_COMPLETE onIoOpenComplete, void * onIoOpenCompleteContext, ON_BYTES_RECEIVED onBytesReceived, void * onBytesReceivedContext, ON_IO_ERROR onIoError, void * onIoErrorContext);
int tlsioNetburnerClose(CONCRETE_IO_HANDLE concreteIo, ON_IO_CLOSE_COMPLETE on_io_close_complete, void* callback_context);
int tlsioNetburnerSend(CONCRETE_IO_HANDLE concreteIo, const void * buffer, size_t size, ON_SEND_COMPLETE onSendComplete, void* callbackContext);
void tlsioNetburnerDowork(CONCRETE_IO_HANDLE concreteIo);
int tlsioNetburnerSetoption(CONCRETE_IO_HANDLE concreteIo, const char * optionName, const void * value);
OPTIONHANDLER_HANDLE tlsioNetburnerRetrieveOptions(CONCRETE_IO_HANDLE concrete_io);

CONCRETE_IO_HANDLE tlsioNetburnerCreate(void * io_create_parameters)
{
	iprintf("Initializing TLSIO\r\n");
    NetBurnerTLS * tlsioInstance;
    if (io_create_parameters == NULL)
    {
        iprintf("Invalid TLS parameters.");
        tlsioInstance = NULL;
        return tlsioInstance;
    }

    tlsioInstance = (NetBurnerTLS*)malloc(sizeof(NetBurnerTLS));
    if (tlsioInstance == NULL)
    {
        iprintf("There is not enough memory to create the TLS instance.");
        tlsioInstance = NULL;
    }
    else
    {
        TLSIO_CONFIG * tlsioConfig = (TLSIO_CONFIG*)io_create_parameters;
        tlsio_options_initialize(&tlsioInstance->options, TLSIO_OPTION_BIT_NONE);

        // Initialized the callback pointers
        tlsioInstance->onIoOpenComplete = NULL;
        tlsioInstance->onIoOpenCompleteContext = NULL;
        tlsioInstance->onBytesReceived = NULL;
        tlsioInstance->onBytesReceivedContext = NULL;
        tlsioInstance->onIoError = NULL;
        tlsioInstance->onIoErrorContext = NULL;
        tlsioInstance->onIoCloseComplete = NULL;
        tlsioInstance->onIoCloseCompleteContext = NULL;

        tlsioInstance->state = TLSIO_NETBURNER_STATE_CLOSED;

        iprintf("%s\n", tlsioConfig->hostname);
        if (GetHostByName(tlsioConfig->hostname, &tlsioInstance->remoteAddr, INADDR_ANY, TICKS_PER_SECOND * 5) == 0)
        {
            tlsioInstance->remoteHost = (char *)tlsioConfig->hostname;
            tlsioInstance->remotePort = (uint16_t)tlsioConfig->port;
        }
        else
        {
            iprintf("Host %s not found.", tlsioConfig->hostname);
            free(tlsioInstance);
            tlsioInstance = NULL;
        }
    }

    return (CONCRETE_IO_HANDLE)tlsioInstance;
}

void tlsioNetburnerDestroy(CONCRETE_IO_HANDLE concreteIo)
{
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;

    if (concreteIo == NULL)
    {
        iprintf("NULL TLS handle.\n");
        return;
    }

    switch (tlsioInstance->state)
    {
    case TLSIO_NETBURNER_STATE_OPENING:
    case TLSIO_NETBURNER_STATE_OPEN:
    case TLSIO_NETBURNER_STATE_CLOSING:
        iprintf("TLS destroyed with a SSL connection still active.\n");
        break;
    case TLSIO_NETBURNER_STATE_CLOSED:
    case TLSIO_NETBURNER_STATE_ERROR:
    case TLSIO_NETBURNER_STATE_NULL:
    default:
        break;
    }

    tlsio_options_release_resources(&tlsioInstance->options);
    free(tlsioInstance);
}

int tlsioNetburnerOpen(
        CONCRETE_IO_HANDLE concreteIo,
        ON_IO_OPEN_COMPLETE onIoOpenComplete,
        void * onIoOpenCompleteContext,
        ON_BYTES_RECEIVED onBytesReceived,
        void * onBytesReceivedContext,
        ON_IO_ERROR onIoError,
        void * onIoErrorContext)
{
    int result;
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;

    iprintf("Calling TLSIO Open\r\n");

    if (concreteIo == NULL)
    {
        iprintf("NULL TLS handle.\n");
        result = __FAILURE__;
        return result;
    }

    // Hook up instance to the SDK's operating environment.
    tlsioInstance->onIoOpenComplete = onIoOpenComplete;
    tlsioInstance->onIoOpenCompleteContext = onIoOpenCompleteContext;
    tlsioInstance->onBytesReceived = onBytesReceived;
    tlsioInstance->onBytesReceivedContext = onBytesReceivedContext;
    tlsioInstance->onIoError = onIoError;
    tlsioInstance->onIoErrorContext = onIoErrorContext;


    iprintf("Opening an SSL socket with state: %d\r\n", tlsioInstance->state);

    // Check to see if
    switch (tlsioInstance->state)
    {
    case TLSIO_NETBURNER_STATE_ERROR:
    case TLSIO_NETBURNER_STATE_OPENING:
    case TLSIO_NETBURNER_STATE_OPEN:
    case TLSIO_NETBURNER_STATE_CLOSING:
        iprintf("Try to open a connection with an already opened TLS.\n");
        tlsioInstance->state = TLSIO_NETBURNER_STATE_ERROR;
        result = __FAILURE__;
        break;
    case TLSIO_NETBURNER_STATE_CLOSED: // Being the "0" value, this the default value for newly created instances.
    	iprintf("Connecting to %I on port %d\r\n", tlsioInstance->remoteAddr, tlsioInstance->remotePort );
        tlsioInstance->fds = SSL_connect(tlsioInstance->remoteAddr, 0, tlsioInstance->remotePort, 10000, tlsioInstance->remoteHost, false, true);
        iprintf("Result: %d\r\n", tlsioInstance->fds );
        if (tlsioInstance->fds > 0)
        {
            tlsioInstance->state = TLSIO_NETBURNER_STATE_OPENING;
            result = 0;
        }
        else
        {
            iprintf("TLS failed to start the connection process. %d\n", tlsioInstance->fds);
            tlsioInstance->state = TLSIO_NETBURNER_STATE_ERROR;
            result = __FAILURE__;
        }
        break;
    default:
        iprintf("It's NULL?\n");
        // The NULL value is not defined, nor used, and so has undefined behavior.
        result = __FAILURE__;
        break;
    }

    iprintf("Result of opening: %d\n", result);
    if (result != 0)
    {
        iprintf("Result is not 0\n");
        if (onIoOpenComplete != NULL)
        {
            iprintf("onOpenComplete (but is an error)\n");
            (void)onIoOpenComplete(onIoOpenCompleteContext, IO_OPEN_ERROR);
        }

        if (onIoError != NULL)
        {
            (void)onIoError(onIoErrorContext);
        }
    }
    else
    {
        // I think this should not be called immediately if the connection is really open right away. I think the
        //  Arduino library must return false the first time it is called, otherwise the transport_data->mqttClientStatus
        //  in InitializeConnection gets set to CONNECTING after it is set CONNECTED. Race condition in Arduino.
        tlsioNetburnerDowork(concreteIo);
    }
    return result;
}

int tlsioNetburnerClose(
        CONCRETE_IO_HANDLE concreteIo,
        ON_IO_CLOSE_COMPLETE onIoCloseComplete,
        void* onIoCloseCompleteContext)
{
    int result;
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;

    if (concreteIo == NULL)
    {
        iprintf("NULL TLS handle.\n");
        result = __FAILURE__;
        return result;
    }

    tlsioInstance->onIoCloseComplete = onIoCloseComplete;
    tlsioInstance->onIoCloseCompleteContext = onIoCloseCompleteContext;

    switch (tlsioInstance->state)
    {
    case TLSIO_NETBURNER_STATE_CLOSED:
    case TLSIO_NETBURNER_STATE_ERROR:
        tlsioInstance->state = TLSIO_NETBURNER_STATE_CLOSED;
        result = 0;
        break;
    case TLSIO_NETBURNER_STATE_OPENING:
    case TLSIO_NETBURNER_STATE_CLOSING:
        iprintf("Try to close the connection with an already closed TLS.\n");
        tlsioInstance->state = TLSIO_NETBURNER_STATE_ERROR;
        result = __FAILURE__;
        break;
    case TLSIO_NETBURNER_STATE_OPEN:
        close(tlsioInstance->fds);
        tlsioInstance->state = TLSIO_NETBURNER_STATE_CLOSED;
        result = 0;
        tlsioNetburnerDowork(concreteIo);
        break;
    default:
        // The state defined as NULL in the original source code has undefined behavior, and that state is never used.
        result = __FAILURE__;
        break;
    }

    return result;
}

int tlsioNetburnerSend(
        CONCRETE_IO_HANDLE concreteIo,
        const void * buffer,
        size_t size,
        ON_SEND_COMPLETE onSendComplete,
        void* callbackContext)
{
    iprintf("Writing something\n");
    int result;
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;

    if ((concreteIo == NULL) || (buffer == NULL) || (size == 0))
    {
        iprintf("Invalid parameter\n");
        result = __FAILURE__;
        return result;
    }

    switch (tlsioInstance->state)
    {
    case TLSIO_NETBURNER_STATE_OPEN:
    {
        size_t sendResult;
        size_t sendSize = size;
        const char * runBuffer = (const char *)buffer;
        iprintf("Write Bytes: %d to %d", size, tlsioInstance->fds);
        for(int i = 0; i < size; i++) {
            if (i % 20 == 0) iprintf("\n\t");
            iprintf(" %02x", runBuffer[i]);
        }
        iprintf("\n\n");
        result = __FAILURE__;
        while (sendSize > 0)
        {
            sendResult = write(tlsioInstance->fds, runBuffer, sendSize);

            if (sendResult == 0)
            {
                iprintf("TLS failed sending data\n");
                if (onSendComplete != NULL)
                {
                    onSendComplete(callbackContext, IO_SEND_ERROR);
                }
                sendSize = 0;
            }
            else if (sendResult >= sendSize)
            {
                if (onSendComplete != NULL)
                {
                    onSendComplete(callbackContext, IO_SEND_OK);
                }
                result = 0;
                sendSize = 0;
            }
            else
            {
                runBuffer += sendResult;
                sendSize -= sendResult;
            }
        }
        break;
    }
    case TLSIO_NETBURNER_STATE_OPENING:
    case TLSIO_NETBURNER_STATE_CLOSED:
    case TLSIO_NETBURNER_STATE_CLOSING:
    case TLSIO_NETBURNER_STATE_NULL:
    default:
        iprintf("TLS is not ready to send data\n");
        result = __FAILURE__;
    }

    return result;
}

void tlsioNetburnerDowork(CONCRETE_IO_HANDLE concreteIo)
{
    iprintf("Starting Dowork\n");
    if (concreteIo == NULL)
    {
        iprintf("Invalid parameter\n");
        return;
    }

    int received;
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;
    char RecvBuffer[RECEIVE_BUFFER_SIZE];

    // With the deprecation of OPENING and CLOSING in this implementation, this block really only serves to filter calls
    //   to the dowork function that happen at other times in the lifecycle.
    iprintf("State: %d\n", tlsioInstance->state);
    switch (tlsioInstance->state)
    {
    case TLSIO_NETBURNER_STATE_OPEN:
        //while ((received = SSLReadWithTimeout(tlsioInstance->fds, RecvBuffer, RECEIVE_BUFFER_SIZE, RECEIVE_TIMEOUT)) > 0)
    	while ((received = ReadWithTimeout(tlsioInstance->fds, RecvBuffer, RECEIVE_BUFFER_SIZE, RECEIVE_TIMEOUT)) > 0)
        {
            iprintf("Read Bytes: %d", received);
            for(int i = 0; i < received; i++) {
                if (i % 20 == 0) iprintf("\n\t");
                iprintf(" %02x", RecvBuffer[i]);
            }
            iprintf("\n\n");
            if (tlsioInstance->onBytesReceived != NULL)
            {
                iprintf("Found onBytesReceived\n");
                (void)tlsioInstance->onBytesReceived(tlsioInstance->onBytesReceivedContext, (const unsigned char*)RecvBuffer, received);
            }
        }
        switch (received)
        {
        case TCP_ERR_TIMEOUT:
            // This is acceptable; if there was a timeout, it just means that there was no data waiting this cycle.
            iprintf("Timeout looking for new bytes.\n");
            break;
        case TCP_ERR_CLOSING:
            tlsioInstance->state = TLSIO_NETBURNER_STATE_ERROR;
            iprintf("SSL closed the connection.\n");
            CallErrorCallback();
            break;
        default:
            iprintf("Received: %d\n", received);
        }
        break;
    case TLSIO_NETBURNER_STATE_OPENING:
        tlsioInstance->state = TLSIO_NETBURNER_STATE_OPEN;
        if (tlsioInstance->onIoOpenComplete != NULL) {
            tlsioInstance->onIoOpenComplete(tlsioInstance->onIoOpenCompleteContext, IO_OPEN_OK);
        }
        break;
    case TLSIO_NETBURNER_STATE_CLOSING:
    case TLSIO_NETBURNER_STATE_CLOSED:
    case TLSIO_NETBURNER_STATE_ERROR:
    default:
        break;
    }
}

int tlsioNetburnerSetoption(
        CONCRETE_IO_HANDLE concreteIo,
        const char * optionName,
        const void * value)
{
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;
    int result;

    if (tlsioInstance == NULL)
    {
        iprintf("NULL tlsio\n");
        result = __FAILURE__;
        return result;
    }

    TLSIO_OPTIONS_RESULT optionsResult = tlsio_options_set(&tlsioInstance->options, optionName, value);
    if (optionsResult != TLSIO_OPTIONS_RESULT_SUCCESS)
    {
        iprintf("Failed tlsio_options_set\n");
        result = __FAILURE__;
    }
    else
    {
        result = 0;
    }

    return result;
}

OPTIONHANDLER_HANDLE tlsioNetburnerRetrieveOptions(CONCRETE_IO_HANDLE concreteIo)
{
    NetBurnerTLS * tlsioInstance = (NetBurnerTLS*)concreteIo;
    OPTIONHANDLER_HANDLE result;
    if (tlsioInstance == NULL)
    {
        iprintf("NULL tlsio\n");
        result = NULL;
        return result;
    }

    result = tlsio_options_retrieve_options(&tlsioInstance->options, tlsioNetburnerSetoption);

    return result;
}

extern const IO_INTERFACE_DESCRIPTION tlsio_handle_interface_description =
{
    tlsioNetburnerRetrieveOptions,
    tlsioNetburnerCreate,
    tlsioNetburnerDestroy,
    tlsioNetburnerOpen,
    tlsioNetburnerClose,
    tlsioNetburnerSend,
    tlsioNetburnerDowork,
    tlsioNetburnerSetoption
};
