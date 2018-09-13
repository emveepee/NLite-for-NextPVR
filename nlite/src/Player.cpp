#include "stdafx.h"
#include "Player.h"
#include <stdlib.h>
#include <string.h>

#define VIDEOWIDTH 1280
#define VIDEOHEIGHT 720


static void *lockSTUB(void *data, void **p_pixels)
{
	((Player *)data)->lock(p_pixels);
	return NULL;
}

static void unlockSTUB(void *data, void *id, void *const *p_pixels) 
{
	((Player *)data)->unlock(id, p_pixels);
}

static void displaySTUB(void *data, void *id) 
{
	((Player *)data)->display(id);
}

static void playerEventSTUB(const struct libvlc_event_t* event, void* userData)
{
	((Player *)userData)->playerEvent(event);
}

Player::Player(PlayerDelegate *playerDelegate, char *url, SDL_Renderer *renderer, SDL_Texture *videoTexture, SDL_mutex *mutex, bool useServerSkip, int forceDurationSeconds, bool useGPU)
{
	this->playerDelegate = playerDelegate;
	this->mutex = mutex;
	this->videoTexture = videoTexture;
	this->renderer = renderer;
	this->osdSurface = NULL;
	this->playbackEnded = false;
	this->osdTexture = NULL;	 
	this->localOSDTexture = NULL;
	this->libvlc = NULL;    
    this->mp = NULL;
	this->paused = false;
	this->serverSideSkip = useServerSkip;
	this->forceDurationSeconds = forceDurationSeconds;
	this->localOSDHideTime = 0;
	this->aspectRatioMode = AspectRatio_Fill;
	this->hasAudio = false;
	this->hasVideo = false;
	this->queueSize = 0;
	this->queuePointer = 0;
	this->frameCount = 0;

	if (useGPU)
	{
		char const *vlc_argv[] = {
			"--no-xlib", // Don't use Xlib.
			"--avcodec-hw=any"			
			//"--avcodec-hw=dxva2"			
		};
		int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);

		// Initialise libVLC.
		libvlc = libvlc_new(vlc_argc, vlc_argv);
	}
	else
	{
		char const *vlc_argv[] = {
			"--no-xlib", // Don't use Xlib.			
			"--network-caching=800",
			"--avcodec-hw=any"	
			//"--ffmpeg-hw",
			//"--file-caching=800",
			//"-vvv",
		};
		int vlc_argc = sizeof(vlc_argv) / sizeof(*vlc_argv);

		// Initialise libVLC.
		libvlc = libvlc_new(vlc_argc, vlc_argv);

	}
    if (NULL == libvlc) 
	{
        printf("LibVLC initialization failure.\n");        
    }

	printf("Player@1\n");

	forceAspectX = 0;
	forceAspectY = 0;

	// prepare URL
	libvlc_media_t *m = libvlc_media_new_location(libvlc, url);
	mp = libvlc_media_player_new_from_media(m);
	libvlc_media_release(m);

	//// prepare formats and callbacks
	//libvlc_video_set_callbacks(mp, lockSTUB, unlockSTUB, displaySTUB, this);
	//libvlc_video_set_format(mp, "RV16", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);

	// initialise events
	libvlc_event_manager_t* em = libvlc_media_player_event_manager(mp);
	libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, playerEventSTUB, this);
	libvlc_event_attach(em, libvlc_MediaPlayerEndReached, playerEventSTUB, this);

	// default to auto for mp4/avi/mkv
	//aspectRatioMode = AspectRatio_Fill;
	aspectRatioMode = AspectRatio_Auto;
	if (strstr(url, ".mp4") != NULL || strstr(url, ".mkv") != NULL || strstr(url, ".avi") != NULL || strstr(url, ".m4v") != NULL)
	{
		aspectRatioMode = AspectRatio_Auto;
	}


	//if (strstr(url, "http://"))
	//{
	//	libvlc_media_parse(m);
	//	char *str = libvlc_media_get_meta(m, libvlc_meta_Title);

	//	libvlc_media_track_t **pTracks = NULL;
	//	int elements = libvlc_media_tracks_get(m, &pTracks);	
	//	if (elements > 0)
	//	{
	//		for (int i=0; i<elements; i++)
	//		{
	//		}
	//	}
	//}

	// video callback for video urls
	bool addedVideo = false;
	if (strstr(url, ".ts") != NULL || strstr(url, ".mp4") != NULL || strstr(url, ".mkv") != NULL || strstr(url, ".avi") != NULL || strstr(url, ".m4v") != NULL || strstr(url, "channel=") != NULL)
	{
		libvlc_video_set_callbacks(mp, lockSTUB, unlockSTUB, displaySTUB, this);
		//libvlc_video_set_format(mp, "RV16", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);
		libvlc_video_set_format(mp, "YVYU", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);
		
		addedVideo = true;
	}
	else
	{
		libvlc_video_set_callbacks(mp, lockSTUB, unlockSTUB, displaySTUB, this);
		//libvlc_video_set_format(mp, "RV16", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);
		libvlc_video_set_format(mp, "YVYU", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);	
	}

	if (useGPU)
	{
		libvlc_media_add_option(m, ":avcodec-hw=dxva2");
	}

	// start playback	
	int rc = libvlc_media_player_play(mp);
	if (rc == -1)
	{
		const char *msg = libvlc_errmsg();
		printf("Failed to start requested stream: %s\n", msg);
		playerDelegate->StopPlayback();
	}

	//libvlc_video_set_deinterlace(mp, "mean"); // mean, bob, linear, blend
	libvlc_video_set_deinterlace(mp, "blend"); // mean, bob, linear, blend

	printf("Playback started\n");
	
	{
		libvlc_media_track_t **pTracks = NULL;
		int elements = libvlc_media_tracks_get(m, &pTracks);	
		for (int i=0; i<100; i++)
		{
			if (elements > 0)
				break;

			if (pTracks != NULL)
				libvlc_media_tracks_release(pTracks, 0); 

			SDL_Delay(50);	
			elements = libvlc_media_tracks_get(m, &pTracks);	
		}		
		if (elements > 0)
		{
			for (int i=0; i<elements; i++)
			{
				libvlc_media_track_t *track = pTracks[i];
				if (track->i_type == libvlc_track_audio)
				{
					hasAudio = true;
				}
				if (track->i_type == libvlc_track_video)
				{
					hasVideo = true;

					/*
					if (!addedVideo)
					{
						libvlc_media_player_stop(mp);

						libvlc_video_set_callbacks(mp, lockSTUB, unlockSTUB, displaySTUB, this);
						libvlc_video_set_format(mp, "RV16", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);

						libvlc_media_player_play(mp);
						addedVideo = true;
					}*/

					printf("video track\n");
					printf("i_height=%d, i_width=%d\n", track->video->i_height, track->video->i_width);
					printf("frame_rate_num=%d, frame_rate_den=%d\n", track->video->i_frame_rate_num, track->video->i_frame_rate_den);
					printf("sar_den=%d, sar_num=%d\n", track->video->i_sar_den, track->video->i_sar_num);
					//forceAspectY = track->video->i_sar_den;
					//forceAspectX = track->video->i_sar_num;
				}
			}

			libvlc_media_tracks_release(pTracks, elements);                                  
		}
	}

	if (hasAudio && !hasVideo)
	{
		printf("audio only\n");
	}
	else if (hasVideo)
	{
		// prepare formats and callbacks
		//libvlc_video_set_callbacks(mp, lockSTUB, unlockSTUB, displaySTUB, this);
		//libvlc_video_set_format(mp, "RV16", VIDEOWIDTH, VIDEOHEIGHT, VIDEOWIDTH*2);
	}

	//libvlc_media_player_set_rate(mp, 0.5f);

	
	// since playback is over http, libvlc doesn't seem to give us any playback position info, so
	// here we're faking the tracking of playback position
	lastKnownPosition = 0;
	lastKnownPositionTimestamp = SDL_GetTicks();

	// we'll notify the server of the playback position about every second
	nextPositionNotificationEvent = lastKnownPositionTimestamp + 1000;

	// check if the URL overrides the start position (quick and ugly)
	if (strstr(url, "seek") != NULL)
	{
		char copy[256];
		strcpy(copy, strstr(url, "seek"));
		if (strstr(copy, "&") != NULL)
		{
			strstr(copy, "&")[0] = '\0';
		}
		lastKnownPosition = atoi(strstr(copy, "=") + 1);		
	}
}


Player::~Player(void)
{	
	// Stop stream and clean up libVLC.
	if (mp != NULL)
	{
		libvlc_media_player_stop(mp);
		libvlc_media_player_release(mp);
	}
    libvlc_release(libvlc);

	// released queued steams
	for (int i=0; i<queueSize; i++)
	{
		if (queuedURLs[i] != NULL)
		{
			delete []queuedURLs[i];
		}
	}
}

bool Player::AudioOnly()
{
	if (hasAudio && !hasVideo)
		return true;
	return false;
}

void Player::SetWindowSize(int windowWidth, int windowHeight)
{
	this->windowWidth = windowWidth;
	this->windowHeight = windowHeight;
}

void Player::UpdateRenderer(SDL_Renderer *renderer, SDL_Texture *videoTexture)
{
	this->renderer = renderer;
	this->videoTexture = videoTexture;

	this->nextPositionNotificationEvent = 0;
}


void Player::playerEvent(const struct libvlc_event_t* event)
{	
	if (event->type == libvlc_MediaPlayerEncounteredError)
	{		
		const char *msg = libvlc_errmsg();
		printf("event %s\n", msg);
		

		char message[1024];
		sprintf(message, "Unexpected error playing requested stream");
		if (msg != NULL)
		{
			strcat(message, ":\n\n");
			strcat(message, msg);
		}

		playerDelegate->StopPlayback(message);
	}
	if (event->type == libvlc_MediaPlayerEndReached)
	{		
		printf("playback ended\n");
		playbackEnded = true;
	}
}

void Player::Pause()
{
	libvlc_media_player_pause(mp);
	paused = true;
}

void Player::Play()
{
	libvlc_media_player_play(mp);
	paused = false;
}

void Player::SkipTo(int seconds, int serverReportedDuration)
{		
	int ms = (seconds * 1000);
	// get the duration
	libvlc_time_t duration = libvlc_media_player_get_length(mp);

	// libvlc isn't very good at getting the duration from some types of files (like avi files)
	if (duration == 0 && forceDurationSeconds != 0)
		duration = (forceDurationSeconds * 1000);
	else if (duration == 0 && serverReportedDuration != 0)
		duration = (serverReportedDuration * 1000);

	// skip to new location
	float position = ((1.0f / duration) * ms);
	libvlc_media_player_set_position(mp, position);

	lastKnownPosition = ms;
	lastKnownPositionTimestamp = SDL_GetTicks();
}

bool Player::UsingServerSideSkip()
{
	return serverSideSkip;
}

bool Player::HandleKeyboardEvent(SDL_Event *sdlEvent)
{
	bool handled = false;	

	// hanlde play/pause toggle
	if ((sdlEvent->key.keysym.sym == SDLK_q && (sdlEvent->key.keysym.mod & KMOD_CTRL)) || (sdlEvent->key.keysym.sym == SDLK_SPACE) || sdlEvent->key.keysym.sym == SDLK_AUDIOPLAY)
	{
		handled = true;
		if (paused)
		{
			// resume playback
			libvlc_media_player_play(mp);
			paused = false;

			lastKnownPositionTimestamp = SDL_GetTicks();
		}
		else
		{					
			// pause playback
			libvlc_media_player_pause(mp);
			paused = true;

			// need to update for our fake position tracking...
			lastKnownPosition = ((lastKnownPosition + (SDL_GetTicks() - lastKnownPositionTimestamp)) / 1000.0);			
			if (lastKnownPositionTimestamp + 500 < SDL_GetTicks())
			{
				lastKnownPositionTimestamp = SDL_GetTicks();
			
				// let server know how current position					
				playerDelegate->PlaybackPosition(lastKnownPosition);				
			}
		}	

		playerDelegate->ForceRefreshOSD();
	}
	// handle various 'stop' keystrokes
	else if (sdlEvent->key.keysym.sym == SDLK_AUDIOSTOP || (sdlEvent->key.keysym.sym == SDLK_s && (sdlEvent->key.keysym.mod & KMOD_CTRL)) || sdlEvent->key.keysym.sym == SDLK_AC_BACK || (sdlEvent->key.keysym.sym == SDLK_ESCAPE && osdTexture == NULL))
	{ 		
		handled = true;

		// no need to play any further queued items
		queuePointer = queueSize;

		// let server know how current position					
		playerDelegate->PlaybackPosition(lastKnownPosition);

		if (mp != NULL)
		{
			libvlc_media_player_stop(mp);
			libvlc_media_player_release(mp);
		}
		mp = NULL;

		playerDelegate->StopPlayback();
	}
	// toggle subtitles
	else if ((sdlEvent->key.keysym.sym == SDLK_x && (sdlEvent->key.keysym.mod & KMOD_CTRL)) || (sdlEvent->key.keysym.sym == SDLK_y && (sdlEvent->key.keysym.mod & KMOD_ALT)))
	{
		int count = libvlc_video_get_spu_count(mp);

		if (count > 0)
		{
			int subtitleTrack = libvlc_video_get_spu(mp);

			int indexes[32];
			int i = 0;
			int selectedIndex = 0;

			printf("%d subtitle tracks\n", count);
			libvlc_track_description_t *pTrack = libvlc_video_get_spu_description(mp);
			while (pTrack != NULL)
			{
				printf("- %d:%s\n", pTrack->i_id, pTrack->psz_name);
				indexes[i] = pTrack->i_id;
				if (subtitleTrack == pTrack->i_id)
					selectedIndex = i;
				i++;
				pTrack = pTrack->p_next;
			}

			selectedIndex++;
			if (selectedIndex >= count)
				selectedIndex = 0;

			libvlc_video_set_spu(mp, indexes[selectedIndex]);

			printf("selected subtitle track = %d\n", indexes[selectedIndex]);

			// send message to OSD
			pTrack = libvlc_video_get_spu_description(mp);
			while (pTrack != NULL)
			{				
				if (indexes[selectedIndex] == pTrack->i_id)
				{
					char message[512];
					sprintf(message, "Subtitle: %s", pTrack->psz_name);
					playerDelegate->GenerateOSDMessage(message);
					break;
				}
				pTrack = pTrack->p_next;
			}
		}
		
		handled = true;
	}

	// toggle audio stream
	else if (sdlEvent->key.keysym.sym == SDLK_g && (sdlEvent->key.keysym.mod & KMOD_ALT))
	{
		int count = libvlc_audio_get_track_count(mp);
		if (count > 0)
		{
			int currentAudioTrack = libvlc_audio_get_track(mp);
			printf("%d audio tracks\n", count);

			int indexes[32];
			int i = 0;
			int selectedIndex = 0;

			libvlc_track_description_t *pTrack = libvlc_audio_get_track_description(mp);
			while (pTrack != NULL)
			{
				printf("- %d:%s\n", pTrack->i_id, pTrack->psz_name);
				indexes[i] = pTrack->i_id;
				if (currentAudioTrack == pTrack->i_id)
					selectedIndex = i;
				i++;
				pTrack = pTrack->p_next;
			}

			selectedIndex++;
			if (selectedIndex >= count)
				selectedIndex = 0;

			libvlc_audio_set_track(mp, indexes[selectedIndex]);

			printf("selected audio track = %d\n", indexes[selectedIndex]);

			// send message to OSD
			pTrack = libvlc_audio_get_track_description(mp);
			while (pTrack != NULL)
			{				
				if (indexes[selectedIndex] == pTrack->i_id)
				{
					char message[512];
					sprintf(message, "Audio: %s", pTrack->psz_name);
					playerDelegate->GenerateOSDMessage(message);
					break;
				}
				pTrack = pTrack->p_next;
			}
		}
		
		handled = true;
	}
	// toggle aspect ratio
	else if (sdlEvent->key.keysym.sym == SDLK_F7)
	{
		if (aspectRatioMode == AspectRatio_Auto)
		{
			aspectRatioMode = AspectRatio_CenterCutZoom;
			playerDelegate->GenerateOSDMessage("Aspect Ratio: Center-Cut");
		}
		else if (aspectRatioMode == AspectRatio_CenterCutZoom)
		{
			aspectRatioMode = AspectRatio_Fill;
			playerDelegate->GenerateOSDMessage("Aspect Ratio: Fill");
		}
		else if (aspectRatioMode == AspectRatio_Fill)
		{
			aspectRatioMode = AspectRatio_Auto;
			playerDelegate->GenerateOSDMessage("Aspect Ratio: Auto");
		}

		handled = true;
	}

	// if we're about to pass a ctrl key combo to server, update the server with our current location first
	else if (sdlEvent->key.keysym.mod & KMOD_CTRL)
	{
		// need to update for our fake position tracking...
		lastKnownPosition = ((lastKnownPosition + (SDL_GetTicks() - lastKnownPositionTimestamp)) / 1000.0);			
		if (lastKnownPositionTimestamp + 500 < SDL_GetTicks())
		{
			lastKnownPositionTimestamp = SDL_GetTicks();
			
			// let server know how current position					
			playerDelegate->PlaybackPosition(lastKnownPosition);
		}
	}
	

	return handled;
}

void Player::HandleMouseEvent(SDL_Event *sdlEvent)
{
	
}

bool Player::ShowingOSD()
{
	if (osdTexture != NULL)
		return true;
	return false;
}

void Player::SetLocalOSDTexture(SDL_Texture *osdTexture)
{
	SDL_LockMutex(mutex);
	
	// free any previous texture
	if (localOSDTexture != NULL)
	{
		SDL_DestroyTexture(localOSDTexture);
		localOSDTexture = NULL;
	}

	// keep a reference to it, and note the time it should be hidden
	localOSDTexture = osdTexture;
	localOSDHideTime = SDL_GetTicks() + 3000;

	SDL_UnlockMutex(mutex);
}

void Player::SetOSD(SDL_Surface *image)
{
	SDL_LockMutex(mutex);
		
	if (osdSurface != NULL)
	{
		SDL_FreeSurface(osdSurface);
		osdSurface = NULL;		
	}

	if (osdTexture != NULL)
	{
		SDL_DestroyTexture(osdTexture);
		osdTexture = NULL;		
	}

	if (image != NULL)
	{
		osdSurface = SDL_ConvertSurface(image, image->format, SDL_SWSURFACE);		
		osdTexture = SDL_CreateTextureFromSurface(renderer, image);
	}

	SDL_UnlockMutex(mutex);
}

// VLC prepares to render a video frame.
void *Player::lock(void **p_pixels) 
{
    int pitch;
    SDL_LockMutex(mutex);
	if (videoTexture != NULL)
	{
		SDL_LockTexture(videoTexture, NULL, p_pixels, &pitch);
	}

    return NULL; // Picture identifier, not needed here.
}

#define RED_MASK 0xF800
#define GREEN_MASK 0x07E0
#define BLUE_MASK 0x001F

// VLC just rendered a video frame.
void Player::unlock(void *id, void *const *p_pixels) 
{
    uint16_t *pixels = (uint16_t *)*p_pixels;
	/*
	static int lastX = 0;	
	if (osdSurface != NULL)
	{
		int size = (VIDEOWIDTH * VIDEOHEIGHT);
		//DWORD *osd = (DWORD *)osdSurface->pixels;

		double a1 = 0.3f;
		double a2 = 1.0f;

		unsigned char *osd = (unsigned char *)osdSurface->pixels;
		for (int i=0; i<size; i++)
		{
			unsigned char alpha = osd[3];
			if (alpha != 0)
			{
				
				unsigned char red = osd[0];
				unsigned char green = osd[1];
				unsigned char blue = osd[2];
				
				uint16_t BGRColor = red >> 3;
				BGRColor |= (green & 0xFC) << 3;
				BGRColor |= (blue  & 0xF8) << 8;
				
				//// alpha blend version of OSD
				//unsigned char osdRed = osd[2];
				//unsigned char osdGreen = osd[1];
				//unsigned char osdBlue = osd[0];

				//uint16_t rgb565 = pixels[i];
				//unsigned char r5 = (rgb565 & RED_MASK)   >> 11;
				//unsigned char g6 = (rgb565 & GREEN_MASK) >> 5;
				//unsigned char b5 = (rgb565 & BLUE_MASK);

				//unsigned char finalRed = (int) (a1 * osdRed + a2 * (1 - a1) * r5);
				//unsigned char finalGreen = (int) (a1 * osdGreen + a2 * (1 - a1) * g6);
				//unsigned char finalBlue = (int) (a1 * osdBlue + a2 * (1 - a1) * b5);

				//uint16_t BGRColor = finalRed >> 3;
				//BGRColor |= (finalGreen & 0xFC) << 3;
				//BGRColor |= (finalBlue  & 0xF8) << 8;

				pixels[i] = BGRColor;
			}
			osd += 4;

			//pixels += 2;
		}
	}
	*/

	if (videoTexture != NULL)
	{
		SDL_UnlockTexture(videoTexture);
	}
    SDL_UnlockMutex(mutex);
}
 
// VLC wants to display a video frame.
void Player::display(void *id) 
{
	SDL_LockMutex(mutex);
	
	// on the first frame, clear the screen
	//if (frameCount < 10)
	{
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		if (SDL_RenderClear(renderer) == -1)
		{
			printf("error: %s\n", SDL_GetError());		
			SDL_UnlockMutex(mutex);
			return;
		}
	}
	frameCount++;

	// 'fill' aspect ratio
	SDL_Rect displayRect = {0, 0, windowWidth, windowHeight};

	// correct aspect ratio based on source material
	if (aspectRatioMode == AspectRatio_Auto || aspectRatioMode == AspectRatio_CenterCutZoom)
	{
		unsigned int w = 0;
		unsigned int h = 0;	
		if (libvlc_video_get_size(mp, 0, &w, &h) == 0)
		{
			if (w == 0 || h == 0)
			{
				w = VIDEOWIDTH;
				h = VIDEOHEIGHT;
			}


			float k = (float)h / (float)w;

			// handle slightly-off 16:9 aspect ratios (like 1920x1090)
			if (k >= 0.56f && k <= 0.57f)
			{
				k = 0.5625f;
			}

			if (forceAspectX != 0)
			{
				k = (float)forceAspectX / (float)forceAspectY;
			}

			int targetWidth = (int)windowWidth;
			int targetHeight = (int)(windowWidth * k);
			if (targetHeight > windowHeight)
			{
				targetHeight = (int)windowHeight;
				targetWidth = (int)(targetHeight / k);
			}
			int targetX = 0;
			if (targetWidth < windowWidth)
			{
				targetX = (int)((windowWidth - targetWidth) / 2.0);
			}
			int targetY = 0;
			if (targetHeight < windowHeight)
			{
				targetY = (int)((windowHeight - targetHeight) / 2.0);
			}

			// zoom for center cut
			if (aspectRatioMode == AspectRatio_CenterCutZoom)
			{
				int zoomX = (int)(windowWidth * 0.17);
				int zoomY = (int)(windowHeight * 0.17);

				targetX -= zoomX;
				targetY -= zoomY;

				targetWidth += (zoomX * 2);
				targetHeight += (zoomY * 2);
			}

			displayRect.x += targetX;
			displayRect.y += targetY;
			displayRect.w = targetWidth;
			displayRect.h = targetHeight;
		}
	}	
	
	// render video frame	
	if (videoTexture != NULL)
	{
		SDL_RenderCopy(renderer, videoTexture, NULL, &displayRect);	
	}

	// render any server generated OSD
	if (osdTexture != NULL)
	{
		SDL_RenderCopy(renderer, osdTexture, NULL, NULL);	
	}

	// render any locally generated OSD
	if (localOSDTexture != NULL)
	{		
		int w = 0;
		int h = 0;
		SDL_QueryTexture(localOSDTexture, NULL, NULL, &w, &h);
		if (w > 0)
		{
			SDL_Rect rect;
			rect.x = (VIDEOWIDTH / 2) - (w / 2);
			rect.y = (VIDEOHEIGHT / 8) - (h / 2);
			rect.w = w;
			rect.h = h;
			SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0x80);
			SDL_RenderFillRect(renderer, &rect);
			SDL_RenderCopy(renderer, localOSDTexture, NULL, &rect);	
		}
	}

	// present
    SDL_RenderPresent(renderer);	

	// time to release local OSD texture?
	if (localOSDTexture != NULL && localOSDHideTime < SDL_GetTicks())
	{
		localOSDHideTime = 0;
		SDL_DestroyTexture(localOSDTexture);
		localOSDTexture = NULL;
	}	

	SDL_UnlockMutex(mutex);
}

void Player::QueueStream(char *url)
{
	printf(" - queueing: %s\n", url);

	if (queuePointer < (MAX_QUEUE_SIZE - 1))
	{
		queuedURLs[queueSize] = new char[strlen(url)+1];
		strcpy(queuedURLs[queueSize], url);

		queueSize++;
	}
}

void Player::Tick()
{
	if (playbackEnded)
	{
		if (queuePointer < queueSize)
		{		
			// Stop stream and clean up libVLC.
			if (mp != NULL)
			{
				libvlc_media_player_stop(mp);
				libvlc_media_player_release(mp);
				mp = NULL;
			}

			// prepare URL
			libvlc_media_t *m = libvlc_media_new_location(libvlc, queuedURLs[queuePointer]);
			mp = libvlc_media_player_new_from_media(m);
			libvlc_media_release(m);

			// initialise events
			libvlc_event_manager_t* em = libvlc_media_player_event_manager(mp);
			libvlc_event_attach(em, libvlc_MediaPlayerEncounteredError, playerEventSTUB, this);
			libvlc_event_attach(em, libvlc_MediaPlayerEndReached, playerEventSTUB, this);

			// start playback	
			int rc = libvlc_media_player_play(mp);
			if (rc == -1)
			{
				const char *msg = libvlc_errmsg();
				printf("Failed to start requested stream: %s\n", msg);
				playerDelegate->StopPlayback();
			}

			playbackEnded = false;

			queuePointer++;
			return;
		}

		playerDelegate->StopPlayback();
		return;
	}
	
	// is it time to let server know the current playback position again? (sp it can update OSD etc)
	//if (osdSurface != NULL && nextPositionNotificationEvent < SDL_GetTicks() && paused == false)
	if (!AudioOnly())
	{
		if (nextPositionNotificationEvent < SDL_GetTicks() && paused == false)
		{
			double currentPosition = ((lastKnownPosition + (SDL_GetTicks() - lastKnownPositionTimestamp)) / 1000.0);
			playerDelegate->PlaybackPosition(currentPosition);

			// lets do it all again in another second or so
			nextPositionNotificationEvent = SDL_GetTicks() + 1000;
		}	
	}
}
