/* Include the SDL main definition header */
#include "SDL_main.h"
#undef main

/* Standard includes */
#include <stdio.h>

/* SDL includes */
#include "../../video/gamecube/SDL_gamecube_video.h"

/* OGC includes */
#include <fat.h>
#include <ogcsys.h>

/* Do initialisation which has to be done first for the console to work */
/* Entry point */
int main(int argc, char *argv[])
{
//	SYS_SetPowerCallback(ShutdownCB);
//	SYS_SetResetCallback(ResetCB);
	PAD_Init();
	GAMECUBE_InitVideoSystem();
	fatInitDefault();
	/* Call the user's main function */
	return(SDL_main(argc, argv));
}
