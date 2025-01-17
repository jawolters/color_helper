#ifndef COLOR_DETECT_H_INCLUDED
#define COLOR_DETECT_H_INCLUDED

typedef struct {
    // RGB color space
    int r;
    int g;
    int b;

    // YUV color space
    int y;
    int u;
    int v;

    char name[60];
} color;

void yuv_from_rgb( int* y, int* u, int* v, int r, int g, int b );
void hsv_from_rgb( float* h, float* s, float* v, int r, int g, int b );
void hsl_from_hsv( float* hsl_h, float* hsl_s, float* hsl_l, float hsv_h, float hsv_s, float hsv_v );
color nearest_color( int r, int g, int b, color* colors, int ncolors );
int read_colors( color** color_list, unsigned char* buffer, size_t bufferSize, int* max_colors );

#endif /*COLOR_DETECT_H_INCLUDED*/
