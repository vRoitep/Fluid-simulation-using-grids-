#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define WIDTH 800
#define HEIGHT 600
#define CELL_SIZE 1
#define GRID_WIDTH (WIDTH / CELL_SIZE)
#define GRID_HEIGHT (HEIGHT / CELL_SIZE)

typedef struct {
    float *current;
    float *previous;
    float damping;
} FluidGrid;

// Store previous mouse position for continuous drag
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

void update_fluid(FluidGrid *fluid) {
    // Update fluid simulation
    for (int y = 1; y < GRID_HEIGHT - 1; y++) {
        for (int x = 1; x < GRID_WIDTH - 1; x++) {
            int idx = y * GRID_WIDTH + x;
            
            // Wave propagation using discrete Laplace
            float laplacian = 
                fluid->previous[idx - 1] +
                fluid->previous[idx + 1] +
                fluid->previous[idx - GRID_WIDTH] +
                fluid->previous[idx + GRID_WIDTH] -
                4.0f * fluid->previous[idx];
            
            fluid->current[idx] = 
                2.0f * fluid->previous[idx] - 
                fluid->current[idx] + 
                laplacian * 0.25f;
            
            fluid->current[idx] *= fluid->damping;
        }
    }
    
    // Swap buffers
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
    // Create continuous wave between two points (for dragging)
    float dx = x2 - x1;
    float dy = y2 - y1;
    float distance = sqrtf(dx*dx + dy*dy);
    
    if (distance < 1.0f) {
        // Single point
        add_disturbance(fluid, x1, y1, intensity);
        return;
    }
    
    // Create multiple points along the line for continuous effect
    int steps = (int)(distance * 2.0f) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = (float)i / (float)steps;
        int cx = (int)(x1 + dx * t);
        int cy = (int)(y1 + dy * t);
        
        // Add disturbance with fading intensity
        float current_intensity = intensity * (1.0f - t * 0.3f);
        
        // Add a small area around each point for smoother waves
        for (int dy = -2; dy <= 2; dy++) {
            for (int dx = -2; dx <= 2; dx++) {
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist <= 2.0f) {
                    float falloff = 1.0f - (dist / 2.0f);
                    add_disturbance(fluid, cx + dx, cy + dy, 
                                   current_intensity * falloff * 0.5f);
                }
            }
        }
    }
}

void add_velocity_field(FluidGrid *fluid, int x, int y, float intensity) {
    // Create a more realistic velocity-based disturbance
    if (x >= 2 && x < GRID_WIDTH - 2 && y >= 2 && y < GRID_HEIGHT - 2) {
        // Create a directional wave pattern
        for (int dy = -3; dy <= 3; dy++) {
            for (int dx = -3; dx <= 3; dx++) {
                float dist = sqrtf(dx*dx + dy*dy);
                if (dist <= 3.0f) {
                    // Create wave pattern based on position
                    float wave = cosf(dist * 0.8f) * (1.0f - dist/3.0f);
                    add_disturbance(fluid, x + dx, y + dy, intensity * wave);
                }
            }
        }
    }
}

void render_fluid(SDL_Renderer *renderer, FluidGrid *fluid) {
    // Start with white background
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    
    // Render grid cells based on fluid height
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = y * GRID_WIDTH + x;
            float value = fluid->current[idx];
            
            // Convert fluid height to black intensity
            // Higher waves = darker (more black)
            // Calm water = white
            Uint8 intensity = (Uint8)fmin(fabs(value) * 512, 255);
            
            // Invert: high waves become black, calm areas stay white
            Uint8 color = 255 - intensity;
            
            SDL_SetRenderDrawColor(renderer, color, color, color, 255);
            
            SDL_Rect rect = {
                x * CELL_SIZE, y * CELL_SIZE,
                CELL_SIZE, CELL_SIZE
            };
            SDL_RenderFillRect(renderer, &rect);
            
            // Add subtle grid lines for better visibility
            if (CELL_SIZE > 2) {
                SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
                SDL_RenderDrawRect(renderer, &rect);
            }
        }
    }
}

void render_fluid_alternative(SDL_Renderer *renderer, FluidGrid *fluid) {
    // Alternative rendering: black grid on white background
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    SDL_RenderClear(renderer);
    
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = y * GRID_WIDTH + x;
            float value = fluid->current[idx];
            
            // Only draw cells that have significant wave activity
            if (fabs(value) > 0.1f) {
                // Higher waves = more black
                Uint8 intensity = (Uint8)fmin(fabs(value) * 400, 255);
                SDL_SetRenderDrawColor(renderer, 
                    255 - intensity, 255 - intensity, 255 - intensity, 255);
            } else {
                // Calm water = white
                SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            }
            
            SDL_Rect rect = {
                x * CELL_SIZE, y * CELL_SIZE,
                CELL_SIZE, CELL_SIZE
            };
            SDL_RenderFillRect(renderer, &rect);
            
            // Subtle grid lines
            SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
            SDL_RenderDrawRect(renderer, &rect);
        }
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window *window = SDL_CreateWindow(
        "Black & White Fluid Grid", 
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIDTH, HEIGHT, SDL_WINDOW_SHOWN
    );
    
    if (window == NULL) {
        printf("SDL_CreateWindow Error: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    
    SDL_Renderer *renderer = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED
    );
    
    if (renderer == NULL) {
        printf("SDL_CreateRenderer Error: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    
    FluidGrid fluid;
    init_fluid(&fluid);
    
    int running = 1;
    int mouse_down = 0;
    SDL_Event event;
    
    
    while (running) {
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
                        
                        // Add initial click with velocity field
                        add_velocity_field(&fluid, prev_mouse_x, prev_mouse_y, 25.0f);
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    if (mouse_down && (event.motion.state & SDL_BUTTON_LMASK)) {
                        int current_x = event.motion.x / CELL_SIZE;
                        int current_y = event.motion.y / CELL_SIZE;
                        
                        if (prev_mouse_x != -1 && prev_mouse_y != -1) {
                            // Create continuous wave between previous and current position
                            add_continuous_wave(&fluid, 
                                prev_mouse_x, prev_mouse_y, 
                                current_x, current_y, 20.0f);
                            
                            // Also add velocity field at current position
                            add_velocity_field(&fluid, current_x, current_y, 15.0f);
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
                        // Add random disturbance
                        add_velocity_field(&fluid, 
                            rand() % (GRID_WIDTH - 4) + 2, 
                            rand() % (GRID_HEIGHT - 4) + 2, 30.0f);
                    } else if (event.key.keysym.sym == SDLK_r) {
                        // Reset simulation
                        free_fluid(&fluid);
                        init_fluid(&fluid);
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    break;
            }
        }
        
        update_fluid(&fluid);
        
        // Render with black and white scheme
        render_fluid(renderer, &fluid);
        
        SDL_RenderPresent(renderer);
        SDL_Delay(16); // ~60 FPS
    }
    
    free_fluid(&fluid);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    return 0;
}