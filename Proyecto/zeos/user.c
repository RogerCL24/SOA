#include <libc.h>

#define CLONE_THREAD 1

char buff[24];

char char_map2[] =
{
  '\0','\0','1','2','3','4','5','6',
  '7','8','9','0','\'','�','\0','\0',
  'q','w','e','r','t','y','u','i',
  'o','p','`','+','\0','\0','a','s',
  'd','f','g','h','j','k','l','�',
  '\0','�','\0','�','z','x','c','v',
  'b','n','m',',','.','-','\0','*',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0','\0','\0','\0','\0','\0','7',
  '8','9','-','4','5','6','+','1',
  '2','3','0','\0','\0','\0','<','\0',
  '\0','\0','\0','\0','\0','\0','\0','\0',
  '\0','\0'
};

char keyboard[128];
unsigned short* screen;

int pid;

void *thread_func_1(void *arg) {
  if (*(int*)arg == 1) write(1, "Hello from thread 1!\n", 22);
  pthread_exit();
}

void test_simple_clone() {
  int val = 1;
  int tid = clone(CLONE_THREAD, thread_func_1, &val, 1024);
  if (tid < 0)  write(1, "clone failed\n", 13);
  
  pause(50);
  write(1, "Done boss\n", 10);
}

void *thread_func_N(void *arg) {
  char* msg = (char *)arg;
  write(1, msg, strlen(msg));
  pthread_exit();
}

void test_multiple_threads() {
  int tid =  clone(CLONE_THREAD, thread_func_N, "Thread A\n", 1024);
  if (tid < 0) write(1, "clone failed\n", 13);

  tid = clone(CLONE_THREAD, thread_func_N, "Thread B\n", 1024);
  if (tid < 0) write(1, "clone failed\n", 13);

  tid = clone(CLONE_THREAD, thread_func_N, "Thread C\n", 1024);
  if (tid < 0) write(1, "clone failed\n", 13);

  pause(200);
  write(1, "Done boss, multiple\n", 20);
}

void *prio_func(void *arg) {
  int val = *((int*)arg);   // race condition...
  if (val == 0) {
    SetPriority(25);   // LOW
    pause(1);
  }
  else SetPriority(30);  // HIGH
  
  if (val == 0)
    for (int i = 0; i < 10; ++i)  write(1, "LOW\n", 4);

  else
    for (int i = 0; i < 10; ++i) { 
      if (i == 5) pause(1);			
      write(1, "HIGH\n", 5);
    }

  pthread_exit();
}

void test_priority() {
  int val1 = 0;
  clone(CLONE_THREAD, prio_func, &val1, 1024);  // LOW

  int val2 = 1;
  clone(CLONE_THREAD, prio_func, &val2, 1024);  // HIGH
  
  pause(1);
  write(1, "HIGH GOES BEFORE, RIGHT?\n", 25);
}

void draw_screen(unsigned short* screen) {

  if (screen != (void*) -1) {
    for (int i = 0; i < 2000; ++i) {
      screen[i] = ' ';
    }
  } 
  else perror("Screen error");

  unsigned short color = 0x07;

  screen[0] = color << 8 | 'k';
  screen[1] = color << 8 | 'e';
  screen[2] = color << 8 | 'y';
  screen[3] = color << 8 | 's';
  screen[4] = color << 8 | ' ';
  screen[5] = color << 8 | 'p';
  screen[6] = color << 8 | 'r';
  screen[7] = color << 8 | 'e';
  screen[8] = color << 8 | 's';
  screen[9] = color << 8 | 's';
  screen[10] = color << 8 | 'e';
  screen[11] = color << 8 | 'd';
  screen[12] = color << 8 | ':';
  screen[13] = color << 8 | ' ';
  
  while (1) {
    if (GetKeyboardState(keyboard) < 0) perror();
    int pos = 14;

    color = 1;
    for (int i = 0; i < 128; ++i) {
      if (keyboard[i] == 1) {
        screen[pos] = color << 8 | char_map2[i];
        pos += 2;
        ++color;
      }
    }
    for (int i = pos; i < 128; ++i) screen[i] = ' ';
    pause(1000);

  }
}

void *son_thread(void *arg) {
  write(1, "THREAD RUNNING\n", 15);
  pause(500); 

  test_simple_clone();
  pthread_exit();
}

void *forking(void *arg) {

  int val = *((int*)arg); //el thread al llegar aqui tiene el puntero apuntando a un valor != 1  
  int pid = fork();
  if (pid > 0) {
    write(1, "Cloned and forked!\n",20);
  }
  else if (pid == 0){
    write(1, "My father is a clone...\n",25);  // 2.
    if (val == 1) write(1,"YES\n",4);
  }
  else write(1,"ERROR\n",6);

  pthread_exit();
}

void test_clone_and_fork() {
 int val = 1;
 int ret = clone(CLONE_THREAD, forking, &val, 8192);  
 if (ret == 0) write(1, "Cloned succesfully, forking?\n", 30);
}

void test_exit_from_main() {
  int pid = fork();
  if (pid == 0) {
    write(1, "CHILD: I will now call exit from main thread\n", 45);
    exit(); // Debería terminar el proceso entero 1.
  } 
  else {
    pause(100);
    write(1, "PARENT: If I see this, child exited correctly\n", 46);
  }
}

void *killer_thread(void *arg) {
  write(1, "THREAD: Calling exit from a thread!\n", 35);
  exit(); // Deberia matar a todo el proceso
  return 0;
}

void test_exit_from_thread() {
  int pid = fork();
  if (pid == 0) {
    int val = 0;
    int ret = clone(CLONE_THREAD, killer_thread, &val, 4096);
    if (ret < 0) perror();
    pause(1);
    write(1, "You cannot see me\n", 19);// No deberia llegar aqui, será matado por el thread
  } 
  else {
    pause(200);
    write(1, "PARENT: Child should have exited\n", 33);
  }
}

//Tests de semafors
int sem_id;

char aux[1];
void *sem_thread(void *arg) {
  char id = *(char*)arg;
  aux[0] = id;
  //buff[0] = id;   // --> Si es descomenta (i comenta la posterior), en estar fora de la zona d'exclusió mutua hi ha problemes de condicio de carrera
  
  write(1, "Thread trying to enter critical section: ", 41);
  write(1, aux, 1);
  write(1, "\n", 1);
  
  if (sem_wait(sem_id) == 0) {
    buff[0] = id;
    write(1, "Thread in critical section: ", 28);
    write(1, buff, 1);
    write(1, "\n", 1);
    
    pause(13000);

    write(1, "Thread leaving critical section: ", 33);
    write(1, buff, 1);
    write(1, "\n", 1);

    sem_post(sem_id);
  } else {
    perror();
  }

  pthread_exit();
}

void test_semaphores() {
  write(1, "\n--- Testing Semaphores ---\n", 28);

  sem_id = sem_init(1); // inicializa con valor 1 (mutex)
  if (sem_id < 0) {
    perror();
    return;
  }

  // Crea varias letras distintas para identificar a los hilos
  char ids[3] = { 'A', 'B', 'C' };

  for (int i = 0; i < 3; ++i) {
    if (clone(CLONE_THREAD, sem_thread, &ids[i], 2048) < 0) {
      perror();
    }
    pause(10); // Pequeño delay para mezclar la ejecución
  }

  // Espera a que los hilos terminen
  sem_wait(sem_id);
  write(1, "All threads finished\n", 21);

  // Destruye el semáforo
  if (sem_destroy(sem_id) != 0) {
    perror();
  } else {
    write(1, "Semaphore destroyed successfully\n", 33);
  }

  write(1, "--- End of Semaphore Test ---\n\n", 32);
}










/////////// CODI PEL JOC //////////////
//defines pel control del joc
#define NUM_COL 80

#define KEY_N 49
#define KEY_W 17
#define KEY_A 30
#define KEY_S 31
#define KEY_D 32

#define COLOR_WHITE 0x7
#define COLOR_DARK_BLUE 0x1
#define COLOR_LIGHT_BLUE 0x9
#define COLOR_RED 0xC
#define COLOR_CYAN 0x3
#define COLOR_GREEN 0xA
#define COLOR_MAGENTA 0xD
#define COLOR_YELLOW 0xE
#define COLOR_ORANGE 0x6
#define COLOR_WHITE 0x7

#define PACMAN_INIT_POS_X 40
#define PACMAN_INIT_POS_Y 14

#define FIXED_UPDATE_TICKS 100

#define SPARE_BLOCKS 4

#define CORNER_PINKY_X 26
#define CORNER_PINKY_Y 5
#define CORNER_BLINKY_X 52
#define CORNER_BLINKY_Y 5
#define CORNER_INKY_X 16
#define CORNER_INKY_Y 17
#define CORNER_CLYDE_X  62
#define CORNER_CLYDE_Y 17

#define VULNERABLE_FRAMES 40
#define DEAD_FRAMES 40

//Structs d'objectes
enum scenes {MENU_SCENE, GAME_SCENE, GAMEOVER_SCENE, WIN_SCENE};
enum directions {NONE, LEFT, RIGHT, UP, DOWN};
enum ghost_order {PINKY, BLINKY, INKY, CLYDE};

//Struct pel pacman i pels ghosts
typedef struct {
  //Caracteristiques comunes
  int pos_x, pos_y;
  int dir;
  char character;
  int color;
  //Caracteristiques úniques dels ghosts
  int respawn_timer;
  //Caracteristica única del pacman per fluidesa de canvi de direccio
  int next_dir;
  int spare_blocks;
} Entity;

//Variables
//char keyboard[128];         --> Definits a dalt de tot, s'utilitzen en altres tests
//unsigned short* screen;
int sem_game;

//Variables d'entitats del joc
Entity pacman;
Entity ghosts[4];

//Variables GameState
int current_scene;
int points;
char original_map[24][80] = {
  "                                                                                ",
  "                                                                                ",
  "                                                                                ",
  "            #######################################################             ",
  "            #######################################################             ",
  "            ### o o o o o o o o o o o ### o o o o o o o o o o o ###             ",
  "            ### o ####### o ####### o ### o ####### o ####### o ###             ",
  "            ### $ ####### o ####### o ### o ####### o ####### $ ###             ",
  "            ### o o o o o o o o o o o o o o o o o o o o o o o o ###             ",
  "            ### o ####### o ##### o o o o o o ##### o ####### o ###             ",
  "            ### o ####### o ##### o o o o o o ##### o ####### o ###             ",
  "            ### o o o o o o o o o o o o o o o o o o o o o o o o ###             ",
  "            ############# o ####### o ####### o ####### o #########             ",
  "            ############# o ####### o ####### o ####### o #########             ",
  "            ### o o o o o o o o o o o o o o o o o o o o o o o o ###             ",
  "            ### $ ######### o ######### o ####### o ######### $ ###             ",
  "            ### o ######### o ######### o ####### o ######### o ###             ",
  "            ### o o o o o o o o o o o o o o o o o o o o o o o o ###             ",
  "            #######################################################             ",
  "            #######################################################             ",
  "                                                                                ",
  "                                                                                ",
  "                                                                                ",
  "                                                                                "
};
char map[24][80];
unsigned short back_buffer[25*80];

int vulnerable_frames;

//Variables per controlar velocitat joc
int ticks_passed;
int ticks_last_time;

int fixed_cont;

int fps;
int sec_cont;
int frames_cont;

//Calcula el valor absolut d'una resta
int abs_diff(int a, int b) {
  int diff = a - b;
  return diff < 0 ? -diff : diff;
}

//Posa el caracter corresponent a la pantalla
void put_char(int x, int y, char c, int color) {
  if (x < 0 || x >= 80 || y < 0 || y >= 25) return;
  screen[y * 80 + x] = (color << 8) | c;
}

//Dibuixa un string en la line indicada a partir de la columna indicada del color indicat
void print_str(int x, int y, const char* str, int color) {
  while (*str) {
      put_char(x++, y, *str++, color);
  }
}

//Dibuixa la pantalla de títol
void draw_title_screen() {
  int y = 3;

  // Limpiar pantalla
  for (int i = 0; i < 80 * 25; ++i) {
      screen[i] = (0x0 << 8) | ' ';
  }

  // 1. TÍTOL PACMAN
  print_str(20, y++, "______  ___  _____ ___  ___  ___   _   _ ", COLOR_YELLOW);
  print_str(20, y++, "| ___ \\/ _ \\/  __ \\|  \\/  | / _ \\ | \\ | |", COLOR_YELLOW);
  print_str(20, y++, "| |_/ / /_\\ \\ /  \\/| .  . |/ /_\\ \\|  \\| |", COLOR_YELLOW);
  print_str(20, y++, "|  __/|  _  | |    | |\\/| ||  _  || . ` |", COLOR_YELLOW);
  print_str(20, y++, "| |   | | | | \\__/\\| |  | || | | || |\\  |", COLOR_YELLOW);
  print_str(20, y++, "\\_|   \\_| |_/\\____/\\_|  |_/\\_| |_/\\_| \\_/", COLOR_YELLOW);

  y++; 

  // 2. Subtitol
  print_str(33, y++, "Zeos ver.", COLOR_YELLOW);

  y += 2; 

  // 3. ASCII ART de PACMAN
  print_str(24, y++, "   .--.", COLOR_WHITE);                   
  print_str(24, y++, "  / _.-' .-.  .-.  .-.  .-.  .-. ", COLOR_WHITE);
  print_str(24, y++, " |  '-.  '-'  '-'  '-'  '-'  '-' ", COLOR_WHITE);
  print_str(24, y++, "  \\__.'", COLOR_WHITE);

  y += 3;

  // 4. Noms i curs
  print_str(34, y++, "Roger Cot", COLOR_WHITE);
  print_str(33, y++, "Arnau Garcia", COLOR_WHITE);
  print_str(34, y++, "SOA 2025", COLOR_WHITE);
}

//Dibuixa la pantalla de Victoria
void draw_win_screen() {
  int y = 4;

  // Limpiar pantalla
  for (int i = 0; i < 80 * 25; ++i) {
      screen[i] = (0x0 << 8) | ' ';
  }

  print_str(2, y++, " _____                             _         _       _   _                 ", COLOR_YELLOW);
  print_str(2, y++, "/  __ \\                           | |       | |     | | (_)                ", COLOR_YELLOW);
  print_str(2, y++, "| /  \\/ ___  _ __   __ _ _ __ __ _| |_ _   _| | __ _| |_ _  ___  _ __  ___ ", COLOR_YELLOW);
  print_str(2, y++, "| |    / _ \\| '_ \\ / _` | '__/ _` | __| | | | |/ _` | __| |/ _ \\| '_ \\/ __|", COLOR_YELLOW);
  print_str(2, y++, "| \\__/\\ (_) | | | | (_| | | | (_| | |_| |_| | | (_| | |_| | (_) | | | \\__ \\", COLOR_YELLOW);
  print_str(2, y++, " \\____/\\___/|_| |_|\\__, |_|  \\__,_|\\__|\\__,_|_|\\__,_|\\__|_|\\___/|_| |_|___/", COLOR_YELLOW);
  print_str(2, y++, "                    __/ |                                                  ", COLOR_YELLOW);
  print_str(2, y++, "                   |___/                                                   ", COLOR_YELLOW);

  y++; 
  y++;

  print_str(20, y++, "__   __            _    _             ", COLOR_LIGHT_BLUE);
  print_str(20, y++, "\\ \\ / /           | |  | |            ", COLOR_LIGHT_BLUE);
  print_str(20, y++, " \\ V /___  _   _  | |  | | ___  _ __  ", COLOR_LIGHT_BLUE);  
  print_str(20, y++, "  \\ // _ \\| | | | | |/\\| |/ _ \\| '_ \\ ", COLOR_LIGHT_BLUE);  
  print_str(20, y++, "  | | (_) | |_| | \\  /\\  / (_) | | | |", COLOR_LIGHT_BLUE);  
  print_str(20, y++, "  \\_/\\___/ \\__,_|  \\/  \\/ \\___/|_| |_|", COLOR_LIGHT_BLUE);  
}

//Dibuixa la pantalla de Game Over
void draw_game_over_screen() {
  int y = 4;

  // Limpiar pantalla
  for (int i = 0; i < 80 * 25; ++i) {
      screen[i] = (0x0 << 8) | ' ';
  }

  print_str(14, y++, " _____                        _____                ", COLOR_RED);
  print_str(14, y++, "|  __ \\                      |  _  |               ", COLOR_RED);
  print_str(14, y++, "| |  \\/ __ _ _ __ ___   ___  | | | |_   _____ _ __ ", COLOR_RED);
  print_str(14, y++, "| | __ / _` | '_ ` _ \\ / _ \\ | | | \\ \\ / / _ \\ '__|", COLOR_RED);
  print_str(14, y++, "| |_\\ \\ (_| | | | | | |  __/ \\ \\_/ /\\ V /  __/ |   ", COLOR_RED);
  print_str(14, y++, " \\____/\\__,_|_| |_| |_|\\___|  \\___/  \\_/ \\___|_|   ", COLOR_RED);


  y++; 
  y++;

  print_str(16, y++, " _____             ___              _       ", COLOR_WHITE);
  print_str(16, y++, "|_   _|           / _ \\            (_)      ", COLOR_WHITE);
  print_str(16, y++, "  | |_ __ _   _  / /_\\ \\ __ _  __ _ _ _ __  ", COLOR_WHITE);
  print_str(16, y++, "  | | '__| | | | |  _  |/ _` |/ _` | | '_ \\ ", COLOR_WHITE);
  print_str(16, y++, "  | | |  | |_| | | | | | (_| | (_| | | | | |", COLOR_WHITE);
  print_str(16, y++, "  \\_/_|   \\__, | \\_| |_/\\__, |\\__,_|_|_| |_|", COLOR_WHITE);
  print_str(16, y++, "           __/ |         __/ |              ", COLOR_WHITE);
  print_str(16, y++, "          |___/         |___/               ", COLOR_WHITE);

}

//Comprova si la posició es vàlida, 1 si ho es, 0 altrament (Les posicions no valides son les pareds)
int  can_move(int x, int y) {
  if (x < 15 || x > 67 || y < 5 || y > 20) return 0;
  return map[y][x] != '#';
}

//Mou a la entitat, retorna 1 si s'ha pogut moure (Al final només s'utilitza pel pacman)
int move_entity(Entity *entity, int move_dir) {
  switch (move_dir)
  {
  case LEFT:
    if (can_move(entity->pos_x - 2, entity->pos_y)) {
      entity->pos_x -= 2;
      return 1;
    }
    break;

  case RIGHT:
    if (can_move(entity->pos_x + 2, entity->pos_y)) {
      entity->pos_x += 2;
      return 1;
    }
    break;
  
  case UP:
    if (can_move(entity->pos_x, entity->pos_y - 1)) {
      entity->pos_y -= 1;
      return 1;
    }
    break;

  case DOWN:
    if (can_move(entity->pos_x, entity->pos_y + 1)) {
      entity->pos_y += 1;
      return 1;
    }
    break;

  default:
    break;
  }
  return 0;
}

//Retorna la direccio oposada a dir
int opposite(int dir) {
  switch(dir) {
      case UP:    return DOWN;
      case DOWN:  return UP;
      case LEFT:  return RIGHT;
      case RIGHT: return LEFT;
  }
  return -1;
}

// Calcula la “distancia al cuadrat” entre dos punts
int dist2(int x1, int y1, int x2, int y2) {
  int dx = x2 - x1;
  int dy = y2 - y1;
  return dx*dx + dy*dy;
}

//Mou al fantasma en la direccio que li deixa més a prop del target (les comproba totes i aplica el millor moviment)
void move_ghost_to_target(Entity *ghost, int target_x, int target_y) {
  int best_dir = -1;
  int best_score = 100000;

  int x = ghost->pos_x;
  int y = ghost->pos_y;

  int forbidden = opposite(ghost->dir);

  for (int d = NONE+1; d < NONE+5; ++d) {
    if (d == forbidden) continue;

    int nx = x, ny = y;
    switch (d) {
        case UP:    ny -= 1; break;
        case DOWN:  ny += 1; break; 
        case LEFT:  nx -= 2; break;
        case RIGHT: nx += 2; break;
    }

    // Convierte a coords de grilla para el chequeo de muros
    if (!can_move(nx, ny))
        continue;

    // Calcula distancia al cuadrado en coords de terminal
    int score = dist2(nx/2, ny, target_x/2, target_y);
    if (score < best_score) {
        best_score = score;
        best_dir = d;
    }
}

if (best_dir >= 0) {
    ghost->dir = best_dir;
    // actualiza posición al mismo tiempo si lo deseas:
    switch (best_dir) {
        case UP:    ghost->pos_y--;          break;
        case DOWN:  ghost->pos_y++;          break;
        case LEFT:  ghost->pos_x -= 2;       break;
        case RIGHT: ghost->pos_x += 2;       break;
    }
}
}

//Es mou agafant de target la posicio del pacman
void update_red_ghost(Entity *blinky) {
  if (vulnerable_frames > 0) move_ghost_to_target(blinky, CORNER_BLINKY_X, CORNER_BLINKY_Y);
  else move_ghost_to_target(blinky, pacman.pos_x, pacman.pos_y);
}

//Es mou agafant de target 3 caselles per davant del pacman
void update_pink_ghost (Entity *pinky){
  if (vulnerable_frames > 0) move_ghost_to_target(pinky, CORNER_PINKY_X, CORNER_PINKY_Y);
  else {
    int target_x = pacman.pos_x;
    int target_y = pacman.pos_y;

    switch (pacman.dir) {
      case UP:    target_y -= 3;         break;
      case DOWN:  target_y -= 3;         break;
      case LEFT:  target_x -= 6;         break;
      case RIGHT: target_x += 6;         break;
    }
    if (!can_move(target_x, target_y)) {
      target_x = pacman.pos_x;
      target_y = pacman.pos_y;
    }

    move_ghost_to_target(pinky, target_x, target_y);
  }
}

//Es mou fent un vector entre la posicio del pacman + dos caselles i la posició del fantasma vermell
void update_blue_ghost (Entity *inky) {
  if (vulnerable_frames > 0) move_ghost_to_target(inky, CORNER_INKY_X, CORNER_INKY_Y);
  else {
    int fx = pacman.pos_x;
    int fy = pacman.pos_y;

    // Calcula el punt 2 celdas davant del Pac-Man
    switch (pacman.dir) {
        case UP: fy -= 2;       break;
        case DOWN: fy += 2;     break;
        case LEFT: fx -= 2;     break;
        case RIGHT: fx += 2;    break;
    }

    int vx = fx - ghosts[BLINKY].pos_x;
    int vy = fy - ghosts[BLINKY].pos_y;

    int target_x = ghosts[BLINKY].pos_x + 2 * vx;
    int target_y = ghosts[BLINKY].pos_y + 2 * vy;

    move_ghost_to_target(inky, target_x, target_y);
  }
}

//Persegueix al pacman fins que esta a menys de 6 caselles, aleshores escapa a la seva cantonada
void update_orange_ghost(Entity *clyde) {
  if (vulnerable_frames > 0) move_ghost_to_target(clyde, CORNER_CLYDE_X, CORNER_CLYDE_Y);
  else {
    int target_x = pacman.pos_x;
    int target_y = pacman.pos_y;

    if (abs_diff(clyde->pos_x, target_x) < 10 && abs_diff(clyde->pos_y, target_y) < 5) {
      target_x = CORNER_CLYDE_X;
      target_y = CORNER_CLYDE_Y;
    }

    move_ghost_to_target(clyde, target_x, target_y);
  }
}

//Activa que els fantasmes siguin vulnerables i els fa els canvis pertinents (canviar el caracter i color)
void activate_powerup() {
  vulnerable_frames = VULNERABLE_FRAMES;
  for (int i = 0; i < 4; ++i) {
    ghosts[i].character = '?';
    ghosts[i].color = COLOR_GREEN;
  }
}

//Fa els canvis als fantasmes per desfer la vulnerabilitat
void deactivate_powerup() {
  for (int i = 0; i < 4; ++i) {
    ghosts[i].character = 'X';
  }
  ghosts[PINKY].color = COLOR_MAGENTA;
  ghosts[BLINKY].color = COLOR_RED;
  ghosts[INKY].color = COLOR_LIGHT_BLUE;
  ghosts[CLYDE].color = COLOR_ORANGE;
}

//Fa que el fantasma mori (el desactiva temporalment i reinicia la seva posicio)
void ghost_die(Entity *ghost, int idx) {
  ghost->respawn_timer = DEAD_FRAMES;
  if (idx == BLINKY) {
    ghost->pos_x = CORNER_BLINKY_X;
    ghost->pos_y = CORNER_BLINKY_Y;
  }
  else if (idx == PINKY) {
    ghost->pos_x = CORNER_PINKY_X;
    ghost->pos_y = CORNER_PINKY_Y;
  }
  else if (idx == INKY) {
    ghost->pos_x = CORNER_INKY_X;
    ghost->pos_y = CORNER_INKY_Y;
  }
  else if (idx == CLYDE) {
    ghost->pos_x = CORNER_CLYDE_X;
    ghost->pos_y = CORNER_CLYDE_Y;
  }
}

//Comprova les colisions dels fantasmes amb el pacman i actua segons si hi ha vulneravilitat o no
void check_collisions() {
  for (int i = 0; i < 4; ++i) {
    if (ghosts[i].respawn_timer <= 0 && ghosts[i].pos_x == pacman.pos_x && ghosts[i].pos_y == pacman.pos_y) {
      if (vulnerable_frames > 0) ghost_die(&(ghosts[i]), i);
      else current_scene = GAMEOVER_SCENE;
    }
  }
}

//fa updates dels components del joc
void updateGame() {
  //Movem al Pacman
  if (pacman.next_dir != NONE && pacman.spare_blocks > 0 && move_entity(&pacman, pacman.next_dir)) {
    pacman.dir = pacman.next_dir;
    pacman.next_dir = NONE;
  }
  else {
    if (pacman.spare_blocks > 0) --pacman.spare_blocks;
    else pacman.next_dir = 0;
    move_entity(&pacman, pacman.dir);
  }

  //Comprobar si hi ha punts en la nova posicio del pacman
  if (map[pacman.pos_y][pacman.pos_x] == 'o') {
    ++points;
    map[pacman.pos_y][pacman.pos_x] = ' ';
  }
  else if (map[pacman.pos_y][pacman.pos_x] == '$') {
    ++points;
    map[pacman.pos_y][pacman.pos_x] = ' ';
    activate_powerup();
  }

  //Movem als fantasmes si el pacman s'ha començat a moure
  if (pacman.dir != NONE) {
    if (ghosts[BLINKY].respawn_timer <= 0)update_red_ghost(&(ghosts[BLINKY]));
    else --ghosts[BLINKY].respawn_timer;
    if (ghosts[PINKY].respawn_timer <= 0)update_pink_ghost(&(ghosts[PINKY]));
    else --ghosts[PINKY].respawn_timer;
    if (ghosts[INKY].respawn_timer <= 0)update_blue_ghost(&(ghosts[INKY]));
    else --ghosts[INKY].respawn_timer;
    if (ghosts[CLYDE].respawn_timer <= 0)update_orange_ghost(&(ghosts[CLYDE]));
    else --ghosts[CLYDE].respawn_timer;

    //Comprovem si alguna posicio coincideix amb la del pacman -> Per imitar al joc original si "intercanvien posicions" no hi ha colisio.
    check_collisions();    

    //Actualitzem la info de vulnerabilitat
    if (vulnerable_frames-- <= 0) {
      deactivate_powerup();
    }
  }

  //Si agafem tots els punts passem a la pantalla final
  if (points >= 168) current_scene = WIN_SCENE;
}

//Dibuixa a la pantalla el mapa i les entitats
void renderGame() {
  //Render map -> En el buffer auxiliar
  int idx = NUM_COL;
  int color = COLOR_WHITE;
  for (int i = 0; i < 24; ++i) {
    for (int j = 0; j < 80; ++j) {
      if (map[i][j] == '#') color = COLOR_DARK_BLUE;  //Blau pels murs
      else if (map[i][j] == '$') color = COLOR_CYAN;  //Cian per powerup
      else color = COLOR_WHITE;                       //Blanc resta
      back_buffer[idx++] = color << 8 | map[i][j];  
    }
  }

  //Render Entities -> En el buffer auxiliar
  idx = NUM_COL + pacman.pos_y * NUM_COL + pacman.pos_x;
  back_buffer[idx] = pacman.color << 8 | pacman.character;
  
  for (int i = 0; i < 4; ++i) {
    if (ghosts[i].respawn_timer <= 0) {
      idx = NUM_COL + ghosts[i].pos_y * NUM_COL + ghosts[i].pos_x;
      back_buffer[idx] = ghosts[i].color <<8 | ghosts[i].character;
    }
  }

  //Draw the auxiliar buffer
  for (int i = NUM_COL; i < 24*80; ++i) {
    screen[i] = back_buffer[i];
  }
  
  //Draw game info
  print_str(3, 0, "FPS: ", COLOR_WHITE);
  itoa(fps, buff);
  print_str(9, 0, buff, COLOR_WHITE);
}

//Inicialitza el mapa a utilitzar a partir del mapa original
void init_map() {
  for (int i = 0; i < 24; ++i) {
    for (int j = 0; j < 80; ++j) {
      map[i][j] = original_map[i][j];
    }
  }
}

//Inicialitza el joc
void init_game() {
  ticks_passed = 0;
  ticks_last_time = gettime();
  fps = 0;
  sec_cont = 0;
  frames_cont = 0;

  vulnerable_frames = 0;

  //Borrem pantalla
  for (int i = 0; i < 80 * 25; ++i) {
    screen[i] = ' ';
  }

  //Inicialitzem el mapa
  init_map();

  //Inicialitzem Entities
    //Pacman
  pacman.character = '@';
  pacman.color = COLOR_YELLOW;
  pacman.dir = NONE;
  pacman.next_dir = NONE;
  pacman.spare_blocks = SPARE_BLOCKS;
  pacman.pos_x = PACMAN_INIT_POS_X;
  pacman.pos_y = PACMAN_INIT_POS_Y;

    //Ghosts
  for (int i = 0; i < 4; ++i) {
    ghosts[i].character = 'X';
    ghosts[i].respawn_timer = 0;
    ghosts[i].dir = NONE;
  }

  ghosts[PINKY].color = COLOR_MAGENTA;
  ghosts[PINKY].pos_x = CORNER_PINKY_X;
  ghosts[PINKY].pos_y = CORNER_PINKY_Y;

  ghosts[BLINKY].color = COLOR_RED;
  ghosts[BLINKY].pos_x = CORNER_BLINKY_X;
  ghosts[BLINKY].pos_y = CORNER_BLINKY_Y;

  ghosts[INKY].color = COLOR_LIGHT_BLUE;
  ghosts[INKY].pos_x = CORNER_INKY_X;
  ghosts[INKY].pos_y = CORNER_INKY_Y;

  ghosts[CLYDE].color = COLOR_ORANGE;
  ghosts[CLYDE].pos_x = CORNER_CLYDE_X;
  ghosts[CLYDE].pos_y = CORNER_CLYDE_Y;
  //Inicialitzem GameState
  points = 0;
}

//Codi del thread que porta la gestió del joc
void game_thread() {
  while (1) {
    pause(200);

    current_scene = MENU_SCENE;
    draw_title_screen();

    //PREGUNTAR: Habria que asegurar que se dumpea la pantalla (si pones pause no se dibuja nunca) -> Dura poco en ejecucion y pause no lo hace
      //Puedo poner el dump_screen en la rutina de pause? Para asegurar que se hace -> SOlucionaria tambien el siguiente problema
    while(current_scene == MENU_SCENE) {
      //pause(1000);
      getpid();
    }

    init_game();

    while (current_scene == GAME_SCENE) {
      int current_time = gettime();
      ticks_passed = current_time - ticks_last_time;      
      
      frames_cont++;
      sec_cont += ticks_passed;
      fixed_cont += ticks_passed;
      //Cada 18 ticks ha passat un segon (18 interrupcions de rellotge per segon)
      //PREGUNTAR: Cada 18 ticks una segundo, va muy rapido, cosa de la maquina virtual?
      if (sec_cont >= 18) {
        fps = frames_cont;
        frames_cont = 0;
        sec_cont = 0;
      }

      if (fixed_cont >= FIXED_UPDATE_TICKS) {
        fixed_cont = 0;
        sem_wait(sem_game);
        updateGame();
        sem_post(sem_game);
      }

      sem_wait(sem_game);
      renderGame();
      sem_post(sem_game);
      //pause(100);


      ticks_last_time = current_time;
    }

    //PREGUNTAR: Si no poso això no fa el dump_screen (no es veu ultim frame del joc), es bloqueja abans de fer la interrupcio
    int i =0;
    while (++i < 1000000) getpid();
    //pause(50000);

    if (current_scene == WIN_SCENE) {  
      draw_win_screen();
      
    }

    if (current_scene == GAMEOVER_SCENE) {
      draw_game_over_screen();
    }

    //PREGUNTAR: Lo mismo
    while (current_scene != MENU_SCENE) {
      getpid();
      //pause(100)
    }
  }
}

//Rutina que fa el thread de llegir el teclat -> Llegeix el teclat i es bloqueja un temps
void read_keyboard_thread() {
  while(1) {
    sem_wait(sem_game);

    if (GetKeyboardState(keyboard) < 0) perror();

    if (current_scene == MENU_SCENE) {
      for (int i = 0; i < 128; ++i) {
        if (keyboard[i] == 1) {
          ++current_scene;
          break;
        }
      }
    }
    else if (current_scene == GAME_SCENE) {
      if (keyboard[KEY_W] == 1) {
        pacman.next_dir = UP;
        pacman.spare_blocks = SPARE_BLOCKS;
      }
      else if (keyboard[KEY_S] == 1) {
        pacman.next_dir = DOWN;
        pacman.spare_blocks = SPARE_BLOCKS;
      }
      else if (keyboard[KEY_A] == 1) {
        pacman.next_dir = LEFT;
        pacman.spare_blocks = SPARE_BLOCKS;
      }
      else if (keyboard[KEY_D] == 1) {
        pacman.next_dir = RIGHT;
        pacman.spare_blocks = SPARE_BLOCKS;
      }
    }
    else if (current_scene == GAMEOVER_SCENE || current_scene == WIN_SCENE) {
      for (int i = 0; i < 128; ++i) {
        if (keyboard[i] == 1) {
          current_scene = MENU_SCENE;
          break;
        }
      }
    }
      

    sem_post(sem_game);
    pause(100);
  }
}

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */

  for (int i = 0; i < 128; ++i) keyboard[i] = 0;
  write(1, "\nHello!\n", 8);

  screen = (unsigned short*)StartScreen();
  if (screen == (void*)-1) perror("Error al acceder a la pantalla");
  else {
    for (int i = 0; i < 80 * 25; ++i) {
        screen[i] = ' ';
    }
  }

  sem_game = sem_init(1); 
  if (sem_game < 0) {
      write(1, "Error initializing semaphore\n", 30);
      return -1;
  }
  
  //Creamos el thread auxiliar que controlará el juego
  int tid1 = clone(CLONE_THREAD, game_thread, 0, 1024);
  if (tid1 < 0) {
      perror();
      sem_destroy(sem_game);
      return -1;
  }

  //Thread principal lee el teclado();
  read_keyboard_thread();

  /*
    for (int i = 0; i < 12; ++i) {
        int id = fork();
        if (id < 0) perror();
        if (id > 0) test_semaphores();
        if (id == 0) exit();
    }
    write(1, "\nFINISHED\n", 10);
    while(1);
  */
 
  //test_simple_clone();
  //test_multiple_threads();
  //test_priority();

  // thread hace fork
  //test_clone_and_fork();


/*
  int pid = fork();
  if (pid < 0) perror();
  else if (pid > 0) {
     //exit desde el thread principal
    test_exit_from_main();
    //exit desde thread secundario
    test_exit_from_thread();
    draw_screen(screen);
  }
  else {
    // Hijo crea thread
    int val = 0;
    int ret = clone(CLONE_THREAD, son_thread, &val, 4096);
    if (ret < 0) perror();
    while(1) {

    }
  }
*/

}
