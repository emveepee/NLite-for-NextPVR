#pragma once

#include "Player.h"
#include "NPVRSession.h"
#include "CurlHelper.h"

struct context 
{
    SDL_Renderer *renderer;
	SDL_Texture *menuTexture;
	SDL_Texture *videoTexture;
	//SDL_Texture *statusTexture;
    SDL_mutex *mutex;
    int n;
};

class UserInterface: public PlayerDelegate
{
public:
	UserInterface();
	int Run(int argc, char *argv[]);
	void RegularTimerCallback();

	// player delegate functions
	void StopPlayback(const char *message = NULL);	
	void PlaybackPosition(double position);		
	void ForceRefreshOSD();
	void GenerateOSDMessage(const char *message); 
	void NeedsReset();

private:
	bool NeedsRendering();
	void ProcessActivity();
	void HandleKeyboardEvent(SDL_Event *sdlEvent);
	void HandleMouseEvent(SDL_Event *sdlEvent);
	void HandleMouseMoveEvent(SDL_Event *sdlEvent);
	void HandleMouseWheelEvent(SDL_Event *sdlEvent);

private:
	struct context context;
	NPVRSession session;
	char baseURL[512];
	char clientID[96];
	char sid[128];
	SDL_Window *window;
	TTF_Font *font;
	bool shutdown;	
	bool useGPU;
	Player *videoPlayer;
	Player *lazyDeletePlayer;
	bool initComplete;
	unsigned int lastKeyPress;
	int refreshCycle;
	unsigned int lastActivityCheck;
};