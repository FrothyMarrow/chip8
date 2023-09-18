#include "chip8.h"
// std
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int main(int argc, char *argv[]) {
  if (argc != 2) {
    fprintf(stderr, "Usage: chip8 <rom>\n");
    return EXIT_FAILURE;
  }

  // Default configuration
  Config config = {0};
  defaultConfig(&config);

  // Initialize SDL and Chip8
  Sdl sdl = {0};
  Chip8 chip8 = {0};

  if (!initSdl(&sdl, &config) || !initChip8(&chip8, &config)) {
    return EXIT_FAILURE;
  }

  // Load the ROM
  if (!loadRom(&chip8, argv[1])) {
    return EXIT_FAILURE;
  }

  // Seed the random number generator
  srand(time(NULL));

  while (chip8.state != QUIT) {
    // Poll and handle input events
    handleInput(&chip8);

    // Skip if the emulator is paused
    if (chip8.state == PAUSED) continue;

    const uint64_t beginFrame = SDL_GetTicks();

    // Uniformly execute instructions per frame
    for (uint32_t i = 0; i < config.instructionsPerSecond / FRAME_RATE; i++)
      emulateInstruction(&chip8, &config);

    const uint64_t endFrame = SDL_GetTicks();

    // Delay if finished early to maintain a constant frame rate
    SDL_Delay(FRAME_DURATION_IN_MS - (beginFrame - endFrame));

    // Update the screen and play audio
    draw(&sdl, &chip8, &config);
    sound(&chip8, &sdl);

    // Decrement the delay and sound timers at the rate of 60Hz
    updateTimers(&chip8);
  }

  // Cleanup SDL and Chip8
  cleanup(&sdl);

  return EXIT_SUCCESS;
}

void defaultConfig(Config *config) {
  config->scaleFactor = 20;              // Scale the window by 20
  config->foregroundColor = 0xD169B6FF;  // Pink
  config->backgroundColor = 0x38374CFF;  // Dark blue
  config->sampleFrequency = 44100;       // Standard CD quality
  config->sampleSize = 2048;             // Buffer size in samples
  config->audioFrequency = 440;          // A4 frequency
  config->audioAmplitude = 5000;         // Volume
  config->instructionsPerSecond = 700;   // Emulation speed
  config->outlines = true;               // Draw outlines
}

bool initSdl(Sdl *sdl, Config *config) {
  const int32_t success = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);

  if (success != 0) {
    fprintf(stderr, "Failed to initialize SDL: %s\n", SDL_GetError());
    return false;
  }

  // Initialize sdl audio and graphics
  if (!initGraphics(sdl, config) || !initAudio(sdl, config)) {
    return false;
  }

  return true;
}

bool initGraphics(Sdl *sdl, const Config *config) {
  // Opens a window of size 64 x 32 pixels multiplied by the scale factor
  sdl->window =
      SDL_CreateWindow("Chip8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       WINDOW_WIDTH * config->scaleFactor,
                       WINDOW_HEIGHT * config->scaleFactor, 0);

  if (sdl->window == NULL) {
    fprintf(stderr, "Failed to create SDL window: %s\n", SDL_GetError());
    return false;
  }

  // Use the first available renderer
  sdl->renderer = SDL_CreateRenderer(sdl->window, -1, 0);

  if (sdl->renderer == NULL) {
    fprintf(stderr, "Failed to create SDL renderer: %s\n", SDL_GetError());
    return false;
  }

  return true;
}

bool initAudio(Sdl *sdl, Config *config) {
  sdl->want.freq = config->sampleFrequency;  // Samples per second
  sdl->want.format = AUDIO_S16;              // Signed 16-bit samples
  sdl->want.channels = 1;                    // Mono audio
  sdl->want.samples = config->sampleSize;    // Buffer size in samples
  sdl->want.userdata = config;               // Passed to the callback function
  sdl->want.callback = squareWave;           // Generates a square wave

  // Open the default audio device and allow SDL to adjust the spec
  sdl->audioDevice = SDL_OpenAudioDevice(NULL, 0, &sdl->want, &sdl->have, 0);

  if (sdl->audioDevice == 0) {
    fprintf(stderr, "Failed to open audio device: %s\n", SDL_GetError());
    return false;
  }

  // Audio format mismatch
  if (sdl->want.format != sdl->have.format) {
    fprintf(stderr, "Failed to get the desired AudioSpec\n");
    return false;
  }

  return true;
}

void squareWave(void *userdata, Uint8 *stream, const int32_t len) {
  Config *config = (Config *)userdata;

  // Track the current sample index
  static uint32_t sampleIndex = 0;

  const uint32_t sampleCount = len / sizeof(int16_t);
  const uint32_t samplesPerHalfCycle =
      (config->sampleFrequency / config->audioFrequency) / 2;

  // Flip the sample value between positive and negative
  // depending on crest or trough of the wave
  for (uint32_t i = 0; i < sampleCount; i++, sampleIndex++) {
    // Current sample buffer
    int16_t *sampleBuffer = (int16_t *)stream + i;
    const uint32_t halfCycleIndex = sampleIndex / samplesPerHalfCycle;

    // Flip the wave every half cycle
    *sampleBuffer =
        halfCycleIndex % 2 ? config->audioAmplitude : -config->audioAmplitude;
  }
}

void updateTimers(Chip8 *chip8) {
  if (chip8->delayTimer) chip8->delayTimer--;
  if (chip8->soundTimer) chip8->soundTimer--;
}

void sound(const Chip8 *chip8, const Sdl *sdl) {
  // Play the audio device if the sound timer is active
  if (chip8->soundTimer) {
    SDL_PauseAudioDevice(sdl->audioDevice, 0);
  } else {
    SDL_PauseAudioDevice(sdl->audioDevice, 1);
  }
}

bool initChip8(Chip8 *chip8, const Config *config) {
  // Initialize the stack pointer to the top of the stack
  chip8->stackPointer = chip8->stack;

  // Load the font into memory
  loadFont(chip8);

  chip8->state = RUNNING;

  return true;
}

void loadFont(Chip8 *chip8) {
  const uint8_t font[FONT_SIZE] = {
      0xF0, 0x90, 0x90, 0x90, 0xF0,  // 0
      0x20, 0x60, 0x20, 0x20, 0x70,  // 1
      0xF0, 0x10, 0xF0, 0x80, 0xF0,  // 2
      0xF0, 0x10, 0xF0, 0x10, 0xF0,  // 3
      0x90, 0x90, 0xF0, 0x10, 0x10,  // 4
      0xF0, 0x80, 0xF0, 0x10, 0xF0,  // 5
      0xF0, 0x80, 0xF0, 0x90, 0xF0,  // 6
      0xF0, 0x10, 0x20, 0x40, 0x40,  // 7
      0xF0, 0x90, 0xF0, 0x90, 0xF0,  // 8
      0xF0, 0x90, 0xF0, 0x10, 0xF0,  // 9
      0xF0, 0x80, 0x80, 0x80, 0xF0,  // C
      0xE0, 0x90, 0x90, 0x90, 0xE0,  // D
      0xF0, 0x80, 0xF0, 0x80, 0xF0,  // E
      0xF0, 0x80, 0xF0, 0x80, 0x80   // F
  };

  memcpy(&chip8->ram, &font, FONT_SIZE);
}

bool loadRom(Chip8 *chip8, const char *filePath) {
  FILE *rom = fopen(filePath, "rb");

  if (rom == NULL) {
    fprintf(stderr, "Failed to open ROM file: %s\n", filePath);
    return false;
  }

  // Chip8 programs start at 0x200
  const uint32_t programEntryPoint = 0x200;

  // Get the size of the ROM
  fseek(rom, 0, SEEK_END);
  const size_t romSize = ftell(rom);
  rewind(rom);

  // Read the ROM into memory
  fread(&chip8->ram[programEntryPoint], romSize, 1, rom);

  fclose(rom);

  // Point the program counter to the start of the ROM
  chip8->programCounter = programEntryPoint;

  return true;
}

void draw(const Sdl *sdl, Chip8 *chip8, const Config *config) {
  // Don't update the display if the draw flag is not set
  if (!chip8->draw) {
    return;
  }

  // Clear the frame buffer before drawing
  // to avoid artifacts
  clearFrameBuffer(sdl, config);

  const uint32_t frameBufferSize = WINDOW_WIDTH * WINDOW_HEIGHT;

  // Draw the pixels to the frame buffer if they are on
  for (uint32_t i = 0; i < frameBufferSize; i++) {
    if (chip8->frameBuffer[i]) {
      drawPixel(sdl, config, i % WINDOW_WIDTH, i / WINDOW_WIDTH);
    }
  }

  SDL_RenderPresent(sdl->renderer);

  // Reset the draw flag
  chip8->draw = false;
}

void drawPixel(const Sdl *sdl, const Config *config, const uint32_t x,
               const uint32_t y) {
  drawColor(sdl, config->foregroundColor);

  // Rectange scaled by the scale factor
  SDL_Rect rectangle = {.x = x * config->scaleFactor,
                        .y = y * config->scaleFactor,
                        .w = config->scaleFactor,
                        .h = config->scaleFactor};

  SDL_RenderFillRect(sdl->renderer, &rectangle);

  // Draw outlines if enabled
  if (config->outlines) {
    drawColor(sdl, config->backgroundColor);
    SDL_RenderDrawRect(sdl->renderer, &rectangle);
  }
}

void drawColor(const Sdl *sdl, const uint32_t color) {
  // Get the RGBA components of the color
  uint8_t r = (color & 0xFF000000) >> 24;
  uint8_t g = (color & 0x00FF0000) >> 16;
  uint8_t b = (color & 0x0000FF00) >> 8;
  uint8_t a = (color & 0x000000FF);

  SDL_SetRenderDrawColor(sdl->renderer, r, g, b, a);
}

void clearFrameBuffer(const Sdl *sdl, const Config *config) {
  drawColor(sdl, config->backgroundColor);
  SDL_RenderClear(sdl->renderer);
}

void emulateInstruction(Chip8 *chip8, const Config *config) {
  // Fetch the next instruction
  nextInstruction(chip8);

  // Instruction decoding
  switch (chip8->instruction.raw >> 12) {
    case 0x0:
      // 0x00E0 clear the screen
      if (chip8->instruction.kk == 0xE0) {
        memset(chip8->frameBuffer, 0, sizeof(chip8->frameBuffer));
        chip8->draw = true;
      }
      // 0x00EE return from subroutine by subtracting one from the stackPointer
      // and then setting the programCounter to the address on top of stock
      else if (chip8->instruction.kk == 0xEE) {
        chip8->programCounter = *--chip8->stackPointer;
      }
      break;
    case 0x1:
      // 0x1NNN jump to instruction nnn
      chip8->programCounter = chip8->instruction.nnn;
      break;
    case 0x2:
      // 0x2NNN call subroutine at address nnn, store the current
      // address of the programCounter on top of stack and point
      // the programCounter to nnn
      *chip8->stackPointer++ = chip8->programCounter;
      chip8->programCounter = chip8->instruction.nnn;
      break;
    case 0x3:
      // 0x3XKK skip next instruction if V[X] == KK
      if (chip8->V[chip8->instruction.x] == chip8->instruction.kk) {
        chip8->programCounter += 2;
      }
      break;
    case 0x4:
      // 0x4XKK skip next instruction if V[X] != KK
      if (chip8->V[chip8->instruction.x] != chip8->instruction.kk) {
        chip8->programCounter += 2;
      }
      break;
    case 0x5:
      // 05XY0 skip next instruction if V[X] == V[Y]
      if (chip8->V[chip8->instruction.x] == chip8->V[chip8->instruction.y]) {
        chip8->programCounter += 2;
      }
      break;
    case 0x6:
      // 0x6XKK store the KK in the register V[X]
      chip8->V[chip8->instruction.x] = chip8->instruction.kk;
      break;
    case 0x7:
      // 0x7XKK add KK and V[X] and store the result in V[X] register
      chip8->V[chip8->instruction.x] += chip8->instruction.kk;
      break;
    case 0x8:
      switch (chip8->instruction.n) {
        case 0x0:
          // 0x8XY0  store the value of V[Y] in V[X]
          chip8->V[chip8->instruction.x] = chip8->V[chip8->instruction.y];
          break;
        case 0x1:
          // 0x8XY1 perform bitwise or operator on V[X] and V[Y] and
          // store the result in V[X]
          chip8->V[chip8->instruction.x] |= chip8->V[chip8->instruction.y];
          break;
        case 0x2:
          // 0x8XY2 perform bitwise and operator on V[X] and V[Y] and
          // store the result in V[X]
          chip8->V[chip8->instruction.x] &= chip8->V[chip8->instruction.y];
          break;
        case 0x3:
          // 0x8XY3 perform bitwise or operator on V[X] and V[Y] xor
          // store the result in V[X]
          chip8->V[chip8->instruction.x] ^= chip8->V[chip8->instruction.y];
          break;
        case 0x4: {
          // 0x8XY4 add V[X] and V[Y], if the result is greater than 0xFF
          // then store 1 in V[0xF] otherwise store the lower 8 bits in
          // the V[X]
          uint8_t carry = (chip8->V[chip8->instruction.x] +
                           chip8->V[chip8->instruction.y]) > 0xFF;

          chip8->V[chip8->instruction.x] += chip8->V[chip8->instruction.y];
          chip8->V[0xF] = carry;
        } break;
        case 0x5:
          // 0x8XY5 store 1 in V[X] if greater than V[Y] otherwise 0
          chip8->V[0xF] =
              chip8->V[chip8->instruction.x] > chip8->V[chip8->instruction.y];
          chip8->V[chip8->instruction.x] -= chip8->V[chip8->instruction.y];
          break;
        case 0x6:
          // 0x8XY6 store 1 in V[F] if the least significant bit in V[X]
          // is 1 otherwise 0. Then divide the result by 2
          chip8->V[0xF] = chip8->V[chip8->instruction.x] & 1;
          chip8->V[chip8->instruction.x] >>= 1;
          break;
        case 0x7:
          // 0x8XY7 store 1 in V[F] if V[X] > V[Y] otherwise 0. Then
          // subtracting V[Y] from V[X] and store the result in V[X]
          chip8->V[0xF] =
              chip8->V[chip8->instruction.y] > chip8->V[chip8->instruction.x];
          chip8->V[chip8->instruction.x] =
              chip8->V[chip8->instruction.y] - chip8->V[chip8->instruction.x];
          break;
        case 0xE:
          // 0x8XYE store 1 in V[F] if the most significant bit in V[X]
          // is 1 otherwise 0
          chip8->V[0xF] = (chip8->V[chip8->instruction.x] >> 7) & 1;
          chip8->V[chip8->instruction.x] <<= 1;
          break;
      }
      break;
    case 0x9:
      // 0x9XY0 if V[X] != V[Y] skip next instruction
      if (chip8->V[chip8->instruction.x] != chip8->V[chip8->instruction.y]) {
        chip8->programCounter += 2;
      }
      break;
    case 0xA:
      // 0xANNN set the indexRegister to NNN
      chip8->indexRegister = chip8->instruction.nnn;
      break;
    case 0xB:
      // 0xBNNN jump to instruction at NNN + V[0x0]
      chip8->programCounter = chip8->instruction.nnn + chip8->V[0x0];
      break;
    case 0xC:
      // 0xCXKK generate a random number between 0 and 255, & it with KK
      // and store the result in V[X]
      chip8->V[chip8->instruction.x] = (rand() % 256) & chip8->instruction.kk;
      break;
    case 0xD:
      // 0xDXYN draw a n byte sprite at V[X], V[Y] coordinates
      // Set V[0xF] to collision detection
      {
        uint8_t dx = chip8->V[chip8->instruction.x] % WINDOW_WIDTH;
        uint8_t dy = chip8->V[chip8->instruction.y] % WINDOW_HEIGHT;
        const uint8_t x = dx;

        // Set V[0xF] to 0 in case of no collision
        chip8->V[0xF] = 0;

        for (uint8_t i = 0; i < chip8->instruction.n; i++) {
          const uint8_t spriteByte = chip8->ram[chip8->indexRegister + i];
          dx = x;

          for (uint8_t j = 7; j > 0; j--) {
            const uint8_t spriteBit = spriteByte & (1 << j);
            uint8_t *frameBufferByte =
                &chip8->frameBuffer[dy * WINDOW_WIDTH + dx];

            // Collision detection
            if (spriteBit && *frameBufferByte) {
              chip8->V[0xF] = (spriteBit && *frameBufferByte);
            }

            *frameBufferByte ^= spriteBit;

            if (++dx >= WINDOW_WIDTH) break;
          }
          if (++dy >= WINDOW_HEIGHT) break;
        }
        chip8->draw = true;
      }
      break;
    case 0xE:
      switch (chip8->instruction.kk) {
        case 0x9E:
          // 0xEX9E skip next instruction if the key stored in V[X] is pressed
          if (chip8->keypad[chip8->V[chip8->instruction.x]]) {
            chip8->programCounter += 2;
          }
          break;
        case 0xA1:
          // 0xEXA1 skip next instruction if the key stored in V[X] is not
          // pressed
          if (!chip8->keypad[chip8->V[chip8->instruction.x]]) {
            chip8->programCounter += 2;
          }
          break;
      }
      break;
    case 0xF:
      switch (chip8->instruction.kk) {
        case 0x07:
          // 0xFX07 Set V[X] to the value of the delay timer
          chip8->V[chip8->instruction.x] = chip8->delayTimer;
          break;
        case 0x0A: {
          // 0xFX0A Wait for a key press, store the value of the key in V[X]
          static uint8_t keyPressed = CHIP8_KEY_UP;
          static uint16_t key = 0xFF;

          for (uint32_t i = 0; i < KEYS && key == 0xFF; i++) {
            if (chip8->keypad[i]) {
              keyPressed = CHIP8_KEY_DOWN;
              key = i;
              break;
            }
          }

          if (!keyPressed) {
            chip8->programCounter -= 2;
          } else {
            if (!chip8->keypad[key]) {
              chip8->programCounter -= 2;
            } else {
              chip8->V[chip8->instruction.x] = key;
              keyPressed = CHIP8_KEY_UP;
              key = 0xFF;
            }
          }
        } break;
        case 0x15:
          // 0xFX15 set the delay timer to V[X]
          chip8->delayTimer = chip8->V[chip8->instruction.x];
          break;
        case 0x18:
          // 0xFX18 set the sound timer to V[X]
          chip8->soundTimer = chip8->V[chip8->instruction.x];
          break;
        case 0x1E:
          // 0xFX1E add V[X] to indexRegister and store the result in
          // indexRegister
          chip8->indexRegister += chip8->V[chip8->instruction.x];
          break;
        case 0x29:
          // 0xFX29 set the indexRegister to the hexadecimal font
          // pointed to by the V[X]
          chip8->indexRegister = chip8->V[chip8->instruction.x] * 5;
          break;
        case 0x33: {
          // 0xFX33 store the binary-coded decimal representation of V[X]
          uint8_t bcd = chip8->V[chip8->instruction.x];
          chip8->ram[chip8->indexRegister + 2] = bcd % 10;
          bcd /= 10;
          chip8->ram[chip8->indexRegister + 1] = bcd % 10;
          bcd /= 10;
          chip8->ram[chip8->indexRegister] = bcd;
          break;
        }
        case 0x55:
          // 0xFX55 Store V[0] to V[X] in memory starting at indexRegister
          for (uint8_t i = 0; i <= chip8->instruction.x; i++) {
            chip8->ram[chip8->indexRegister + i] = chip8->V[i];
          }
          break;
        case 0x65:
          // 0xFX65 Store memory starting at indexRegister to V[0] to V[X]
          for (uint8_t i = 0; i <= chip8->instruction.x; i++) {
            chip8->V[i] = chip8->ram[chip8->indexRegister + i];
          }
          break;
      }
      break;
    default:
      fprintf(stderr, "Unknown instruction: %04X\n", chip8->instruction.raw);
      break;
  }
}

void handleInput(Chip8 *chip8) {
  SDL_Event event;
  // Fetch the next event
  SDL_PollEvent(&event);

  // Keymappings:
  switch (event.type) {
    case SDL_QUIT:
      // Quit the emulator
      chip8->state = QUIT;
      break;
    case SDL_KEYDOWN:
      switch (event.key.keysym.sym) {
        case SDLK_SPACE:
          // Pause or unpause the emulator
          chip8->state = chip8->state == PAUSED ? RUNNING : PAUSED;
          break;
        case SDLK_1:
          chip8->keypad[0x1] = CHIP8_KEY_DOWN;
          break;
        case SDLK_2:
          chip8->keypad[0x2] = CHIP8_KEY_DOWN;
          break;
        case SDLK_3:
          chip8->keypad[0x3] = CHIP8_KEY_DOWN;
          break;
        case SDLK_4:
          chip8->keypad[0xC] = CHIP8_KEY_DOWN;
          break;
        case SDLK_q:
          chip8->keypad[0x4] = CHIP8_KEY_DOWN;
          break;
        case SDLK_w:
          chip8->keypad[0x5] = CHIP8_KEY_DOWN;
          break;
        case SDLK_e:
          chip8->keypad[0x6] = CHIP8_KEY_DOWN;
          break;
        case SDLK_r:
          chip8->keypad[0xD] = CHIP8_KEY_DOWN;
          break;
        case SDLK_a:
          chip8->keypad[0x7] = CHIP8_KEY_DOWN;
          break;
        case SDLK_s:
          chip8->keypad[0x8] = CHIP8_KEY_DOWN;
          break;
        case SDLK_d:
          chip8->keypad[0x9] = CHIP8_KEY_DOWN;
          break;
        case SDLK_f:
          chip8->keypad[0xE] = CHIP8_KEY_DOWN;
          break;
        case SDLK_z:
          chip8->keypad[0xA] = CHIP8_KEY_DOWN;
          break;
        case SDLK_x:
          chip8->keypad[0x0] = CHIP8_KEY_DOWN;
          break;
        case SDLK_c:
          chip8->keypad[0xB] = CHIP8_KEY_DOWN;
          break;
        case SDLK_v:
          chip8->keypad[0xF] = CHIP8_KEY_DOWN;
          break;
      }
      break;
    case SDL_KEYUP:
      switch (event.key.keysym.sym) {
        case SDLK_1:
          chip8->keypad[0x1] = CHIP8_KEY_UP;
          break;
        case SDLK_2:
          chip8->keypad[0x2] = CHIP8_KEY_UP;
          break;
        case SDLK_3:
          chip8->keypad[0x3] = CHIP8_KEY_UP;
          break;
        case SDLK_4:
          chip8->keypad[0xC] = CHIP8_KEY_UP;
          break;
        case SDLK_q:
          chip8->keypad[0x4] = CHIP8_KEY_UP;
          break;
        case SDLK_w:
          chip8->keypad[0x5] = CHIP8_KEY_UP;
          break;
        case SDLK_e:
          chip8->keypad[0x6] = CHIP8_KEY_UP;
          break;
        case SDLK_r:
          chip8->keypad[0xD] = CHIP8_KEY_UP;
          break;
        case SDLK_a:
          chip8->keypad[0x7] = CHIP8_KEY_UP;
          break;
        case SDLK_s:
          chip8->keypad[0x8] = CHIP8_KEY_UP;
          break;
        case SDLK_d:
          chip8->keypad[0x9] = CHIP8_KEY_UP;
          break;
        case SDLK_f:
          chip8->keypad[0xE] = CHIP8_KEY_UP;
          break;
        case SDLK_z:
          chip8->keypad[0xA] = CHIP8_KEY_UP;
          break;
        case SDLK_x:
          chip8->keypad[0x0] = CHIP8_KEY_UP;
          break;
        case SDLK_c:
          chip8->keypad[0xB] = CHIP8_KEY_UP;
          break;
        case SDLK_v:
          chip8->keypad[0xF] = CHIP8_KEY_UP;
          break;
      }
      break;
  }
}

void nextInstruction(Chip8 *chip8) {
  // Fetch raw opcode
  chip8->instruction.raw = (chip8->ram[chip8->programCounter] << 8) |
                           chip8->ram[chip8->programCounter + 1];

  // Point the program counter to the next instruction
  chip8->programCounter += 2;

  // Descontruct the opcode
  chip8->instruction.nnn = chip8->instruction.raw & 0x0FFF;       //*nnn
  chip8->instruction.n = chip8->instruction.raw & 0x000F;         //***n
  chip8->instruction.x = (chip8->instruction.raw >> 8) & 0x000F;  //*x**
  chip8->instruction.y = (chip8->instruction.raw >> 4) & 0x000F;  //**y*
  chip8->instruction.kk = chip8->instruction.raw & 0x00FF;        //**kk
}

void cleanup(const Sdl *sdl) {
  SDL_CloseAudioDevice(sdl->audioDevice);
  SDL_DestroyRenderer(sdl->renderer);
  SDL_DestroyWindow(sdl->window);
  SDL_Quit();
}
