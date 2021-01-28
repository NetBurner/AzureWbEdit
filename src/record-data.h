/*
 * temperature-data.h
 *
 *  Created on: Aug 8, 2018
 *      Author: jonca
 */

#ifndef RECORD_DATA_H_
#define RECORD_DATA_H_

#include <json_lexer.h>

#define COMMAND_DATA_LEN 50

/**
 * @brief Defines a struct used to track our postId and time information.
 */
typedef struct PostRecordS
{
	char* postTime;
    int postId = 0;
} PostRecord;

/**
 * @brief Builds our MQTT message using a JSON data set.
 */
void SerializeRecordJson(PostRecord &record, ParsedJsonDataSet &json);

enum CommandType
{
	NONE = 0,
	SET_IS_COLLECTING
};

typedef struct ResponseS
{
	CommandType type = NONE;
	char data[COMMAND_DATA_LEN];
} Response;

void DeserializeResponseJson(Response &comm, ParsedJsonDataSet &json);

#endif /* RECORD_DATA_H_ */
