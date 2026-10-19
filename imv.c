#include <stdio.h>
#include <string.h>
#include <SDL2/SDL_events.h>
#include <SDL2/SDL_pixels.h>
#include <SDL2/SDL_rect.h>
#include <SDL2/SDL_stdinc.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

// TODO:
// - Add comment support for PPM files
// - Window resizing
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

int main(int argc, char *argv[])
{
    // Check arguments
    if (argc != 2)
    {
        fprintf(stderr, "Missing filename parameter\n");
        return 1;
    }

    // Open image file
    FILE *pimg = fopen(argv[1], "r");
    if (pimg == NULL)
    {
        perror("Can't open image file");
        return 1;
    }

    // Declare a window
    SDL_Window *pwin;

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

    switch (imgf)
    {
        case PPM_P3:
        {
            // PPM "P3" file variables
            uint8_t magic_num[2];
            int img_width, img_height;
            int max_saturation;

            fread(magic_num, 1, sizeof(magic_num), pimg); // Get magic number
            fgetc(pimg); // Skip trailing '\n'

            fscanf(pimg, "%d %d", &img_width, &img_height); // Get image width and height
            fgetc(pimg); // Skip trailing '\n'

            fscanf(pimg, "%d", &max_saturation); // Get max saturation value
            fgetc(pimg); // Skip trailing '\n'

            // Create window
            char win_title[96];
            snprintf(win_title, sizeof(win_title), "[imv-ppm] Viewing PPM(Plain) image - %dx%d", img_width, img_height);
            pwin = SDL_CreateWindow(win_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, img_width, img_height, 0);

            // Draw image to window
            SDL_Surface *psurf = SDL_GetWindowSurface(pwin);
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
            SDL_UpdateWindowSurface(pwin);

            break;
        }
        case PPM_P6:
        {
            // PPM "P6" file variables
            uint8_t magic_num[2];
            int img_width, img_height;
            int max_saturation;

            fread(magic_num, 1, sizeof(magic_num), pimg); // Get magic number
            fgetc(pimg); // Skip trailing '\n'

            fscanf(pimg, "%d %d", &img_width, &img_height); // Get image width and height
            fgetc(pimg); // Skip trailing '\n'

            fscanf(pimg, "%d", &max_saturation); // Get max saturation value
            fgetc(pimg); // Skip trailing '\n'

            // Create window
            char win_title[96];
            snprintf(win_title, sizeof(win_title), "[imv-ppm] Viewing PPM(Raw) image - %dx%d", img_width, img_height);
            pwin = SDL_CreateWindow(win_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, img_width, img_height, 0);

            // Draw image to window
            SDL_Surface *psurf = SDL_GetWindowSurface(pwin);
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
            SDL_UpdateWindowSurface(pwin);

            break;
        }
    }

    fclose(pimg); // Close image file

    // Program event loop
    int program_is_running = 1;
    while (program_is_running)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
                program_is_running = 0;
        }
        SDL_Delay(100);
    }

    SDL_DestroyWindow(pwin); // Destroy window

    return 0;
}
