// *******************************************************
// Author: Paolo "Lazharous" Mainieri
// Date: 21 Aug 2014
// Description: Yace stands for Yet Another Chip8 Emulator
// See http://en.wikipedia.org/wiki/CHIP-8 and
// http://devernay.free.fr/hacks/chip8/C8TECH10.HTM
// *******************************************************

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <SDL.h>
#include <SDL_opengl.h>

#define YACE_STACK_SIZE 16
#define YACE_ROM_SIZE 0xfff
#define YACE_SCREEN_WIDTH 640
#define YACE_SCREEN_HEIGHT 320
#define YACE_SCREEN_SCALE 10

//typedef unsigned int WORD;
//typedef unsigned char BYTE;

// **********************************
// Structure holding the Chip8 state
// **********************************
typedef struct _SCHIP8
{
	// ROM here is a file
	FILE *ROM;
	// RAM is 4kb
	BYTE RAM[4096];
	// Address register
	WORD I;
	// Program counter
	WORD PC;
	// Registers V0 - VF
	// The VF register doubles as a carry flag
	WORD V[16];
	// Keyboard buttons
	WORD Key[16];
	// Stack (should be 12)
	WORD Stack[YACE_STACK_SIZE];
	// Stack pointer
	WORD SP;
	// Delay Timer, count down to 0 at 60Hz
	WORD delayTimer;
	// Sound Timer, count down to 0 at 60Hz
	WORD soundTimer;
	// Video Screen
	BYTE Video[YACE_SCREEN_WIDTH][YACE_SCREEN_WIDTH][3];
	// Window for screen
	SDL_Window *Window;
} SCHIP8;

// *********************
// functions prototypes
// *********************
void YACE_Message(void);
void YACE_Reset(SCHIP8 *ctx);

int YACE_OpenROM(SCHIP8 *ctx, char *filename);
WORD YACE_FetchOpcode(SCHIP8 *ctx);
void YACE_ExecuteOpcode(SCHIP8 *ctx, WORD opcode);

void YACE_ShowHexROM(SCHIP8 *ctx);

void YACE_InitScreen(SCHIP8 *ctx);
void YACE_BeginScene(void);
void YACE_Render(SCHIP8 *ctx);
void YACE_EndScene(SCHIP8 *ctx);

int YACE_GetInput(SCHIP8 *ctx);
void YACE_PlaySound(void);

void YACE_Decode0NNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute1NNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute2NNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute3XNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute4XNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute5XY0Opcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute6XNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute7XNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Decode8XYNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_Execute9XY0Opcode(SCHIP8 *ctx, WORD opcode);
void YACE_ExecuteANNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_ExecuteBNNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_ExecuteCXNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_ExecuteDXYNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_DecodeEXNNOpcode(SCHIP8 *ctx, WORD opcode);
void YACE_DecodeFXNNOpcode(SCHIP8 *ctx, WORD opcode);

// ***************
// functions
// ***************
void YACE_Message(void)
{
	printf("YACE v0.2 BUILD 140821\n"
		   "Usage: yace ROM\n");
}

void YACE_Reset(SCHIP8 *ctx)
{
	memset(ctx->Stack, 0, YACE_STACK_SIZE);
	memset(ctx->RAM, 0, YACE_ROM_SIZE);
	memset(ctx->Key, 0, 16);
	ctx->I = 0;
	ctx->PC = 512;
	ctx->SP = 0;
	ctx->delayTimer = 0;
	ctx->soundTimer = 0;
}

int YACE_OpenROM(SCHIP8 *ctx, char *filename)
{
	ctx->ROM = fopen(filename, "rb");
	if (!ctx->ROM)
		return 0;

	fread(&ctx->RAM[512], 0xfff, 1, ctx->ROM);
	fclose(ctx->ROM);

	return 1;
}

void YACE_ShowHexROM(SCHIP8 *ctx)
{
	int i,j;

	for (i = 0; i < 256; i+=16)
	{
		for (j = 0; j < 16; j++)
			printf("%02x ", (ctx->RAM[i+j + 512] & 0xff));

		printf("\n");
	}
}

WORD YACE_FetchOpcode(SCHIP8 *ctx)
{
	int opcode = ((ctx->RAM[ctx->PC] << 8) | ctx->RAM[ctx->PC + 1]);
	ctx->PC += 2;
	return (opcode & 0xffff);
}

void YACE_Decode0NNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	int i, j;

	switch (opcode & 0xf)
	{
		// Clears the screen.
		case 0x0:
		{
			for (i = 0; i < YACE_SCREEN_WIDTH; i++)
			{
				for (j = 0; j < YACE_SCREEN_HEIGHT; j++)
				{
					ctx->Video[j][i][0] = 0;
					ctx->Video[j][i][1] = 0;
					ctx->Video[j][i][2] = 0;
				}
			}
		} break;
		// Returns from a subroutine.
		case 0xE: ctx->PC = ctx->Stack[ctx->SP--]; break;
	}
}

// Jumps to address NNN.
void YACE_Execute1NNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->PC = (opcode & 0x0fff);
}

// Calls subroutine at NNN.
void YACE_Execute2NNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->Stack[ctx->SP++] = ctx->PC;
	ctx->PC = (opcode & 0x0fff);
}

// Skips the next instruction if VX equals NN.
void YACE_Execute3XNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	if (ctx->V[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF))
		ctx->PC += 2;
}

// Skips the next instruction if VX doesn't equal NN.
void YACE_Execute4XNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	if (ctx->V[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF))
		ctx->PC += 2;
}

// Skips the next instruction if VX equals VY.
void YACE_Execute5XY0Opcode(SCHIP8 *ctx, WORD opcode)
{
	if (ctx->V[(opcode & 0x0F00) >> 8] == ctx->V[(opcode & 0x00F0) >> 4])
		ctx->PC += 2;
}

// Sets VX to NN.
void YACE_Execute6XNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->V[(opcode & 0x0F00) >> 8] = (opcode & 0x00FF);
}

// Adds NN to VX.
void YACE_Execute7XNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->V[(opcode & 0x0F00) >> 8] += (opcode & 0x00FF);
}

// Decode the 8XYN opcode
void YACE_Decode8XYNOpcode(SCHIP8 *ctx, WORD opcode)
{
	switch (opcode & 0x000F)
	{
		// Sets VX to the value of VY.
		case 0x0:
			ctx->V[(opcode & 0x0F00) >> 8] = ctx->V[(opcode & 0x00F0) >> 4];
		break;
		// Sets VX to VX or VY.
		case 0x1:
			ctx->V[(opcode & 0x0F00) >> 8] |= ctx->V[(opcode & 0x00F0) >> 4];
		break;
		// Sets VX to VX and VY.
		case 0x2:
			ctx->V[(opcode & 0x0F00) >> 8] &= ctx->V[(opcode & 0x00F0) >> 4];
		break;
		// Sets VX to VX xor VY.
		case 0x3:
			ctx->V[(opcode & 0x0F00) >> 8] ^= ctx->V[(opcode & 0x00F0) >> 4];
		break;
		// Adds VY to VX. VF is set to 1 when there's a carry, and to 0 when there isn't.
		case 0x4:
		{
			int value = (ctx->V[(opcode & 0x0F00) >> 8] + ctx->V[(opcode & 0x00F0) >> 4]);
			
			if (value > 0xFF)
				ctx->V[0xF] = 1;
			else
				ctx->V[0xF] = 0;

			ctx->V[(opcode & 0x0F00) >> 8] = value;
		} break;
		// VY is subtracted from VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
		case 0x5:
		{
			int value = (ctx->V[(opcode & 0x0F00) >> 8] - ctx->V[(opcode & 0x00F0) >> 4]);

			if (value > 0)
				ctx->V[0xF] = 1;
			else
				ctx->V[0xF] = 0;

			ctx->V[(opcode & 0x0F00) >> 8] = value;
		} break;
		// Shifts VX right by one. VF is set to the value of the least significant bit of VX before the shift
		case 0x6:
		{
			ctx->V[0xF] = ctx->V[(opcode & 0x0F00) >> 8] & 0x0001;
			ctx->V[(opcode & 0x0F00) >> 8] >>= 1;
		} break;
		// Sets VX to VY minus VX. VF is set to 0 when there's a borrow, and 1 when there isn't.
		case 0x7:
		{
			int value = ctx->V[(opcode & 0x00F0) >> 4] - ctx->V[(opcode & 0x0F00) >> 8];
			ctx->V[(opcode & 0x0F00) >> 8] = value;

			if (value < 0)
				ctx->V[0xF] = 0;
			else
				ctx->V[0xF] = 1;
		} break;
		// Shifts VX left by one. VF is set to the value of the most significant bit of VX before the shift.
		case 0xE:
		{
			ctx->V[0xF] = ctx->V[(opcode & 0x0F00) >> 8] >> 7;
			ctx->V[(opcode & 0x0F00) >> 8] <<= 1;
		} break;
	}
}

// Skips the next instruction if VX doesn't equal VY.
void YACE_Execute9XY0Opcode(SCHIP8 *ctx, WORD opcode)
{
	if (ctx->V[(opcode & 0x0F00) >> 8] != ctx->V[(opcode & 0x00F0) >> 4])
		ctx->PC += 2;
}

// Sets I to the address NNN.
void YACE_ExecuteANNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->I = (opcode & 0x0FFF);
}

// Jumps to the address NNN plus V0.
void YACE_ExecuteBNNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->PC = ctx->V[0] + (opcode & 0x0FFF);
}

// Sets VX to a random number and NN.
void YACE_ExecuteCXNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	ctx->V[(opcode & 0x0F00) >> 8] = rand() + (opcode & 0x00FF);
}

// Sprites stored in memory at location in index register (I),
// maximum 8bits wide. Wraps around the screen. If when drawn,
// clears a pixel, register VF is set to 1 otherwise it is zero.
// All drawing is XOR drawing (e.g. it toggles the screen pixels)
void YACE_ExecuteDXYNOpcode(SCHIP8 *ctx, WORD opcode)
{
	int xcoord = ctx->V[(opcode & 0x0F00) >> 8] * YACE_SCREEN_SCALE;
	int ycoord = ctx->V[(opcode & 0x00F0) >> 4] * YACE_SCREEN_SCALE;
	int height = opcode & 0x000F;

	ctx->V[0xF] = 0;

	for (int yline = 0; yline < height; yline++)
	{
		BYTE data = ctx->RAM[ctx->I + yline];

		int xpixel = 0;
		int xpixelinv = 7;
		
		for (xpixel = 0; xpixel < 8; xpixel++, xpixelinv--)
		{
			if (data & (1 << xpixelinv))
			{
				int x = (xpixel * YACE_SCREEN_SCALE) + xcoord;
				int y = (yline * YACE_SCREEN_SCALE) + ycoord;

				if (ctx->Video[y][x][0] == 0)
					ctx->V[0xF] = 1;

				for (int i = 0; i < YACE_SCREEN_SCALE; i++)
				{
					for (int j = 0; j < YACE_SCREEN_SCALE; j++)
					{
						ctx->Video[y + i][x + j][0] ^= 0xFF;
						ctx->Video[y + i][x + j][1] ^= 0xFF;
						ctx->Video[y + i][x + j][2] ^= 0xFF;
					}
				}

			}
		}
	}
}

void YACE_DecodeEXNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	int index = ctx->V[(opcode & 0x0F00) >> 8];

	switch (opcode & 0x00FF)
	{
		// Skips the next instruction if the key stored in VX is pressed.
		case 0x9E:
		{
			if (ctx->Key[index] == 1)
				ctx->PC += 2;
		} break;
		// Skips the next instruction if the key stored in VX isn't pressed.
		case 0xA1:
		{
			if (ctx->Key[index] != 1)
				ctx->PC += 2;
		} break;
	}
}

void YACE_DecodeFXNNOpcode(SCHIP8 *ctx, WORD opcode)
{
	switch (opcode & 0x00FF)
	{
		// Sets VX to the value of the delay timer.
		case 0x07:
			ctx->V[(opcode & 0x0F00) >> 8] = ctx->delayTimer;
			break;
		// A key press is awaited, and then stored in VX.
		case 0x0A:
		{
			int k = YACE_GetInput(ctx);

			if (k == -1)
				ctx->PC -= 2;
			else
				ctx->V[(opcode & 0x0F00) >> 8] = k;
		} break;
		// Sets the delay timer to VX.
		case 0x15:
			ctx->delayTimer = ctx->V[(opcode & 0x0F00) >> 8];
			break;
		// Sets the sound timer to VX.
		case 0x18:
			ctx->soundTimer = ctx->V[(opcode & 0x0F00) >> 8];
			break;
		// Adds VX to I.
		// VF is set to 1 when range overflow (I+VX>0xFFF), and 0 when there isn't.
		// This is undocumented feature of the Chip-8 and used by Spacefight 2019! game.
		case 0x1E:
		{
			ctx->I += ctx->V[(opcode & 0x0F00) >> 8];

			if (ctx->I + ctx->V[(opcode & 0x0F00) >> 8] > 0xFFF)
				ctx->V[0xF] = 1;
			else
				ctx->V[0xF] = 0;

		} break;
		// Sets I to the location of the sprite for the character in VX.
		// Characters 0-F (in hexadecimal) are represented by a 4x5 font.
		case 0x29:
			ctx->I = ctx->V[(opcode & 0x0F00) >> 8] * 5;
			break;
		// Stores the Binary-coded decimal representation of VX,
		// with the most significant of three digits at the address in I,
		// the middle digit at I plus 1, and the least significant digit at I plus 2.
		// (In other words, take the decimal representation of VX,
		// place the hundreds digit in memory at location in I,
		// the tens digit at location I+1, and the ones digit at location I+2.)
		case 0x33:
		{
			int value = ctx->V[(opcode & 0x0F00) >> 8];

			ctx->RAM[ctx->I + 0] = value / 100;
			ctx->RAM[ctx->I + 1] = (value / 10) % 10;
			ctx->RAM[ctx->I + 2] = value % 10;
		} break;
		// Stores V0 to VX in memory starting at address I.
		// On the original interpreter, when the operation is done, I=I+X+1.
		case 0x55:
		{
			int i;
			int N = (opcode & 0x0F00) >> 8;

			for (i = 0; i <= N; i++)
				ctx->RAM[i + ctx->I] = ctx->V[i];

			ctx->I += ctx->V[N] + 1;
		} break;
		// Fills V0 to VX with values from memory starting at address I.
		case 0x65:
		{
			int i;
			int N = (opcode & 0x0F00) >> 8;

			for (i = 0; i <= N; i++)
				ctx->V[i] = ctx->RAM[i + ctx->I];

			ctx->I += ctx->V[N] + 1;
		} break;
	}
}

void YACE_ExecuteOpcode(SCHIP8 *ctx, WORD opcode)
{
	// *********************************
	// Since we have 0NNN-FNNN opcodes,
	// we see the first letter first
	// *********************************
	switch (opcode & 0xF000)
	{
		case 0x0000: YACE_Decode0NNNOpcode(ctx, opcode); break;
		case 0x1000: YACE_Execute1NNNOpcode(ctx, opcode); break;
		case 0x2000: YACE_Execute2NNNOpcode(ctx, opcode); break;
		case 0x3000: YACE_Execute3XNNOpcode(ctx, opcode); break;
		case 0x4000: YACE_Execute4XNNOpcode(ctx, opcode); break;
		case 0x5000: YACE_Execute5XY0Opcode(ctx, opcode); break;
		case 0x6000: YACE_Execute6XNNOpcode(ctx, opcode); break;
		case 0x7000: YACE_Execute7XNNOpcode(ctx, opcode); break;
		case 0x8000: YACE_Decode8XYNOpcode(ctx, opcode); break;
		case 0x9000: YACE_Execute9XY0Opcode(ctx, opcode); break;
		case 0xA000: YACE_ExecuteANNNOpcode(ctx, opcode); break;
		case 0xB000: YACE_ExecuteBNNNOpcode(ctx, opcode); break;
		case 0xC000: YACE_ExecuteCXNNOpcode(ctx, opcode); break;
		case 0xD000: YACE_ExecuteDXYNOpcode(ctx, opcode); break;
		case 0xE000: YACE_DecodeEXNNOpcode(ctx, opcode); break;
		case 0xF000: YACE_DecodeFXNNOpcode(ctx, opcode); break;
	}
}

// Returns the index of the key pressed, if any
int YACE_GetInput(SCHIP8 *ctx)
{
	int key = -1;
	SDL_Event evt;
	SDL_PollEvent(&evt);

	switch (evt.type)
	{
		case SDL_QUIT:
			exit(0);
			break;

		case SDL_KEYUP:
		{
			switch (evt.key.keysym.sym)
			{
				case SDLK_LEFT:
					key = 4;
					break;
				case SDLK_UP:
					key = 1;
					break;
				case SDLK_RIGHT:
					key = 6;
					break;
				case SDLK_DOWN:
					key = 9;
					break;
			}

			if (key != -1)
			{
				ctx->Key[key] = 0;
				key = -1;
			}

		} break;

		case SDL_KEYDOWN:
		{
			switch (evt.key.keysym.sym)
			{
				case SDLK_LEFT:
					key = 4;
					break;
				case SDLK_UP:
					key = 1;
					break;
				case SDLK_RIGHT:
					key = 6;
					break;
				case SDLK_DOWN:
					key = 9;
					break;
			}

			if (key != -1)
				ctx->Key[key] = 1;
		}
	}

	return key;
}

void YACE_InitScreen(SCHIP8 *ctx)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

	ctx->Window = SDL_CreateWindow("Yace",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		YACE_SCREEN_WIDTH, YACE_SCREEN_HEIGHT,
		SDL_WINDOW_OPENGL);

	SDL_GL_CreateContext(ctx->Window);

	glViewport(0, 0, YACE_SCREEN_WIDTH , YACE_SCREEN_HEIGHT );
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glOrtho(0, YACE_SCREEN_WIDTH, YACE_SCREEN_HEIGHT, 0, -1.0, 1.0);
	glClearColor(0x40, 0x40, 0x40, 1.0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glShadeModel(GL_FLAT);
	glEnable(GL_TEXTURE_2D);
	glDisable(GL_BLEND);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DITHER);
}

void YACE_BeginScene(void)
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void YACE_Render(SCHIP8 *ctx)
{
	glLoadIdentity();
	glRasterPos2i(-1, 1);
	glPixelZoom(1, -1);
	glDrawPixels(YACE_SCREEN_WIDTH,
				 YACE_SCREEN_HEIGHT,
				 GL_RGB, GL_UNSIGNED_BYTE, ctx->Video);
}

void YACE_EndScene(SCHIP8 *ctx)
{
	SDL_GL_SwapWindow(ctx->Window);
	glFlush();
}

void YACE_PlaySound(void)
{
	printf("\a");
}

void YACE_Loop(SCHIP8 *ctx)
{
	int i;
	int done = 0;
	unsigned int t2;
	float update_rate = 1000 / 60;
	float opcode_per_sec = 800 / 60;
	unsigned int t = SDL_GetTicks();

	while (!done)
	{
		YACE_GetInput(ctx);

		t2 = SDL_GetTicks();

		// If 1000/60 passed, update the CPU
		if ((t + update_rate) < t2)
		{
			if (ctx->delayTimer > 0) ctx->delayTimer--;
			if (ctx->soundTimer > 0) ctx->soundTimer--;
			if (ctx->soundTimer > 0) YACE_PlaySound();

			for (i = 0; i < opcode_per_sec; i++)
			{
				WORD opcode = YACE_FetchOpcode(ctx);
				printf("OPCODE: %04x\n", opcode);
				YACE_ExecuteOpcode(ctx, opcode);
			}

			t = t2;

			YACE_BeginScene();
			YACE_Render(ctx);
			YACE_EndScene(ctx);
		}
	}
}

int main(int argc, char *argv[])
{
	SCHIP8 *emu = (SCHIP8 *)malloc(sizeof(SCHIP8));

	srand(time(NULL));
	// First reset the emulator state
	YACE_Reset(emu);
	
	if (argc < 2)
		YACE_Message();

	// load the ROM in RAM starting from 0x200 until 0xfff
	YACE_OpenROM(emu, argv[1]);
	YACE_InitScreen(emu);
	YACE_BeginScene();

	YACE_Loop(emu);

	free(emu);
	return 0;
}
