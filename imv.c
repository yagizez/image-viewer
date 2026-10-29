#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// TODO:
// - Add multiplicative zoom steps
// - Add zooming toward the cursor

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

#define ZOOM_MIN 0.5f
#define ZOOM_MAX 8.0f
#define ZOOM_STEP 0.25f
#define ZOOM_SMOOTH_FILTERING_LIMIT 3.0f

#define BACKGROUND_R 31
#define BACKGROUND_G 31
#define BACKGROUND_B 31

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define CLAMP(val, min_val, max_val) MAX(min_val, MIN(max_val, val))

SDL_Window *create_window(int win_w, int win_h, const char *win_info, int img_w, int img_h, float zoom)
{
    char window_title[96];
    snprintf(window_title, sizeof(window_title), "%s - %dx%d - %d%%", win_info, img_w, img_h, (int)(zoom * 100));

    SDL_Window *window = SDL_CreateWindow(window_title, win_w, win_h, SDL_WINDOW_RESIZABLE);
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

float clamp_imgoff(float imgoff, int win_len, int img_len, float zoom)
{
    float zoomed_img_len = img_len * zoom; // Calculate zoomed image length

    if (zoomed_img_len <= win_len) // Image's length does fit the window
    {
        // Reset image's offset
        return 0.0f;
    }
    else // Image's length doesn't fit the window
    {
        // Set and clamp image's offset
        float imgoff_lim = (zoomed_img_len - win_len) * 0.5f;
        return CLAMP(imgoff, -imgoff_lim, imgoff_lim);
    }
}

void set_dstfrect(SDL_FRect *frect, int win_w, int win_h, int img_w, int img_h, float zoom, float imgoff_x, float imgoff_y)
{
    frect->w = img_w * zoom; // Calculate image width
    frect->h = img_h * zoom; // Calculate image height
    frect->x = (win_w - frect->w) * 0.5f + imgoff_x; // Calculate image's x position
    frect->y = (win_h - frect->h) * 0.5f + imgoff_y; // Calculate image's y position
}

void render_window(SDL_Renderer *renderer, SDL_Texture *image, int win_w, int win_h, float zoom, float imgoff_x, float imgoff_y)
{
    SDL_SetRenderDrawColor(renderer, BACKGROUND_R, BACKGROUND_G, BACKGROUND_B, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer); // Fill window with drawing color

    SDL_FRect dstfrect; // Coordinates of where the image is displayed on window
    set_dstfrect(&dstfrect, win_w, win_h, image->w, image->h, zoom, imgoff_x, imgoff_y);

    // Change scale mode to smooth when zoomed out, sharp when zoomed in
    SDL_SetTextureScaleMode(image, (zoom < ZOOM_SMOOTH_FILTERING_LIMIT) ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);

    // Draw image texture to destination frect
    SDL_RenderTexture(renderer, image, NULL, &dstfrect);

    // Update window with the rendered image texture
    SDL_RenderPresent(renderer);
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
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "x11");

    // Initialize SDL3 subsystem(s)
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO))
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
    Uint8 magic_nums[8];
    fread(magic_nums, 1, sizeof(magic_nums), pimg);
    rewind(pimg);
    if (memcmp(magic_nums, (Uint8[]){0x50, 0x33}, 2) == 0)
    {
        imgf = PPM_P3;
    }
    else if (memcmp(magic_nums, (Uint8[]){0x50, 0x36}, 2) == 0)
    {
        imgf = PPM_P6;
    }
    else
    {
        fprintf(stderr, "Can't open image file (incompatible)\n");
        return 1;
    }

    SDL_Window *pwindow; // Declare pointer to window
    SDL_Renderer *prenderer; // Declare pointer to renderer
    SDL_Surface *pimgsurf; // Declare pointer to surface that will store the image
    SDL_Texture *pimgtext; // Declare pointer to texture that will store the image

    int win_w, win_h; // Window width and height
    int img_w, img_h; // Image width and height
    float zoom = 1.0f; // Zoom value of image
    float imgoff_x = 0.0f, imgoff_y = 0.0f; // Offset of image relative to center of window

    switch (imgf)
    {
        case PPM_P3:
        {
            // PPM "P3" file variables
            Uint8 magic_num[2];
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
            pwindow = create_window(img_w, img_h, "Image", img_w, img_h, zoom);

            // Create renderer and enable VSync
            prenderer = SDL_CreateRenderer(pwindow, NULL);
            SDL_SetRenderVSync(prenderer, 1);

            // Get window size
            SDL_GetRenderOutputSize(prenderer, &win_w, &win_h);

            // Create image surface
            pimgsurf = SDL_CreateSurface(img_w, img_h, SDL_PIXELFORMAT_XRGB8888);

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
                    Uint32 color = SDL_MapSurfaceRGB(pimgsurf, r, g, b);
                    set_pixel_32bit(pimgsurf, col, row, color);
                }
            }

            // Unlock surface
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_UnlockSurface(pimgsurf);

            // Create a GPU texture from the image surface
            pimgtext = SDL_CreateTextureFromSurface(prenderer, pimgsurf);

            // Destroy image surface
            SDL_DestroySurface(pimgsurf);

            render_window(prenderer, pimgtext, win_w, win_h, zoom, imgoff_x, imgoff_y);

            break;
        }
        case PPM_P6:
        {
            // PPM "P6" file variables
            Uint8 magic_num[2];
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
            pwindow = create_window(img_w, img_h, "Image", img_w, img_h, zoom);

            // Create renderer and enable VSync
            prenderer = SDL_CreateRenderer(pwindow, NULL);
            SDL_SetRenderVSync(prenderer, 1);

            // Get window size
            SDL_GetRenderOutputSize(prenderer, &win_w, &win_h);

            // Create image surface
            pimgsurf = SDL_CreateSurface(img_w, img_h, SDL_PIXELFORMAT_XRGB8888);

            // Lock surface for pixel manipulation
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_LockSurface(pimgsurf);

            // Fill surface with image
            for (int row = 0; row < img_h; row++)
            {
                for (int col = 0; col < img_w; col++)
                {
                    Uint8 r = fgetc(pimg), g = fgetc(pimg), b = fgetc(pimg);
                    Uint32 color = SDL_MapSurfaceRGB(pimgsurf, r, g, b);
                    set_pixel_32bit(pimgsurf, col, row, color);
                }
            }

            // Unlock surface
            if (SDL_MUSTLOCK(pimgsurf))
                SDL_UnlockSurface(pimgsurf);

            // Create a GPU texture from the image surface
            pimgtext = SDL_CreateTextureFromSurface(prenderer, pimgsurf);

            // Destroy image surface
            SDL_DestroySurface(pimgsurf);

            render_window(prenderer, pimgtext, win_w, win_h, zoom, imgoff_x, imgoff_y);

            break;
        }
    }

    fclose(pimg);

    bool lmb_down = false; // Is left mouse button pressed

    // Program event loop
    SDL_Event event;
    while (SDL_WaitEvent(&event))
    {
        bool update_window = false; // Does window need updating
        bool update_window_title = false; // Does window title need updating

        do
        {
            if (event.type == SDL_EVENT_QUIT) goto quit_program; // Check for quit request

            // If left ctrl is pressed and the wheel is scrolled; change zoom, rerender window and update window title
            if (event.type == SDL_EVENT_MOUSE_WHEEL && (SDL_GetModState() & SDL_KMOD_LCTRL))
            {
                float prev_zoom = zoom;

                // Set zoom and clamp it between ZOOM_MIN and ZOOM_MAX
                zoom = CLAMP(zoom + (event.wheel.y > 0.0f ? ZOOM_STEP : -ZOOM_STEP), ZOOM_MIN, ZOOM_MAX);

                // Update offset values according to zoom change
                imgoff_x = clamp_imgoff(imgoff_x * (zoom / prev_zoom), win_w, img_w, zoom);
                imgoff_y = clamp_imgoff(imgoff_y * (zoom / prev_zoom), win_h, img_h, zoom);

                update_window = true;
                update_window_title = true;
            }

            // Track left mouse button state
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
                lmb_down = true;
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP && event.button.button == SDL_BUTTON_LEFT)
                lmb_down = false;

            // If left mouse button is pressed and the mouse moved, change image offset and rerender window
            if (event.type == SDL_EVENT_MOUSE_MOTION && lmb_down)
            {
                imgoff_x = clamp_imgoff(imgoff_x + event.motion.xrel, win_w, img_w, zoom);
                imgoff_y = clamp_imgoff(imgoff_y + event.motion.yrel, win_h, img_h, zoom);

                update_window = true;
            }

            // If window size has changed, rerender window
            if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
            {
                // Get updated window width and height
                SDL_GetRenderOutputSize(prenderer, &win_w, &win_h);

                // Update offset values according to window size change
                imgoff_x = clamp_imgoff(imgoff_x, win_w, img_w, zoom);
                imgoff_y = clamp_imgoff(imgoff_y, win_h, img_h, zoom);

                update_window = true;
            }

            // If window has been exposed, rerender window
            if (event.type == SDL_EVENT_WINDOW_EXPOSED)
                update_window = true;
        }
        while (SDL_PollEvent(&event));

        if (update_window)
            render_window(prenderer, pimgtext, win_w, win_h, zoom, imgoff_x, imgoff_y);

        if (update_window_title)
            set_window_title(pwindow, "Image", img_w, img_h, zoom);
    }

    quit_program:

    SDL_DestroyTexture(pimgtext);
    SDL_DestroyRenderer(prenderer);
    SDL_DestroyWindow(pwindow);

    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    SDL_Quit();

    return 0;
}
