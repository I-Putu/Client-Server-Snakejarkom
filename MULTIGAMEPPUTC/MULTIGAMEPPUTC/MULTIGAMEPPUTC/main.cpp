#include <SDL.h>
#include <SDL_net.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

const int SCREEN_WIDTH = 640;
const int SCREEN_HEIGHT = 480;
const int GRID_SIZE = 20;
const int SNAKE_SPEED = 10;

SDL_Renderer* renderer;
TCPsocket server;
int snakeX[100], snakeY[100];
int snakeLength;
int direction;
int gameover = 0;

void sendDirection(int dir) {
	char message[1];
	message[0] = (char)dir;
	SDLNet_TCP_Send(server, message, 1);
}

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	if (SDLNet_Init() == -1) {
		fprintf(stderr, "SDLNet_Init: %s\n", SDLNet_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Multiplayer Snake Game (Client)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

	snakeLength = 1;
	direction = 0;

	IPaddress serverIP;
	server = NULL;

	if (SDLNet_ResolveHost(&serverIP, "10.252.39.123", 1234) == -1) {
		fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		return 1;
	}

	server = SDLNet_TCP_Open(&serverIP);
	if (!server) {
		fprintf(stderr, "SDLNet_TCP_Open: %s\n", SDLNet_GetError());
		return 1;
	}

	SDL_Event event;
	while (!gameover) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				gameover = 1;
			}
			if (event.type == SDL_KEYDOWN) {
				switch (event.key.keysym.sym) {
				case SDLK_d:
					if (direction != 1) {
						direction = 0; // Right
						sendDirection(direction);
					}
					break;
				case SDLK_a:
					if (direction != 0) {
						direction = 1; // Left
						sendDirection(direction);
					}
					break;
				case SDLK_w:
					if (direction != 3) {
						direction = 2; // Up
						sendDirection(direction);
					}
					break;
				case SDLK_s:
					if (direction != 2) {
						direction = 3; // Down
						sendDirection(direction);
					}
					break;
				}
			}
		}

		// Receive game state from the server
		char message[1024];
		int received = SDLNet_TCP_Recv(server, message, sizeof(message));

		if (received <= 0) {
			// Server disconnected
			SDLNet_TCP_Close(server);
			server = NULL;
			gameover = 1;
		}
		else {
			int position = 0;
			int clientID = (int)message[position++];
			int len = (int)message[position++];
			for (int i = 0; i < len; i++) {
				snakeX[i] = (int)message[position++] * GRID_SIZE;
				snakeY[i] = (int)message[position++] * GRID_SIZE;
			}
			snakeLength = len;
		}

		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

		// Draw the snake
		for (int i = 0; i < snakeLength; i++) {
			SDL_Rect rect = { snakeX[i], snakeY[i], GRID_SIZE, GRID_SIZE };
			SDL_RenderFillRect(renderer, &rect);
		}

		SDL_RenderPresent(renderer);
		SDL_Delay(1000 / SNAKE_SPEED);
	}

	// Cleanup
	if (server) {
		SDLNet_TCP_Close(server);
	}
	SDLNet_Quit();
	SDL_Quit();

	return 0;
}
