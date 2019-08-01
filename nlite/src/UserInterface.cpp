// SDLVLC.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#ifdef WIN32
#include <atlconv.h>
#include <shellapi.h>
#endif

#include <stdio.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include <assert.h>

extern "C" 
{
#include <SDL.h>
#include <SDL_mutex.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <curl/curl.h>
#ifdef WIN32
#define ssize_t size_t
#endif
#include "vlc/vlc.h"
}

#include "tinyxml2.h"

#include "NPVRSession.h"

#include "UserInterface.h"

// include the libraries we'll need
#ifdef WIN32
#define SDL_VIDEO_DRIVER_WINDOWS
#include <SDL_syswm.h>
#include "MCEHelper.h"
#pragma comment(lib, "../libcurl/libcurl.lib")
#pragma comment(lib, "../vlc/lib/libvlc.lib")
#pragma comment(lib, "../SDL2/lib/x86/SDL2.lib")
#pragma comment(lib, "../SDL2_image/lib/x86/SDL2_image.lib")
#pragma comment(lib, "../SDL2_ttf/lib/x86/SDL2_ttf.lib")
#undef main
#else
#include <unistd.h>
typedef unsigned char byte;
#endif

int WIDTH = 1280;
int HEIGHT = 720;

int VIDEOWIDTH = 1280;
int VIDEOHEIGHT = 720;



static void quit(int c) {
    SDL_Quit();
    exit(c);
}




UserInterface::UserInterface()
{	 
	videoPlayer = NULL;
	lazyDeletePlayer = NULL;
	window = NULL;
	font = NULL;	
	useGPU = false;
	initComplete = false;
	memset(&context, 0, sizeof(context));
}

bool UserInterface::NeedsRendering()
{	
	/*
	char url[2048];
	sprintf(url, "%s/activity?client=%s&updates=1&sid=%s", baseURL, clientID, sid);

	// check if any action was requested
	struct MemoryStruct activityChunk; 
	activityChunk.memory = (unsigned char *)malloc(1);  
	activityChunk.size = 0;    
	DownloadURL(url, &activityChunk);

	if (activityChunk.size > 0)
	{
		char *activity = (char *)activityChunk.memory;
		if (strstr(activity, "<needsRendering>true</needsRendering>") != NULL)
		{
			return true;
		}
	}

	return false;*/
	return ProcessActivity(true);
}

void UserInterface::GenerateOSDMessage(const char *message)
{	
	if (videoPlayer != NULL)
	{
		if (font != NULL)
		{
			SDL_Surface *text;
			SDL_Color text_color = {255, 255, 255}; 
			text = TTF_RenderText_Blended(font, message, text_color);	

			SDL_Texture *messageTexture = SDL_CreateTextureFromSurface(context.renderer, text);			
			videoPlayer->SetLocalOSDTexture(messageTexture);
		}
		else
		{
			videoPlayer->SetLocalOSDTexture(NULL);
		}
	}
}

bool UserInterface::ProcessActivity(bool updates)
{	
	char url[2048];
	sprintf(url, "%s/activity?client=%s&format=xml&sid=%s", baseURL, clientID, sid);		
	if (updates)
	{
		strcat(url, "&updates=1");
	}

	// check if any action was requested
	struct MemoryStruct activityChunk; 
	activityChunk.memory = (unsigned char *)malloc(1);  /* will be grown as needed by the realloc above */ 
	activityChunk.size = 0;    /* no data at this point */ 
	DownloadURL(url, &activityChunk);

	// if so, perform appropriate activity
	if (activityChunk.size > 0)
	{		
		printf("activity: \n%s\n", activityChunk.memory);
		
		if (updates)
		{
			if (strstr((const char *)activityChunk.memory, "<needsRendering>true</needsRendering>") != NULL)
			{
				return true;
			}
		}

		// load xml dom		
		tinyxml2::XMLDocument doc;
		doc.Parse((const char *)activityChunk.memory);

		char *activity = new char[activityChunk.size + 1];
		activity[0] = '\0';		

		// extract activity from xml
		int skipToSeconds = -1;
		bool userServerSkip = false;
		int durationSeconds = 0;
		char file[512];
		file[0] = '\0';
		tinyxml2::XMLElement *activityNode = doc.FirstChildElement("activity");
		if (activityNode != NULL)
		{
			tinyxml2::XMLElement *urlNode = activityNode->FirstChildElement("url");
			if (urlNode != NULL)
			{
				const char *urlText = urlNode->GetText();
				if (urlText != NULL)
				{
					strcpy(activity, urlText);		
				}
			}

			tinyxml2::XMLElement *actionNode = activityNode->FirstChildElement("action");
			if (actionNode != NULL)
			{
				const char *actionText = actionNode->GetText();
				if (actionText != NULL && strcmp(actionText,"exit") == 0)
				{
					strcpy(activity, actionText);		
				}
				else
				{
					printf("Ignoring action %s\n", actionText);
				}
			}

			tinyxml2::XMLElement *fileNode = activityNode->FirstChildElement("file");
			if (fileNode != NULL)
			{
				const char *urlText = fileNode->GetText();
				if (urlText != NULL)
				{
					strcpy(file, urlText);		
				}
			}

			tinyxml2::XMLElement *skipToNode = activityNode->FirstChildElement("skip_to");
			if (skipToNode != NULL)
			{
				skipToSeconds = atoi(skipToNode->GetText());
			}

			tinyxml2::XMLElement *serverSkipNode = activityNode->FirstChildElement("supports_server_skip");
			if (serverSkipNode != NULL)
			{
				userServerSkip = true;
			}

			tinyxml2::XMLElement *durationNode = activityNode->FirstChildElement("recording_duration");
			if (durationNode != NULL)
			{
				durationSeconds = atoi(durationNode->GetText());
			}
			else
			{
				tinyxml2::XMLElement *durationNode = activityNode->FirstChildElement("duration");
				if (durationNode != NULL)
				{
					durationSeconds = atoi(durationNode->GetText());
				}
			}
		}


		if (strlen(activity) > 0)
		{
			// handle 'exit'
			if (strcmp(activity, "exit") == 0)
			{
				shutdown = 1;
			}
			else if (videoPlayer != NULL && videoPlayer->UsingServerSideSkip() == false && skipToSeconds >= 0)
			{				
				videoPlayer->SkipTo(skipToSeconds, durationSeconds);
			}
			// handle media playback activities
			else
			{
				// stop any player already in progress
				if (videoPlayer != NULL)
				{
					delete videoPlayer;
					videoPlayer = NULL;
				}

				/*if (font != NULL)
				{
					SDL_Surface *text;
					SDL_Color text_color = {255, 255, 255}; 
					text = TTF_RenderText_Blended(font, "...requesting video stream...", text_color);							
					SDL_Texture *messageTexture = SDL_CreateTextureFromSurface(context.renderer, text);

					SDL_Rect rect;
					rect.x = (WIDTH / 2) - (text->w / 2);
					rect.y = (HEIGHT / 2) - (text->h / 2);
					rect.w = text->w;
					rect.h = text->h;

					// show black screen
					SDL_SetRenderDrawColor(context.renderer, 0, 0, 0, 255);
					SDL_RenderClear(context.renderer);				
					SDL_RenderCopy(context.renderer, messageTexture, NULL, &rect);
					SDL_RenderPresent(context.renderer);

					// texture no longer  
					SDL_DestroyTexture(messageTexture);						
				}
				else
				{
					// show black screen
					SDL_SetRenderDrawColor(context.renderer, 0, 0, 0, 255);
					SDL_RenderClear(context.renderer);								
					SDL_RenderPresent(context.renderer);
				}
				*/

				// get sid
				char sid[128];
				session.GetSID(sid, sizeof(sid));


				// single URL, or multiple				
				char *p = strchr(activity, '\n');
				if (p == NULL)
				{					
					// setup video playback for single url							
					char videoURL[2048];
					sprintf(videoURL, "%s%s&sid=%s", baseURL, activity, sid);

					// special case for network URLs
					if ((strstr(file, "http://") == file) || (strstr(file, "rtmp://") == file))
					{
						strcpy(videoURL, file);
					}

					// request playback
					printf("About to ask for: %s\n", videoURL);
					videoPlayer = new Player(this, videoURL, context.renderer, context.videoTexture, context.mutex, userServerSkip, durationSeconds, useGPU);
				}
				else
				{
					// setup video playback for multiple urls
					int count;
					const int maxURLs = 200;
					char url[maxURLs][512];
				
					char *pch = strtok(activity, "\n");
					int index = 0;
					while (pch != NULL)
					{
						int length = strlen(pch);						
						strcpy(url[index], pch);

						index++;
						if (index >= maxURLs)
							break;

						pch = strtok (NULL, "\n");
					}

					// if we had at least a single url
					if (index > 0)
					{
						// setup video playback for first url						
						char streamURL[2048];
						sprintf(streamURL, "%s%s&sid=%s", baseURL, activity, sid);
						printf("About to ask for: %s\n", streamURL);
						videoPlayer = new Player(this, streamURL, context.renderer, context.videoTexture, context.mutex, userServerSkip, durationSeconds, useGPU);

						// queue any extra urls
						if (index > 1)
						{
							for (int i=1; i<index; i++)
							{
								sprintf(streamURL, "%s%s&sid=%s", baseURL, url[i], sid);
								videoPlayer->QueueStream(streamURL);
							}
						}
					}
				}

				// let video player know the current window size
				if (videoPlayer != NULL)
				{
					int w = 0;
					int h = 0;
					SDL_GetWindowSize(window, &w, &h);
					videoPlayer->SetWindowSize(w, h);
				}
			}
		}

		delete []activity;
	}
	free(activityChunk.memory);
	return false;
}

void UserInterface::HandleMouseWheelEvent(SDL_Event *sdlEvent)
{
	if (sdlEvent->wheel.y < 0)
	{
		SDL_Event fakeEvent;
		fakeEvent.key.keysym.sym = SDLK_DOWN;
		HandleKeyboardEvent(&fakeEvent);
	}
	else if (sdlEvent->wheel.y > 0)
	{
		SDL_Event fakeEvent;
		fakeEvent.key.keysym.sym = SDLK_UP;
		HandleKeyboardEvent(&fakeEvent);
	}
}

void UserInterface::HandleMouseMoveEvent(SDL_Event *sdlEvent)
{	
	static Uint32 lastMouseMove = 0;
	if (lastMouseMove + 250 < SDL_GetTicks())
	{
		int w = 0;
		int h = 0;
		SDL_GetWindowSize(window, &w, &h);
		int x = (int)((100.0 / w) * sdlEvent->button.x);
		int y = (int)((100.0 / h) * sdlEvent->button.y);
		SDL_Log("click at %d,%d", x,y);

		// prepare chunk for holding jpeg download
		struct MemoryStruct imageChunk; 
		imageChunk.memory = (unsigned char *)malloc(1);  /* will be grown as needed by the realloc above */ 
		imageChunk.size = 0;    /* no data at this point */ 

		// build URL to call
		char url[2048];
		if (sdlEvent->button.clicks == 2)
		{
			sprintf(url, "%s/control?quality=high&client=%s&move=%dx%d&sid=%s", baseURL, clientID, x, y, sid);
		}
		else
		{
			sprintf(url, "%s/control?quality=high&client=%s&move=%dx%d&sid=%s", baseURL, clientID, x, y, sid);
		}

		// send the request
		DownloadURL(url, &imageChunk);

		// decode osd png
		if (imageChunk.size > 0)
		{
			SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
			SDL_Surface *image = IMG_LoadPNG_RW(rwop);
			SDL_FreeRW(rwop);

			if (image != NULL)
			{
				// adjust alpha level of OSD pixels
				int pixelBytes = ((image->w * image->h) * 4);
				byte *pixels = (byte *)image->pixels;
				for (int i=0; i<pixelBytes; i+=4)
				{	
					if (pixels[i+3] != 0)
					{
						//pixels[i+3] = 0xAF;
					}
				}

				videoPlayer->SetOSD(image); 

				// create new texture and let video player know				
				//context.osdTexture = SDL_CreateTextureFromSurface(context.renderer, image);
				//videoPlayer->SetOSDTexture(context.osdTexture);

				// no longer need surface
				SDL_FreeSurface(image);
			}
		}
		else
		{
			// clear OSD
			videoPlayer->SetOSD(NULL);
		}

		lastMouseMove = SDL_GetTicks();
	}
}

void UserInterface::NeedsReset()
{
	SDL_LockMutex(context.mutex);

	SDL_DestroyTexture(context.menuTexture);
	context.menuTexture = NULL;

	SDL_DestroyTexture(context.videoTexture);
	context.videoTexture = NULL;

	SDL_DestroyRenderer(context.renderer);
	context.renderer = NULL;

	context.renderer = SDL_CreateRenderer(window, 0, SDL_RENDERER_ACCELERATED);   
	context.videoTexture = SDL_CreateTexture(
			context.renderer,
			//SDL_PIXELFORMAT_BGR565, SDL_TEXTUREACCESS_STREAMING,
			SDL_PIXELFORMAT_YVYU, SDL_TEXTUREACCESS_STREAMING,
			VIDEOWIDTH, VIDEOHEIGHT);

	if (videoPlayer != NULL)
	{
		int w = VIDEOWIDTH;
		int h = VIDEOHEIGHT;
		SDL_GetWindowSize(window, &w, &h);
		videoPlayer->SetWindowSize(w, h);

		videoPlayer->UpdateRenderer(context.renderer, context.videoTexture);
	}

	SDL_UnlockMutex(context.mutex);	

	if (videoPlayer != NULL)
	{
		videoPlayer->Tick();
	}
}


void UserInterface::HandleMouseEvent(SDL_Event *sdlEvent)
{	
	if (sdlEvent->button.button == SDL_BUTTON_LEFT)
	{
		int w = 0;
		int h = 0;
		SDL_GetWindowSize(window, &w, &h);
		int x = (int)((100.0 / w) * sdlEvent->button.x);
		int y = (int)((100.0 / h) * sdlEvent->button.y);
		SDL_Log("click at %d,%d", x,y);

		if (videoPlayer != NULL)
		{
			videoPlayer->HandleMouseEvent(sdlEvent);
		}
		//else



		{
			// prepare chunk for holding jpeg download
			struct MemoryStruct imageChunk; 
			imageChunk.memory = (unsigned char *)malloc(1);  /* will be grown as needed by the realloc above */ 
			imageChunk.size = 0;    /* no data at this point */ 

			// build URL to call
			char url[2048];
			if (sdlEvent->button.clicks == 2)
			{
				sprintf(url, "%s/control?quality=high&client=%s&dblclick=%dx%d&sid=%s", baseURL, clientID, x, y, sid);
			}
			else
			{
				sprintf(url, "%s/control?quality=high&client=%s&click=%dx%d&sid=%s", baseURL, clientID, x, y, sid);
			}

			// send the request
			if (DownloadURL(url, &imageChunk) == false)
			{
				DownloadURL(url, &imageChunk);
			}

			lastActivityCheck = SDL_GetTicks();
			if (videoPlayer == NULL || videoPlayer->AudioOnly())
			{
				// decode osd png
				if (imageChunk.size > 0)
				{
					// decode screen jpg
					SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
					SDL_Surface *image = IMG_LoadJPG_RW(rwop);		
					SDL_FreeRW(rwop);

					// create new texture and show screen
					SDL_DestroyTexture(context.menuTexture);
					context.menuTexture = SDL_CreateTextureFromSurface(context.renderer, image);
					SDL_RenderCopy(context.renderer, context.menuTexture, NULL, NULL);
					SDL_RenderPresent(context.renderer);

					// no longer need surface
					SDL_FreeSurface(image);
				}				
			}
			else
			{
				// decode osd png
				if (imageChunk.size > 0)
				{
					SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
					SDL_Surface *image = IMG_LoadPNG_RW(rwop);
					SDL_FreeRW(rwop);

					if (image != NULL)
					{
						// adjust alpha level of OSD pixels
						int pixelBytes = ((image->w * image->h) * 4);
						byte *pixels = (byte *)image->pixels;
						for (int i=0; i<pixelBytes; i+=4)
						{	
							if (pixels[i+3] != 0)
							{
								pixels[i+3] = 0xAF;
							}
						}

						videoPlayer->SetOSD(image);

						// create new texture and let video player know				
						//context.osdTexture = SDL_CreateTextureFromSurface(context.renderer, image);
						//videoPlayer->SetOSDTexture(context.osdTexture);

						// no longer need surface
						SDL_FreeSurface(image);
					}
				}
				else
				{
					// clear OSD
					videoPlayer->SetOSD(NULL);
				}
			}

			// handle any activity requested by the server
			ProcessActivity();
		}	
	}
	else if (sdlEvent->button.button == SDL_BUTTON_RIGHT && videoPlayer != NULL)
	{
		StopPlayback();
	}
}

void UserInterface::HandleKeyboardEvent(SDL_Event *sdlEvent)
{	
	// if video player is active, give it the first chance to handle and key presses
	if (videoPlayer != NULL)
	{
		if (videoPlayer->HandleKeyboardEvent(sdlEvent))
		{
			return;
		}
	}

	// check for fullscreen toggle key combo
	if (sdlEvent->key.keysym.sym == SDLK_RETURN && (sdlEvent->key.keysym.mod & KMOD_ALT))
	{
		if (videoPlayer != NULL)
		{
			videoPlayer->Pause();
			SDL_Delay(100);
		}

		Uint32 flags = SDL_GetWindowFlags(window); 
		if (flags & SDL_WINDOW_FULLSCREEN)
		{
			SDL_SetWindowFullscreen(window, SDL_FALSE);
		}
		else
		{
			SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		}

		if (videoPlayer != NULL)
		{
			SDL_Delay(500);
			videoPlayer->Play();
		}
	}

	// otherwise let NextPVR handle the keypress
	{
		int action = sdlEvent->key.keysym.sym;

		// ignore a few keys (done separately to simplify keycode debugging)
		switch(sdlEvent->key.keysym.sym )
		{
			// ignore ctrl/alt/shift
			case SDLK_LALT:
			case SDLK_RALT:
			case SDLK_LCTRL:
			case SDLK_RCTRL:
			case SDLK_LSHIFT:
			case SDLK_RSHIFT:
				return;
				break;
		}


		// a few keycode translations required
		switch(sdlEvent->key.keysym.sym )
		{
			case SDLK_LEFT:
				action = 37;
				break;
			case SDLK_RIGHT:
				action = 39;
				break;
			case SDLK_UP:
				action = 38; 
				break;
			case SDLK_DOWN:
				action = 40;
				break;

			case SDLK_PAGEDOWN:
				action = 0x22;
				break;
			case SDLK_PAGEUP:
				action = 0x21;
				break;

			case SDLK_KP_MINUS:
				action = 0x6D;
				break;
			case SDLK_KP_PLUS:
				action = 0x6B;
				break;

			case SDLK_AUDIONEXT:
				action = 0xB0;
				if (videoPlayer != NULL)
				{
					action = (39 | 0x20000); // Ctrl-RightArrow
				}
				break;
			case SDLK_AUDIOPREV:
				action = 0xB1;
				if (videoPlayer != NULL)
				{
					action = (37 | 0x20000); // Ctrl-LeftArrow
				}
				break;

			case SDLK_AUDIOSTOP:
				action = ('S' | 0x20000);				
				break;

			case SDLK_AUDIOPLAY:
				action = ('Q' | 0x20000);
				break;

			case SDLK_AC_BACK:
				action = 0x1B;
				break;

			case SDLK_F1:
				action = 0x70;
				break;
			case SDLK_F2:
				action = 0x71;
				break;
			case SDLK_F3:
				action = 0x72;
				break;
			case SDLK_F4:
				action = 0x73;
				break;
			case SDLK_F5:
				action = 0x74;
				break;
			case SDLK_F6:
				action = 0x75;
				break;
			case SDLK_F7:
				action = 0x76;
				break;
			case SDLK_F8:
				action = 0x77;
				break;
			case SDLK_F9:
				action = 0x78;

			case SDLK_KP_0:
				action = '0';
				break;
			case SDLK_KP_1:
				action = '1';
				break;
			case SDLK_KP_2:
				action = '2';
				break;
			case SDLK_KP_3:
				action = '3';
				break;
			case SDLK_KP_4:
				action = '4';
				break;
			case SDLK_KP_5:
				action = '5';
				break;
			case SDLK_KP_6:
				action = '6';
				break;
			case SDLK_KP_7:
				action = '7';
				break;
			case SDLK_KP_8:
				action = '8';
				break;
			case SDLK_KP_9:
				action = '9';
				break;
			case SDLK_KP_ENTER:
				action = SDLK_RETURN;
				break;

			//case SDLK_KP_PLUS:
			//	action = SDLK_PLUS;
			//	break;
			//case SDLK_KP_MINUS:
			//	action = SDLK_MINUS;
				break;
				 
			case SDLK_HOME:
				action = 0x24;
				break;

			case SDLK_BACKSPACE:
				action = 0x08;
				break;


			default:
				{
					// special case for A-Z
					if (action >= SDLK_a && action <= SDLK_z)
					{
						action = toupper(action);			
					}		
				}
				break;
		}	

		// special case for modifiers
		if (action != 0)
		{
			// translate modifiers to match what server expects
			switch (sdlEvent->key.keysym.mod)
			{
				case KMOD_LSHIFT:
				case KMOD_RSHIFT:
					action = (action | 0x10000);
					break;

				case KMOD_LCTRL:
				case KMOD_RCTRL:
					action = (action | 0x20000);
					break;

				case KMOD_LALT:
				case KMOD_RALT:
					action = (action | 0x40000);
					break;
			}

			// free up previous texture
			if (context.menuTexture != NULL)
			{
				SDL_DestroyTexture(context.menuTexture);
			}

			// prepare chunk for holding jpeg download
			struct MemoryStruct imageChunk; 
			imageChunk.memory = (unsigned char *)malloc(1);  /* will be grown as needed by the realloc above */ 
			imageChunk.size = 0;    /* no data at this point */ 

			// send keystroke download, and it'll return the current screen as a jpg
			char url[2048];
			sprintf(url, "%s/control?quality=high&client=%s&key=%d&sid=%s", baseURL, clientID, action, sid);
			if (DownloadURL(url, &imageChunk) == false)
			{
				//DownloadURL(url, &imageChunk);
			}
		

			// update screen texture and display it...
			lastActivityCheck = SDL_GetTicks();
			if (videoPlayer == NULL || videoPlayer->AudioOnly())
			{
				// decode osd png
				if (imageChunk.size > 0)
				{
					// decode screen jpg
					SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
					SDL_Surface *image = IMG_LoadJPG_RW(rwop);		
					SDL_FreeRW(rwop);

					// create new texture and show screen
					SDL_DestroyTexture(context.menuTexture);
					context.menuTexture = SDL_CreateTextureFromSurface(context.renderer, image);
					SDL_RenderCopy(context.renderer, context.menuTexture, NULL, NULL);
					SDL_RenderPresent(context.renderer);

					// no longer need surface
					SDL_FreeSurface(image);
				}
			}
			else
			{
				// decode osd png
				if (imageChunk.size > 0)
				{
					SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
					SDL_Surface *image = IMG_LoadPNG_RW(rwop);
					SDL_FreeRW(rwop);

					if (image != NULL)
					{
						// adjust alpha level of OSD pixels
						int pixelBytes = ((image->w * image->h) * 4);
						byte *pixels = (byte *)image->pixels;
						for (int i=0; i<pixelBytes; i+=4)
						{	
							if (pixels[i+3] != 0)
							{
								pixels[i+3] = 0x9f;
							}
						}
						videoPlayer->SetOSD(image);

						// no longer need surface
						SDL_FreeSurface(image);
					}
				}
				else
				{
					// clear OSD
					videoPlayer->SetOSD(NULL);
				}
			}

			// no longer required
			free(imageChunk.memory);

			// handle any activity requested by the server
			if (initComplete)
			{
				ProcessActivity();
			}
		}
	}
}

void UserInterface::StopPlayback(const char *message)
{
	// delete the player
	if (videoPlayer != NULL)
	{
		// delete videoPlayer;
		lazyDeletePlayer = videoPlayer;
		videoPlayer = NULL;

		// let NextPVR session known that playback session has stopped (so it starts present menus from now on)
		char url[2048];
		if (message == NULL)
		{
			sprintf(url, "%s/control?client=%s&media=stop&sid=%s", baseURL, clientID, sid);
		}
		else
		{
			char encodedMessage[512];
			EscapeString(message, strlen(message), encodedMessage, sizeof(encodedMessage));
			sprintf(url, "%s/control?client=%s&media=stop&message=%s&sid=%s", baseURL, clientID, encodedMessage, sid);
		}
		
		// prepare for screen image
		struct MemoryStruct imageChunk; 
		imageChunk.memory = (unsigned char *)malloc(1);  
		imageChunk.size = 0;    

		// send the request
		if (DownloadURL(url, &imageChunk) == true)
		{
			// decode screen jpg
			SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
			SDL_Surface *image = IMG_LoadJPG_RW(rwop);		
			SDL_FreeRW(rwop);

			// create new texture from result
			if (context.menuTexture != NULL)
			{
				SDL_DestroyTexture(context.menuTexture);
			}
			SDL_DestroyTexture(context.menuTexture);
			context.menuTexture = SDL_CreateTextureFromSurface(context.renderer, image);
		}		
	}	

	
	// rerender the UI
	SDL_RenderCopy(context.renderer, context.menuTexture, NULL, NULL);
	SDL_RenderPresent(context.renderer);

	// some times needs a fake keystroke to get the UI to reappear, not sure why yet.
	SDL_Delay(50);
	SDL_Event firstEvent;
	firstEvent.key.keysym.sym = 'q';
	firstEvent.key.keysym.mod = KMOD_LCTRL;
	HandleKeyboardEvent(&firstEvent);
	//SDL_Delay(50);
	//firstEvent.key.keysym.sym = 'q';
	//firstEvent.key.keysym.mod = KMOD_LCTRL;
	//HandleKeyboardEvent(&firstEvent);	

	lastActivityCheck = 0;
}

void UserInterface::PlaybackPosition(double position)
{
	if (videoPlayer != NULL)
	{
		// let NextPVR session known that playback session has stopped (so it starts present menus from now on)
		char url[2048];
		sprintf(url, "%s/control?client=%s&media=%f&sid=%s", baseURL, clientID, position, sid);

		// prepare for screen image
		struct MemoryStruct imageChunk; 
		imageChunk.memory = (unsigned char *)malloc(1);  
		imageChunk.size = 0;    

		// send the request
		DownloadURL(url, &imageChunk);
		
		if (!videoPlayer->AudioOnly())
		{
			// decode osd png
			if (imageChunk.size > 0)
			{
				SDL_RWops *rwop = SDL_RWFromConstMem(imageChunk.memory, imageChunk.size);
				SDL_Surface *image = IMG_LoadPNG_RW(rwop);
				SDL_FreeRW(rwop);

				if (image != NULL)
				{
					// adjust alpha level of OSD pixels
					int pixelBytes = ((image->w * image->h) * 4);
					byte *pixels = (byte *)image->pixels;
					for (int i=0; i<pixelBytes; i+=4)
					{	
						if (pixels[i+3] != 0)
						{
							pixels[i+3] = 0x9f;
						}
					}

					videoPlayer->SetOSD(image);

					// no longer need surface
					SDL_FreeSurface(image);
				}
			}
			else
			{
				// clear OSD
				videoPlayer->SetOSD(NULL);
			}
		}

		// no longer required
		free(imageChunk.memory);
	}
}

void UserInterface::ForceRefreshOSD()
{
	SDL_Event showOSDEvent;
	memset(&showOSDEvent, 0, sizeof(showOSDEvent));
	showOSDEvent.key.keysym.sym = 'B';
	showOSDEvent.key.keysym.mod = KMOD_LALT;
	HandleKeyboardEvent(&showOSDEvent);	
}

void UserInterface::RegularTimerCallback()
{
	if (videoPlayer != NULL)
	{
		videoPlayer->Tick();
	}
}

Uint32 RegularTimerCallbackSTUB(Uint32 interval, void *param)
{
	SDL_Event timerEvent;
	memset(&timerEvent, 0, sizeof(timerEvent));
	timerEvent.type = SDL_USEREVENT;
	timerEvent.user.code = 1;
	SDL_PushEvent(&timerEvent);
    //((UserInterface *)param)->RegularTimerCallback();
    return interval;
}

//int main(int argc, char *argv[]) 
int UserInterface::Run(int argc, char *argv[])
{	
	printf("Checking configuration\n");

	// handle any command line args
	int renderDriver = -1;
	bool fullscreen = false;
	for (int i=0; i<argc; i++)
	{
		if (strcmp(argv[i], "-server") == 0)
		{
			session.SetServer(argv[i+1]);

			// skip parameter
			i++;
		}
		else if (strcmp(argv[i], "-gpu") == 0)
		{
			useGPU = true;
		}
		else if (strcmp(argv[i], "-driver:0") == 0)
		{
			renderDriver = 0;
		}
		else if (strcmp(argv[i], "-driver:1") == 0)
		{
			renderDriver = 1;
		}
		else if (strcmp(argv[i], "-driver:2") == 0)
		{
			renderDriver = 2;
		}
		else if (strcmp(argv[i], "-driver:3") == 0)
		{
			renderDriver = 3;
		}
		else if (strcmp(argv[i], "-driver:4") == 0)
		{
			renderDriver = 3;
		}
		else if (strcmp(argv[i], "-fullscreen") == 0)
		{
			fullscreen = true;
		}
		else if (strcmp(argv[i], "-res") == 0)
		{
			char *res = argv[i+1];
			sscanf(res, "%dx%d", &WIDTH, &HEIGHT);

			VIDEOWIDTH = WIDTH;
			VIDEOHEIGHT = HEIGHT;

			// skip parameter
			i++;
		}
	}

	if (fullscreen == false)
	{
		WIDTH = 1280;
		HEIGHT = 720;
		VIDEOWIDTH = WIDTH;
		VIDEOHEIGHT = HEIGHT;
	}

	printf("About to connect to server\n");

	// locate NextPVR server
	if (session.FindServer() == false)
	{
		printf("Unable to find and connect to a NextPVR server. Exiting.\n");
		return 0;
	}
	session.GetBaseURL(baseURL, sizeof(baseURL));
	session.GetSID(sid, sizeof(sid));

	printf("About to get hostname\n");

	// determine a client identifier
	char hostname[64];
	gethostname(hostname, sizeof(hostname));
	sprintf(clientID, "sdl-%s", hostname);	

    SDL_Event event;
    int done = 0, action = 0, pause = 0, n = 0;    

	shutdown = false;

	printf("About to initialize SDL\n");

    // Initialise libSDL.
    if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
        printf("Could not initialize SDL: %s.\n", SDL_GetError());
        return EXIT_FAILURE;
    }
	
	printf("About to create window\n");

    // Create SDL graphics objects.
	Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
	if (fullscreen) {
		flags |= SDL_WINDOW_FULLSCREEN;
	}
    window = SDL_CreateWindow(
            "NextPVR Client",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            WIDTH, HEIGHT,
			flags);
    if (!window) {
        fprintf(stderr, "Couldn't create window: %s\n", SDL_GetError());
        quit(3);
    }

	printf("\n");
	int nRenderDrivers = SDL_GetNumRenderDrivers();	
	for (int i=0; i < nRenderDrivers; i++) {
		SDL_RendererInfo info;
		SDL_GetRenderDriverInfo(i, &info); 		
		printf("====info name %d: %s =====\n", i, info.name);
		printf("====max_texture_height %d =====\n", i, info.max_texture_height);
		printf("====max_texture_width %d =====\n", i, info.max_texture_width);

		// prefer direct3d on Windows
		if (renderDriver == -1 && strcmp(info.name, "direct3d") == 0)
		{
			renderDriver = i;
		}

		printf("\n");
	}


	printf("Requesting renderer: %d\n", renderDriver);

    //context.renderer = SDL_CreateRenderer(window, renderDriver, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
	context.renderer = SDL_CreateRenderer(window, renderDriver, SDL_RENDERER_ACCELERATED);
    if (!context.renderer) {
        fprintf(stderr, "Couldn't create renderer: %s\n", SDL_GetError());
        quit(4);
    }

    context.videoTexture = SDL_CreateTexture(
            context.renderer,
			SDL_PIXELFORMAT_YVYU, SDL_TEXTUREACCESS_STREAMING,			
            //SDL_PIXELFORMAT_BGR565, SDL_TEXTUREACCESS_STREAMING,						
            VIDEOWIDTH, VIDEOHEIGHT);
    if (!context.videoTexture) {
        fprintf(stderr, "Couldn't create texture: %s\n", SDL_GetError());
        quit(5);
    }

    context.mutex = SDL_CreateMutex();

	context.menuTexture = NULL;

	// linear filtering for nice resizing of menu screens
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
	

    // If you don't have this variable set you must have plugins directory
    // with the executable or libvlc_new() will not work!
    printf("VLC_PLUGIN_PATH=%s\n", getenv("VLC_PLUGIN_PATH"));

#ifdef WIN32	
	struct SDL_SysWMinfo wmInfo; 
	SDL_VERSION(&wmInfo.version); 
	SDL_GetWindowWMInfo(window, &wmInfo);

	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE); 

	CoInitialize(0);

	RAWINPUTDEVICE rid[2];
	rid[0].usUsagePage = 0xFFBC; 
	rid[0].usUsage = 0x88; 
	rid[0].dwFlags = RIDEV_INPUTSINK | (0x03 & (RIDEV_NOLEGACY | RIDEV_APPKEYS | RIDEV_NOHOTKEYS));

	rid[1].usUsagePage = 0x0C; 
	rid[1].usUsage = 0x01; 
	rid[1].dwFlags = RIDEV_INPUTSINK | (0x03 & (RIDEV_NOLEGACY | RIDEV_APPKEYS | RIDEV_NOHOTKEYS));

	if (RegisterRawInputDevices(rid, 2, sizeof(rid[0])) == FALSE) 
	{
		DWORD rc = GetLastError();
		printf("error: %d\n", rc);
	}
#endif

	// initialise TTF font
	if (TTF_Init() != 0)
	{
        printf("SDL_TTF initialization failure.\n");
        return EXIT_FAILURE;
	}
	font = TTF_OpenFont("c:\\windows\\fonts\\arial.ttf", 20);
	if (font == NULL)
		font = TTF_OpenFont("c:\\windows\\fonts\\tahoma.ttf", 20);
	//if (font == NULL)
	//{
	//	printf("Warning, failed to load tahoma.ttf.\n");
	//}

	// setup regular callback every 100ms
	SDL_TimerID my_timer_id = SDL_AddTimer(100, RegularTimerCallbackSTUB, this);

	// trigger a fake key press to get things started
	SDL_Event firstEvent;
	memset(&firstEvent, 0, sizeof(firstEvent));
	firstEvent.key.keysym.sym = 31;
	HandleKeyboardEvent(&firstEvent);	
	SDL_Delay(1500);

	// make sure any previous playback on this webclient host has stopped.
	{
		char url[2048];
		sprintf(url, "%s/control?client=%s&media=stop&sid=%s", baseURL, clientID, sid);
		
		// prepare for request
		struct MemoryStruct imageChunk; 
		imageChunk.memory = (unsigned char *)malloc(1);  
		imageChunk.size = 0;    

		// send the request
		DownloadURL(url, &imageChunk);
	}

	// trigger screen update (yep, hackety-hack-hack...)
	HandleKeyboardEvent(&firstEvent);
	SDL_Delay(500);
	HandleKeyboardEvent(&firstEvent);

	// Nlite is now up and running
	lastActivityCheck = SDL_GetTicks();
	initComplete = true;

    // Main loop.
	shutdown = false;
    while(!shutdown) {

        action = 0;
		
		//if (lastActivityCheck + 1000 < SDL_GetTicks() && videoPlayer == NULL)
		if (lastActivityCheck + 1000 < SDL_GetTicks())
		{
			if (NeedsRendering())
			{
				HandleKeyboardEvent(&firstEvent);
			}
			lastActivityCheck = SDL_GetTicks();
		}

        // Keys: enter (fullscreen), space (pause), escape (quit).
        while( SDL_PollEvent( &event )) {

            switch(event.type) {
                case SDL_QUIT:
                    shutdown = true;
                    break;

				case SDL_USEREVENT:
					if (event.user.code == 1)
					{
						RegularTimerCallback();
					}
					break;

                case SDL_KEYDOWN:
					{
						HandleKeyboardEvent(&event);		

						lastKeyPress = SDL_GetTicks();
						refreshCycle = 3;							
					}
                    break;

				case SDL_MOUSEMOTION:
					{
						// if playing video, fake keypress to force presentation of OSD
						if (videoPlayer != NULL && videoPlayer->AudioOnly() == false && videoPlayer->ShowingOSD() == false)
						{
							HandleMouseMoveEvent(&event);
							//SDL_Event fakeEvent;
							//fakeEvent.key.keysym.sym = 32;
							//HandleKeyboardEvent(&fakeEvent);
						}
					}
					break;

				case SDL_MOUSEBUTTONDOWN:
					{
						HandleMouseEvent(&event);						
						lastKeyPress = SDL_GetTicks();
						refreshCycle = 3;						
					}
					break;

				case SDL_MOUSEWHEEL:
					{						
						HandleMouseWheelEvent(&event);
					}
					break;

				case SDL_SYSWMEVENT:
					{
#ifdef WIN32
						if (event.syswm.msg->subsystem == SDL_SYSWM_WINDOWS)
						{
							// enable access to MCE remote control events on Windows
							if (event.syswm.msg->msg.win.msg == WM_INPUT)
							{
								SDL_Log("WM_APPCOMMAND");
								MCEHelper::GetTranslatedMCE(event.syswm.msg->msg.win.lParam, event.syswm.msg->msg.win.wParam);
							}
						}
#endif	
					}
					break;

				case SDL_WINDOWEVENT:
					{
						//printf("resize\n");
						switch (event.window.event) 
						{
							case SDL_WINDOWEVENT_EXPOSED:
								SDL_Log("Window %d exposed",
										event.window.windowID);
								{
									//SDL_RenderCopy(context.renderer, context.menuTexture, NULL, NULL);
									//SDL_RenderPresent(context.renderer);								
								}
								break;
							case SDL_WINDOWEVENT_MOVED:
								SDL_Log("Window %d moved to %d,%d",
										event.window.windowID, event.window.data1,
										event.window.data2);
								break;
							case SDL_WINDOWEVENT_RESIZED:
								SDL_Log("Window %d resized to %dx%d",
										event.window.windowID, event.window.data1,
										event.window.data2);
								if (videoPlayer != NULL)
								{
									NeedsReset();
								}
								break;
							case SDL_WINDOWEVENT_SIZE_CHANGED:
								SDL_Log("Window %d size changed to %dx%d",
										event.window.windowID, event.window.data1,
										event.window.data2);  
								{
									// repaint screen
									//SDL_RenderCopy(context.renderer, context.menuTexture, NULL, NULL);
									//SDL_RenderPresent(context.renderer);
								}
								break;							
							default:
								SDL_Log("Window %d got unknown event %d",
										event.window.windowID, event.window.event);
								break;
						}						
					}
					break;
            }
        }


        if(!pause) { context.n++; }

		/*
		// work around to pickup and async screen update within last second of last key press 
		if (lastKeyPress != 0 && (lastKeyPress + 500 < SDL_GetTicks()))
		{			
			printf("doing fake async (refreshCycle=%d)\n", refreshCycle);
			SDL_Event firstEvent;
			firstEvent.key.keysym.sym = 31;
			HandleKeyboardEvent(&firstEvent);
			
			refreshCycle--;
			if (refreshCycle == 0)
			{
				lastKeyPress = 0;
			}
			else
			{
				lastKeyPress = SDL_GetTicks();
			}							
		}*/
		
        //SDL_Delay(1000/50);
		
		SDL_Delay(10);
		
		if (lazyDeletePlayer != NULL)
		{
			delete lazyDeletePlayer;
			lazyDeletePlayer = NULL;
		}
    }

	// stop video player if running
	if (videoPlayer != NULL)
	{
		StopPlayback();
	}

	// Shutdown the TTF library
	if (font != NULL)
		TTF_CloseFont(font);
    TTF_Quit();

    // Close window and clean up libSDL.
    SDL_DestroyMutex(context.mutex);
    SDL_DestroyRenderer(context.renderer);

    quit(0);

    return 0;
}


#ifdef WIN32
LPSTR* CommandLineToArgvA(LPSTR lpCmdLine, INT *pNumArgs)
{
	int retval;
	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, NULL, 0);
	if (!SUCCEEDED(retval))
		return NULL;

	LPWSTR lpWideCharStr = (LPWSTR)malloc(retval * sizeof(WCHAR));
	if (lpWideCharStr == NULL)
		return NULL;

	retval = MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, lpCmdLine, -1, lpWideCharStr, retval);
	if (!SUCCEEDED(retval))
	{
		free(lpWideCharStr);
		return NULL;
	}

	int numArgs;
	LPWSTR* args;
	args = CommandLineToArgvW(lpWideCharStr, &numArgs);
	free(lpWideCharStr);
	if (args == NULL)
		return NULL;

	int storage = numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, NULL, 0, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(args);
			return NULL;
		}

		storage += retval;
	}

	LPSTR* result = (LPSTR*)LocalAlloc(LMEM_FIXED, storage);
	if (result == NULL)
	{
		LocalFree(args);
		return NULL;
	}

	int bufLen = storage - numArgs * sizeof(LPSTR);
	LPSTR buffer = ((LPSTR)result) + numArgs * sizeof(LPSTR);
	for (int i = 0; i < numArgs; ++i)
	{
		assert(bufLen > 0);
		BOOL lpUsedDefaultChar = FALSE;
		retval = WideCharToMultiByte(CP_ACP, 0, args[i], -1, buffer, bufLen, NULL, &lpUsedDefaultChar);
		if (!SUCCEEDED(retval))
		{
			LocalFree(result);
			LocalFree(args);
			return NULL;
		}

		result[i] = buffer;
		buffer += retval;
		bufLen -= retval;
	}

	LocalFree(args);

	*pNumArgs = numArgs;
	return result;
}



int main(int argc, _TCHAR* argv[])
//int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, LPSTR lpCmdLine, int nShowCmd)
#else
int main(int argc, char *argv[])
#endif
{
#ifdef WIN32	
	//ShowWindow(GetConsoleWindow(), SW_HIDE);
	//int argc;
	//LPSTR *argv = CommandLineToArgvA(GetCommandLineA(), &argc);

#endif

	UserInterface userInterface;
	return userInterface.Run(argc, argv);
}
