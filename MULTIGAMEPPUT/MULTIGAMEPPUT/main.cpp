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
const int MAX_CLIENTS = 2; // Maximum number of players

SDL_Renderer* renderer;
TCPsocket clients[MAX_CLIENTS];
int snakeX[MAX_CLIENTS][100], snakeY[MAX_CLIENTS][100];
int snakeLength[MAX_CLIENTS];
int direction[MAX_CLIENTS];
int foodX, foodY;
int gameover = 0;

void generateFood() {
	foodX = rand() % (SCREEN_WIDTH / GRID_SIZE) * GRID_SIZE;
	foodY = rand() % (SCREEN_HEIGHT / GRID_SIZE) * GRID_SIZE;
}

void moveSnake(int clientID) {
	// Move the snake by adding a new head in the direction of motion
	for (int i = snakeLength[clientID] - 1; i > 0; i--) {
		snakeX[clientID][i] = snakeX[clientID][i - 1];
		snakeY[clientID][i] = snakeY[clientID][i - 1];
	}
	switch (direction[clientID]) {
	case 0: snakeX[clientID][0] += GRID_SIZE; break; // Right
	case 1: snakeX[clientID][0] -= GRID_SIZE; break; // Left
	case 2: snakeY[clientID][0] -= GRID_SIZE; break; // Up
	case 3: snakeY[clientID][0] += GRID_SIZE; break; // Down
	}

	// Check for collisions with food
	if (snakeX[clientID][0] == foodX && snakeY[clientID][0] == foodY) {
		snakeLength[clientID]++;
		generateFood();
	}

	// Check for collisions with the wall or itself
	if (snakeX[clientID][0] < 0 || snakeX[clientID][0] >= SCREEN_WIDTH || snakeY[clientID][0] < 0 || snakeY[clientID][0] >= SCREEN_HEIGHT) {
		gameover = 1;
	}
	for (int i = 1; i < snakeLength[clientID]; i++) {
		if (snakeX[clientID][0] == snakeX[clientID][i] && snakeY[clientID][0] == snakeY[clientID][i]) {
			gameover = 1;
		}
	}
}

void sendGameState() {
	char message[1024];
	int position = 0;

	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i] && !gameover) {
			position = 0;
			message[position++] = i; // Client ID
			message[position++] = snakeLength[i]; // Snake Length
			for (int j = 0; j < snakeLength[i]; j++) {
				message[position++] = snakeX[i][j] / GRID_SIZE;
				message[position++] = snakeY[i][j] / GRID_SIZE;
			}

			SDLNet_TCP_Send(clients[i], message, position);
		}
	}
}

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	if (SDLNet_Init() == -1) {
		fprintf(stderr, "SDLNet_Init: %s\n", SDLNet_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("Multiplayer Snake Game (Server)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);

	// Generate initial food
	srand(time(0));
	generateFood();

	// Initialize client arrays
	for (int i = 0; i < MAX_CLIENTS; i++) {
		clients[i] = NULL;
		snakeX[i][0] = SCREEN_WIDTH / 2;
		snakeY[i][0] = SCREEN_HEIGHT / 2;
		snakeLength[i] = 1;
		direction[i] = 0;
	}

	IPaddress serverIP;
	TCPsocket serverSocket;

	if (SDLNet_ResolveHost(&serverIP, NULL, 1234) == -1) {
		fprintf(stderr, "SDLNet_ResolveHost: %s\n", SDLNet_GetError());
		return 1;
	}

	serverSocket = SDLNet_TCP_Open(&serverIP);

	SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(MAX_CLIENTS + 1);

	SDLNet_TCP_AddSocket(socketSet, serverSocket);

	SDL_Event event;
	while (!gameover) {
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				gameover = 1;
			}
		}

		int activeSockets = SDLNet_CheckSockets(socketSet, 0);
		if (activeSockets == -1) {
			fprintf(stderr, "SDLNet_CheckSockets: %s\n", SDLNet_GetError());
		}

		// Accept new connections
		if (SDLNet_SocketReady(serverSocket)) {
			for (int i = 0; i < MAX_CLIENTS; i++) {
				if (!clients[i]) {
					clients[i] = SDLNet_TCP_Accept(serverSocket);
					if (clients[i]) {
						SDLNet_TCP_AddSocket(socketSet, clients[i]);
						break;
					}
				}
			}
		}

		// Receive and process client messages
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i]) {
				if (SDLNet_SocketReady(clients[i])) {
					char message[1024];
					int received = SDLNet_TCP_Recv(clients[i], message, sizeof(message));

					if (received <= 0) {
						// Client disconnected
						SDLNet_TCP_DelSocket(socketSet, clients[i]);
						SDLNet_TCP_Close(clients[i]);
						clients[i] = NULL;
					}
					else {
						// Update client direction
						direction[i] = message[0];
					}
				}
			}
		}

		// Move the snakes
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i] && !gameover) {
				moveSnake(i);
			}
		}

		// Check for collisions
		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i] && !gameover) {
				for (int j = 0; j < MAX_CLIENTS; j++) {
					if (j != i && clients[j] && !gameover) {
						for (int k = 0; k < snakeLength[j]; k++) {
							if (snakeX[i][0] == snakeX[j][k] && snakeY[i][0] == snakeY[j][k]) {
								gameover = 1;
							}
						}
					}
				}
			}
		}

		// Send game state to clients
		sendGameState();

		// Draw game objects (snakes and food)
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		SDL_RenderClear(renderer);
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

		for (int i = 0; i < MAX_CLIENTS; i++) {
			if (clients[i] && !gameover) {
				for (int j = 0; j < snakeLength[i]; j++) {
					SDL_Rect rect = { snakeX[i][j], snakeY[i][j], GRID_SIZE, GRID_SIZE };
					SDL_RenderFillRect(renderer, &rect);
				}
			}
		}

		// Draw the food
		SDL_Rect foodRect = { foodX, foodY, GRID_SIZE, GRID_SIZE };
		SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
		SDL_RenderFillRect(renderer, &foodRect);

		SDL_RenderPresent(renderer);
		SDL_Delay(1000 / SNAKE_SPEED);
	}

	// Cleanup
	for (int i = 0; i < MAX_CLIENTS; i++) {
		if (clients[i]) {
			SDLNet_TCP_Close(clients[i]);
		}
	}
	SDLNet_TCP_Close(serverSocket);
	SDLNet_Quit();
	SDL_Quit();

	return 0;
}