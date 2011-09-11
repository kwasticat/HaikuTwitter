/*
 * Copyright 2011 Martin Hebnes Pedersen, martinhpedersen @ "google mail"
 * All rights reserved. Distributed under the terms of the MIT License.
 */ 

#include "HTTimelineParser.h"

#include <string>
#include <iostream>
#include <sstream>
#include <List.h>

using namespace std;

extern "C" size_t decode_html_entities_utf8(char *dest, const char *src);

//Define some constants for parsing
namespace TwitterTags {
	const char* ENTRY_TAG 			= "<status>";
	const char* WHEN_TAG			= "<created_at>";
	const char* CONTENT_TAG			= "<text>";
	const char* USERNAME_TAG		= "<screen_name>";
	const char* REALNAME_TAG	 	= "<name>";
	const char* SOURCE_TAG			= "<source>";
	const char* ID_TAG				= "<id>";
	const char* FOLLOW_TAG			= "<following>";
	const char* PROFILEIMAGEURL_TAG	= "<profile_image_url>";
}

//Define some constants for using the twitter api
namespace TwitterAPI {	
	const string TIME_FORMAT			= "%*s %s %d %d:%d:%d %d %d";//Sat Sep 10 10:17:38 +0000 2011
	const string ID_FORMAT				= "tag:search.twitter.com,2005:%llu";//tag:search.twitter.com,2005:50093060641656832
}

HTTimelineParser::HTTimelineParser()
{
   fTweets = new BList();
}

HTTimelineParser::~HTTimelineParser()
{
	while(!fTweets->IsEmpty())
		delete (HTTweet *)fTweets->RemoveItem((int32)0);
	delete fTweets;
}

BList*
HTTimelineParser::Tweets() {
	
	return fTweets;
}

status_t
HTTimelineParser::Parse(const std::string& data)
{    
    status_t status = B_OK;
	
	if(data.length() < 30)
		return B_ERROR;
	
	//Devide data into nodes.
	BList* nodeList = new BList();
	string buffer("");
	size_t pos = 0;
	while(true) {
		pos = FindValue(&buffer, TwitterTags::ENTRY_TAG, data, pos, false); //We don't want to bother with decoding html right now
		if(pos == string::npos)
			break;
		nodeList->AddItem(new string(buffer));
	}
	if(nodeList->IsEmpty())	
		return B_OK;
	
	//Parse nodes
	status = _ParseNodes(nodeList, fTweets);
	
	//Delete all nodes
	while(!nodeList->IsEmpty())
		delete (string *)nodeList->RemoveItem((int32)0);
	delete nodeList;
	
	return status;
}

status_t
HTTimelineParser::_ParseNodes(BList* nodeList, BList* resultList)
{
	status_t status = B_OK;
	
	string* parsingNode;
	HTTweet* currentTweet;
	string buffer("");
	
	while(!nodeList->IsEmpty()) {
		status = B_OK;
		buffer = string("");
		parsingNode = (string *)nodeList->RemoveItem((int32)0);
		if(parsingNode == NULL)
			return B_BAD_INDEX;	
		currentTweet = new HTTweet();
				
		//Content
		if(FindValue(&buffer, TwitterTags::CONTENT_TAG, *parsingNode, 0) == string::npos)
			status = B_ERROR;
		else
			currentTweet->setText(buffer);
			
		//Screen name
		if(status == B_OK && FindValue(&buffer, TwitterTags::USERNAME_TAG, *parsingNode, 0) == string::npos)
			status = B_ERROR;
		else
			currentTweet->setScreenName(buffer);
			
		//Real name
		if(status == B_OK && FindValue(&buffer, TwitterTags::REALNAME_TAG, *parsingNode, 0) == string::npos)
			status = B_ERROR;
		else
			currentTweet->setFullName(buffer);
			
		//Profile image url
		if(status == B_OK && FindValue(&buffer, TwitterTags::PROFILEIMAGEURL_TAG, *parsingNode, 0) == string::npos, false)
			status = B_ERROR;
		else
			currentTweet->setProfileImageUrl(buffer);
		
		//When
		if(status == B_OK && FindValue(&buffer, TwitterTags::WHEN_TAG, *parsingNode, 0) == string::npos)
			status = B_ERROR;
		else
			currentTweet->setDate( _StrToTime(buffer.c_str()) );
			
		//Follow
		if(status == B_OK && FindValue(&buffer, TwitterTags::FOLLOW_TAG, *parsingNode, 0) == string::npos, false)
			status = B_ERROR;
		else
			currentTweet->setFollowing(buffer.find("true") != string::npos);
		
		//Source
		if(status == B_OK && FindValue(&buffer, TwitterTags::SOURCE_TAG, *parsingNode, 0) == string::npos)
			status = B_ERROR;
		else {
			// Parse the data for Application name
            int pos = buffer.find(">", 0); //<a href="http://www.tweetdeck.com/" rel="nofollow">TweetDeck</a>
			if(pos != std::string::npos) {
				int start = pos;
				int end = pos;
				while(end < buffer.length() && buffer[end] != '<') {
					end++;
				}
				string sourceName = buffer.substr(start+1, end-start-1);
				currentTweet->setSourceName(sourceName.c_str());
			}
			else
				currentTweet->setSourceName(buffer.c_str());
		}
			
		//Tweet id
		if(status == B_OK && FindValue(&buffer, TwitterTags::ID_TAG, *parsingNode, 0) == string::npos, false)
			status = B_ERROR;
		else
			currentTweet->setId(buffer.c_str());
		
		if(status == B_OK)
			resultList->AddItem(currentTweet);
		else
			delete currentTweet;
	}
	
	return status;
}

//Convert from Twitter format to time_t
time_t
HTTimelineParser::_StrToTime(const char* str)
{	
	struct tm when;
	int32 yyyy=0, mm=0, dd=0, hour=0, min=0, sec=0, off=0;
	const char buffer[4] = "";
	
	sscanf(str, TwitterAPI::TIME_FORMAT.c_str(), &buffer, &dd, &hour, &min, &sec, &off, &yyyy);
	
	mm = _StrToMonth(buffer);
		
	when.tm_year = yyyy-1900;	//tm_year is year since 1900
	when.tm_mon = mm;			//tm_mon range:		0-11
	when.tm_mday = dd;			//tm_mday range:	1-31
	when.tm_hour = hour+off;	//tm_hour range:	0-23
	when.tm_min = min;			//tm_min range:		0-59
	when.tm_sec = sec;			//tm_sec range:		0-59
		
	return mktime(&when);
}

int32
HTTimelineParser::_StrToMonth(const char *date) const
{
	//jan feb mar apr may jun jul aug sep oct nov dec
	if(strncmp(date, "Jan", 3) == 0)
		return 0;
	if(strncmp(date, "Feb", 3) == 0)
		return 1;
	if(strncmp(date, "Mar", 3) == 0)
		return 2;
	if(strncmp(date, "Apr", 3) == 0)
		return 3;
	if(strncmp(date, "May", 3) == 0)
		return 4;
	if(strncmp(date, "Jun", 3) == 0)
		return 5;
	if(strncmp(date, "Jul", 3) == 0)
		return 6;
	if(strncmp(date, "Aug", 3) == 0)
		return 7;
	if(strncmp(date, "Sep", 3) == 0)
		return 8;
	if(strncmp(date, "Oct", 3) == 0)
		return 9;
	if(strncmp(date, "Nov", 3) == 0)
		return 10;
	if(strncmp(date, "Dec", 3) == 0)
		return 11;
	#ifdef DEBUG_ENABLED
	std::cout << "HTTweet::stringToMonth: Failed" << std::endl;
	#endif
	return 0;
}

//Convert from Twitter id string to uint64
/*uint64
HTTimelineParser::_StrToId(const char* str)
{
	uint64 id = 0;
		
	sscanf(str, TwitterAPI::ID_FORMAT.c_str(), &id);
		
	return id;
}*/

size_t
HTTimelineParser::FindValue(std::string* buffer, const char* tag, const std::string& data, size_t pos, bool decodeHtml)
{
	std::string ourBuffer;
	
	std::string endTag(tag);
	endTag.insert(1, "/");
	
	size_t start = data.find(tag, pos);
	size_t end;
	if(start == std::string::npos)
		return std::string::npos;
		
	start += strlen(tag);
	end = data.find(endTag, start);
	
	if(end != std::string::npos)
		ourBuffer = data.substr(start, end-start).c_str();
		
	if(decodeHtml) {
		char decodeBuffer[1024];
		decode_html_entities_utf8(&decodeBuffer[0], ourBuffer.c_str());
		*buffer = std::string(&decodeBuffer[0]);
	}
	else
		*buffer = std::string(ourBuffer);

	return end;
}