#include "stdafx.h"
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include "CurlHelper.h"

CURL *curl = NULL;


static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
  size_t realsize = size * nmemb;
  struct MemoryStruct *mem = (struct MemoryStruct *)userp;
 
  mem->memory = (unsigned char *)realloc(mem->memory, mem->size + realsize + 1);
  if(mem->memory == NULL) {
    printf("not enough memory (realloc returned NULL)\n");
    return 0;
  }
 
  memcpy(&(mem->memory[mem->size]), contents, realsize);
  mem->size += realsize;
  mem->memory[mem->size] = 0;

  return realsize;
}

bool DownloadURL(char *url, MemoryStruct *chunk)
{	
	CURLcode res; 
	if (curl == NULL)
		curl = curl_easy_init();
			
	curl_easy_setopt(curl, CURLOPT_URL, url);

	curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 3000); 	
 
	// set callback for receiving data
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
	// pass our 'chunk' struct to the callback function 
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)chunk);

	//curl_easy_setopt(curl, CURLOPT_FAILONERROR, true);

	printf("curl_easy_perform(%s)\n", url);

	// do request
	res = curl_easy_perform(curl);

	// log status code if not 200
	long http_code = 0;
	curl_easy_getinfo (curl, CURLINFO_RESPONSE_CODE, &http_code);
	if (http_code != 200)
	{
		printf("HTTP status: %d\n", http_code);
	}
	
	// check for any errors
	if(res != CURLE_OK)
	{
		fprintf(stderr, "curl_easy_perform(%s) failed: %s\n", url, curl_easy_strerror(res));
		return false;
	}
 
	/* always cleanup */ 
	//curl_easy_cleanup(curl);

	return true;
}

void EscapeString(const char *source, int sourceLength, char *dest, int destLength)
{
	memset(dest, 0, destLength);
	char *encodedMessage = curl_easy_escape(curl, source, sourceLength);
	strncpy(dest, encodedMessage, destLength);
	curl_free(encodedMessage);
}