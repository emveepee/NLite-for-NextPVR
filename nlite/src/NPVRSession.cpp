#include "stdafx.h"
#include "NPVRSession.h"
#include "Utility.h"

#ifdef WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h> 
#endif

#include <curl/curl.h>
#include "tinyxml2.h"
#include "CurlHelper.h"
#include "MD5.h"


#ifdef WIN32
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#else
const int INVALID_SOCKET = -1;
#define SOCKADDR sockaddr
#endif

NPVRSession::NPVRSession(void)
{
	memset(serverAddress, 0, sizeof(serverAddress));
	memset(serverMD5, 0, sizeof(serverMD5));
	memset(sid, 0, sizeof(sid));

#ifdef WIN32
	// init winsock
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}


NPVRSession::~NPVRSession(void)
{
}

void NPVRSession::SetServer(char *server)
{
	char *pch = strtok(server, ":");
	int partIndex = 0;
	while (pch != NULL)
	{
		if (partIndex == 0)
		{
			strcpy(serverAddress, pch);
		}
		else if (partIndex == 1)
		{
			serverPort = atoi(pch);
		}
		else if (partIndex == 2)
		{
			char password[32];
			strcpy(password, pch);				

			// calculate login md5
			unsigned char digest[16];
			MD5_CTX md5Context;
			MD5_Init(&md5Context);
			MD5_Update(&md5Context, password, strlen(password));
			MD5_Final(digest, &md5Context);

			// convert md5 to hex string
			static const char hexchars[] = "0123456789ABCDEF";
			char md5[128];	
			for (int i = 0; i < 16; i++)
			{
				unsigned char b = digest[i];

				md5[(i * 2) + 0] = hexchars[b >> 4];
				md5[(i * 2) + 1] = hexchars[b & 0xF];
				md5[(i * 2) + 2] = 0;
			}

			strcpy(serverMD5, md5);			
			strlwr(serverMD5);
		}
		pch = strtok (NULL, ":");
		partIndex ++;
	}
}

#if defined(WIN32)
// convert a numeric IP address into its string representation
static void Inet_NtoA(unsigned int addr, char * ipbuf)
{
   sprintf(ipbuf, "%li.%li.%li.%li", (addr>>24)&0xFF, (addr>>16)&0xFF, (addr>>8)&0xFF, (addr>>0)&0xFF);
}

// convert a string represenation of an IP address into its numeric equivalent
static unsigned int Inet_AtoN(const char * buf)
{
   // net_server inexplicably doesn't have this function; so I'll just fake it
   unsigned int ret = 0;
   int shift = 24;  // fill out the MSB first
   bool startQuad = true;
   while((shift >= 0)&&(*buf))
   {
      if (startQuad)
      {
         unsigned char quad = (unsigned char) atoi(buf);
         ret |= (((unsigned int)quad) << shift);
         shift -= 8;
      }
      startQuad = (*buf == '.');
      buf++;
   }
   return ret;
}


unsigned int Broadcast(SOCKET sock, const char *buffer, int bufferSize)
{
	unsigned int broadcastAddress = INADDR_BROADCAST;
   // implementation of multi-home broadcast sourced from http://developerweb.net/viewtopic.php?id=5085

   // Adapted from example code at http://msdn2.microsoft.com/en-us/library/aa365917.aspx
   // Now get Windows' IPv4 addresses table.  Once again, we gotta call GetIpAddrTable()
   // multiple times in order to deal with potential race conditions properly.
   MIB_IPADDRTABLE * ipTable = NULL;
   {
      ULONG bufLen = 0;
      for (int i=0; i<5; i++)
      {
         DWORD ipRet = GetIpAddrTable(ipTable, &bufLen, false);
         if (ipRet == ERROR_INSUFFICIENT_BUFFER)
         {
            free(ipTable);  // in case we had previously allocated it
            ipTable = (MIB_IPADDRTABLE *) malloc(bufLen);
         }
         else if (ipRet == NO_ERROR) break;
         else
         {
            free(ipTable);
            ipTable = NULL;
            break;
         }
     }
   }

   if (ipTable)
   {
      // Try to get the Adapters-info table, so we can given useful names to the IP
      // addresses we are returning.  Gotta call GetAdaptersInfo() up to 5 times to handle
      // the potential race condition between the size-query call and the get-data call.
      // I love a well-designed API :^P
      IP_ADAPTER_INFO * pAdapterInfo = NULL;
      {
         ULONG bufLen = 0;
         for (int i=0; i<5; i++)
         {
            DWORD apRet = GetAdaptersInfo(pAdapterInfo, &bufLen);
            if (apRet == ERROR_BUFFER_OVERFLOW)
            {
               free(pAdapterInfo);  // in case we had previously allocated it
               pAdapterInfo = (IP_ADAPTER_INFO *) malloc(bufLen);
            }
            else if (apRet == ERROR_SUCCESS) break;
            else
            {
               free(pAdapterInfo);
               pAdapterInfo = NULL;
               break;
            }
         }
      }

      for (DWORD i=0; i<ipTable->dwNumEntries; i++)
      {
         const MIB_IPADDRROW & row = ipTable->table[i];

         // Now lookup the appropriate adaptor-name in the pAdaptorInfos, if we can find it
         const char * name = NULL;
         const char * desc = NULL;
         if (pAdapterInfo)
         {
            IP_ADAPTER_INFO * next = pAdapterInfo;
            while((next)&&(name==NULL))
            {
               IP_ADDR_STRING * ipAddr = &next->IpAddressList;
               while(ipAddr)
               {
                  if (Inet_AtoN(ipAddr->IpAddress.String) == ntohl(row.dwAddr))
                  {
                     name = next->AdapterName;
                     desc = next->Description;
                     break;
                  }
                  ipAddr = ipAddr->Next;
               }
               next = next->Next;
            }
         }
         char buf[128];
         if (name == NULL)
         {
            sprintf(buf, "unnamed-%i", i);
            name = buf;
         }

         unsigned int ipAddr  = ntohl(row.dwAddr);
         unsigned int netmask = ntohl(row.dwMask);
         unsigned int baddr   = ipAddr & netmask;
         if (row.dwBCastAddr) baddr |= ~netmask;


		 if (ipAddr != 0x7f000001)
		 {
			 char ifaAddrStr[32];  Inet_NtoA(ipAddr,  ifaAddrStr);
			 char maskAddrStr[32]; Inet_NtoA(netmask, maskAddrStr);
			 char dstAddrStr[32];  Inet_NtoA(baddr,   dstAddrStr);
			 printf("  Found interface:  name=[%s] desc=[%s] address=[%s] netmask=[%s] broadcastAddr=[%s]\n", name, desc?desc:"unavailable", ifaAddrStr, maskAddrStr, dstAddrStr);
			 
			 const int NEXTPVR_LOCATOR_PORT = 16891;

			 sockaddr_in add;
			 memset((char *) &add, 0, sizeof(add));	
			 add.sin_family = AF_INET;
			 add.sin_addr.s_addr = ntohl(baddr);
			 add.sin_port = htons(NEXTPVR_LOCATOR_PORT);

			 int ret = sendto(sock, (const char *)buffer, sizeof(buffer), 0, reinterpret_cast<SOCKADDR *>(&add), sizeof(add));
			 printf("  Locator sent to broadcastAddr=[%s]\n", dstAddrStr);
			 //broadcastAddress = baddr;
		 }
      }

      free(pAdapterInfo);
      free(ipTable);
   }

   return broadcastAddress;
}
#endif




bool NPVRSession::FindServer()
{
	bool foundServer = false;
	if (strlen(serverMD5) > 0)
	{
		sprintf(baseURL, "http://%s:%d", serverAddress, serverPort);
		if (InitiateSession() == true)
		{
			LoginSession();
			foundServer = true;
		}
	}
	else
	{
		const int NEXTPVR_LOCATOR_PORT = 16891;

#ifdef WIN32
		SOCKET sock;
#else
		int sock;
#endif
		sockaddr_in add;

		// create socket
		sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (sock == INVALID_SOCKET)
		{
			printf("Error opening socket");        
			return false;
		}

		int yes = 1;
		setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (const char *)&yes, sizeof(yes));


		memset((char *) &add, 0, sizeof(add));	
		add.sin_family = AF_INET;
		add.sin_addr.s_addr = INADDR_BROADCAST;
		add.sin_port = htons(NEXTPVR_LOCATOR_PORT);

		// construct send buffer, also used for receiving response
		char buffer[512];
		strcpy(buffer, "NextPVR, you listening?");

		// broadcast request to server
#ifdef WIN32
		Broadcast(sock, (const char *)buffer, sizeof(buffer));
		//int ret = sendto(sock, (const char *)buffer, sizeof(buffer), 0, reinterpret_cast<SOCKADDR *>(&add), sizeof(add));		
		int ret = 1;
#else
		int ret = sendto(sock, (const char *)buffer, sizeof(buffer), 0, reinterpret_cast<SOCKADDR *>(&add), sizeof(add));
#endif
		if (ret > 0)
		{      
#ifdef WIN32
			int nTimeout = 5000; // 5 seconds tieout
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nTimeout, sizeof(int));

			int serverlen = sizeof(add);			
#else
			struct timeval tv;
			tv.tv_sec = 5;  // 5 seconds timeout 
			tv.tv_usec = 0;
			setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(struct timeval));

			socklen_t serverlen = sizeof(add);
#endif		
			ret = recvfrom(sock, (char *)buffer, sizeof(buffer), 0, reinterpret_cast<SOCKADDR *>(&add), &serverlen);
			if (ret > 0)
			{
				char *pch = strtok(buffer, ":");
				int partIndex = 0;
				while (pch != NULL)
				{
					if (partIndex == 0)
					{
						strcpy(serverAddress, pch);
					}
					else if (partIndex == 1)
					{
						serverPort = atoi(pch);
					}
					else if (partIndex == 2)
					{
						strcpy(serverMD5, pch);	
						strlwr(serverMD5);
					}
					pch = strtok (NULL, ":");
					partIndex ++;
				}
							
				sprintf(baseURL, "http://%s:%d", serverAddress, serverPort);
				if (InitiateSession() == true)
				{
					LoginSession();
					foundServer = true;
				}
			}
			else
			{
				printf("Failed to discover any NextPVR servers on the local network...\n");
			}
		}

		// close socket
#ifdef WIN32
		closesocket(sock);	
#else
		close(sock);
#endif
	}

	return foundServer;
}

void NPVRSession::GetBaseURL(char *buf, int len)
{
	//sprintf_s(buf, len, "http://%s:%d", serverAddress, serverPort);
	sprintf(buf, "http://%s:%d", serverAddress, serverPort);
}

bool NPVRSession::InitiateSession()
{
	char url[2048];
	sprintf(url, "%s/services/service?method=session.initiate&ver=1.0&device=sdl", baseURL);	

	// check if any action was requested
	struct MemoryStruct responseChunk; 
	responseChunk.memory = (unsigned char *)malloc(1);  /* will be grown as needed by the realloc above */ 
	responseChunk.size = 0;    /* no data at this point */ 
	DownloadURL(url, &responseChunk);
	
	// load xml dom
	tinyxml2::XMLDocument doc;
    doc.Parse((const char *)responseChunk.memory);

	tinyxml2::XMLElement *responseNode = doc.FirstChildElement("rsp");
	if (responseNode != NULL)
	{
		tinyxml2::XMLElement *saltNode = responseNode->FirstChildElement("salt");
		strcpy(this->salt, saltNode->GetText());		

		tinyxml2::XMLElement *sidNode = responseNode->FirstChildElement("sid");
		strcpy(this->sid, sidNode->GetText());
	}

	// release memory
	free(responseChunk.memory);

	return true;
}

bool NPVRSession::LoginSession()
{
	char combined[1024];
	sprintf(combined, ":%s:%s", serverMD5, salt);

	// calculate login md5
	unsigned char digest[16];
	MD5_CTX md5Context;
	MD5_Init(&md5Context);
	MD5_Update(&md5Context, combined, strlen(combined));
	MD5_Final(digest, &md5Context);

	// convert md5 to hex string
	static const char hexchars[] = "0123456789ABCDEF";
	char md5[128];	
	for (int i = 0; i < 16; i++)
	{
	    unsigned char b = digest[i];

		md5[(i * 2) + 0] = hexchars[b >> 4];
		md5[(i * 2) + 1] = hexchars[b & 0xF];
		md5[(i * 2) + 2] = 0;
	}

	// construct url
	char url[2048];
	sprintf(url, "%s/services/service?method=session.login&sid=%s&md5=%s", baseURL, sid, md5);	

	// do request
	struct MemoryStruct responseChunk; 
	responseChunk.memory = (unsigned char *)malloc(1);  /* will be grown as needed by the realloc above */ 
	responseChunk.size = 0;    /* no data at this point */ 
	DownloadURL(url, &responseChunk);

	// a bit of hack...you didn't see this...
	if (strstr((const char *)responseChunk.memory, "stat=\"ok\"") != NULL)
	{
		return true;
	}

	printf("Found server, but login failed\n");

	return false;
}


void NPVRSession::GetSID(char *buf, int len)
{
	sprintf(buf, "%s", sid);
}