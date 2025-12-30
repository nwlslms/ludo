#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>

typedef struct {
	int bidak;
	int y, x;
	int warna;
} Bidak;

Bidak bidak[16] = {
	{1, 1, 2, 1},
	{2, 1, 3, 1},
	{1, 1, 11, 2},
	{2, 1, 12, 2},
	{3, 3, 2, 1},
	{4, 3, 3, 1},
	{3, 3, 11, 2},
	{4, 3, 12, 2},
	{1, 11, 2, 4},
	{2, 11, 3, 4},
	{1, 11, 11, 3},
	{2, 11, 12, 3},
	{3, 13, 2, 4},
	{4, 13, 3, 4},
	{3, 13, 11, 3},
	{4, 13, 12, 3}
};

char map[15][15] = {"111111000222222",
					"100001022200002",
					"100001920200002",
					"100001020200002",
					"100001020200002",
					"111111020222222",
					"010000122000900",
					"011111193333330",
					"009000443000030",
					"444444040333333",
					"400004040300003",
					"400004040300003",
					"400004049300003",
					"400004440300003",
					"444444000333333"};

typedef struct {
	short warna;
	short inside[4];
	short insd;
	short outside[4];;
	short otsd;
} Team;

void print();
void menu();
void mulai();
void ngocok(Team);
void ambilPawn(Team *p);
// void setBidak(short, short);
Team set(short);
short checkExit();
short dadu();

int main () {
	menu();
	system("CLS");
	printf("Thanks for playing!\n");
	return 0;
}

void menu() {
	short pil = 1;
	char cmd;
	while (1) {
		system("CLS");
		printf("==============================\n");
		printf("+             LUDO           +\n");
		printf("==============================\n");
		printf("1. Play game %s", (pil == 1) ? "<<<\n" : "\n");
		printf("2. Exit      %s", (pil == 2) ? "<<<\n" : "\n");
		printf("==============================\n>> ");
		cmd = getch();
		switch(cmd) {
			case '1': pil = 1; break;
			case '2': pil = 2; break;
			case 'w':
				if(pil == 1) {
					pil = 1;
				} else {
					pil--;
				}
				break;
			case 's':
				if(pil == 2) {
					pil = 2;
				} else {
					pil++;
				}
				break;
			case '\r':
				if(pil == 1) {
					mulai();
				} else if (pil == 2) {
					short balik = checkExit();
					if(balik == 1) return;
				}
				break;
			default: break;
		}
	}
}

short checkExit() {
	char cmd;
	do {
		system("CLS");
		printf("Are you sure you want to exit?\nAny progress won't be saved! (y/n)\n>> ");
		cmd = getch();
	} while (cmd != 'y' && cmd != 'n');
	if(cmd == 'y') {
		return 1;
	} else {
		return 0;
	}
}

void print() {
    int i, j, k;
    for (i = 0; i < 15; i++) {
        for (j = 0; j < 15; j++) {
            short printed = 0;
            for (k = 0; k < 16; k++) {
                if (i == bidak[k].y && j == bidak[k].x) {
                    switch(bidak[k].warna) {
						case 1: printf("\033[102m    \033[0m"); break;
						case 2: printf("\033[103m    \033[0m"); break;
						case 3: printf("\033[104m    \033[0m"); break;
						case 4: printf("\033[101m    \033[0m"); break;
						default: break;
					}
					printed = 1;
                }
            }
            if (!printed) {
                switch (map[i][j]) {
                    case '1': printf("\033[42m    \033[0m"); break; // hijau
                    case '2': printf("\033[43m    \033[0m"); break; // kuning
                    case '3': printf("\033[44m    \033[0m"); break; // biru
                    case '4': printf("\033[41m    \033[0m"); break; // merah
                    case '9': printf("\033[100m    \033[0m"); break; // putih
                    default:  printf("\033[47m    \033[0m"); break; // default putih
                }
            }
        }
        printf("\n");
    }
}

short dadu() {
	return (rand() % 6) + 1;
}

Team set(short warna) {
	Team temp;
	temp.warna = warna;
	temp.inside[0] = 1;
	temp.inside[1] = 2;
	temp.inside[2] = 3;
	temp.inside[3] = 4;
	temp.insd = 4;
	temp.otsd = 0;
	memset(temp.outside, 0, sizeof(temp.outside));
	return temp;
}

void mulai() {
	Team green = set(1);
	Team yellow = set(2);
	Team blue = set(3);
	Team red = set(4);
	system("CLS");
	printf("Game start!\n");
	Sleep(1500);
	system("CLS");

	srand(time(NULL));
	Team Player;
	short player = (rand() % 4) + 1;
	short jalan = (rand() % 4) + 1;
	printf("You play as ");
	if (player == 1) {
		printf("Green!\n");
		Player = green;
	} else if (player == 2) {
		printf("Yellow!\n");
		Player = yellow;
	} else if (player == 3) {
		printf("Blue!\n");
		Player = blue;
	} else {
		printf("Red!\n");
		Player = red;
	}
	Sleep(1000);
	switch(jalan) {
		case 1: printf("Green first!\n"); break;
		case 2: printf("Yellow first!\n"); break;
		case 3: printf("Blue first!\n"); break;
		case 4: printf("Red first!\n"); break;
		default: break;
	}
	Sleep(2000);
	while(1) {
		if(jalan == player) {
			ngocok(Player);
			short kocokan = dadu();
			printf("\nYour dice: %d\n", kocokan);
			if (Player.otsd == 0) {
				if (kocokan != 6) {
					printf("You need to get 6 in order take the pawn! SKIPPED!\n");
					system("PAUSE");
				} else {
					ambilPawn(&Player);
					ngocok(Player);
					kocokan = dadu();
					while(kocokan == 6) {
						if (Player.otsd < 4) {
							char pilihan;
							do {
								system("CLS");
								print();
								printf("Your number: %hd\n", kocokan);
								printf("1. Set pawn free\n2. Move\n>> ");
								pilihan = getch();
							} while (pilihan != '1' && pilihan != '2');
							if (pilihan == '1') {
								ambilPawn(&Player);
							} else {

							}
						}
					}
				}
			}
			//selesai jalan
			if (jalan == 4) {
				jalan = 1;
			} else {
				jalan++;
			}
		} else {
			//proses bot

			//selesai
			if (jalan == 4) {
				jalan = 1;
			} else {
				jalan++;
			}
		}
	}
	return;
}

void ngocok(Team Player)
{
	char cmd;
	do {
		system("CLS");
		print();
		printf("Your pawn: ");
		for (int i = 0; i < 4; i++) {
			if (Player.outside[i] != 0) printf("%hd ", Player.outside[i]);
		}
		printf("\nYour turn!\n");
		printf("Type to roll the dice! (R/r)\n>> ");
		cmd = getch();
	} while (cmd != 'R' && cmd != 'r');
}

void ambilPawn(Team *p) {
    short pil;
    do {
		system("CLS");
		print();
        printf("\nThe pawn you can take: ");
        for (int i = 0; i < 4; i++) {
            if (p->inside[i] != 0) printf("%hd ", p->inside[i]);
        }
        printf("\nChoose the pawn you want to take: ");
        scanf("%hd", &pil);
    } while (pil < 1 || pil > 4 || p->inside[pil-1] == 0);
    printf("\nYou take the pawn number %hd!\n", pil);
    p->inside[pil-1] = 0;
    p->outside[pil-1] = pil;
    p->insd--;
    p->otsd++;
}
