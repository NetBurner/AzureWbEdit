/*
 * netburner-platform.cpp
 *
 *  Created on: Sep 3, 2018
 *      Author: jonca
 */

#include <azure_c_shared_utility/platform.h>
#include <azure_c_shared_utility/lock.h>
#include <azure_c_shared_utility/socketio.h>
#include <azure_c_shared_utility/threadapi.h>

extern const IO_INTERFACE_DESCRIPTION tlsio_handle_interface_description;

int platform_init()
{
    return 0;
}

void platform_deinit()
{

}

const IO_INTERFACE_DESCRIPTION* platform_get_default_tlsio()
{
    return &tlsio_handle_interface_description;
}

STRING_HANDLE platform_get_platform_info(PLATFORM_INFO_OPTION option)
{
    return STRING_construct_sprintf("NetBurner Device.");
}

LOCK_HANDLE Lock_Init()
{
	return (void*)1;
}

LOCK_RESULT Lock(LOCK_HANDLE lock)
{
	return LOCK_OK;
}


LOCK_RESULT Unlock(LOCK_HANDLE lock)
{
	return LOCK_OK;
}

const IO_INTERFACE_DESCRIPTION* socketio_get_interface_description()
{
	return (const IO_INTERFACE_DESCRIPTION*)1;
}

THREADAPI_RESULT ThreadAPI_Create( THREAD_HANDLE* handle, THREAD_START_FUNC func, void* arg)
{
	return THREADAPI_OK;
}

void ThreadAPI_Exit( int res )
{
	return;
}
