#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_hints.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

// TODO:
// - Integer zooming

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

void render_window(SDL_Window *window, SDL_Surface *image)
{
    int win_w, win_h;
    SDL_GetWindowSize(window, &win_w, &win_h); // Get window size
    SDL_Surface *pwinsurf = SDL_GetWindowSurface(window); // Get window surface

    SDL_Rect display_rect;
    display_rect.x = (win_w - image->w) / 2;
    display_rect.y = (win_h - image->h) / 2;
    display_rect.w = image->w;
    display_rect.h = image->h;
    SDL_FillRect(pwinsurf, NULL, SDL_MapRGB(pwinsurf->format, 0, 0, 0)); // Fill window surface with black
    SDL_BlitScaled(image, NULL, pwinsurf, &display_rect); // Fill middle of window surface with image

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
    SDL_Surface *psurf; // Declare pointer to surface that will store the image

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
            char win_title[96];
            snprintf(win_title, sizeof(win_title), "Viewing plain PPM image - %dx%d", img_width, img_height);
            pwin = SDL_CreateWindow(win_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, img_width, img_height, SDL_WINDOW_RESIZABLE);

            // Create surface
            psurf = SDL_CreateRGBSurfaceWithFormat(0, img_width, img_height, 32, SDL_PIXELFORMAT_RGB888);

            // Fill surface with image
            for (int row = 0; row < img_height; row++)
            {
                for (int col = 0; col < img_width; col++)
                {
                    SDL_Rect pixel = (SDL_Rect){col, row, 1, 1};
                    Uint8 r, g, b;
                    fscanf(pimg, "%hhu %hhu %hhu", &r, &g, &b);
                    Uint32 color = SDL_MapRGB(psurf->format, r, g, b);
                    SDL_FillRect(psurf, &pixel, color);
                }
            }

            render_window(pwin, psurf);

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
            char win_title[96];
            snprintf(win_title, sizeof(win_title), "Viewing raw PPM image - %dx%d", img_width, img_height);
            pwin = SDL_CreateWindow(win_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, img_width, img_height, SDL_WINDOW_RESIZABLE);

            // Create surface
            psurf = SDL_CreateRGBSurfaceWithFormat(0, img_width, img_height, 32, SDL_PIXELFORMAT_RGB888);

            // Fill surface with image
            for (int row = 0; row < img_height; row++)
            {
                for (int col = 0; col < img_width; col++)
                {
                    SDL_Rect pixel = (SDL_Rect){col, row, 1, 1};
                    Uint8 r = fgetc(pimg), g = fgetc(pimg), b = fgetc(pimg);
                    Uint32 color = SDL_MapRGB(psurf->format, r, g, b);
                    SDL_FillRect(psurf, &pixel, color);
                }
            }

            render_window(pwin, psurf);

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
            render_window(pwin, psurf);
    }

    SDL_FreeSurface(psurf);
    SDL_DestroyWindow(pwin);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return 0;
}
