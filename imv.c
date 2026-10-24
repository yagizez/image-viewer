#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

// TODO:
// - Moving the image around with lmb
// - Integer zooming with ctrl + scroll

// "Plain" PPM formatted images consist of the following bytes:
// 1. "P3" (magic number) followed by a newline
// 2. Comment(s) that start with an "#" and end with a newline (possible to have no comments)
// 3. Width and height separated by a space, followed by a newline
// 4. Max saturation value (at most 255), followed by a newline
// 5. The raster is represented by 3 ASCII decimal numbers in order of red, green and blue
//    separated by a whitespace, no line in the raster can be longer than 70 characters

// "Raw" PPM formatted images consist of the following bytes:
// 1. "P6" (magic number) followed by a newline
// 2. Comment(s) that start with an "#" and end with a newline (possible to have no comments)
// 3. Width and height separated by a space, followed by a newline
// 4. Max saturation value (at most 255), followed by a newline
// 5. The raster is represented by 3 bytes in order of red, green and blue

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

SDL_Window *create_window(const char *win_info, int img_w, int img_h, int win_w, int win_h)
{
    char window_title[96];
    snprintf(window_title, sizeof(window_title), "%s - %dx%d", win_info, img_w, img_h);

    SDL_Window *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, SDL_WINDOW_RESIZABLE);
    return (window);
}

void set_pixel_32bit(SDL_Surface *surface, int x, int y, Uint32 color)
{
    Uint32 *pixels = surface->pixels;
    pixels[(y * (surface->pitch / sizeof(Uint32))) + x] = color;
}

void set_image_rect(SDL_Rect *rect, int img_w, int img_h, int win_w, int win_h)
{
    rect->x = MAX(0, (img_w - win_w) / 2);
    rect->y = MAX(0, (img_h - win_h) / 2);
    rect->w = MIN(img_w, win_w);
    rect->h = MIN(img_h, win_h);
}

void set_display_rect(SDL_Rect *rect, int img_w, int img_h, int win_w, int win_h)
{
    rect->x = MAX(0, (win_w - img_w) / 2);
    rect->y = MAX(0, (win_h - img_h) / 2);
    rect->w = MIN(img_w, win_w);
    rect->h = MIN(img_h, win_h);
}

void render_window(SDL_Window *window, SDL_Surface *image)
{
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h); // Get window size
    SDL_Surface *pwinsurf = SDL_GetWindowSurface(window); // Get window surface

    Uint32 background_color = SDL_MapRGB(pwinsurf->format, 31, 31, 31);
    SDL_FillRect(pwinsurf, NULL, background_color); // Fill window surface with background color

    SDL_Rect image_rect; // Rect of how much of the image is visible on window
    set_image_rect(&image_rect, image->w, image->h, win_w, win_h);
    SDL_Rect display_rect; // Rect of where the image is displayed on window
    set_display_rect(&display_rect, image->w, image->h, win_w, win_h);

    SDL_BlitScaled(image, &image_rect, pwinsurf, &display_rect); // Blit image to window surface and keep it at the middle

    SDL_UpdateWindowSurface(window);
}

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 2)
    {
        fprintf(stderr, "Missing filename parameter\n");
        return 1;
    }

    // Run on Xwayland
    SDL_SetHint(SDL_HINT_VIDEODRIVER, "x11");

    // Initialize SDL2 subsystem(s)
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "SDL_InitSubSystem failed: %s\n", SDL_GetError());
        return 1;
    }

    // Open image file
    FILE *pimg = fopen(argv[1], "r");
    if (pimg == NULL)
    {
        perror("Can't open image file");
        return 1;
    }

    // Define supported image formats
    typedef enum ImageFormat {
        PPM_P3,
        PPM_P6,
    } ImageFormat;
    ImageFormat imgf;

    // Get image format
    uint8_t magic_nums[8];
    fread(magic_nums, 1, sizeof(magic_nums), pimg);
    rewind(pimg);
    if (memcmp(magic_nums, (uint8_t[]){0x50, 0x33}, 2) == 0)
    {
        imgf = PPM_P3;
    }
    else if (memcmp(magic_nums, (uint8_t[]){0x50, 0x36}, 2) == 0)
    {
        imgf = PPM_P6;
    }
    else
    {
        fprintf(stderr, "Can't open image file (incompatible)\n");
        fclose(pimg);
        return 1;
    }

    SDL_Window *pwin; // Declare pointer to window
    SDL_Surface *pimgsurf; // Declare pointer to surface that will store the image

    switch (imgf)
    {
        case PPM_P3:
        {
            // PPM "P3" file variables
            uint8_t magic_num[2];
            int img_width, img_height;
            int max_saturation;

            fread(magic_num, 1, sizeof(magic_num), pimg); // Get magic number
            fgetc(pimg); // Skip trailing '\n' or space

            // Skip comment(s)
            int ch;
            while ((ch = fgetc(pimg)) == '#')
                while (fgetc(pimg) != '\n');
            ungetc(ch, pimg);

            fscanf(pimg, "%d %d", &img_width, &img_height); // Get image width and height
            fgetc(pimg); // Skip trailing '\n' or space

            fscanf(pimg, "%d", &max_saturation); // Get max saturation value
            fgetc(pimg); // Skip trailing '\n'

            // Create window
            pwin = create_window("Plain PPM image", img_width, img_height, img_width, img_height);

            // Create image surface
            pimgsurf = SDL_CreateRGBSurfaceWithFormat(0, img_width, img_height, 32, SDL_PIXELFORMAT_RGB888);

            // Lock surface for pixel manipulation
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_LockSurface(pimgsurf);

            // Fill surface with image
            for (int row = 0; row < img_height; row++)
            {
                for (int col = 0; col < img_width; col++)
                {
                    Uint8 r, g, b;
                    fscanf(pimg, "%hhu %hhu %hhu", &r, &g, &b);
                    Uint32 color = SDL_MapRGB(pimgsurf->format, r, g, b);
                    set_pixel_32bit(pimgsurf, col, row, color);
                }
            }

            // Unlock surface
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_UnlockSurface(pimgsurf);

            render_window(pwin, pimgsurf);

            break;
        }
        case PPM_P6:
        {
            // PPM "P6" file variables
            uint8_t magic_num[2];
            int img_width, img_height;
            int max_saturation;

            fread(magic_num, 1, sizeof(magic_num), pimg); // Get magic number
            fgetc(pimg); // Skip trailing '\n' or space

            // Skip comment(s)
            int ch;
            while ((ch = fgetc(pimg)) == '#')
                while (fgetc(pimg) != '\n');
            ungetc(ch, pimg);

            fscanf(pimg, "%d %d", &img_width, &img_height); // Get image width and height
            fgetc(pimg); // Skip trailing '\n' or space

            fscanf(pimg, "%d", &max_saturation); // Get max saturation value
            fgetc(pimg); // Skip trailing '\n'

            // Create window
            pwin = create_window("Raw PPM image", img_width, img_height, img_width, img_height);

            // Create image surface
            pimgsurf = SDL_CreateRGBSurfaceWithFormat(0, img_width, img_height, 32, SDL_PIXELFORMAT_RGB888);

            // Lock surface for pixel manipulation
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_LockSurface(pimgsurf);

            // Fill surface with image
            for (int row = 0; row < img_height; row++)
            {
                for (int col = 0; col < img_width; col++)
                {
                    Uint8 r = fgetc(pimg), g = fgetc(pimg), b = fgetc(pimg);
                    Uint32 color = SDL_MapRGB(pimgsurf->format, r, g, b);
                    set_pixel_32bit(pimgsurf, col, row, color);
                }
            }

            // Unlock surface
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_UnlockSurface(pimgsurf);

            render_window(pwin, pimgsurf);

            break;
        }
    }

    fclose(pimg);

    // Program event loop
    SDL_Event event;
    while (SDL_WaitEvent(&event))
    {
        if (event.type == SDL_QUIT) break; // Check for quit request

        // If window size has changed or window has been exposed rerender window
        if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_EXPOSED))
            render_window(pwin, pimgsurf);
    }

    SDL_FreeSurface(pimgsurf);
    SDL_DestroyWindow(pwin);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return 0;
}
