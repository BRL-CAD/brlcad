#ifndef SEEN_DM_COLOR
#define SEEN_DM_COLOR

extern unsigned long dm_get_pixel(unsigned char r, unsigned char g, unsigned char b, long unsigned int *pixels, int cd);
extern void dm_copy_default_colors();
extern void dm_allocate_color_cube();
#endif /* SEEN_DM_COLOR */
