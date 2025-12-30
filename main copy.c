#include <stdio.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <conio.h>
#include <windows.h>

typedef struct {
	short masuk;
	short kebal;
	short tamat;
    short y, x;
} Bidak;

Bidak bidak[4][4] = {
    { {0, 0, 0, 1, 2}, {0, 0, 0, 1, 3}, {0, 0, 0, 3, 2}, {0, 0, 0, 3, 3} },
    { {0, 0, 0, 1, 11}, {0, 0, 0, 1, 12}, {0, 0, 0, 3, 11}, {0, 0, 0, 3, 12} },
    { {0, 0, 0, 11, 11}, {0, 0, 0, 11, 12}, {0, 0, 0, 13, 11}, {0, 0, 0, 13, 12} },
    { {0, 0, 0, 11, 2}, {0, 0, 0, 11, 3}, {0, 0, 0, 13, 2}, {0, 0, 0, 13, 3} }
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
	short outside[4];
	short otsd;
	short selesai;
} Team;

Team green, yellow, blue, red;

void print();
void menu();
void mulai();
void ngocok(Team *);
void ambilPawn(Team *p);
void pawnKeluar(short, Team *);
void maju(short, Team*);
void pergerakan(Team*, short, short);
void bidakMenang(Team *, short);
void orangMain(Team *);
void botMain(Team *);
void botPawn(Team *);
void botMaju(Team *, short);


Team set(short);
short checkExit();
short dadu();
int eliminasi(Team *, short);

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
    int i, j, k, l;
    for (i = 0; i < 15; i++) {
        for (j = 0; j < 15; j++) {
            short total = 0;
            int lastTeam = -1;
            int lastPawn = -1;
            for (k = 0; k < 4; k++) {
                for (l = 0; l < 4; l++) {
                    if (bidak[k][l].y == i && bidak[k][l].x == j) {
                        total++;
                        lastTeam = k;
                        lastPawn = l;
                    }
                }
            }
            if (total > 0) {
                if (total > 1) {
                    switch (lastTeam) {
                        case 0: printf("\033[102m(%d)\033[0m", total); break;
                        case 1: printf("\033[103m(%d)\033[0m", total); break;
                        case 2: printf("\033[104m(%d)\033[0m", total); break;
                        case 3: printf("\033[101m(%d)\033[0m", total); break;
                    }
                } else {
                    switch (lastTeam) {
                        case 0: printf("\033[102m %d \033[0m", lastPawn+1); break;
                        case 1: printf("\033[103m %d \033[0m", lastPawn+1); break;
                        case 2: printf("\033[104m %d \033[0m", lastPawn+1); break;
                        case 3: printf("\033[101m %d \033[0m", lastPawn+1); break;
                    }
                }
            } else {
                switch (map[i][j]) {
                    case '1': printf("\033[42m   \033[0m"); break; // hijau
                    case '2': printf("\033[43m   \033[0m"); break; // kuning
                    case '3': printf("\033[44m   \033[0m"); break; // biru
                    case '4': printf("\033[41m   \033[0m"); break; // merah
                    case '9': printf("\033[100m   \033[0m"); break; // abu-abu
                    default:  printf("\033[47m   \033[0m"); break; // putih default
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
	temp.selesai = 0;
	memset(temp.outside, 0, sizeof(temp.outside));
	return temp;
}

void mulai() {
	green = set(1);
	yellow = set(2);
	blue = set(3);
	red = set(4);
	system("CLS");
	printf("Game start!\n");
	Sleep(1500);
	system("CLS");

	srand(time(NULL));
	Team *Player, *Bot1;
	short player = (rand() % 4) + 1;
	short jalan = (rand() % 4) + 1;
	printf("You play as ");
	if (player == 1) {
		printf("Green!\n");
		Player = &green;
	} else if (player == 2) {
		printf("Yellow!\n");
		Player = &yellow;
	} else if (player == 3) {
		printf("Blue!\n");
		Player = &blue;
	} else {
		printf("Red!\n");
		Player = &red;
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
		if (jalan == player) {
			orangMain(Player);
			jalan = (jalan == 4) ? 1 : jalan + 1;
		} else {
			if (jalan == 1) {
				Bot1 = &green;
			} else if (jalan == 2) {
				Bot1 = &yellow;
			} else if (jalan == 3) {
				Bot1 = &blue;
			} else {
				Bot1 = &red;
			}
			botMain(Bot1);
			jalan = (jalan == 4) ? 1 : jalan + 1;
		}
	}
	return;
}

void botPawn(Team *p) {
	short bidakBot = (rand() % 4) + 1;
	printf("Bot took the pawn number %hd out.\n", bidakBot);
	p->insd--;
	p->otsd++;
	p->inside[bidakBot-1] = 0;
	p->outside[bidakBot-1] = bidakBot;
	pawnKeluar(bidakBot, p);
}

void botMaju(Team *p, short kocokan) {
    system("CLS");
    print();
    short yangMaju = (rand() % 4) + 1;
    while (p->outside[yangMaju-1] == 0) {
        yangMaju = (rand() % 4) + 1;
    }
    printf("Bot chose pawn number %hd. Moved it %hd far.\n", yangMaju, kocokan);
    pergerakan(p, kocokan, yangMaju);
    while (eliminasi(p, yangMaju) == 1) {
        printf("Bot eliminated an opponent! Extra turn!\n");
        Sleep(1000);
        short bonus = dadu();
        printf("Bot rolled again and got %hd!\n", bonus);
        yangMaju = (rand() % 4) + 1;
        while (p->outside[yangMaju-1] == 0) {
            yangMaju = (rand() % 4) + 1;
        }
        printf("Bot chose pawn number %hd. Moved it %hd far.\n", yangMaju, bonus);
        pergerakan(p, bonus, yangMaju);
    }
    Sleep(1500);
}

void botMain(Team *p) {
	short kocokan = dadu();
	system("CLS");
	print();
	printf("Bot's pawn: ");
	for (int i = 0; i < 4; i++) {
		if (p->outside[i] != 0) printf("%hd ", p->outside[i]);
	}
	printf("\nBot's turn!\n");
	Sleep(1000);
	if (p->otsd == 0) {
		if (kocokan != 6) {
			printf("Bot has rolled the dice and get %hd. Yet, 6 is needed to take the pawn out\n", kocokan);
			Sleep(1500);
		} else {
			botPawn(p);
			kocokan = dadu();
			Sleep(1500);
		}
	}
	while (kocokan == 6) {
		if (p->otsd < 4) {
			system("CLS");
			print();
			printf("Bot has rolled the dice and get %hd!\n", kocokan);
			Sleep(1000);
			short milih = (rand() % 2) + 1;
			printf("Bot chose %s", (milih == 1) ? "set the pawn free!\n" : "move the pawn!\n");
			if (milih == 1) {
				botPawn(p);
				kocokan = dadu();
			} else {
				botMaju(p, kocokan);
			}
		} else {
			botMaju(p, kocokan);
		}
		kocokan = dadu();
	}
	if (p->otsd > 0 && kocokan != 0) {
		botMaju(p, kocokan);
	}
	printf("Bot has rolled the dice and get %hd!\n", kocokan);
}

void orangMain(Team *p) {
	ngocok(p);
	short kocokan = dadu();
	printf("\nYour dice: %d\n", kocokan);
	Sleep(1000);
	if (p->otsd == 0) {
		if (kocokan != 6) {
			printf("You need to get 6 in order to take the pawn. SKIPPED!\n");
			Sleep(1000);
		} else {
			ambilPawn(p);
			ngocok(p);
			kocokan = dadu();
		}
	}
	while(kocokan == 6) {
		if (p->otsd < 4) {
			char pilihan;
			do {
				system("CLS");
				print();
				printf("Your number: %hd\n", kocokan);
				printf("1. Set pawn free\n2. Move\n>> ");
				pilihan = getch();
				switch(pilihan) {
					case '1': ambilPawn(p); break;
					case '2': maju(kocokan, p); break;
					default: break;
				}
			} while(pilihan != '1' && pilihan != '2');
		} else {
			maju(kocokan, p);
		}
		ngocok(p);
		kocokan = dadu();
	}
	if (p->otsd > 0 && kocokan != 0) {
		maju(kocokan, p);
	}
}

void pergerakan(Team *p, short kocokan, short pilihan) {
    while (kocokan > 0) {
        switch (p->warna) {
            case 1:
                if (bidak[p->warna-1][pilihan-1].x == 0 && bidak[p->warna-1][pilihan-1].y == 8)
                    bidak[p->warna-1][pilihan-1].masuk = 1;
                break;
            case 2:
                if (bidak[p->warna-1][pilihan-1].x == 6 && bidak[p->warna-1][pilihan-1].y == 0)
                    bidak[p->warna-1][pilihan-1].masuk = 1;
                break;
            case 3:
                if (bidak[p->warna-1][pilihan-1].x == 14 && bidak[p->warna-1][pilihan-1].y == 6)
                    bidak[p->warna-1][pilihan-1].masuk = 1;
                break;
            case 4:
                if (bidak[p->warna-1][pilihan-1].x == 8 && bidak[p->warna-1][pilihan-1].y == 14)
                    bidak[p->warna-1][pilihan-1].masuk = 1;
                break;
        }
        if (bidak[p->warna-1][pilihan-1].masuk == 1 &&
            (
                ((p->warna == 2 || p->warna == 4) && bidak[p->warna-1][pilihan-1].x == 7) ||
                ((p->warna == 1 || p->warna == 3) && bidak[p->warna-1][pilihan-1].y == 7)
            )
        ) {
            if (bidak[p->warna-1][pilihan-1].x == 7) {
                if (p->warna == 2 && bidak[p->warna-1][pilihan-1].y < 6)
                    bidak[p->warna-1][pilihan-1].y++;
                else if (p->warna == 4 && bidak[p->warna-1][pilihan-1].y > 8)
                    bidak[p->warna-1][pilihan-1].y--;
            } else {
                if (p->warna == 1 && bidak[p->warna-1][pilihan-1].x < 6)
                    bidak[p->warna-1][pilihan-1].x++;
                else if (p->warna == 3 && bidak[p->warna-1][pilihan-1].x > 8)
                    bidak[p->warna-1][pilihan-1].x--;
            }
        } else {
            if (bidak[p->warna-1][pilihan-1].x == 0) {
                if (bidak[p->warna-1][pilihan-1].y == 6) bidak[p->warna-1][pilihan-1].x++;
                else bidak[p->warna-1][pilihan-1].y--;
            } else if (bidak[p->warna-1][pilihan-1].x == 6) {
                if (bidak[p->warna-1][pilihan-1].y == 9) {
                    bidak[p->warna-1][pilihan-1].x--;
                    bidak[p->warna-1][pilihan-1].y--;
                } else if (bidak[p->warna-1][pilihan-1].y == 0) {
                    bidak[p->warna-1][pilihan-1].x++;
                } else {
                    bidak[p->warna-1][pilihan-1].y--;
                }
            } else if (bidak[p->warna-1][pilihan-1].x == 8) {
                if (bidak[p->warna-1][pilihan-1].y == 5) {
                    bidak[p->warna-1][pilihan-1].x++;
                    bidak[p->warna-1][pilihan-1].y++;
                } else if (bidak[p->warna-1][pilihan-1].y == 14) {
                    bidak[p->warna-1][pilihan-1].x--;
                } else {
                    bidak[p->warna-1][pilihan-1].y++;
                }
            } else if (bidak[p->warna-1][pilihan-1].x == 14) {
                if (bidak[p->warna-1][pilihan-1].y == 8) bidak[p->warna-1][pilihan-1].x--;
                else bidak[p->warna-1][pilihan-1].y++;
            } else if (bidak[p->warna-1][pilihan-1].y == 0) {
                if (bidak[p->warna-1][pilihan-1].x == 8) bidak[p->warna-1][pilihan-1].y++;
                else bidak[p->warna-1][pilihan-1].x++;
            } else if (bidak[p->warna-1][pilihan-1].y == 6) {
                if (bidak[p->warna-1][pilihan-1].x == 5) {
                    bidak[p->warna-1][pilihan-1].y--;
                    bidak[p->warna-1][pilihan-1].x++;
                } else if (bidak[p->warna-1][pilihan-1].x == 14) {
                    bidak[p->warna-1][pilihan-1].y++;
                } else {
                    bidak[p->warna-1][pilihan-1].x++;
                }
            } else if (bidak[p->warna-1][pilihan-1].y == 8) {
                if (bidak[p->warna-1][pilihan-1].x == 9) {
                    bidak[p->warna-1][pilihan-1].y++;
                    bidak[p->warna-1][pilihan-1].x--;
                } else if (bidak[p->warna-1][pilihan-1].x == 0) {
                    bidak[p->warna-1][pilihan-1].y--;
                } else {
                    bidak[p->warna-1][pilihan-1].x--;
                }
            } else if (bidak[p->warna-1][pilihan-1].y == 14) {
                if (bidak[p->warna-1][pilihan-1].x == 6) bidak[p->warna-1][pilihan-1].y--;
                else bidak[p->warna-1][pilihan-1].x--;
            }
        }

        kocokan--;
    }
    if ((bidak[p->warna-1][pilihan-1].x == 6 && bidak[p->warna-1][pilihan-1].y == 13) ||
        (bidak[p->warna-1][pilihan-1].x == 8 && bidak[p->warna-1][pilihan-1].y == 12) ||
        (bidak[p->warna-1][pilihan-1].x == 13 && bidak[p->warna-1][pilihan-1].y == 8) ||
        (bidak[p->warna-1][pilihan-1].x == 12 && bidak[p->warna-1][pilihan-1].y == 6) ||
        (bidak[p->warna-1][pilihan-1].x == 8 && bidak[p->warna-1][pilihan-1].y == 1) ||
        (bidak[p->warna-1][pilihan-1].x == 6 && bidak[p->warna-1][pilihan-1].y == 2) ||
        (bidak[p->warna-1][pilihan-1].x == 1 && bidak[p->warna-1][pilihan-1].y == 6) ||
        (bidak[p->warna-1][pilihan-1].x == 2 && bidak[p->warna-1][pilihan-1].y == 8)) {
        bidak[p->warna-1][pilihan-1].kebal = 1;
    } else {
        bidak[p->warna-1][pilihan-1].kebal = 0;
    }
	bidakMenang(p, pilihan);
}

void bidakMenang(Team *p, short pilihan) {
	switch(p->warna) {
		case 1:
			if (bidak[p->warna-1][pilihan-1].x == 6 && bidak[p->warna-1][pilihan-1].y == 7) {
				bidak[p->warna-1][pilihan-1].tamat = 1;
				green.otsd--;
				green.outside[pilihan-1] = 0;
				green.selesai++;
			}
			break;
		case 2:
			if (bidak[p->warna-1][pilihan-1].x == 7 && bidak[p->warna-1][pilihan-1].y == 6) {
				bidak[p->warna-1][pilihan-1].tamat = 1;
				yellow.otsd--;
				yellow.outside[pilihan-1] = 0;
				yellow.selesai++;
			}
			break;
		case 3:
			if (bidak[p->warna-1][pilihan-1].x == 8 && bidak[p->warna-1][pilihan-1].y == 7) {
				bidak[p->warna-1][pilihan-1].tamat = 1;
				blue.otsd--;
				blue.outside[pilihan-1] = 0;
				blue.selesai++;
			}
			break;
		case 4:
			if (bidak[p->warna-1][pilihan-1].x == 7 && bidak[p->warna-1][pilihan-1].y == 8) {
				bidak[p->warna-1][pilihan-1].tamat = 1;
				red.otsd--;
				red.outside[pilihan-1] = 0;
				red.selesai++;
			}
			break;
		default: break;
	}
}

int eliminasi(Team *p, short pilihan) {
    int base_pos[4][4][2] = {
        { {1,2}, {1,3}, {3,2}, {3,3} },   
        { {1,11}, {1,12}, {3,11}, {3,12} },  
        { {11,11}, {11,12}, {13,11}, {13,12} },
        { {11,2}, {11,3}, {13,2}, {13,3} }   
    };
	int elim = 0;

    for (int i = 0; i < 4; i++) {        
        for (int j = 0; j < 4; j++) {    
            if (bidak[i][j].x == bidak[p->warna-1][pilihan-1].x &&
                bidak[i][j].y == bidak[p->warna-1][pilihan-1].y) {

                if (i == p->warna-1) {
                    continue; 
                } else {
                    if (bidak[i][j].kebal == 0) {
						bidak[i][j].y = base_pos[i][j][0];
						bidak[i][j].x = base_pos[i][j][1];
						bidak[i][j].masuk = 0;
						bidak[i][j].kebal = 0;

						Team *target;
						if (i == 0) target = &green;
						else if (i == 1) target = &yellow;
						else if (i == 2) target = &blue;
						else target = &red;
						target->outside[j] = 0;
						target->inside[j] = j+1;
						elim = 1;
					}
                }
            }
        }
    }
	return elim;
}

void maju(short kocokan, Team *p) {
    short pilihan;
    char cmd;
    do {
        system("CLS");
        print();
        printf("Your pawn outside: ");
        for (int i = 0; i < 4; i++) {
            if (p->outside[i] != 0) printf("%hd ", p->outside[i]);
        }
        printf("\nYour dice: %hd\nChoose which pawn you want to move (1-4): ", kocokan);
        cmd = getch();
        if (cmd >= '1' && cmd <= '4') {
            pilihan = cmd - '0';
        }
    } while (pilihan < 1 || pilihan > 4 || p->outside[pilihan-1] == 0);

    printf("You chose pawn number %hd!\nMoved it %hd far.\n", pilihan, kocokan);
    pergerakan(p, kocokan, pilihan);
    while (eliminasi(p, pilihan) == 1) {
        printf("You eliminated an opponent! Extra turn!\n");
        ngocok(p);
        short bonus = dadu();
        printf("\nYour dice: %hd\n", bonus);
        do {
            system("CLS");
            print();
            printf("Your pawn outside: ");
            for (int i = 0; i < 4; i++) {
                if (p->outside[i] != 0) printf("%hd ", p->outside[i]);
            }
            printf("\nYour dice: %hd\nChoose which pawn you want to move (1-4): ", kocokan);
            cmd = getch();
            if (cmd >= '1' && cmd <= '4') {
                pilihan = cmd - '0';
            }
        } while (pilihan < 1 || pilihan > 4 || p->outside[pilihan-1] == 0);
        printf("You chose pawn number %hd!\nMoved it %hd far.\n", pilihan, bonus);
        pergerakan(p, bonus, pilihan);
    }

    system("PAUSE");
}

void ngocok(Team *Player)
{
	char cmd;
	do {
		system("CLS");
		print();
		printf("Your pawn: ");
		for (int i = 0; i < 4; i++) {
			if (Player->outside[i] != 0) printf("%hd ", Player->outside[i]);
		}
		printf("\nYour turn!\n");
		printf("Type to roll the dice! (R/r)\n>> ");
		cmd = getch();
	} while (cmd != 'R' && cmd != 'r');
}

void pawnKeluar(short pilihan, Team *player) {
    switch(player->warna) {
        case 1: bidak[player->warna-1][pilihan-1].x = 1; bidak[player->warna-1][pilihan-1].y = 6; break;
        case 2: bidak[player->warna-1][pilihan-1].x = 8; bidak[player->warna-1][pilihan-1].y = 1; break;
        case 3: bidak[player->warna-1][pilihan-1].x = 13; bidak[player->warna-1][pilihan-1].y = 8; break;
        case 4: bidak[player->warna-1][pilihan-1].x = 6; bidak[player->warna-1][pilihan-1].y = 13; break;
    }
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
	pawnKeluar(pil, p);
	system("PAUSE");
}
