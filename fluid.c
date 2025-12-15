#include <SDL2/SDL.h>
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define WIDTH 800
#define HEIGHT 600
#define CELL_SIZE 2
#define GRID_WIDTH (WIDTH / CELL_SIZE)
#define GRID_HEIGHT (HEIGHT / CELL_SIZE)

typedef struct {
    float *current;
    float *previous;
    float damping;
} FluidGrid;

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
        
        // Add to neighbors for smoother effect
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0) continue;
                int nidx = (y + dy) * GRID_WIDTH + (x + dx);
                fluid->previous[nidx] += intensity * 0.5f;
            }
        }
    }
}

void render_fluid(SDL_Renderer *renderer, FluidGrid *fluid) {
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            int idx = y * GRID_WIDTH + x;
            float value = fluid->current[idx];
            
            // Convert to color (blue-ish fluid)
            Uint8 intensity = (Uint8)fmin(fabs(value) * 255, 255);
            SDL_SetRenderDrawColor(renderer, 
                intensity/3, intensity/2, intensity, 255);
            
            SDL_Rect rect = {
                x * CELL_SIZE, y * CELL_SIZE,
                CELL_SIZE, CELL_SIZE
            };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
}

int main() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }
    
    SDL_Window *window = SDL_CreateWindow(
        "Fluid Simulation", 
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
    
    printf("Fluid Simulation Controls:\n");
    printf("- Click and drag to create ripples\n");
    printf("- Press SPACE to add random disturbance\n");
    printf("- Press ESC to quit\n");
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                    
                case SDL_MOUSEBUTTONDOWN:
                    mouse_down = 1;
                    // Add initial click disturbance
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        int mouse_x = event.button.x / CELL_SIZE;
                        int mouse_y = event.button.y / CELL_SIZE;
                        add_disturbance(&fluid, mouse_x, mouse_y, 15.0f);
                    }
                    break;
                    
                case SDL_MOUSEMOTION:
                    if (mouse_down && (event.motion.state & SDL_BUTTON_LMASK)) {
                        int mouse_x = event.motion.x / CELL_SIZE;
                        int mouse_y = event.motion.y / CELL_SIZE;
                        
                        // Add multiple points for dragging effect
                        for (int dy = -2; dy <= 2; dy++) {
                            for (int dx = -2; dx <= 2; dx++) {
                                float dist = sqrtf(dx*dx + dy*dy);
                                if (dist <= 2) {
                                    add_disturbance(&fluid, 
                                        mouse_x + dx, mouse_y + dy, 
                                        8.0f * (1.0f - dist/2.0f));
                                }
                            }
                        }
                    }
                    break;
                    
                case SDL_MOUSEBUTTONUP:
                    mouse_down = 0;
                    break;
                    
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        // Add random disturbance
                        add_disturbance(&fluid, 
                            rand() % (GRID_WIDTH - 2) + 1, 
                            rand() % (GRID_HEIGHT - 2) + 1, 20.0f);
                    } else if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = 0;
                    }
                    break;
            }
        }
        
        update_fluid(&fluid);
        
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        
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