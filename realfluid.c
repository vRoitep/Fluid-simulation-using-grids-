#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#define WIDTH 1200
#define HEIGHT 800
#define CELL_SIZE 1  // water dot size
#define GRID_WIDTH (WIDTH / CELL_SIZE)
#define GRID_HEIGHT (HEIGHT / CELL_SIZE)

typedef struct {
    float *current;
    float *previous;
    float damping; 
} FluidGrid;

typedef struct {
    SDL_Texture *texture;
    uint32_t *pixels;
    int pitch;
} FluidRenderer;

// mouse x,y positionss
static int prev_mouse_x = -1;
static int prev_mouse_y = -1;

void init_fluid(FluidGrid *fluid) {
    fluid->current = calloc(GRID_WIDTH * GRID_HEIGHT, sizeof(float));
    fluid->previous = calloc(GRID_WIDTH * GRID_HEIGHT, sizeof(float));
    fluid->damping = 0.99f;
}

void free_fluid(FluidGrid *fluid) {
    free(fluid->current);
    free(fluid->previous);
}

int init_fluid_renderer(SDL_Renderer *renderer, FluidRenderer *frenderer) {
    // Create texture for fluid rendering
    frenderer->texture = SDL_CreateTexture(renderer, 
        SDL_PIXELFORMAT_ARGB8888, 
        SDL_TEXTUREACCESS_STREAMING, 
        GRID_WIDTH, GRID_HEIGHT);
    
    if (!frenderer->texture) {
        printf("Failed to create texture: %s\n", SDL_GetError());
        return 0;
    }
    
    frenderer->pixels = malloc(GRID_WIDTH * GRID_HEIGHT * sizeof(uint32_t));
    if (!frenderer->pixels) {
        printf("Failed to allocate pixel buffer\n");
        return 0;
    }
    
    frenderer->pitch = GRID_WIDTH * sizeof(uint32_t);
    return 1;
}

void free_fluid_renderer(FluidRenderer *frenderer) {
    if (frenderer->texture) SDL_DestroyTexture(frenderer->texture);
    if (frenderer->pixels) free(frenderer->pixels);
}

// simd memory
void update_fluid(FluidGrid *fluid) {
    // inside grid
    for (int y = 1; y < GRID_HEIGHT - 1; y++) {
        int row_start = y * GRID_WIDTH;
        
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            int idx = row_start + x;
            
            // wave
            float laplacian = 
                fluid->previous[idx - 1] +
                fluid->previous[idx + 1] +
                fluid->previous[idx - GRID_WIDTH] +
                fluid->previous[idx + GRID_WIDTH] -
                4.0f * fluid->previous[idx];
            
            // up damp
            fluid->current[idx] = 
                (2.0f * fluid->previous[idx] - fluid->current[idx] + laplacian * 0.25f) 
                * fluid->damping;
        }
    }
    
    // buffers 
    float *temp = fluid->current;
    fluid->current = fluid->previous;
    fluid->previous = temp;
}

void add_disturbance(FluidGrid *fluid, int x, int y, float intensity) {
    if (x >= 1 && x < GRID_WIDTH - 1 && y >= 1 && y < GRID_HEIGHT - 1) {
        int idx = y * GRID_WIDTH + x;
        fluid->previous[idx] += intensity;
    }
}

void add_continuous_wave(FluidGrid *fluid, int x1, int y1, int x2, int y2, float intensity) {
    // 2 poitns
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distance = sqrtf(dx*dx + dy*dy);
    
    if (distance < 1.0f) {
        add_disturbance(fluid, x1, y1, intensity);
        return;
    }
    
    //pre calculation 
    int steps = (int)(distance * 1.5f) + 1;
    float inv_steps = 1.0f / steps;
    
    for (int i = 0; i <= steps; i++) {
        float t = i * inv_steps;
        int cx = (int)(x1 + dx * t);
        int cy = (int)(y1 + dy * t);
        float current_intensity = intensity * (1.0f - t * 0.3f);
        
        // smaller radius
        if (cx >= 2 && cx < GRID_WIDTH - 2 && cy >= 2 && cy < GRID_HEIGHT - 2) {
            add_disturbance(fluid, cx, cy, current_intensity);
        }
    }
}

// realestic distrubince
void add_water_drop(FluidGrid *fluid, int x, int y, float intensity) {
    if (x >= 3 && x < GRID_WIDTH - 3 && y >= 3 && y < GRID_HEIGHT - 3) {
        // Create a realistic water drop pattern
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist <= 3.0f) {
                    // gaus distrurbiacne 
                    float falloff = expf(-dist * dist * 0.3f);
                    float wave = cosf(dist * 1.5f) * falloff;
                    add_disturbance(fluid, x + dx, y + dy, intensity * wave);
                }
            }
        }
    }
}

// water color 
uint32_t water_color(float height, float x, float y, Uint32 time) {
    //color
    float base_r = 0.1f;
    float base_g = 0.2f;
    float base_b = 0.4f;
    
    // wave effect
    float wave_intensity = fabsf(height) * 2.0f;
    
    // foam
    float foam = fmaxf(0.0f, height - 0.3f) * 3.0f;
    foam = fminf(foam, 1.0f);
    
    // refelction
    float light = fmaxf(0.0f, height) * 1.5f;
    light = fminf(light, 0.8f);
    
    // combine w,f,r
    float r = base_r + foam + light * 0.3f;
    float g = base_g + foam * 0.8f + light * 0.4f;
    float b = base_b + foam + light * 0.2f;
    
    r = fminf(fmaxf(r, 0.0f), 1.0f);
    g = fminf(fmaxf(g, 0.0f), 1.0f);
    b = fminf(fmaxf(b, 0.0f), 1.0f);
    
    return ((uint32_t)(r * 255) << 16) | 
           ((uint32_t)(g * 255) << 8) | 
           ((uint32_t)(b * 255)) | 
           0xFF000000;
}

uint32_t bw_water_color(float height) {
    
    float intensity = fabsf(height) * 3.0f;
    
    // Foam on wave peaks
    if (height > 0.2f) {
        float foam = (height - 0.2f) * 4.0f;
        intensity -= foam * 0.5f; // Make peaks lighter (foam)
    }
    
    intensity = fminf(fmaxf(intensity, 0.0f), 1.0f);
    
    uint8_t value = (uint8_t)((1.0f - intensity) * 255);
    
    return (0xFF << 24) | (value << 16) | (value << 8) | value;
}

void update_fluid_texture(FluidRenderer *frenderer, FluidGrid *fluid, Uint32 time) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = y * GRID_WIDTH + x;
            float height = fluid->current[idx];
            
            frenderer->pixels[idx] = water_color(height, x, y, time);  // Color
        }
    }
}

void render_fluid(SDL_Renderer *renderer, FluidRenderer *frenderer) {

    SDL_UpdateTexture(frenderer->texture, NULL, frenderer->pixels, frenderer->pitch);
    
    // scale texture
    SDL_RenderCopy(renderer, frenderer->texture, NULL, NULL);
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window *window = SDL_CreateWindow(
        "water simm", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN
    );
    
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    
    if (renderer == NULL) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    FluidGrid fluid;
    FluidRenderer frenderer = {0};
    
    init_fluid(&fluid);
    
    if (!init_fluid_renderer(renderer, &frenderer)) {
        printf("failed to open\n");
        return 1;
    }
    
    int running = 1;
    int mouse_down = 0;
    SDL_Event event;
    Uint32 start_time = SDL_GetTicks();
    
    
    while (running) {
        Uint32 current_time = SDL_GetTicks() - start_time;
        
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_down = 1;
                        prev_mouse_x = event.button.x / CELL_SIZE;
                        prev_mouse_y = event.button.y / CELL_SIZE;
                        add_water_drop(&fluid, prev_mouse_x, prev_mouse_y, 20.0f);
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    if (mouse_down && (event.motion.state & SDL_BUTTON_LMASK)) {
                        int current_x = event.motion.x / CELL_SIZE;
                        int current_y = event.motion.y / CELL_SIZE;
                        
                        if (prev_mouse_x != -1 && prev_mouse_y != -1) {
                            add_continuous_wave(&fluid, 
                                prev_mouse_x, prev_mouse_y, 
                                current_x, current_y, 15.0f);
                        }
                        
                        prev_mouse_x = current_x;
                        prev_mouse_y = current_y;
                    }
                    break;
                    
                case SDL_MOUSEBUTTONUP:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        mouse_down = 0;
                        prev_mouse_x = -1;
                        prev_mouse_y = -1;
                    }
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        add_water_drop(&fluid, 
                            rand() % (GRID_WIDTH - 6) + 3, 
                            rand() % (GRID_HEIGHT - 6) + 3, 25.0f);
                    } else if (event.key.keysym.sym == SDLK_r) {
                        free_fluid(&fluid);
                        init_fluid(&fluid);
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    break;
            }
        }
        
        // Update physics
        update_fluid(&fluid);
        
        // Update rendering
        update_fluid_texture(&frenderer, &fluid, current_time);
        
        // Clear and render
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
        render_fluid(renderer, &frenderer);
        
        SDL_RenderPresent(renderer);
        
        // Cap at 60 FPS
        SDL_Delay(16);
    }
    
    free_fluid(&fluid);
    free_fluid_renderer(&frenderer);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}