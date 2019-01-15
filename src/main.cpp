//Main.c
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <dynamic_libs/os_functions.h>
#include <dynamic_libs/fs_functions.h>
#include <dynamic_libs/gx2_functions.h>
#include <dynamic_libs/sys_functions.h>
#include <dynamic_libs/vpad_functions.h>
#include <dynamic_libs/socket_functions.h>
#include <dynamic_libs/curl_functions.h>
#include <libutils/system/memory.h>
#include <libutils/utils/logger.h>
#include <libutils/utils/utils.h>
#include <dynamic_libs/os_defs.h>
#include <libutils/network/FileDownloader.h>
#include <libutils/fs/FSUtils.h>
#include <libutils/fs/sd_fat_devoptab.h>
#include "main.h"
#include "patch_list.h"
#include "exploit.h"
#include "screen.h"
#include "internet_cfg.h"

const char *installer_version = "v1.2.1";
#define EXIT_RELAUNCH_ON_LOAD 0xFFFFFFFD

const char* fileURL = "https://raw.githubusercontent.com/NexoDevelopment/python_bin2h/master/README.md";
const char* filePath = "sd:/index_html.bin";

static int already_done = 0;

int wait_for_vpad_input()
{
	VPADData vpad;
	s32 err_h;

	while(1)
	{
		VPADRead(0, &vpad, 1, &err_h);
		if(vpad.btns_h)
		{
			return vpad.btns_h;
		}
	}

	
}

int dl_file(const char* url)
{

   NSSLInit();
   CURL * curl = n_curl_easy_init();
   n_curl_easy_setopt(curl, CURLOPT_URL, url);
   n_curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1);
   n_curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);

   ProxyConfig *yeet = get_proxy_cfg();
   if(yeet->using_proxy)
   {

      n_curl_easy_setopt(curl, CURLOPT_PROXY, &yeet->ip);
      n_curl_easy_setopt(curl, CURLOPT_PROXYPORT, yeet->port);

   }

   s32 ssl_context = NSSLCreateContext(0);
   for(int i = 100; i<106; i++) {
      NSSLAddServerPKI(ssl_context,i);
   }

   for(int i = 1001; i<1034; i++) {
     NSSLAddServerPKI(ssl_context,i);
   }

   memset((int*)0xF5200000, 0, 0x100);

   /* *** Which actually is CURLOPT_NSSL_CTX *** */
   n_curl_easy_setopt(curl, CURLOPT_GSSAPI_DELEGATION, ssl_context);
   n_curl_easy_perform(curl);

   NSSLDestroyContext(ssl_context);
   NSSLFinish();
   n_curl_easy_cleanup(curl);

   return *(int*)0xF5200000-4;

}

int write_data(void *buffer, int size, int nmemb, void *userp)
{

	int filepos = 0;
	int insize = size*nmemb;
	memcpy((int*)0xF5200000+filepos, buffer, insize);
	DCFlushRange((int*)0xF5200000+filepos, insize);
	filepos += insize;
	return insize;

}

int Menu_Main(void)
{


	InitOSFunctionPointers();
	InitSysFunctionPointers();
	InitSocketFunctionPointers();
	InitFSFunctionPointers();
	InitVPadFunctionPointers();
	InitCurlFunctionPointers();

	mount_sd_fat("sd");

	/* *** Avoid the infinite loading loop ... (with the done flag) *** */

	uint64_t tid = OSGetTitleID();
	if((tid != 0x000500101004A000 + 0x000 && tid != 0x000500101004A000 + 0x100 && tid != 0x000500101004A000 + 0x200) &&  // Mii Maker
		(tid != 0x0005000013374842 + 0x000 && tid != 0x0005000013374842 + 0x100 && tid != 0x0005000013374842 + 0x200))	 // Homebrew Channel
	{
		return EXIT_RELAUNCH_ON_LOAD;
	}
	else if(already_done)
	{
		return 0;
	}

	initScreen();
	printf_("Welcome to the Pretendo Installer %s", (u32)installer_version);
	printf_("This installer will patch your Console temporarily.", 0);
	printf_("", 0);
	printf_("Press A for URL+SSL patches.", 0);
	printf_("Press B for SSL patches.", 0);
	printf_("Press HOME to exit.", 0);

	while(1)
	{	
		int ret = wait_for_vpad_input();

		if(ret & VPAD_BUTTON_A)
		{
			*(volatile int*)0xF5FFFFFC = 0;
			break;
		}

		if(ret & VPAD_BUTTON_B) 
		{
			*(volatile int*)0xF5FFFFFC = 1; 
			break;
		}

		if(ret & VPAD_BUTTON_HOME) return 0;
	}

	SYSLaunchMenu();

	DCFlushRange((void*)0xF5FFFFFC, 4);

	clearScreen();

	printf_("Loading patches into RAM...", 0);

	/* *** OSEffectiveToPhysical(0x10000000) = 0x50000000 *** */
	*(int*)0x1080B000 = sizeof(patches) / sizeof(URL_Patch); 
	void *patch_mem = (int*)0x1080C000;
	
	/* *** memcpy() and flush our patches *** */
	memcpy(patch_mem, patches, sizeof(patches));
	DCFlushRange((void*)0x1080B000, sizeof(patches) + 0x1000);

	os_sleep(1);

	printf_("Done.", 0);
	printf_("Performing Kernel Exploit and patching IOSU...", 0);
	printf_("Thanks to Kinnay for the No-SSL patch !", 0);

	// TODO: Actual Error Handling because IOS_Open can fail sometimes /shrug
	int ret = IOSU_Kernel_Exploit();
	os_sleep(1);

	printf_("0x%08X", ret);
	printf_("Exiting ...", 0);

	os_sleep(1);

	// 0x11446E38 w/ version
	// 0x11446EB8 w/out version
	// 0x1114f6a4 Miiverse

	deinitScreen();
	already_done++;
	return EXIT_RELAUNCH_ON_LOAD;
}

