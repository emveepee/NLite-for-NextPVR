#include "stdafx.h"
#include "MCEHelper.h"
#include <Windows.h>

unsigned int MCEHelper::GetTranslatedMCE(unsigned int lParam, unsigned int wParam)
{
	UINT dwSize = 0;
	//unsigned int temp = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, 0, &size, sizeof(rih));

	// request size of the raw input buffer to dwSize
	UINT rc = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
	if (dwSize == 0)
		return 0;

	// allocate buffer for input data
	RAWINPUT *buffer = (RAWINPUT*)HeapAlloc(GetProcessHeap(), 0, dwSize);
	memset(buffer, 0, dwSize);

	if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, buffer, &dwSize, sizeof(RAWINPUTHEADER)) > 0)
	{ 
		if(buffer->header.dwType == RIM_TYPEKEYBOARD && buffer->data.keyboard.Message == WM_KEYDOWN)
		{
			printf("test");
		}
	}

	// free the buffer	
	HeapFree(GetProcessHeap(), 0, buffer);

	//Logger.Debug("raw message: 0x" + ((raw.keyboard.Message & 0xFF00) >> 8).ToString("X"));

	return 0;
}
