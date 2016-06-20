#include "utils.h"

int fh = 8; // font height

void init_n64()
{
    init_interrupts();
	
	current_resolution = RESOLUTION_320x240;
	current_bitdepth = DEPTH_32_BPP;
	current_buffers = 2;
	current_gamma = GAMMA_NONE;
	current_antialias = ANTIALIAS_OFF;

	set_video();
	
    dfs_init(DFS_DEFAULT_LOCATION);
    rdp_init();
    controller_init();
    timer_init();
}

void DrawString(int x, int y, int r, int g, int b, char *str)
{
	uint32_t color = 0;

	color = graphics_make_color(r, g, b, 0xff);
	graphics_set_color(color, 0x00000000);
	graphics_draw_text(disp, x, y, str);
}

void DrawStringS(int x, int y, int r, int g, int b, char *str)
{
	uint32_t color = 0;

	color = graphics_make_color(r, g, b, 0xff);
    graphics_set_color(0x00000000, 0x00000000);
	graphics_draw_text(disp, x+2, y+2, str);
	graphics_set_color(color, 0x00000000);
	graphics_draw_text(disp, x, y, str);
}
