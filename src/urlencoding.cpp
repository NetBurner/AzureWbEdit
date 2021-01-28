/*
 * urlencoding.cpp
 *
 *  Created on: Aug 28, 2018
 *      Author: jonca
 */

#include <stdio.h>

typedef char entry[3];

const entry map[] = {
        { '%', '2', '0' } /* ' ' */,
        { '%', '2', '1' } /* '!', Reserved */,
        { '%', '2', '2' } /* '"' */,
        { '%', '2', '3' } /* '#', Reserved */,
        { '%', '2', '4' } /* '$', Reserved */,
        { '%', '2', '5' } /* '%', Reserved(ish) */,
        { '%', '2', '6' } /* '&', Reserved */,
        { '%', '2', '7' } /* ''', Reserved */,
        { '%', '2', '8' } /* '(', Reserved */,
        { '%', '2', '9' } /* ')', Reserved */,
        { '%', '2', 'A' } /* '*', Reserved */,
        { '%', '2', 'B' } /* '+', Reserved */,
        { '%', '2', 'C' } /* ',', Reserved */,
        { '%', '2', 'F' } /* '/', Reserved */,
        { '%', '3', 'A' } /* ':', Reserved */,
        { '%', '3', 'B' } /* ';', Reserved */,
        { '%', '3', 'C' } /* '<' */,
        { '%', '3', 'D' } /* '=', Reserved */,
        { '%', '3', 'E' } /* '>' */,
        { '%', '3', 'F' } /* '?', Reserved */,
        { '%', '4', '0' } /* '@', Reserved */,
        { '%', '5', 'B' } /* '[', Reserved */,
        { '%', '5', 'C' } /* '\' */,
        { '%', '5', 'D' } /* ']', Reserved */,
        { '%', '5', 'E' } /* '^' */,
        { '%', '6', '0' } /* '`' */,
        { '%', '7', 'B' } /* '{' */,
        { '%', '7', 'C' } /* '|' */,
        { '%', '7', 'D' } /* '}' */

};

int URLEncode(const char * input, int inputLen, char * output, int maxOutputLen)
{
    int bytesWritten = 0;
    const char * maxInputPos = input + inputLen;
    //char * maxOutputPos = output + maxOutputLen;
    for (; input <= maxInputPos && (bytesWritten < maxOutputLen); input++)
    {
        if (0x00 == *input || 0x2D == *input || 0x2E == *input || 0x5F == *input || 0x7E == *input ||
                (0x30 <= *input && 0x39 >= *input) ||
                (0x41 <= *input && 0x5A >= *input) ||
                (0x61 <= *input && 0x7A >= *input))
        {
            *output = *input;
            bytesWritten++;
            output++;
        }
        else
        {
            int entryIndex = *input - 0x20;
            if ( entryIndex > 12 )
            {
                entryIndex -= 0x02;
            }

            if ( entryIndex > 13 )
            {
                entryIndex -= 0x0A;
            }

            if ( entryIndex > 20 )
            {
                entryIndex -= 0x1A;
            }

            if ( entryIndex > 24 )
            {
                entryIndex -= 0x01;
            }

            if ( entryIndex > 25 )
            {
                entryIndex -= 0x1A;
            }

            if ( entryIndex >= 0 && entryIndex < 29 )
            {
                const entry * encodingItem = &map[entryIndex];
                for (int i = 0; i < 3; i++)
                {
                    *output = (*encodingItem)[i];
                    bytesWritten++;
                    output++;
                    if (bytesWritten == maxOutputLen) break;
                }
            } else { // This branch should not be exercised, but is provided to provide for binary data, which as this is generally a poor scheme to use for binary data, is off the efficient path.
                bytesWritten += snprintf(output, 3, "%%%02x", *input);
                output++;
            }
        }
    }

    /*
    if ( bytesWritten < maxOutputLen )
    {
        *output = 0;
    }
    */

    return bytesWritten;
}
