/* 
 * 240p Test Suite
 * Copyright (C)2011-2022 Artemio Urbina
 *
 * This file is part of the 240p Test Suite
 *
 * The 240p Test Suite is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * The 240p Test Suite is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 240p Test Suite; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA	02111-1307	USA
 */

#include <kos.h>
#include <kmg/kmg.h>
#include <kos/img.h>
#include <zlib/zlib.h>
#include <assert.h>

#include "image.h"
#include "vmodes.h"
#include "menu.h"
#include "help.h"
#include "vmu.h"

#ifndef NO_DREAMEYE_DISP
#include <jpeg/jpeg.h>
#endif

#ifdef DCLOAD
int keep_link_alive = 0;
#endif

/**************DrawCylce*******************
Load Graphics
while()
	StartScene();
	DrawImage
	DrawString
	EndScene();
	VMURefresh
	ReadController
		Start -> ShowMenu
			Calls DrawMenu in EndScene
Release Graphics
*******************************************/

extern uint16 DrawMenu;
ImageMem Images[MAX_IMAGES];
uint8 UsedImages = 0;

void InitImages()
{
	uint8	i = 0;
	for(i = 0; i < MAX_IMAGES; i++)
	{
		Images[i].image = NULL;
		Images[i].state = MEM_RELEASED;
	}
	UsedImages = 0;
}

void CleanImages()
{
	uint8	i = 0;
	
	for(i = 0; i < MAX_IMAGES; i++)
	{
		if(Images[i].state != MEM_RELEASED)
		{
			if(Images[i].state == MEM_ERROR)
				dbglog(DBG_CRITICAL, "=== Found image with MEM_ERROR %s (releasing it) ===\n", Images[i].name);
			else
				dbglog(DBG_CRITICAL, "=== Found unreleased image %s (releasing it) ===\n", Images[i].name);

			FreeImage(&Images[i].image);
			Images[i].state = MEM_RELEASED;
			Images[i].name[0] = '\0';
			if(UsedImages)
				UsedImages --;
			else
				dbglog(DBG_CRITICAL, "=== CleanImages: Invalid used image index [%d] ===\n", i);
		}	
	}
}

void RefreshLoadedImages()
{
	uint8	i = 0;

	for(i = 0; i < MAX_IMAGES; i++)
	{
		if(Images[i].state == MEM_TEXRELEASED)
		{
			if(Images[i].name[0] == 'F' && Images[i].name[1] == 'B' 
				&& Images[i].name[2] == '\0')
			{
				if(!ReloadFBTexture())
				{
					dbglog(DBG_CRITICAL, "=== Could not reload FrameBuffer ===\n");
					Images[i].state = MEM_ERROR;
				}
				else
					Images[i].state = MEM_LOADED;
			}
			else
			{
				if(ReLoadIMG(Images[i].image, Images[i].name))
					Images[i].state = MEM_LOADED;
			}
		}	
	}
}

void ReleaseTextures()
{
	uint8	i = 0;

	for(i = 0; i < MAX_IMAGES; i++)
	{
		if(Images[i].state == MEM_LOADED)
		{
			if(!FreeImageData(&Images[i].image))
			{
				dbglog(DBG_CRITICAL, "=== Could not free image: %s ===\n", Images[i].name);
				Images[i].state = MEM_ERROR;
			}
			else
				Images[i].state = MEM_TEXRELEASED;
		}	
	}
}

void InsertImage(ImagePtr image, char *name)
{
	if(UsedImages < MAX_IMAGES)
	{
		uint8 i, placed = 0;

		for(i = 0; i < MAX_IMAGES && !placed; i++)
		{
			if(Images[i].state == MEM_RELEASED)
			{
				Images[i].image = image;
				Images[i].state = MEM_LOADED;
				strncpy(Images[i].name, name, PATH_LEN);
				UsedImages ++;
				placed = 1;
			}
		}
		if(!placed)
			dbglog(DBG_CRITICAL, "=== Image array full (NO FREE) ===\n");
	}
	else
		dbglog(DBG_CRITICAL, "=== Image array full ===\n");
}

void ReleaseImage(ImagePtr image)
{
	uint8	i = 0, deleted = 0;

	if(!image)
	{
		dbglog(DBG_CRITICAL, "=== Called ReleaseImage with a NULL pointer\n ===");
		return;
	}

	for(i = 0; i < MAX_IMAGES; i++)
	{
		if(Images[i].image == image)
		{
			Images[i].image = NULL;
			Images[i].state = MEM_RELEASED;
			Images[i].name[0] = '\0';
			if(UsedImages)
				UsedImages --;
			else
				dbglog(DBG_CRITICAL, "=== ReleaseImage: Invalid used image index ===\n");
			deleted = 1;
			
			break;
		}
	}

	if(!deleted)
		dbglog(DBG_CRITICAL, "=== Image not found for deletion ===\n");
}

int gkmg_to_img(const char * fn, kos_img_t * rv) {	
	kmg_header_t	hdr;
	int		dep;	
	//int length = 0;
	gzFile f;		

	assert( rv != NULL );

	/* Open the file */
	//length = zlib_getlength((char *)fn);	
	f = gzopen(fn, "r");
	if (!f) {		
		dbglog(DBG_ERROR, "gkmg_to_img: can't open file '%s'\n", fn);
		return -1;
	}  
		
	/* Read the header */
	if (gzread(f, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		gzclose(f);
		dbglog(DBG_ERROR, "gkmg_to_img: can't read header from file '%s'\n", fn);
		return -2;
	}

	/* Verify a few things */
	if (hdr.magic != KMG_MAGIC || hdr.version != KMG_VERSION ||
		hdr.platform != KMG_PLAT_DC)
	{
		gzclose(f);
		dbglog(DBG_ERROR, "gkmg_to_img: file '%s' is incompatible:\n"
			"   magic %08lx version %d platform %d\n",
			fn, hdr.magic, (int)hdr.version, (int)hdr.platform);
		return -3;
	}
	
	/* Setup the kimg struct */
	rv->w = hdr.width;
	rv->h = hdr.height;

	dep = 0;
	if (hdr.format & KMG_DCFMT_VQ)
		dep |= PVR_TXRLOAD_FMT_VQ;
	if (hdr.format & KMG_DCFMT_TWIDDLED)
		dep |= PVR_TXRLOAD_FMT_TWIDDLED;

	switch (hdr.format & KMG_DCFMT_MASK) {
	case KMG_DCFMT_RGB565:
		rv->fmt = KOS_IMG_FMT(KOS_IMG_FMT_RGB565, dep);
		break;

	case KMG_DCFMT_ARGB4444:
		rv->fmt = KOS_IMG_FMT(KOS_IMG_FMT_ARGB4444, dep);
		break;

	case KMG_DCFMT_ARGB1555:
		rv->fmt = KOS_IMG_FMT(KOS_IMG_FMT_ARGB1555, dep);
		break;

	case KMG_DCFMT_YUV422:
		rv->fmt = KOS_IMG_FMT(KOS_IMG_FMT_YUV422, dep);
		break;

	case KMG_DCFMT_BUMP:
		/* XXX */
		rv->fmt = KOS_IMG_FMT(KOS_IMG_FMT_RGB565, dep);
		break;

	case KMG_DCFMT_4BPP_PAL:
	case KMG_DCFMT_8BPP_PAL:
	default:
		assert_msg( 0, "currently-unsupported KMG pixel format" );
		gzclose(f);
		if(rv->data)
			free(rv->data);
		return -5;
	}
	
	rv->byte_count = hdr.byte_count;

	/* And load the rest  */
	
	rv->data = malloc(hdr.byte_count);
	if (!rv->data) {
		dbglog(DBG_ERROR, "gkmg_to_img: can't malloc(%d) while loading '%s'\n",
			(int)hdr.byte_count, fn);
		gzclose(f);
		return -4;
	}
	if (gzread(f, rv->data, rv->byte_count) != (int)rv->byte_count) {
		dbglog(DBG_ERROR, "gkmg_to_img: can't read %d bytes while loading '%s'\n",
			(int)hdr.byte_count, fn);
		gzclose(f);
		free(rv->data);
		return -6;
	}

	/* Ok, all done */
	gzclose(f);

	/* If the byte count is not a multiple of 32, bump it up as well.
		 This is for DMA/SQ usage. */
	rv->byte_count = (rv->byte_count + 31) & ~31;
	
	return 0;
}

typedef struct dtex_header_st {
	uint8_t  magic[4]; // 'DTEX'
	uint16_t width;
	uint16_t height;
	uint32_t type;
	uint32_t size;
} dtex_header;

typedef struct dpal_header_st {
	char		id[4];	// 'DPAL'
	uint32_t	numcolors;
} dpal_header;
// It is then followed by 'numcolors' 32-bit packed ARGB values.

int load_palette(const char *fn, pallette *pal)
{
	FILE 		*fp = NULL;
	dpal_header	hdr;
	
	if(!pal)
		return 0;
		
	pal->numcolors = 0;
	pal->colors = NULL;
	fp = fopen(fn, "r");
	if(!fp)
	{
		dbglog(DBG_ERROR, "Could not load %s file\n", fn);
		return 0;
	}
	
	if(fread(&hdr, sizeof(dpal_header), 1, fp) != 1)
	{
		dbglog(DBG_ERROR, "Could not load header from %s file\n", fn);
		return 0;
	}
	
	if(memcmp(hdr.id, "DPAL", 4) || hdr.numcolors == 0)
	{
		fclose(fp);
		dbglog(DBG_ERROR, "load_palette: file '%s' is incompatible:\n"
			"	magic %s num_colors %ld\n",
			fn, hdr.id, hdr.numcolors);
		return 0;
	}
	
	pal->colors = (uint32_t*)malloc(sizeof(uint32_t)*hdr.numcolors);
	if(!pal->colors)
	{
		fclose(fp);
		dbglog(DBG_ERROR, "load_palette: file '%s' could not be stored in RAM\n", fn);
		return 0;
	}
	
	if(fread(pal->colors, sizeof(uint32_t), hdr.numcolors, fp) != hdr.numcolors)
	{
		fclose(fp);
		dbglog(DBG_ERROR, "load_palette: file '%s' could not be loaded to RAM\n", fn);
		return 0;
	}
	fclose(fp);
	
	pal->numcolors = hdr.numcolors;
	return 1;
}

void set_palette(pallette *pal)
{
	uint32_t p = 0;
	
	if(!pal || !pal->numcolors || !pal->colors)
		return;
	
	pvr_set_pal_format(PVR_PAL_ARGB8888);
	for(p = 0; p < pal->numcolors; p++)
		pvr_set_pal_entry(p, pal->colors[p]);
}

void clear_palette()
{
	uint32_t p = 0, black = 0;
	
	black = PACK_ARGB8888(255, 0, 0, 0);
	for(p = 0; p < 256; p++)
		pvr_set_pal_entry(p, black);
}

void release_palette(pallette *pal)
{
	if(!pal)
		return;
	
	if(pal->colors)
	{
		free(pal->colors);
		pal->colors = NULL;
	}
	pal->numcolors = 0;
}

// GZipped DTEXT 
int dtex_to_img(const char * fn, kos_img_t * rv) {	
	dtex_header	hdr;
	int		dep = 0;	
	//int length = 0;
	gzFile f;		

	assert( rv != NULL );

	/* Open the file */
	//length = zlib_getlength((char *)fn);	
	f = gzopen(fn, "r");
	if (!f) {		
		dbglog(DBG_ERROR, "dtex_to_img: can't open file '%s'\n", fn);
		return -1;
	}  
		
	/* Read the header */
	if (gzread(f, &hdr, sizeof(hdr)) != sizeof(hdr)) {
		gzclose(f);
		dbglog(DBG_ERROR, "dtex_to_img: can't read header from file '%s'\n", fn);
		return -2;
	}

	/* Verify it is an 8 bit palette dtex file */
	if(memcmp(hdr.magic, "DTEX", 4) || hdr.type != 0x30000000)
	{
		gzclose(f);
		dbglog(DBG_ERROR, "dtex_to_img: file '%s' is incompatible:\n"
			"   magic %s\n",
			fn, hdr.magic);
		return -3;
	}
	
	/* Setup the kimg struct */
	rv->w = hdr.width;
	rv->h = hdr.height;

	dep |= PVR_TXRLOAD_FMT_TWIDDLED;
	rv->fmt = KOS_IMG_FMT(KOS_IMG_FMT_PAL8BPP, dep);
	rv->byte_count = hdr.size;

	/* And load the rest  */
	
	rv->data = malloc(hdr.size);
	if (!rv->data) {
		dbglog(DBG_ERROR, "dtex_to_img: can't malloc(%d) while loading '%s'\n",
			(int)hdr.size, fn);
		gzclose(f);
		return -4;
	}
	if (gzread(f, rv->data, rv->byte_count) != (int)rv->byte_count) {
		dbglog(DBG_ERROR, "dtex_to_img: can't read %d bytes while loading '%s'\n",
			(int)hdr.size, fn);
		gzclose(f);
		free(rv->data);
		return -6;
	}

	/* Ok, all done */
	gzclose(f);

	/* If the byte count is not a multiple of 32, bump it up as well.
		 This is for DMA/SQ usage. */
	rv->byte_count = (rv->byte_count + 31) & ~31;
	
	return 0;
}

void IgnoreOffset(ImagePtr image)
{
	if(image)
		image->IgnoreOffsetY = 1;
}

void UseDirectColor(ImagePtr image, uint16 a, uint16 r, uint16 g, uint16 b)
{
	if(!image)
		return;
	image->use_direct_color = 1;
	
	image->a_direct = a;
	image->r_direct = r;
	image->g_direct = g;
	image->b_direct = b;
}

#include <kos.h>
#include <string.h>

#ifndef NO_DREAMEYE_DISP
unsigned int next_pow2(unsigned int x) {
	if (x == 0)
		return 1;
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;
	return x;
}

int expand_canvas(const kos_img_t *src, kos_img_t *out, uint16 bg_color) {
	uint32 dst_w, dst_h;

	if (!src || !out)
		return 0;

	// convert to next power of 2
	dst_w = next_pow2(src->w);
	dst_h = next_pow2(src->h);
	out->w = dst_w;
	out->h = dst_h;
	out->fmt = KOS_IMG_FMT(KOS_IMG_FMT_ARGB1555, 0);
	out->byte_count = dst_w * dst_h * sizeof(uint16);

	out->data = malloc(out->byte_count);
	if (!out->data)
	{
		out->w = out->h = 0;
		out->byte_count = 0;
		return 0;
	}

	uint16 *dst = (uint16 *)out->data;
	uint16 *src_data = (uint16 *)src->data;

	// Fill bg
	for (uint32 i = 0; i < dst_w * dst_h; i++)
		dst[i] = bg_color;

	// Copy source
	for (uint32 y = 0; y < src->h; y++)
		memcpy(dst + y * dst_w, src_data + y * src->w, src->w * sizeof(uint16));

	// Convert from RGB565 to ARGB1555
	for(uint32_t i = 0; i < out->byte_count/2; i++) {
		uint16_t rgb = dst[i];

		uint16_t red   = (rgb >> 11) & 0x1F;
		uint16_t green = (rgb >> 5) & 0x3F;
		uint16_t blue  = rgb & 0x1F;

		// Convert green from 6 bits to 5 bits
		green = green >> 1;

		// Alpha set to 1 (opaque)
		dst[i] = (1 << 15) | (red << 10) | (green << 5) | blue;
	}

	return 1;
}
#endif


ImagePtr LoadIMG(const char *filename, int maptoscreen)
{	
	int load = -1, len = 0, dtext888 = 0;
	ImagePtr image = NULL;
	kos_img_t img;
	file_t testFile;
#ifdef BENCHMARK
	uint64	start, end;

	start = timer_us_gettime64();
#endif

	testFile = fs_open(filename, O_RDONLY);
	if(testFile == -1)
	{
		dbglog(DBG_ERROR, "Could not find image file \"%s\" in filesystem\n", filename);
		return(NULL);
	}
	fs_close(testFile);

	image = (ImagePtr)malloc(sizeof(struct image_st));
	if(!image)
	{
		dbglog(DBG_ERROR, "Could not malloc image struct %s\n", filename);
		return(NULL);
	}
	
	len = strlen(filename);
	if(len > 3)
	{
		if(filename[len - 3] == '.' && filename[len - 2] == 'g' && filename[len - 1] == 'z')
			load = gkmg_to_img(filename, &img);
		if(filename[len - 3] == 'k' && filename[len - 2] == 'm' && filename[len - 1] == 'g')
			load = kmg_to_img(filename, &img);
		if(filename[len - 3] == 'd' && filename[len - 2] == 'g' && filename[len - 1] == 'z')
		{
			load = dtex_to_img(filename, &img);
			dtext888 = 1;
		}
#ifdef USE_PNG
		if(filename[len - 3] == 'p' && filename[len - 2] == 'n' && filename[len - 1] == 'g')
			load = png_to_img(filename, PNG_MASK_ALPHA, &img);
#endif
#ifndef NO_DREAMEYE_DISP
		if(filename[len - 3] == 'j' && filename[len - 2] == 'p' && filename[len - 1] == 'g')
		{
			kos_img_t original;

			load = jpeg_to_img(filename, 1, &original);
			if(load == 0)
			{
				/* We only use these for DreamEye, so expand that to 1024x512 that they load */
				if(!expand_canvas(&original, &img, 0))
				{
					dbglog(DBG_ERROR, "Could not expand jpeg to tex dimensions: %s\n", filename);
					load = 1;
				}
				kos_img_free(&original, 0);
			}
		}
#endif
	}

	if(load != 0)
	{
		free(image);
		dbglog(DBG_ERROR, "Could not load %s\n", filename);
		return(NULL);
	}

	image->tex = pvr_mem_malloc(img.byte_count);
	if(!image->tex)
	{
		free(image);
		kos_img_free(&img, 0);	
		dbglog(DBG_ERROR, "Could not load %s to VRAM\n", filename);
		return(NULL);
	}

	pvr_txr_load_kimg(&img, image->tex, 0);
	kos_img_free(&img, 0);	

#ifdef BENCHMARK
	end = timer_us_gettime64();
	dbglog(DBG_INFO, "KMG %s took %g ms\n", filename, (double)(end - start)/1000.0);
#endif

	image->r = 1.0f;
	image->g = 1.0f;
	image->b = 1.0f;

	image->tw = img.w;
	image->th = img.h;
	image->x = 0;
	image->y = 0;
	image->u1 = 0.0f;
	image->v1 = 0.0f;
	image->u2 = 1.0f;
	image->v2 = 1.0f;
	image->layer = 1.0f;
	image->scale = 1;
	image->alpha = 1.0;
	image->w = image->tw;
	image->h = image->th;
	image->FH = 0;
	image->FV = 0;
	image->texFormat = dtext888 ? PVR_TXRFMT_PAL8BPP : PVR_TXRFMT_ARGB1555;
	image->IgnoreOffsetY = 0;
	
	image->use_direct_color = 0;
	image->a_direct = 0xff;
	image->r_direct = 0xff;
	image->g_direct = 0xff;
	image->b_direct = 0xff;

	if(maptoscreen)
	{
		if(image->w < dW && image->w != 8)
			CalculateUV(0, 0, 320, IsPAL ? 264 : 240, image);
		else
			CalculateUV(0, 0, dW, dH, image);

		IgnoreOffset(image);
	}

	InsertImage(image, (char*)filename);

	return image;
}

uint8 ReLoadIMG(ImagePtr image, const char *filename)
{	
	int load = -1, len = 0;
	kos_img_t img;
#ifdef BENCHMARK
	uint64	start, end;
		
	start = timer_us_gettime64();
#endif
	if(!image)
	{
		dbglog(DBG_ERROR, "Invalid image pointer for reload %s\n", filename);
		return 0;
	}
	
	if(image->tex)
	{
		dbglog(DBG_ERROR, "Found unreleased texture while reloading %s\n", filename);
		return 0;
	}

	len = strlen(filename);
	if(len > 3)
	{
		if(filename[len - 3] == '.' && filename[len - 2] == 'g' && filename[len - 1] == 'z')
			load = gkmg_to_img(filename, &img);
		if(filename[len - 3] == 'k' && filename[len - 2] == 'm' && filename[len - 1] == 'g')
			load = kmg_to_img(filename, &img);
		if(filename[len - 3] == 'd' && filename[len - 2] == 'g' && filename[len - 1] == 'z')
			load = dtex_to_img(filename, &img);
#ifdef USE_PNG
		if(filename[len - 3] == 'p' && filename[len - 2] == 'n' && filename[len - 1] == 'g')
			load = png_to_img(filename, PNG_MASK_ALPHA, &img);	
#endif
#ifndef NO_DREAMEYE_DISP
		if(filename[len - 3] == 'j' && filename[len - 2] == 'p' && filename[len - 1] == 'g')
		{
			kos_img_t original;

			load = jpeg_to_img(filename, 1, &original);
			if(load == 0)
			{
				/* We only use these for DreamEye, so expand that to 1024x512 that they load */
				if(!expand_canvas(&original, &img, 0))
				{
					dbglog(DBG_ERROR, "Could not expand jpeg to tex dimensions: %s\n", filename);
					load = 1;
				}
				kos_img_free(&original, 0);
			}
		}
#endif
	}

	if(load != 0)
	{
		dbglog(DBG_ERROR, "Could not load %s\n", filename);
		return 0;
	}

	image->tex = pvr_mem_malloc(img.byte_count);
	if(!image->tex)
	{
		dbglog(DBG_ERROR, "Could not load %s to VRAM\n", filename);
		kos_img_free(&img, 0);	
		return 0;
	}

	pvr_txr_load_kimg(&img, image->tex, 0);
	kos_img_free(&img, 0);	

#ifdef BENCHMARK
	end = timer_us_gettime64();
	dbglog(DBG_INFO, "KMG %s took %g ms\n", filename, (double)(end - start)/1000.0);
#endif
	return 1;
}

void FreeImage(ImagePtr *image)
{	
	if(*image)
	{
		ReleaseImage(*image);
		FreeImageData(image);
		free(*image);
		*image = NULL;
	}
	else
		dbglog(DBG_CRITICAL, "=== Called FreeImage with a NULL pointer ===\n");
}

uint8 FreeImageData(ImagePtr *image)
{	
	if(*image)
	{
		if((*image)->tex)
		{
			pvr_mem_free((*image)->tex);
			(*image)->tex = NULL;
			return 1;
		}
	}
	return 0;
}

void CalculateUV(float posx, float posy, float width, float height, ImagePtr image)
{
	if(!image)
		return;
	
	// These just keep the numbers clean
	if(posx > image->tw)
		posx = posx - image->tw;
	if(posx < -1*image->tw)
		posx = posx + image->tw;

	if(posy > image->th)
		posy = posy - image->th;
	if(posy < -1*image->th)
		posy = posy + image->th;

	image->w = width;
	image->h = height;
	image->u1 = posx/image->tw;
	image->v1 = posy/image->th;
	image->u2 = (posx + width)/image->tw;
	image->v2 = (posy + height)/image->th;
}

void FlipH(ImagePtr image, uint16 flip)
{
	if(!image)
		return;

	if((!image->FH && flip) || (image->FH && !flip))
	{
		float u1;
	
		u1 = image->u1;
		image->u1 = image->u2;	
		image->u2 = u1;	

		image->FH = !image->FH;
	}
}

void FlipV(ImagePtr image, uint16 flip)
{
	if(!image)
		return;

	if((!image->FV && flip) || (image->FV && !flip))
	{
		float v1;
	
		v1 = image->v1;
		image->v1 = image->v2;	
		image->v2 = v1;	
		
		image->FV = !image->FV;
	}
}

void FlipHV(ImagePtr image, uint16 flip)
{
	FlipH(image, flip);
	FlipV(image, flip);
}

void DrawImage(ImagePtr image)
{ 	
	float x, y, w, h; 

	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_vertex_t vert;
	
	if(!image || !image->tex)
		return;

	x = image->x;
	y = image->y;
	w = image->w;
	h = image->h;
	
	// Center display vertically in PAL modes, since images are mostly NTSC
	if(!image->IgnoreOffsetY)
		y+= offsetY;

	if(image->scale && (vmode == VIDEO_480P_SL
			|| vmode == VIDEO_480I_A240
			|| vmode == VIDEO_576I_A264))
	{
		x *= 2;
		y *= 2;
		w *= 2;
		h *= 2;
	}
		
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, image->texFormat, image->tw, image->th, image->tex, PVR_FILTER_NONE);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

#ifdef DCLOAD
	if(image->use_direct_color && image->alpha != 1.0f)
		dbglog(DBG_WARNING, "=== Image uses direct color and has an alpha float value  ===\n");
#endif

	if(image->use_direct_color)
		vert.argb = PACK_ARGB8888(image->a_direct, image->r_direct, image->g_direct, image->b_direct);		
	else
		vert.argb = PVR_PACK_COLOR(image->alpha, image->r, image->g, image->b);
	
	vert.oargb = 0;
	vert.flags = PVR_CMD_VERTEX;
	
	vert.x = x;
	vert.y = y;
	vert.z = image->layer;
	vert.u = image->u1;
	vert.v = image->v1;
	pvr_prim(&vert, sizeof(vert));
	
	vert.x = x + w;
	vert.y = y;
	vert.z = image->layer;
	vert.u = image->u2;
	vert.v = image->v1;
	pvr_prim(&vert, sizeof(vert));
	
	vert.x = x;
	vert.y = y + h;
	vert.z = image->layer;
	vert.u = image->u1;
	vert.v = image->v2;
	pvr_prim(&vert, sizeof(vert));
	
	vert.x = x + w;
	vert.y = y + h;
	vert.z = image->layer;
	vert.u = image->u2;
	vert.v = image->v2;
	vert.flags = PVR_CMD_VERTEX_EOL;
	pvr_prim(&vert, sizeof(vert));
}

#define RAD_CONV 0.0174532925199

void DrawImageRotate(ImagePtr image, float angle)
{ 	
	float x, y, w, h; 

	pvr_poly_cxt_t cxt;
	pvr_poly_hdr_t hdr;
	pvr_vertex_t vert, verttx;
	
	if(!image || !image->tex)
		return;

	angle *= RAD_CONV;

	x = image->x;
	y = image->y;
	w = image->w;
	h = image->h;

	// Center display vertically in PAL modes, since images are mostly NTSC
	if(!image->IgnoreOffsetY)
		y+= offsetY;

	if(image->scale && (vmode == VIDEO_480P_SL
			|| vmode == VIDEO_480I_A240
			|| vmode == VIDEO_576I_A264))
	{
		x *= 2;
		y *= 2;
		w *= 2;
		h *= 2;
	}

	mat_identity(); 
	mat_translate(x + (w/2), y + (h/2), 0);
	mat_rotate_z(angle); 
		
	pvr_poly_cxt_txr(&cxt, PVR_LIST_TR_POLY, image->texFormat, image->tw, image->th, image->tex, PVR_FILTER_NONE);
	pvr_poly_compile(&hdr, &cxt);
	pvr_prim(&hdr, sizeof(hdr));

#ifdef DCLOAD
	if(image->use_direct_color && image->alpha != 1.0f)
		dbglog(DBG_WARNING, "=== Image uses direct color and has an alpha float value  ===\n");
#endif

	if(image->use_direct_color)
		vert.argb = PACK_ARGB8888(image->a_direct, image->r_direct, image->g_direct, image->b_direct);		
	else
		vert.argb = PVR_PACK_COLOR(image->alpha, image->r, image->g, image->b);

	vert.oargb = 0;
	vert.flags = PVR_CMD_VERTEX;
	
	vert.x = -(w/2);
	vert.y = -(h/2);
	vert.z = image->layer;
	vert.u = image->u1;
	vert.v = image->v1;
	mat_transform_sq(&vert, &verttx, 1);
	pvr_prim(&verttx, sizeof(verttx));
	
	vert.x = w/2;
	vert.y = -(h/2);
	vert.z = image->layer;
	vert.u = image->u2;
	vert.v = image->v1;
	mat_transform_sq(&vert, &verttx, 1);
	pvr_prim(&verttx, sizeof(verttx));
	
	vert.x = -(w/2);
	vert.y = h/2;
	vert.z = image->layer;
	vert.u = image->u1;
	vert.v = image->v2;
	mat_transform_sq(&vert, &verttx, 1);
	pvr_prim(&verttx, sizeof(verttx));
	
	vert.x = w/2;
	vert.y = h/2;
	vert.z = image->layer;
	vert.u = image->u2;
	vert.v = image->v2;
	vert.flags = PVR_CMD_VERTEX_EOL;
	mat_transform_sq(&vert, &verttx, 1);
	pvr_prim(&verttx, sizeof(verttx));
}

inline void StartScene()
{
	pvr_scene_begin();
	pvr_list_begin(PVR_LIST_TR_POLY);
}

inline void EndScene()
{
	DrawScanlines();
	pvr_list_finish();
	pvr_scene_finish();
	pvr_wait_ready();

	if(DrawMenu)
	{
		int	type = 0;
		
		type = DrawMenu;
		DrawMenu = 0;
		
		if(type == FB_MENU_NORMAL)
			DrawShowMenu();
		if(type == FB_MENU_HELP)
			HelpWindow(HelpData, fbtexture);
		if(type == FB_MENU_CREDITS)
			DrawCredits(fbtexture);

		FreeTextureFB();
	}
	
	if(vmu_found_bad_lcd_vmu())
		ShowLCDVMUWarning();
		
	sleep_VMU_cycle();
	
	// Check if cable was hotswapped
	vcable = vid_check_cable();
	if(ovcable != vcable)
	{
		switch(vcable)
		{
			case CT_VGA:
				ChangeResolution(VIDEO_480P_SL);
				break;
			case CT_RGB:
				ChangeResolution(VIDEO_240P);
				break;
			case CT_COMPOSITE:
			default:
				if(IsPALDC)
					ChangeResolution(VIDEO_288P);
				else
					ChangeResolution(VIDEO_240P);
				break;
		}
	}
	ovcable = vcable;
	
#ifdef DCLOAD
	// Ping dc-tool link every 1800 frames
	if(keep_link_alive++ > 1800)
	{
		PVRStats("ping");
		keep_link_alive = 0;
	}
#endif
}
