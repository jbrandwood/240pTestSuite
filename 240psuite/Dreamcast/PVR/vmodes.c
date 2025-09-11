#include <kos.h>
#include "vmodes.h"
#include "menu.h"

int vmode 	= -1;
int vcable	= CT_RGB;
int ovmode	= -1;
int ovcable = CT_RGB;
int W		= 0;
int H		= 0;
int dW		= 0;
int dH		= 0;
int offsetY = 0;
int IsPAL 	= 0;
int IsPALDC	= 0;

/* 640x480 VGA 60Hz */
/* DM_640x480_VGA */
vid_mode_t custom_vga =
{
	DM_640x480,
	640, 480,
	0,
	CT_VGA,
	PM_RGB565,
	524, 857,
	172, 40,
	21, 260,
	126, 837,
	36, 516,
	0, 1, 0
};

/* 320x240 NTSC 60Hz */
// outputs PAL 60 when using composite in Euro Flash ROM
vid_mode_t custom_240 = 
	/* DM_320x240_NTSC */
{
	DM_320x240,
	320, 240,
	VID_PIXELDOUBLE | VID_LINEDOUBLE,
	CT_ANY,
	PM_RGB565,
	262, // Number of scanlines. 262 default
	857, // Clocks per scanline. 
	164, // Bitmap X, 157 brings it to 35 2/3 uS centering the signal, 168 is used by the BIOS as documented by Moopthehedgehog (153)
	18, // Bitmap Y 18 starts at NTSC line 22, according to spec
	21, // First scanline interrupt position. 21 default
	260, // Second scanline interrupt position (automatically doubled for VGA) 
	129, // Border X starting. 
	840, // Border X stop position. 
	24, // Border Y starting position.
	264, // Border Y stop position.
	0, 1, 0
};


vid_mode_t custom_288 = 
{	// DM_320x240_PAL
	DM_320x240,
	320, // Width
	264, // Height
	VID_PIXELDOUBLE | VID_LINEDOUBLE | VID_PAL,  // flags
	CT_ANY, // Allowed cable type. 
	PM_RGB565, // Pixel mode. 
	312, // Number of scanlines. 312 default, 
	863, // Clocks per scanline. 863, 727 working
	174, // Bitmap window X position. 
	36, // Bitmap window Y (45 defaulty), with 22 starts at line 23 as PAL dictates, 49 is bottom at PAL limit
	21, // First scanline interrupt position. 21 default
	310, // Second scanline interrupt position (automatically doubled for VGA) 
	143, // Border X starting position. 
	845, // Border X stop position. 
	22, // Border Y starting position. 
	311, // Border Y stop position. 
	0, // Current framebuffer. 
	1, // Number of framebuffers. 
	0 // Offset to framebuffers. 
};

/* 640x480 PAL 50Hz IL */
 /* DM_640x480_PAL_IL */
vid_mode_t custom_576 = 
{
	DM_640x480_PAL_IL,
	640,
	480,
	VID_INTERLACE | VID_PAL,
	CT_ANY,
	PM_RGB565,
	624,
	863,
	175,
	45, //	21 Starts at line #23 in PAL composite, 45 starts centered
	21,  
	310,
	143,
	845,
	0,
	310, // 620
	0,
	1,
	0
};

ImagePtr   scanlines = NULL;

void LoadScanlines()
{
	if(!scanlines)
	{
		scanlines = LoadIMG("/rd/scanlines.kmg.gz", 0);
		scanlines->layer = 5.0;
		scanlines->alpha = settings.scanlines;
		scanlines->scale = 0;
		CalculateUV(0, 0, 640, 480, scanlines);
		if(settings.scanlines_even)
			SetScanlineEven();
		else
			SetScanlineOdd();
	}
}

void SetScanlineEven()
{
	if(scanlines)
	{
		settings.scanlines_even = 1;
		scanlines->y = 0;
	}
}

void SetScanlineOdd()
{
	if(scanlines)
	{
		settings.scanlines_even = 0;
		scanlines->y = -1;
	}
}

void RaiseScanlineIntensity()
{
	if(scanlines)
	{
		scanlines->alpha += 0.05f;
		if(scanlines->alpha > 1.0f)
			scanlines->alpha = 1.0f;
		settings.scanlines = scanlines->alpha;
	}
}

void LowerScanlineIntensity()
{
	if(scanlines)
	{
		scanlines->alpha -= 0.05f;
		if(scanlines->alpha < 0.0f)
			scanlines->alpha = 0.0f;
		settings.scanlines = scanlines->alpha;
	}
}

int ScanlinesEven()
{
	if(scanlines)
		return(scanlines->y == 0);
	return 0;
}

float GetScanlineIntensity()
{
	if(scanlines)
		return((float)scanlines->alpha*100);
	else
		return 0;
}

void SetScanlineIntensity(float value)
{
	settings.scanlines = value;
	if(scanlines)
		scanlines->alpha = value;
}

inline void DrawScanlines()
{
	if(scanlines && vmode == VIDEO_480P_SL)
		DrawImage(scanlines);
}

inline void ReleaseScanlines()
{
	FreeImage(&scanlines);
}

void AdjustPVROptions()
{
	// Disable deflicker filter
	if(!settings.Deflicker && PVR_GET(PVR_SCALER_CFG) != 0x400)
	{
		dbglog(DBG_INFO, "240p TS: Disabling PVR deflicker filter for 240p tests\n");
		PVR_SET(PVR_SCALER_CFG, 0x400);
	}
	else
	{
		if(settings.Deflicker)
			dbglog(DBG_INFO, "240p TS: PVR deflicker filter not disabled (settings)\n");
	}

	// Enable deflicker filter
	if(settings.Deflicker && PVR_GET(PVR_SCALER_CFG) != 0x401)
	{
		dbglog(DBG_INFO, "240p TS: Enabling PVR deflicker filter (settings)\n");
		PVR_SET(PVR_SCALER_CFG, 0x401);
	}

	// Turn off texture dithering
	if(!settings.Dithering && PVR_GET(PVR_FB_CFG_2) != 0x00000001)
	{
		dbglog(DBG_INFO, "240p TS: Disabling PVR dithering for 240p tests\n");
		PVR_SET(PVR_FB_CFG_2, 0x00000001);
	}
	else
	{
		if(settings.Dithering)
			dbglog(DBG_INFO, "240p TS: PVR dithering not disabled (settings)\n");
	}
	
	// Turn on texture dithering
	if(settings.Dithering && PVR_GET(PVR_FB_CFG_2) == 0x00000001)
	{
		dbglog(DBG_INFO, "240p TS: Enabling PVR dithering for 240p tests\n");
		PVR_SET(PVR_FB_CFG_2, 0x00000009);
	}
}

void ChangeVideoRegisters()
{
	if(settings.ChangeVideoRegs)
	{
		uint32 	data = 0;
		vuint32 *regs = (uint32*)0xA05F8000;
		
		dbglog(DBG_INFO, "240p TS: Changing video registers to match specs\n");
		if(IsPAL)
		{
			if(vmode == VIDEO_288P)
			{
				// Video enable 0x100 ORed with PAL mode 0x80.	
				// Sync should be  negative in vertical and
				// horzontal for PAL, they are bits 1 and 2.
				data = 0x100 | 0x80; 
				regs[0x34] = data;

				// Set sync width
				data = 0x05 << 8 | 0x3f | 0x31F << 12 | 0x1f << 22;
				regs[0x38] = data;
			}
			if(vmode == VIDEO_576I || vmode == VIDEO_576I_A264)
			{	
				// Video enable 0x100 ORed with PAL mode 0x80.	
				// Sync should be  negative in vertical and
				// horzontal for PAL, they are bits 1 and 2.
				// 0x10 is for interlaced mode.
				data = 0x100 | 0x80 | 0x10; 
				regs[0x34] = data;

				// Set sync width
				data = 0x05 << 8 | 0x3f | 0x16A << 12 | 0x1f << 22;
				regs[0x38] = data;
			}
		}
		else
		{
			// Set sync width
			if(vmode == VIDEO_240P)
			{
				// Video enable 0x100 ORed with PAL mode 0x80.	
				// Sync should be  negative in vertical and
				// horzontal for PAL, they are bits 1 and 2.
				// 0x10 is for interlaced mode.
				data = 0x100 | 0x40; 
				regs[0x34] = data;

				data = 0x05 << 8 | 0x3f | 0x31F << 12 | 0x1f << 22;
				regs[0x38] = data;
			}
			if(vmode == VIDEO_480I || vmode == VIDEO_480I_A240)
			{	
				// Video enable 0x100 ORed with PAL mode 0x80.	
				// Sync should be  negative in vertical and
				// horzontal for PAL, they are bits 1 and 2.
				// 0x10 is for interlaced mode.
				data = 0x100 | 0x40 | 0x10; 
				regs[0x34] = data;

				data = 0x06 << 8 | 0x3f | 0x16C << 12 | 0x1f << 22;
				regs[0x38] = data;
			}
		}

		// changed init values to match BIOS as documented by Moopthehedgehog
		// https://github.com/ArtemioUrbina/240pTestSuite/issues/2
		if(vmode == VIDEO_480P || vmode == VIDEO_480P_SL)
		{			
			data = 0x03 << 8 | 0x3f | 0x319 << 12 | 0x0f << 22;
			regs[0x38] = data;
		}
	}
	else
		dbglog(DBG_INFO, "240p TS: Using KOS Register defaults (settings)\n");
}

void ChangeResolution(int nvmode)
{
	ChangeResolutionEx(nvmode, 0);
}

void ChangeResolutionEx(int nvmode, int customValues)
{
	vuint32 *regs = (uint32*)0xA05F8000;

	// Skip useless video modes when in VGA
	vcable = vid_check_cable();
	if(vcable == CT_VGA && nvmode == VIDEO_240P)
		nvmode = VIDEO_480P;

	if(nvmode < VIDEO_240P)
		nvmode = VIDEO_240P;
	if(nvmode > VIDEO_480P)
		nvmode = VIDEO_480P_SL;
	if(nvmode > VIDEO_480P)
	{
		if(vcable != CT_VGA)
			nvmode = VIDEO_240P;
	}

	vmode = nvmode;

	switch(vmode)
	{
		case VIDEO_240P:
			W = 320;
			H = 240;
			dW = 320;
			dH = 240;
			offsetY = 0;
			IsPAL = 0;
			break;
		case VIDEO_288P:
			W = 320;
			H = 264;
			dW = 320;
			dH = 264;
			offsetY = 12;
			IsPAL = 1;
			break;
		case VIDEO_480I_A240:
		case VIDEO_480P_SL:
			W = 640;
			H = 480;
			dW = 320;
			dH = 240;
			offsetY = 0;
			IsPAL = 0;
			break;
		case VIDEO_576I_A264:
			W = 640;
			H = 480;
			dW = 320;
			dH = 240;
			offsetY = 0;
			IsPAL = 1;
			break;
		case VIDEO_480I:
		case VIDEO_480P:
			W = 640;
			H = 480;
			dW = 640;
			dH = 480;
			offsetY = 0;
			IsPAL = 0;
			break;
		case VIDEO_576I:
			W = 640;
			H = 480;
			dW = 640;
			dH = 480;
			offsetY = 0;
			IsPAL = 1;
			break;
	}

	AdjustVideoModes();
	ReleaseTextures();

	pvr_shutdown();
	if(customValues)
		dbglog(DBG_INFO, "240p TS: Using Test Video Modes\n");
	else
	{
		if(settings.UseKOSDefaults)
			dbglog(DBG_INFO, "240p TS: Using KOS default video modes\n");
		else
			dbglog(DBG_INFO, "240p TS: Using 240p Test Suite Video Modes\n");
	}
	switch(vmode)
	{
		case VIDEO_240P:
			//vid_set_mode(DM_320x240_NTSC, PM_RGB565); 
			if(!settings.UseKOSDefaults || customValues)
				vid_set_mode_ex(&custom_240); 
			else
				vid_set_mode(DM_320x240_NTSC, PM_RGB565);
			break;
		case VIDEO_288P:
			//vid_set_mode(DM_320x240_PAL, PM_RGB565); 
			if(!settings.UseKOSDefaults || customValues)
				vid_set_mode_ex(&custom_288); 
			else
				vid_set_mode(DM_320x240_PAL, PM_RGB565); 
			break;
		case VIDEO_576I:
		case VIDEO_576I_A264:
			//vid_set_mode(DM_768x576_PAL_IL, PM_RGB565); crashes the DC
			if(!settings.UseKOSDefaults || customValues)
				vid_set_mode_ex(&custom_576); 
			else
				vid_set_mode(DM_640x480_PAL_IL, PM_RGB565); 
			break;
		case VIDEO_480I_A240:
		case VIDEO_480P_SL:
		case VIDEO_480I:
		case VIDEO_480P:
			if(vcable == CT_VGA)
			{
				if(customValues)
					vid_set_mode_ex(&custom_vga);
				else
					vid_set_mode(DM_640x480_VGA, PM_RGB565); 
			}
			else
				vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565); 
			break;
	}

	regs[0x3A] |= 8;	/* Blank */
	regs[0x11] &= ~1;	/* Display disable */

	pvr_init_defaults();

	AdjustPVROptions();
	ChangeVideoRegisters();
	
	RefreshLoadedImages();
	
	// Enable display
	regs[0x3A] &= ~8;
	regs[0x11] |= 1;

	if(!settings.drawborder) // Draw back video signal limits?
		vid_border_color(0, 0, 0);
	else
		vid_border_color(255, 255, 255);
	
	if(!settings.EnablePALBG)
		pvr_set_bg_color(0.0f, 0.0f, 0.0f);
	else
		pvr_set_bg_color(settings.PalBackR, settings.PalBackG, settings.PalBackB);
}

double Toggle240p480i(int mode)
{
	uint64	start, end;

	start = timer_us_gettime64();
	
	// Skip useless video modes when in VGA
	if(vcable == CT_VGA)
		return 0.0f;

	ReleaseTextures();
	pvr_shutdown();

	if(!IsPAL)
	{
		if(mode == 0)
		{
			W = 320;
			H = 240;
			dW = 320;
			dH = 240;
			vmode = VIDEO_240P;

			if(!settings.UseKOSDefaults)
				vid_set_mode_ex(&custom_240); 
			else
				vid_set_mode(DM_320x240_NTSC, PM_RGB565); 
		}
		else
		{
			W = 640;
			H = 480;
			dW = 320;
			dH = 240;
			vmode = VIDEO_480I_A240;
	
			vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565); 
		}
	}
	else
	{
		if(mode == 0)
		{
			W = 320;
			H = 264;
			dW = 320;
			dH = 264;
			vmode = VIDEO_288P;

			if(!settings.UseKOSDefaults)
				vid_set_mode_ex(&custom_288); 
			else
				vid_set_mode(DM_320x240_PAL, PM_RGB565); 
		}
		else
		{
			W = 640;
			H = 480;
			dW = 320;
			dH = 240;
			offsetY = 0;
			vmode = VIDEO_576I_A264;
	
			if(!settings.UseKOSDefaults)
				vid_set_mode_ex(&custom_576); 
			else
				vid_set_mode(DM_640x480_PAL_IL, PM_RGB565); 
		}
	}

	pvr_init_defaults();

	AdjustPVROptions();
	ChangeVideoRegisters();
	RefreshLoadedImages();
	
	end = timer_us_gettime64();
	return((double)((end - start)/1000.0));
}

void GetVideoModeStr(char *res, int shortdesc)
{
	if(!shortdesc)
	{
		switch(vmode)
		{
			case VIDEO_240P:
				if(vcable != CT_VGA)
				{
					if(IsPALDC && vcable == CT_COMPOSITE)
						sprintf(res, "Video: 240p (PAL60)");
					else
						sprintf(res, "Video: 240p");
				}
				break;
			case VIDEO_480I_A240:
				if(vcable != CT_VGA)
				{
					if(IsPALDC && vcable == CT_COMPOSITE)
						sprintf(res, "Video: 480i (scaled 240p/PAL 60)");
					else
						sprintf(res, "Video: 480i (scaled 240p)");
				}
				break;
			case VIDEO_480I:
				if(vcable != CT_VGA)
				{
					if(IsPALDC && vcable == CT_COMPOSITE)
						sprintf(res, "Video: 480i (Scaling disabled/PAL 60)");
					else
						sprintf(res, "Video: 480i (Scaling disabled)");
				}
				break;
			case VIDEO_288P:
				sprintf(res, "Video: 288p");
				break;
			case VIDEO_576I:
				sprintf(res, "Video: 576i (Scaling disabled)");
				break;
			case VIDEO_576I_A264:
				sprintf(res, "Video: 576i (scaled 264p)");
				break;
			case VIDEO_480P:
				if(vcable == CT_VGA)
					sprintf(res, "Video: 480p (Scaling disabled)");
				break;
			case VIDEO_480P_SL:
				if(vcable == CT_VGA)
					sprintf(res, "Video: 480p (scaled 240p)");
				break;
		}
	}
	else
	{
		switch(vmode)
		{
			case VIDEO_240P:
				if(IsPALDC)
					sprintf(res, "[240p P60]");
				else
					sprintf(res, "[240p]");
				break;
			case VIDEO_480I:
				sprintf(res, "[480i 1:1]");
				break;
			case VIDEO_480I_A240:
				sprintf(res, "[480i 1:2]");
				break;
			case VIDEO_288P:
				sprintf(res, "[288p]");
				break;
			case VIDEO_576I:
				sprintf(res, "[576i 1:1]");
				break;
			case VIDEO_576I_A264:
				sprintf(res, "[576i 1:2]");
				break;
			case VIDEO_480P:
				sprintf(res, "[480p 1:1]");
				break;
			case VIDEO_480P_SL:
				sprintf(res, "[480p 1:2]");
				break;
		}
	}
}

char *GetPalStartText()
{
	switch(settings.PALStart)
	{
		case PAL_LINE23:
			return("Top");
		case PAL_CENTERED:
			return("Centered");
		case PAL_BOTTOM:
			return("Bottom");
	}
	return("");
}

void Set576iLine23Option(uint8 set)
{	
	if(set > PAL_BOTTOM)
		set = PAL_LINE23;
		
	switch(set)
	{
		case PAL_LINE23:
			custom_288.bitmapy = 22; // starts at 23
			custom_576.bitmapy = 21; // starts at 23
			break;
		case PAL_CENTERED:
			custom_288.bitmapy = 36; // starts in 37
			custom_576.bitmapy = 45; // starts in 47
			break;
		case PAL_BOTTOM:
			custom_288.bitmapy = 49; // starts in 47
			custom_576.bitmapy = 69; // starts in 71
			break;
	}

	settings.PALStart = set;
}

void AdjustVideoModes()
{
	if(IsPALDC)
	{
		// Adjust bitmapy for European DC
		if(vcable == CT_COMPOSITE)
		{
			// Starts at line 26 when in PAL, lower is invisible via composite	
			custom_240.bitmapy = 22; 
			if(settings.PALStart == PAL_BOTTOM)
				custom_288.bitmapy = 46;
		}
	}	
}

void PVRStats(char *msg)
{
#ifdef DCLOAD
	pvr_stats_t stats;

	pvr_get_stats(&stats);
	
	dbglog(DBG_INFO, "%s VBlanks %u  Frame last time: %u  Frame Rate: %f  Rendering Time: %u\n",
			msg ? msg : "",
			(unsigned int)stats.vbl_count,
			(unsigned int)stats.frame_last_time,
			(double)stats.frame_rate,
			(unsigned int)stats.rnd_last_time);
#else
	(void)msg;
#endif
}

#ifdef TEST_VIDEO

#include "vmu.h"
#include "controller.h"
#include "font.h"

void TestVideoMode(int mode)
{
	int 			done = 0, oldvmode = vmode;
	uint16			pressed, update = 0, sel = 1, showback = 1;		
	controller		*st;
	char			str[50];
	ImagePtr		back = NULL, rlines = NULL, black = NULL;
	vid_mode_t		test_mode;
	vid_mode_t		backup_mode;
	vid_mode_t		*source_mode = NULL;

	if(mode == VIDEO_240P)
		source_mode = &custom_240;
	if(mode == VIDEO_480I)
		source_mode = &custom_240;
	if(mode == VIDEO_480I_A240)
		source_mode = &custom_240;
	if(mode == VIDEO_576I_A264)
		source_mode = &custom_288;
	if(mode == VIDEO_576I)
		source_mode = &custom_576;
	if(mode == VIDEO_288P)
		source_mode = &custom_288;
	if(mode == VIDEO_480P)
		source_mode = &custom_vga;
	if(mode == VIDEO_480P_SL)
		source_mode = &custom_vga;

	if(!source_mode)
		return;

	test_mode = *source_mode;
	backup_mode = test_mode;

	/*
	if(vmode >= HIGH_RES)
	{
		back = LoadIMG("/rd/480/grid-480.kmg.gz", 0);
		if(back)
			back->scale = 0;
	}
	else
	{
		if(IsPAL)
			back = LoadIMG("/rd/gridPAL.kmg.gz", 0);
		else
			back = LoadIMG("/rd/grid.kmg.gz", 0);
	}
	*/

	black = LoadIMG("/rd/black.kmg.gz", 1);
	if(!black)
		return;

	if(vmode == VIDEO_240P || vmode == VIDEO_480I_A240 ||
		vmode == VIDEO_480P_SL)
	{
		// Use 240p Monoscope
		if(!back)
		{
			back = LoadIMG("/rd/monoscope.kmg.gz", 0);
			if(!back)
				return;
			rlines = LoadIMG("/rd/monoscope_lin.kmg.gz", 0);
			if(!rlines)
			{
				FreeImage(&back);
				return;
			}
		}
	}
	
	if(vmode == VIDEO_480I || vmode == VIDEO_480P)
	{
		back = LoadIMG("/rd/480/monoscope-480.kmg.gz", 0);
		if(!back)
			return;
		rlines = LoadIMG("/rd/480/monoscope-480_lin.kmg.gz", 0);
		if(!rlines)
		{
			FreeImage(&back);
			return;
		}
		back->scale = 0;
		rlines->scale = 0;
	}
	
	if(vmode == VIDEO_288P || vmode == VIDEO_576I_A264)
	{
		// Use PAL Monoscope
		if(!back)
		{
			back = LoadIMG("/rd/monoscopePAL.kmg.gz", 0);
			if(!back)
				return;
			rlines = LoadIMG("/rd/monoscopePAL_lin.kmg.gz", 0);
			if(!rlines)
			{
				FreeImage(&back);
				return;
			}
		}
	}
	
	if(vmode == VIDEO_576I)
	{
		back = LoadIMG("/rd/480/monoscopePAL576.kmg.gz", 0);
		if(!back)
			return;
		rlines = LoadIMG("/rd/480/monoscopePAL576_lin.kmg.gz", 0);
		if(!rlines)
		{
			FreeImage(&back);
			return;
		}
		back->scale = 0;
		rlines->scale = 0;
	}

	while(!done && !EndProgram) 
	{
		float	r = 1.0f;
		float	g = 1.0f;
		float	b = 1.0f;
		int 	c = 1;
		float	x = 40.0f;
		float	y = 15.0f;

		StartScene();
		if(showback)
		{
			DrawImage(black);
			DrawImage(back);
			DrawImage(rlines);
		}

		sprintf(str, " Width:     %d ", test_mode.width);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Height:    %d ", test_mode.height);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;

		DrawStringB(x, y, 0, 0,  0 , "                "); y+= fh;

		sprintf(str, " Scanlines: %d ", test_mode.scanlines);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Clocks:    %d ", test_mode.clocks);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;

		DrawStringB(x, y, 0, 0,  0 , "                "); y+= fh;

		sprintf(str, " Bitmap X:  %d ", test_mode.bitmapx);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Bitmap Y:  %d ", test_mode.bitmapy);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;

		DrawStringB(x, y, 0, 0,  0 , "                "); y+= fh;

		sprintf(str, " ScanInt1:  %d ", test_mode.scanint1);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " ScanInt2:  %d ", test_mode.scanint2);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;

		DrawStringB(x, y, 0, 0,  0 , "                "); y+= fh;

		sprintf(str, " Border X1: %d ", test_mode.borderx1);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Border X2: %d ", test_mode.borderx2);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Border Y1: %d ", test_mode.bordery1);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Border Y2: %d ", test_mode.bordery2);
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;

		DrawStringB(x, y, 0, 0,  0 , "                "); y+= fh;

		sprintf(str, " Draw Video Border:   %s ", settings.drawborder == 1 ? "yes" : " no");
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;
		sprintf(str, " Draw PVR Background: %s ", settings.EnablePALBG == 1 ? "yes" : " no");
		DrawStringB(x, y, r, sel == c ? 0 : g,  sel == c ? 0 : b, str); y += fh; c++;

		sprintf(str, " Video Border Size:               %dx%d ", test_mode.borderx2-test_mode.borderx1, test_mode.bordery2-test_mode.bordery1);
		DrawStringB(x, y, 0, 1.0f, 0.0f, str); y += fh; 

		sprintf(str, " Active Area Margin Left:         %d", test_mode.bitmapx - test_mode.borderx1);
		DrawStringB(x, y, 0, 1.0f, 0.0f, str); y += fh; 
		sprintf(str, " Active Area Margin Right:        %d", test_mode.clocks - test_mode.bitmapx - test_mode.width);
		DrawStringB(x, y, 0, 1.0f, 0.0f, str); y += fh; 
		sprintf(str, " Active Area Margin Top:          %d", test_mode.bitmapy);
		DrawStringB(x, y, 0, 1.0f, 0.0f, str); y += fh; 
		sprintf(str, " Active Area Margin Bottom:       %d", test_mode.scanlines - test_mode.bitmapy - test_mode.height);
		DrawStringB(x, y, 0, 1.0f, 0.0f, str); y += fh; 

		DrawStringB(100, 5, 0, 1.0f, 0.0f, "X to reset, Y to set"); y += fh; 
		EndScene();

		VMURefresh("VIDEO", "TEST");

		st = ReadController(0, &pressed);
		if(st)
		{
			if (pressed & CONT_B)
				done =	1;

			if (pressed & CONT_X)
			{
				test_mode = backup_mode;
				update = 1;
			}
						
			if (pressed & CONT_Y)
				update = 1;

			if (pressed & CONT_A)
				showback = !showback;

			if (pressed & CONT_DPAD_UP)
			{
				sel --;
				if(sel < 1)
					sel = c - 1;
			}

			if (pressed & CONT_DPAD_DOWN)
			{
				sel ++;
				if(sel > c - 1)
					sel = 1;
			}

			if (pressed & CONT_DPAD_LEFT)
			{
				switch(sel)
				{
					case 1:
						test_mode.width -=32;
						break;
					case 2:
						test_mode.height -=32;
						break;
					case 3:
						test_mode.scanlines -=32;
						break;
					case 4:
						test_mode.clocks --;
						break;
					case 5:
						test_mode.bitmapx --;
						break;
					case 6:
						test_mode.bitmapy --;
						break;
					case 7:
						test_mode.scanint1 --;
						break;
					case 8:
						test_mode.scanint2 --;
						break;
					case 9:
						test_mode.borderx1 --;
						break;
					case 10:
						test_mode.borderx2 --;
						break;
					case 11:
						test_mode.bordery1 --;
						break;
					case 12:
						test_mode.bordery2 --;
						break;
					case 13:
						settings.drawborder = !settings.drawborder;
						break;
					case 14:
						settings.EnablePALBG = !settings.EnablePALBG;
						break;
				}
			}

			if (pressed & CONT_DPAD_RIGHT)
			{
				switch(sel)
				{
					case 1:
						test_mode.width +=32;
						break;
					case 2:
						test_mode.height +=32;
						break;
					case 3:
						test_mode.scanlines +=32;
						break;
					case 4:
						test_mode.clocks ++;
						break;
					case 5:
						test_mode.bitmapx ++;
						break;
					case 6:
						test_mode.bitmapy ++;
						break;
					case 7:
						test_mode.scanint1 ++;
						break;
					case 8:
						test_mode.scanint2 ++;
						break;
					case 9:
						test_mode.borderx1 ++;
						break;
					case 10:
						test_mode.borderx2 ++;
						break;
					case 11:
						test_mode.bordery1 ++;
						break;
					case 12:
						test_mode.bordery2 ++;
						break;
					case 13:
						settings.drawborder = !settings.drawborder;
						break;
					case 14:
						settings.EnablePALBG = !settings.EnablePALBG;
						break;
				}
			}
		}

		if(update)
		{
			*source_mode = test_mode;
			ChangeResolutionEx(mode, 1);

			/*
			if(vmode == VIDEO_288P)
			{
				dbglog(DBG_INFO, "PVR 0x38: %lX\n", regs[0x38]);
				data = 0x05 | 0x3f << 8 | 0x31F << 12 | 0x1f << 22;
				dbglog(DBG_INFO, "Data to write %lX\n", data);
				regs[0x38] = data;
				dbglog(DBG_INFO, "PVR written at 0x38: %lX\n", regs[0x38]);

				dbglog(DBG_INFO, "PVR 0x34: %lX\n", regs[0x34]);
				data = 0x100 | 0x80 | 0x04;
				dbglog(DBG_INFO, "Data to write at 0x34  %lX\n", data);
				regs[0x34] = data;
				dbglog(DBG_INFO, "PVR written to 0x34: %lX\n", regs[0x34]);
			}

			if(vmode == VIDEO_576I || vmode == VIDEO_576I_A264)
			{
				dbglog(DBG_INFO, "PVR 0x38: %lX\n", regs[0x38]);
				data = 0x05 | 0x3f << 8 | 0x16A << 12 | 0x1f << 22;
				dbglog(DBG_INFO, "Data to write %lX\n", data);
				regs[0x38] = data;
				dbglog(DBG_INFO, "PVR written at 0x38: %lX\n", regs[0x38]);

				dbglog(DBG_INFO, "PVR 0x34: %lX\n", regs[0x34]);
				data = 0x100 | 0x80 | 0x04;
				dbglog(DBG_INFO, "Data to write at 0x34  %lX\n", data);
				regs[0x34] = data;
				dbglog(DBG_INFO, "PVR written to 0x34: %lX\n", regs[0x34]);
			}
			*/

			update = 0;
		}
	}

	FreeImage(&black);
	FreeImage(&back);
	FreeImage(&rlines);

	dbglog(DBG_INFO, "----------EXIT-------\n\n");
	ChangeResolution(oldvmode);
	sprintf(str, " Width:     %d\n", test_mode.width);
	dbglog(DBG_INFO, str);
	sprintf(str, " Height:    %d\n", test_mode.height);
	dbglog(DBG_INFO, str);

	sprintf(str, " Scanlines: %d\n", test_mode.scanlines);
	dbglog(DBG_INFO, str);
	sprintf(str, " Clocks:    %d\n", test_mode.clocks);
	dbglog(DBG_INFO, str);

	sprintf(str, " Bitmap X:  %d\n", test_mode.bitmapx);
	dbglog(DBG_INFO, str);
	sprintf(str, " Bitmap Y:  %d\n", test_mode.bitmapy);
	dbglog(DBG_INFO, str);

	sprintf(str, " ScanInt1:  %d\n", test_mode.scanint1);
	dbglog(DBG_INFO, str);
	sprintf(str, " ScanInt2:  %d\n", test_mode.scanint2);
	dbglog(DBG_INFO, str);

	sprintf(str, " Border X1: %d\n", test_mode.borderx1);
	dbglog(DBG_INFO, str);
	sprintf(str, " Border X2: %d\n", test_mode.borderx2);
	dbglog(DBG_INFO, str);
	sprintf(str, " Border Y1: %d\n", test_mode.bordery1);
	dbglog(DBG_INFO, str);
	sprintf(str, " Border Y2: %d\n", test_mode.bordery2);
	dbglog(DBG_INFO, str);
}

#endif
