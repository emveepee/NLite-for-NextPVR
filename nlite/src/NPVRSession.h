#pragma once
class NPVRSession
{
public:
	NPVRSession(void);
	~NPVRSession(void);

	bool FindServer();
	void GetBaseURL(char *buf, int len);
	void GetSID(char *buf, int len);

private:
	bool InitiateSession();
	bool LoginSession();

private:
	char baseURL[512];
	char sid[128];	
	char salt[128];	

	char serverMD5[128];	
	char serverAddress[128];	
	int serverPort;	
};

