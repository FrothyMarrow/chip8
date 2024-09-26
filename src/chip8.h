#include <SDL2/SDL.h>
// std
#include <stdbool.h>
#include <stdint.h>

#define WINDOW_WIDTH 64
#define WINDOW_HEIGHT 32
#define FRAME_RATE 60
#define FRAME_DURATION_IN_MS (16.67f)

#define NUM_REGISTERS 16
#define RAM_SIZE 0x1000
#define STACK_SIZE 0x40
#define FONT_SIZE 0x200

#define CHIP8_KEY_DOWN 1
#define CHIP8_KEY_UP 0
#define KEYS 16

// Sdl state
typedef struct {
  SDL_Window* window;
  SDL_Renderer* renderer;
  SDL_AudioSpec want;
  SDL_AudioSpec have;
  SDL_AudioDeviceID audioDevice;
} Sdl;

// Emulator configuration
typedef struct {
  uint32_t scaleFactor;
  uint32_t foregroundColor;
  uint32_t backgroundColor;
  uint32_t sampleFrequency;
  uint32_t sampleSize;
  uint32_t audioFrequency;
  uint32_t audioAmplitude;
  uint32_t instructionsPerSecond;
  char* romName;
  bool outlines;
} Config;

// Deconstructed instruction
typedef struct {
  uint16_t raw;
  uint16_t nnn;
  uint8_t n;
  uint8_t x;
  uint8_t y;
  uint8_t kk;
} Instruction;

// Emulator state
typedef enum { QUIT = 0, PAUSED, RUNNING } State;

// Emulator specification
typedef struct {
  uint8_t frameBuffer[WINDOW_WIDTH * WINDOW_HEIGHT];
  uint8_t V[NUM_REGISTERS];
  uint16_t stack[STACK_SIZE];
  uint8_t ram[RAM_SIZE];
  uint8_t keypad[KEYS];
  uint16_t indexRegister;
  uint16_t* stackPointer;
  uint16_t programCounter;
  uint8_t delayTimer;
  uint8_t soundTimer;
  uint8_t draw;
  Instruction instruction;
  State state;
} Chip8;

/**
 * Initializes the emulator configuration.
 * @param config - the emulator configuration
 */
void defaultConfig(Config* config);

/**
 * Initializes the sdl window and renderer.
 * @param sdl - the sdl state
 * @param config - the emulator configuration
 * @return true if initialization was successful, false otherwise
 */
bool initGraphics(Sdl* sdl, const Config* config);

/**
 * Initializes the sdl audio.
 * @param sdl - the sdl state
 * @param config - the emulator configuration
 * @return true if initialization was successful, false otherwise
 */
bool initAudio(Sdl* sdl, Config* config);

/**
 * Initializes the sdl subsystems(video and audio).
 * @param sdl - the sdl state
 * @param config - the emulator configuration
 * @return true if initialization was successful, false otherwise
 */
bool initSdl(Sdl* sdl, Config* config);

/**
 * Initializes the emulator state by loading the font and rom into memory
 * and setting the program counter to the start of the rom.
 * @param chip8 - the emulator state
 * @param config - the emulator configuration
 * @return true if initialization was successful, false otherwise
 */
bool initChip8(Chip8* chip8, const Config* config);

/**
 * Generates a square wave of the given frequency and amplitude.
 * A callback function for the sdl audio device.
 * @param userData - the user data
 * @param stream - the audio stream
 * @param len - the length of the audio stream
 */
void squareWave(void* userData, uint8_t* stream, const int32_t len);

/**
 * Plays the audio sample if the sound timer is greater than 0.
 * @param chip8 - the emulator state
 * @param sdl - the sdl state
 */
void sound(const Chip8* chip8, const Sdl* sdl);

/**
 * Loads the font into memory.
 * @param chip8 - the emulator state
 */
void loadFont(Chip8* chip8);

/**
 * Loads the rom into memory and sets the program counter to the
 * start of the rom.
 * @param chip8 - the emulator state
 * @param filePath - the path to the rom
 * @return true if loading was successful, false otherwise
 */
bool loadRom(Chip8* chip8, const char* filePath);

/**
 * Updates the timers by decrementing them if they are greater than 0
 * at a rate of 60hz.
 * @param chip8 - the emulator state
 */
void updateTimers(Chip8* chip8);

/**
 * Draws the updated frame buffer to the screen if the draw flag is set.
 * @param sdl - the sdl state
 * @param chip8 - the emulator state
 * @param config - the emulator configuration
 */
void draw(const Sdl* sdl, Chip8* chip8, const Config* config);

/**
 * Converts the hex color value to RGBA and sets it as the draw color.
 * @param sdl - the sdl state
 * @param color - the hex color
 */
void drawColor(const Sdl* sdl, const uint32_t color);

/**
 * Clears the frame buffer and draws a pixel to the screen at the
 * given coordinates.
 * @param sdl - the sdl state
 * @param config - the emulator configuration
 * @param x - the x coordinate
 * @param y - the y coordinate
 */
void drawPixel(const Sdl* sdl, const Config* config, const uint32_t x,
               const uint32_t y);

/**
 * Clears the frame buffer.
 * @param sdl - the sdl state
 * @param config - the emulator configuration
 */
void clearFrameBuffer(const Sdl* sdl, const Config* config);

/**
 * Fetches the next instruction from memory.
 * @param chip8 - the emulator state
 */
void nextInstruction(Chip8* chip8);

/**
 * Decodes and executes the instruction.
 * @param chip8 - the emulator state
 * @param config - the emulator configuration
 */
void emulateInstruction(Chip8* chip8, const Config* config);

/**
 * Handles the input by mapping the chip8 keypad to the
 * sdl keyboard.
 * @param chip8 - the emulator state
 */
void handleInput(Chip8* chip8);

/**
 * Destroys the sdl subsystems(video and audio).
 * @param sdl - the sdl state
 */
void cleanup(const Sdl* sdl);
