/*
 * temperature-data.cpp
 *
 *  Created on: Aug 8, 2018
 *      Author: jonca
 */

#include <record-data.h>
#include <stdio.h>
#include <string.h>
#include <tcp.h>


void SerializeRecordJson(PostRecord &record, ParsedJsonDataSet &json)
{
    json.StartBuilding();
    json.Add("time_posted", record.postTime);
    json.Add("post_id", record.postId);
    json.DoneBuilding();
}

void DeserializeResponseJson(Response &response, ParsedJsonDataSet &json)
{
	response.type = (CommandType)json.FindGlobalNumber("command");
	json.FindGlobalElementAfterName("data");
	strncpy(response.data, json.CurrentString(), COMMAND_DATA_LEN);
}
