/*
 * netburner-sleep.cpp
 *
 *  Created on: Sep 3, 2018
 *      Author: jonca
 */

#include <nbrtos.h>
#include <azure_c_shared_utility/threadapi.h>

#define MILLISECONDS_PER_TICK 1000 / TICKS_PER_SECOND

void ThreadAPI_Sleep(unsigned int milliseconds)
{
    OSTimeDly(milliseconds / MILLISECONDS_PER_TICK);
}
