#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

// TODO:
// - Migrate from SDL2 to SDL3
// - Add smooth filtering to images

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

#define ZOOM_MIN 0.5
#define ZOOM_MAX 8.0
#define ZOOM_STEP 0.5

#define BACKGROUND_R 31
#define BACKGROUND_G 31
#define BACKGROUND_B 31

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

SDL_Window *create_window(int win_w, int win_h, const char *win_info, int img_w, int img_h, float zoom)
{
    char window_title[96];
    snprintf(window_title, sizeof(window_title), "%s - %dx%d - %d%%", win_info, img_w, img_h, (int)(zoom * 100));

    SDL_Window *window = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, win_w, win_h, SDL_WINDOW_RESIZABLE);
    return (window);
}

void set_window_title(SDL_Window *win, const char *win_info, int img_w, int img_h, float zoom)
{
    char window_title[96];
    snprintf(window_title, sizeof(window_title), "%s - %dx%d - %d%%", win_info, img_w, img_h, (int)(zoom * 100));

    SDL_SetWindowTitle(win, window_title);
}

void set_pixel_32bit(SDL_Surface *surface, int x, int y, Uint32 color)
{
    Uint32 *pixels = (Uint32 *)surface->pixels;
    pixels[(y * (surface->pitch / sizeof(Uint32))) + x] = color;
}

void set_display_rect(SDL_Rect *rect, int win_w, int win_h, int img_w, int img_h, float *zoom, int *imgoff_x, int *imgoff_y)
{
    *zoom = MAX(ZOOM_MIN, MIN(ZOOM_MAX, *zoom)); // Clamp zoom between ZOOM_MIN and ZOOM_MAX

    rect->w = (int)(img_w * *zoom); // Calculate image width
    rect->h = (int)(img_h * *zoom); // calculate image height

    if (rect->w <= win_w) // Image's width does fit the window
    {
        *imgoff_x = 0; // Reset image's x offset
        rect->x = (win_w - rect->w) / 2 + *imgoff_x; // Calculate image's x position
    }
    else // Image's width doesn't fit the window
    {
        *imgoff_x = MAX((win_w - rect->w) / 2, MIN((rect->w - win_w) / 2, *imgoff_x)); // Clamp image's x offset
        rect->x = (win_w - rect->w) / 2 + *imgoff_x; // Calculate image's x position
        rect->x = MAX(win_w - rect->w, MIN(0, rect->x)); // Clamp image's x position
    }

    if (rect->h <= win_h) // Image's height does fit the window
    {
        *imgoff_y = 0; // Reset image's y offset
        rect->y = (win_h - rect->h) / 2 + *imgoff_y; // Calculate image's y position
    }
    else // Image's height doesn't fit the window
    {
        *imgoff_y = MAX((win_h - rect->h) / 2, MIN((rect->h - win_h) / 2, *imgoff_y)); // Clamp image's y offset
        rect->y = (win_h - rect->h) / 2 + *imgoff_y; // Calculate image's y position
        rect->y = MAX(win_h - rect->h, MIN(0, rect->y)); // Clamp image's y position
    }
}

void render_window(SDL_Window *window, SDL_Surface *image, float *zoom, int *imgoff_x, int *imgoff_y)
{
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h); // Get window size
    SDL_Surface *pwinsurf = SDL_GetWindowSurface(window); // Get window surface

    Uint32 background_color = SDL_MapRGB(pwinsurf->format, BACKGROUND_R, BACKGROUND_G, BACKGROUND_B);
    SDL_FillRect(pwinsurf, NULL, background_color); // Fill window surface with background color

    SDL_Rect display_rect; // Coordinates of where the image is displayed on window
    set_display_rect(&display_rect, win_w, win_h, image->w, image->h, zoom, imgoff_x, imgoff_y);

    SDL_BlitScaled(image, NULL, pwinsurf, &display_rect); // Blit the image to the window surface (with nearest filter)

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

    int img_w, img_h; // Image width and height
    float zoom = 1.0; // Zoom value of image
    int imgoff_x = 0, imgoff_y = 0; // Offset of image relative to center of window

    switch (imgf)
    {
        case PPM_P3:
        {
            // PPM "P3" file variables
            uint8_t magic_num[2];
            int max_saturation;

            fread(magic_num, 1, sizeof(magic_num), pimg); // Get magic number
            fgetc(pimg); // Skip trailing '\n' or space

            // Skip comment(s)
            int ch;
            while ((ch = fgetc(pimg)) == '#')
                while (fgetc(pimg) != '\n');
            ungetc(ch, pimg);

            fscanf(pimg, "%d %d", &img_w, &img_h); // Get image width and height
            fgetc(pimg); // Skip trailing '\n' or space

            fscanf(pimg, "%d", &max_saturation); // Get max saturation value
            fgetc(pimg); // Skip trailing '\n'

            // Create window
            pwin = create_window(img_w, img_h, "Image", img_w, img_h, zoom);

            // Create image surface
            pimgsurf = SDL_CreateRGBSurfaceWithFormat(0, img_w, img_h, 32, SDL_PIXELFORMAT_RGB888);

            // Lock surface for pixel manipulation
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_LockSurface(pimgsurf);

            // Fill surface with image
            for (int row = 0; row < img_h; row++)
            {
                for (int col = 0; col < img_w; col++)
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

            render_window(pwin, pimgsurf, &zoom, &imgoff_x, &imgoff_y);

            break;
        }
        case PPM_P6:
        {
            // PPM "P6" file variables
            uint8_t magic_num[2];
            int max_saturation;

            fread(magic_num, 1, sizeof(magic_num), pimg); // Get magic number
            fgetc(pimg); // Skip trailing '\n' or space

            // Skip comment(s)
            int ch;
            while ((ch = fgetc(pimg)) == '#')
                while (fgetc(pimg) != '\n');
            ungetc(ch, pimg);

            fscanf(pimg, "%d %d", &img_w, &img_h); // Get image width and height
            fgetc(pimg); // Skip trailing '\n' or space

            fscanf(pimg, "%d", &max_saturation); // Get max saturation value
            fgetc(pimg); // Skip trailing '\n'

            // Create window
            pwin = create_window(img_w, img_h, "Image", img_w, img_h, zoom);

            // Create image surface
            pimgsurf = SDL_CreateRGBSurfaceWithFormat(0, img_w, img_h, 32, SDL_PIXELFORMAT_RGB888);

            // Lock surface for pixel manipulation
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_LockSurface(pimgsurf);

            // Fill surface with image
            for (int row = 0; row < img_h; row++)
            {
                for (int col = 0; col < img_w; col++)
                {
                    Uint8 r = fgetc(pimg), g = fgetc(pimg), b = fgetc(pimg);
                    Uint32 color = SDL_MapRGB(pimgsurf->format, r, g, b);
                    set_pixel_32bit(pimgsurf, col, row, color);
                }
            }

            // Unlock surface
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_UnlockSurface(pimgsurf);

            render_window(pwin, pimgsurf, &zoom, &imgoff_x, &imgoff_y);

            break;
        }
    }

    fclose(pimg);

    int lmb_down = 0; // Is left mouse button pressed

    // Program event loop
    SDL_Event event;
    while (SDL_WaitEvent(&event))
    {
        int update_window = 0; // Does window need updating
        int update_window_title = 0; // Does window title need updating

        if (event.type == SDL_QUIT) break; // Check for quit request

        // If left ctrl is held and the wheel is scrolled; change zoom, update window title and rerender window
        if (event.type == SDL_MOUSEWHEEL && (SDL_GetModState() & KMOD_LCTRL))
        {
            zoom += event.wheel.y > 0 ? ZOOM_STEP : -ZOOM_STEP;
            update_window = 1;
            update_window_title = 1;
        }

        // Track left mouse button state
        if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT)
            lmb_down = 1;
        else if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT)
            lmb_down = 0;

        // If left mouse button is pressed and the mouse moved, change image offset and rerender window
        if (event.type == SDL_MOUSEMOTION && lmb_down)
        {
            imgoff_x += event.motion.xrel;
            imgoff_y += event.motion.yrel;
            update_window = 1;
        }

        // If window size has changed or window has been exposed, rerender window
        if (event.type == SDL_WINDOWEVENT && (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || event.window.event == SDL_WINDOWEVENT_EXPOSED))
            update_window  = 1;

        if (update_window)
            render_window(pwin, pimgsurf, &zoom, &imgoff_x, &imgoff_y);

        if (update_window_title)
            set_window_title(pwin, "Image", img_w, img_h, zoom);
    }

    SDL_FreeSurface(pimgsurf);
    SDL_DestroyWindow(pwin);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return 0;
}
