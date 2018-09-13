#pragma once



struct MemoryStruct {
  unsigned char *memory;
  size_t size;  
};


bool DownloadURL(char *url, MemoryStruct *chunk);
void EscapeString(const char *source, int sourceLength, char *dest, int destLength);

