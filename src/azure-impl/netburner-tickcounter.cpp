/*
 * netburner-tickcounter.cpp
 *
 *  Created on: Sep 3, 2018
 *      Author: jonca
 */

#include <stdlib.h>
#include <nbrtos.h>

#include <stdio.h>

#define __FAILURE__ -1

struct TICK_COUNTER_INSTANCE_TAG {
    tick_t startTick;
};

#include <azure_c_shared_utility/tickcounter.h>

#define MILLISECONDS_PER_TICK 1000 / TICKS_PER_SECOND

/**
 * Creates an instance of the tick counter.
 *
 * Returns either a pointer to a newly created tick counter, or NULL.
 */
TICK_COUNTER_HANDLE tickcounter_create()
{
    TICK_COUNTER_HANDLE newHandle = (TICK_COUNTER_HANDLE)malloc(sizeof(TICK_COUNTER_INSTANCE_TAG));
    if (newHandle == NULL)
    {
        iprintf("Error: Failed to create tick counter.");
    }
    else
    {
        newHandle->startTick = TimeTick;
    }

    return newHandle;
}

void tickcounter_destroy(TICK_COUNTER_HANDLE tick_counter)
{
    if (tick_counter != NULL)
    {
        free(tick_counter);
    }
}

int tickcounter_get_current_ms(TICK_COUNTER_HANDLE tick_counter, tickcounter_ms_t* current_ms)
{
    int result;

    if (tick_counter == NULL || current_ms == NULL)
    {
        iprintf("Invalid arguments.\n");
        result = __FAILURE__;
    }
    else
    {
        tick_t currentTime = TimeTick;
        *current_ms = (currentTime - tick_counter->startTick) * MILLISECONDS_PER_TICK;
        result = 0;
    }
    return result;
}
