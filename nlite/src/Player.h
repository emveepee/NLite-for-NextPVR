#pragma once

#include "SDL.h"
#include "SDL_mutex.h"
#include "SDL_image.h"
#include "SDL_ttf.h"
#ifdef WIN32
#define ssize_t size_t
#endif
#include "vlc/vlc.h"

#define MAX_QUEUE_SIZE 200

class PlayerDelegate
{
public:
	virtual ~PlayerDelegate() {}

	virtual void StopPlayback(const char *message = NULL) = 0;
	virtual void PlaybackPosition(double position) = 0;	
	virtual void ForceRefreshOSD() = 0;
	virtual void GenerateOSDMessage(const char *message) = 0; 
	virtual void NeedsReset() = 0;
};

class Player
{
public:
	enum AspectRatioMode { AspectRatio_Auto, AspectRatio_CenterCutZoom, AspectRatio_Fill };
	AspectRatioMode aspectRatioMode;

	Player(PlayerDelegate *playerDelegate, char *url, SDL_Renderer *renderer, SDL_Texture *videoTexture, SDL_mutex *mutex, bool useServerSkip, int forcedDuration, bool useGPU);
	~Player(void);

	// renderer changes
	void SetWindowSize(int windowWidth, int windowHeight);
	void UpdateRenderer(SDL_Renderer *renderer, SDL_Texture *videoTexture);

	// regular callback
	void Tick();

	// audio support
	bool AudioOnly();
	void QueueStream(char *url);

	// handle user interaction
	bool HandleKeyboardEvent(SDL_Event *sdlEvent);
	void HandleMouseEvent(SDL_Event *sdlEvent);

	// libvlc callbacks
	void *lock(void **p_pixels);
	void unlock(void *id, void *const *p_pixels);
	void display(void *id);

	void Pause();
	void Play();	

	// osd graphics
	void SetOSDTexture(SDL_Texture *osdTexture);
	void SetOSD(SDL_Surface *image);
	void SetLocalOSDTexture(SDL_Texture *osdTexture);
	bool ShowingOSD();

	// seeking
	bool UsingServerSideSkip();
	void SkipTo(int ms, int duration=0);

	// error handling	
	void playerEvent(const struct libvlc_event_t* event);

private:
	PlayerDelegate *playerDelegate;
	SDL_mutex *mutex;
	SDL_Texture *videoTexture;
	SDL_Renderer *renderer;

	bool paused;
	bool serverSideSkip;
	bool playbackEnded;

	double lastKnownPosition;
	int forceDurationSeconds;
	unsigned int lastKnownPositionTimestamp;
	unsigned int nextPositionNotificationEvent;
	
	SDL_Surface *osdSurface;
	SDL_Texture *osdTexture;

	SDL_Texture *localOSDTexture;
	Uint32 localOSDHideTime;

	libvlc_instance_t *libvlc;    
    libvlc_media_player_t *mp;

	int forceAspectX;
	int forceAspectY;

	int windowWidth;
	int windowHeight;	

	bool hasAudio;
	bool hasVideo;

	int frameCount;

	int queueSize;
	int queuePointer;
	char *queuedURLs[MAX_QUEUE_SIZE];
};

