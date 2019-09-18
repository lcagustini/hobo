#include <stdio.h>
#include <stdint.h>
#include <SDL2/SDL.h>
#include "cpu.h"

extern void step6502();
extern void reset6502();
extern CPU_State cpu;

uint8_t ram[0x800];
uint8_t rom[0x800];
uint64_t pixel = 0;

SDL_Surface *draw_surface;
SDL_Surface *screen_surface;
SDL_Window *window;

#define SCREEN_WIDTH 256
#define SCREEN_HEIGHT 192

uint8_t read6502(uint16_t address) {
  if (address < 0x800) return ram[address];
  if (address < 0x1000) return rom[address-0x800];
  if (address == 0xFFFC) return 0x00;
  if (address == 0xFFFD) return 0x08;
  return 0;
}

void write6502(uint16_t address, uint8_t value) {
  if (address < 0x800) ram[address] = value;
  if (address == 0x2000) {
    uint32_t *pixels = (uint32_t *)draw_surface->pixels;
    pixels[pixel++] = value | (value << 8) | (value << 16);
    if (pixel >= SCREEN_WIDTH*SCREEN_HEIGHT) {
      pixel = 0;
      printf("%d\n", cpu.clockticks6502);

      SDL_BlitScaled(draw_surface, NULL, screen_surface, NULL);
      SDL_UpdateWindowSurface(window);
    }
  }
}

#define ZOOM 2

int main() {
  FILE *vrom = fopen("vrom", "rb");
  fread(rom, 1, sizeof(rom), vrom);
  fclose(vrom);

  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    exit(1);
  }
  window = SDL_CreateWindow("video", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, ZOOM*SCREEN_WIDTH, ZOOM*SCREEN_HEIGHT, SDL_WINDOW_SHOWN /*| SDL_WINDOW_FULLSCREEN*/);
  if (window == NULL) {
    printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
    exit(1);
  }

  if (!window) exit(1);

  screen_surface = SDL_GetWindowSurface(window);
  draw_surface = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT,
      screen_surface->format->BitsPerPixel,
      screen_surface->format->Rmask, screen_surface->format->Gmask,
      screen_surface->format->Bmask, screen_surface->format->Amask);
  SDL_Event e;

  CPU_State cpu1 = {0};
  cpu1.id = 1;
  CPU_State cpu2 = {0};
  cpu2.id = 2;

  cpu = cpu1;
  reset6502();
  cpu1 = cpu;
  cpu = cpu2;
  reset6502();
  cpu2 = cpu;
  while (1) {
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_QUIT) exit(0);
    }

    cpu = cpu1;
    step6502();
    cpu1 = cpu;
    cpu = cpu2;
    step6502();
    cpu2 = cpu;
    //getc(stdin);
  }
}
