/* Include the SDL main definition header */
#include "SDL_main.h"
#undef main

/* Standard includes */
#include <stdio.h>

/* SDL includes */
#include "../../video/ogc/SDL_ogc_video.h"

/* OGC includes */
#include <fat.h>
#include <ogcsys.h>

/* Do initialisation which has to be done first for the console to work */
/* Entry point */
int main(int argc, char *argv[])
{
//	SYS_SetPowerCallback(ShutdownCB);
//	SYS_SetResetCallback(ResetCB);
	printf("PAD Init\n");
	PAD_Init();
	printf("video init\n");
	OGC_InitVideoSystem();
	printf("fat init\n");
	fatInitDefault();
	/* Call the user's main function */
	printf("call main\n");
	return(SDL_main(argc, argv));
}
