#include "jaakkos.h"

extern char item_list[ITEMS][ITEMSTRSZ];

unsigned char patch_3_instructions[] = {0xB0, 0x52, 0x90, 0x90, 0x90};
unsigned char talent_instructions[] = {0x56, 0x90, 0x90};

#define MAX_ROLLS 1000
#define BEST_ROLLS 16

struct RollResult {
  unsigned char rng[0x100];
  int stats[0x9];
  int items[0x2b9+0x2f];
  unsigned int talents;
};

struct RollerShared {
  int stats[0x9];
  int items[0x2b9+0x2f];
  unsigned int talents;
};

int stat_requirements[0x9] = {10, 10, 10, 10, 10, 10, 10, 10, 10};
int item_requirements = 0x0;// 0x0 for nothing, 0x1...0x2b9 for items, 0x2ba...0x2e8 for spellbooks (0x16 for iron ration)

RollerShared *shm;

const char stat_names[][3] = {"St", "Le", "Wi", "Dx", "To", "Ch", "Ap", "Ma", "Pe"};

static void reseed_rng() {
  uint16_t *rng111 = (uint16_t *)0x082ada40;
  uint16_t *rng12018 = (uint16_t *)0x082CEBA0;
  uint16_t *rng12020 = (uint16_t *)0x082cf6c0;
  uint16_t *rng12021 = (uint16_t *)0x082f3d40;
  uint16_t *rng12022 = (uint16_t *)0x082f5d60;
  uint16_t *rng12023 = (uint16_t *)0x082f6dc0;
  uint16_t *rng12048 = (uint16_t *)0x083316a0;
  uint16_t *rng12049 = (uint16_t *)0x08332c60;
  uint16_t *rng12051 = (uint16_t *)0x0845a000;
  uint16_t *rng12055 = (uint16_t *)0x0845af80;
  uint16_t *rng12059 = (uint16_t *)0x0846d540;
  int adom_version = get_version();
  if (adom_version == 111) {
    for (int i=0; i < (0x100/2); i++)
      rng111[i] = rand();
  }
  else if (adom_version == 12018) {
    for (int i=0; i < (0x100/2); i++)
      rng12018[i] = rand();
  }
  else if (adom_version == 12020) {
    for (int i=0; i < (0x100/2); i++)
      rng12020[i] = rand();
  }
  else if (adom_version == 12021) {
    for (int i=0; i < (0x100/2); i++)
      rng12021[i] = rand();
  }
  else if (adom_version == 12022) {
    for (int i=0; i < (0x100/2); i++)
      rng12022[i] = rand();
  }
  else if (adom_version == 12023) {
    for (int i=0; i < (0x100/2); i++)
      rng12023[i] = rand();
  }
  else if (adom_version == 12048) {
    for (int i=0; i < (0x100/2); i++)
      rng12048[i] = rand();
  }
  else if (adom_version == 12049) {
    for (int i=0; i < (0x100/2); i++)
      rng12049[i] = rand();
  }
  else if (adom_version == 12051) {
    for (int i=0; i < (0x100/2); i++)
      rng12051[i] = rand();
  }
  else if (adom_version == 12055) {
    for (int i=0; i < (0x100/2); i++)
      rng12055[i] = rand();
  }
  else if (adom_version == 12059) {
    for (int i=0; i < (0x100/2); i++)
      rng12059[i] = rand();
  }
  else {
    printf("Don't know where to inject roller. Unknown ADOM version %i ?\n", adom_version);
    return;
  }
}

void load_requirements() {
  char roller_cfg[1024];
  snprintf(roller_cfg, 1024, "%s/roller.cfg", getpwuid(getuid())->pw_dir);

  FILE *fp = fopen(roller_cfg, "r");
  if (fp) {
    fread(stat_requirements, sizeof(int), 0x9, fp);
    fclose(fp);
  }
}

static void save_requirements() {
  char roller_cfg[1024];
  snprintf(roller_cfg, 1024, "%s/roller.cfg", getpwuid(getuid())->pw_dir);

  FILE *fp = fopen(roller_cfg, "w");
  if (fp) {
    fwrite(stat_requirements, 4, 0x9, fp);
    fclose(fp);
  }
}

static void draw_ui(int stat_sel, unsigned char failed,
                    unsigned int max_rolls, RollResult *best_rolls) {
  if (failed == 2) {
    printf("The initial roll finished. Ignore the roller by hitting 'a'.\r\n");
    printf("\r\n");
  }

  else if (failed) {
    printf("The roll finished. The character you asked for WAS NOT FOUND.\r\n");
    printf("\r\n");
  }

  else {
    printf("The roll finished. The character you asked for WAS FOUND.\r\n");
    printf("\r\n");
  }

  printf("Your character has the following attributes.\r\n");

  for (int i=0; i < 0x9; i++)
    printf("%s - rolled %2d %s required %2d%s\r\n", stat_names[i],
      best_rolls[0].stats[i], (best_rolls[0].stats[i] >= stat_requirements[i]) ? "and" : "\x1B[1;37mbut\x1B[0;37m", stat_requirements[i], (stat_sel == i) ? " (selected)" : "");

  printf("S - rolled %d total\r\n", +best_rolls[0].stats[0]
                                    +best_rolls[0].stats[1]
                                    +best_rolls[0].stats[2]
                                    +best_rolls[0].stats[3]
                                    +best_rolls[0].stats[4]
                                    +best_rolls[0].stats[5]
                                    +best_rolls[0].stats[6]
                                    +best_rolls[0].stats[7]
                                    +best_rolls[0].stats[8]);
  printf("I - item #%i from group #%i is %s\r\n", item_requirements%(0x2b9+1)+item_requirements/(0x2b9+1), item_requirements/(0x2b9+1), (item_requirements > 0) ? ((best_rolls[0].items[item_requirements-1]) ? "found" : "not found") : "not an item");
  printf("T - potentially %d talents to be learned\r\n", best_rolls[0].talents);
  printf("\r\n");
  printf("The roller will generate a maximum of %d characters\r\n", max_rolls);
    char *item_requirements_buffer;
    item_requirements_buffer = (char *)malloc(0x100*sizeof(char));
    sprintf(item_requirements_buffer, "require item #%i from group #%i (%.36s%s)", item_requirements%(0x2b9+1)+item_requirements/(0x2b9+1), item_requirements/(0x2b9+1), item_list[item_requirements-1], (strlen(item_list[item_requirements-1]) > 36) ? "..." : "");
  printf("and %s.\r\n", (item_requirements > 0) ? item_requirements_buffer : "not require any items");
  printf("\r\n");
  printf("a/r - accept/refuse the current character\r\n");
  printf("1-9 - select the attribute to modify\r\n");
  printf("p/n - decrease/increase the item index required by 1\r\n");
  printf("b/f - decrease/increase the item index required by 100\r\n");
  printf("-/+ - decrease/increase the selected attribute by 1\r\n");
  printf("</> - decrease/increase the maximum rolls by 100");
  fflush(stdout);
}

static int config_roller(RollResult *best_rolls, unsigned char failed,
                         unsigned int *max_rolls) {
  int input;
  int stat_sel = 0;

  while (1) {
    printf("\033[2J\033[0;0H");
    draw_ui(stat_sel, failed, *max_rolls, best_rolls);
    input = tolower(fgetc(stdin));

    if (isdigit(input)) {
      int no = input-'0';
      if (no < 1 || no > 0x9) continue;
      stat_sel = no-1;
    }

    else {
      int ret;

      switch(input) {
      case 'a':
	    ret = 1;
		break;

      case 'r':
	    ret = 0;
		break;

      case '+':
        if (stat_requirements[stat_sel] < 99)
          stat_requirements[stat_sel]++;
        continue;

      case '-':
        if (stat_requirements[stat_sel] > 0)
          stat_requirements[stat_sel]--;
        continue;

      case '>':
        if (*max_rolls < 100000)
          *max_rolls += 100;
        continue;

      case '<':
        if (*max_rolls > 100)
          *max_rolls -= 100;
        continue;

      case 'n':
        if (item_requirements < 0x2b9+0x2f)
          item_requirements += 1;
        continue;

      case 'p':
        if (item_requirements > 0)
          item_requirements -= 1;
        continue;

      case 'f':
        if (item_requirements < 0x2b9+0x2f-100)
          item_requirements += 100;
        continue;

      case 'b':
        if (item_requirements > 0+100)
          item_requirements -= 100;
        continue;

      default:
	    continue;
      }

      printf("\033[2J\033[0;0H");
      fflush(stdout);

      save_requirements();
      return ret;
    }
  }
}

#define P2(x) ((x)*(x))

static int sqerr(int *data) {
  int sqerr = 0;

  for (int i=0; i < 0x9; i++)
    if (stat_requirements[i] > data[i])
      sqerr += P2(stat_requirements[i]-data[i]);

  return sqerr;
}

// children end up here
static void roll_end(unsigned int talents, void *b, void *c, unsigned int ntalents) {
  uint32_t STATS_ADDR = 0, ITEMS_ADDR = 0, BOOKS_ADDR = 0;
  int adom_version = get_version();

  if (adom_version == 111) {
    STATS_ADDR = 0x082b1728;
    ITEMS_ADDR = 0x082a5980;
    BOOKS_ADDR = 0x082a7e00;
  }
  else if (adom_version == 12018) {
    STATS_ADDR = 0x082DBA7C;
    ITEMS_ADDR = 0x082C6080;
    BOOKS_ADDR = 0x082C7B60;
  }
  else if (adom_version == 12020) {
    STATS_ADDR = 0x082dc5a0;
    ITEMS_ADDR = 0x082c6ba0;
    BOOKS_ADDR = 0x082c8680;
  }
  else if (adom_version == 12021) {
    STATS_ADDR = 0x08302ca0;
    ITEMS_ADDR = 0x082ead40;
    BOOKS_ADDR = 0x082ec820;
  }
  else if (adom_version == 12022) {
    STATS_ADDR = 0x08304cc0;
    ITEMS_ADDR = 0x082ecd60;
    BOOKS_ADDR = 0x082ee840;
  }
  else if (adom_version == 12023) {
    STATS_ADDR = 0x08305d20;
    ITEMS_ADDR = 0x082edda0;
    BOOKS_ADDR = 0x082ef880;
  }
  else if (adom_version == 12048) {
    STATS_ADDR = 0x083425ec;
    ITEMS_ADDR = 0x083286a0;
    BOOKS_ADDR = 0x0832a180;
  }
  else if (adom_version == 12049) {
    STATS_ADDR = 0x08343bac;
    ITEMS_ADDR = 0x08329c60;
    BOOKS_ADDR = 0x0832b740;
  }
  else if (adom_version == 12051) {
    STATS_ADDR = 0x0846af8c;
    ITEMS_ADDR = 0x0844EBC0;
    BOOKS_ADDR = 0x084506A0;
  }
  else if (adom_version == 12055) {
    STATS_ADDR = 0x0846bf0c;
    ITEMS_ADDR = 0x0844fb40;
    BOOKS_ADDR = 0x08451620;
  }
  else if (adom_version == 12059) {
    STATS_ADDR = 0x0847e9cc;
    ITEMS_ADDR = 0x0845a880;
    BOOKS_ADDR = 0x0845c360;
  }
  else {
    printf("Don't know where to inject roller. Unknown ADOM version %i ?\n", adom_version);
    return;
  }

  memcpy(shm->stats, (void*)STATS_ADDR, 0x9*sizeof(int));
  memcpy(shm->items, (void*)ITEMS_ADDR, 0x2b9*sizeof(int));
  memcpy(shm->items+0x2b9, (void*)BOOKS_ADDR, 0x2f*sizeof(int));
  if ((adom_version == 12018) || (adom_version >= 12020)) {
    shm->talents = talents;
  }
  else {
    shm->talents = ntalents;
  }
  exit(EXIT_SUCCESS);
}

static void child() {
  uint32_t PATCH1_ADDR = 0, PATCH2_ADDR = 0, PATCH3_ADDR = 0, CNT_ADDR = 0, TALENT_ADDR = 0, ROLLEND_ADDR = 0, RESUME_ADDR = 0, CORR_ADDR = 0;
  int adom_version = get_version();

  if (adom_version == 111) {
    PATCH1_ADDR = 0x080756e1;
    PATCH2_ADDR = 0x0807571b;
    PATCH3_ADDR = 0x08075728;
    CNT_ADDR = 0x0814a86e;
    ROLLEND_ADDR = 0x0814ec49;
    RESUME_ADDR = 0x08073970;
  }
  else if (adom_version == 12018) {
    PATCH1_ADDR = 0x0807DB25;
    PATCH2_ADDR = 0x0807DB5A;
    PATCH3_ADDR = 0x0807DB66;
    CNT_ADDR = 0x0807BB95;
    TALENT_ADDR = 0x0815CBE1;
    ROLLEND_ADDR = 0x0815CBE5;
    RESUME_ADDR = 0x0807C7B0;
  }
  else if (adom_version == 12020) {
    PATCH1_ADDR = 0x0807dba5;
    PATCH2_ADDR = 0x0807dbda;
    PATCH3_ADDR = 0x0807dbe6;
    CNT_ADDR = 0x0807bc25;
    TALENT_ADDR = 0x0815d041;
    ROLLEND_ADDR = 0x0815d045;
    RESUME_ADDR = 0x0807c820;
  }
  else if (adom_version == 12021) {
    PATCH1_ADDR = 0x08080105;
    PATCH2_ADDR = 0x08080854;
    PATCH3_ADDR = 0x08080860;
    CNT_ADDR = 0x0807e285;
    TALENT_ADDR = 0x08168bc4;
    ROLLEND_ADDR = 0x08168bc8;
    RESUME_ADDR = 0x0807eb70;
    CORR_ADDR = 0x0812E0C3;
  }
  else if (adom_version == 12022) {
    PATCH1_ADDR = 0x08080550;
    PATCH2_ADDR = 0x08080d2c;
    PATCH3_ADDR = 0x08080d38;
    CNT_ADDR = 0x0807e70d;
    TALENT_ADDR = 0x0816a504;
    ROLLEND_ADDR = 0x0816a508;
    RESUME_ADDR = 0x0807f000;
    CORR_ADDR = 0x0812f5c3;
  }
  else if (adom_version == 12023) {
    PATCH1_ADDR = 0x080806f0;
    PATCH2_ADDR = 0x08080ecc;
    PATCH3_ADDR = 0x08080ed8;
    CNT_ADDR = 0x0807e8ad;
    TALENT_ADDR = 0x0816ad44;
    ROLLEND_ADDR = 0x0816ad48;
    RESUME_ADDR = 0x0807f1a0;
    CORR_ADDR = 0x0812fd03;
  }
  else if (adom_version == 12048) {
    PATCH1_ADDR = 0x08082d10;
    PATCH2_ADDR = 0x08083553;
    PATCH3_ADDR = 0x0808355f;
    CNT_ADDR = 0x08080ec5;
    TALENT_ADDR = 0x08178464;
    ROLLEND_ADDR = 0x08178468;
    RESUME_ADDR = 0x080817b0;
    CORR_ADDR = 0x0813b773;
  }
  else if (adom_version == 12049) {
    PATCH1_ADDR = 0x08082e25;
    PATCH2_ADDR = 0x08083663;
    PATCH3_ADDR = 0x0808366f;
    CNT_ADDR = 0x08080f4d;
    TALENT_ADDR = 0x08178ef4;
    ROLLEND_ADDR = 0x08178ef8;
    RESUME_ADDR = 0x08081840;
    CORR_ADDR = 0x0813bf53;
  }
  else if (adom_version == 12051) {
    PATCH1_ADDR = 0x0808AB3A;
    PATCH2_ADDR = 0x0808B19C;
    PATCH3_ADDR = 0x0808B1C7;
    CNT_ADDR = 0x08088a0f;
    TALENT_ADDR = 0x08184d74;
    ROLLEND_ADDR = 0x08184d78;
    RESUME_ADDR = 0x08089310;
    CORR_ADDR = 0x08148083;
  }
  else if (adom_version == 12055) {
    PATCH1_ADDR = 0x0808afea;
    PATCH2_ADDR = 0x0808b64c;
    PATCH3_ADDR = 0x0808b677;
    CNT_ADDR = 0x08088ebf;
    TALENT_ADDR = 0x081856c4;
    ROLLEND_ADDR = 0x081856c8;
    RESUME_ADDR = 0x080897c0;
    CORR_ADDR = 0x08148d13;
  }
  else if (adom_version == 12059) {
    PATCH1_ADDR = 0x0808D7CA;
    PATCH2_ADDR = 0x0808dc73;
    PATCH3_ADDR = 0x0808dc9e;
    CNT_ADDR = 0x0808b84f;
    TALENT_ADDR = 0x0818b9f4;
    ROLLEND_ADDR = 0x0818b9f8;
    RESUME_ADDR = 0x0808c160;
    CORR_ADDR = 0x0814E7FD;
  }
  else {
    printf("Don't know where to inject roller. Unknown ADOM version %i ?\n", adom_version);
    return;
  }

  close(STDOUT_FILENO);
  close(STDIN_FILENO);

  if (mprotect(PAGEBOUND(PATCH1_ADDR), getpagesize(), RWX_PROT) ||
      mprotect(PAGEBOUND(PATCH2_ADDR), getpagesize(), RWX_PROT) ||
      mprotect(PAGEBOUND(PATCH3_ADDR), getpagesize(), RWX_PROT) ||
      mprotect(PAGEBOUND(ROLLEND_ADDR), getpagesize(), RWX_PROT) ||
      mprotect(PAGEBOUND(RESUME_ADDR), getpagesize(), RWX_PROT) ||
      mprotect(PAGEBOUND(CNT_ADDR), getpagesize(), RWX_PROT)) {
    perror("mprotect");
    exit(1);
  }
  // don't prompt to see corruptions during child rolling
  if ((adom_version == 12021) || (adom_version == 12022)) {
      if (mprotect(PAGEBOUND(CORR_ADDR), getpagesize(), RWX_PROT)) {
        perror("mprotect");
        exit(1);
      }
      memcpy((void*)CORR_ADDR, patch_3_instructions, sizeof(patch_3_instructions));
  }

  // don't stop waiting for useless input in roller
  memset((void*)PATCH1_ADDR, 0x90, 5);
  memset((void*)PATCH2_ADDR, 0x90, 5);
  memcpy((void*)PATCH3_ADDR, patch_3_instructions, sizeof(patch_3_instructions));
  // don't write .cnt
  if (adom_version == 111) {
    *((uint16_t *)CNT_ADDR) = 0x71eb;
  }
  else if ((adom_version == 12018) || (adom_version >= 12020)) {
    memset((void*)CNT_ADDR, 0x90, 5);
    if (mprotect(PAGEBOUND(TALENT_ADDR), getpagesize(), RWX_PROT)) {
      perror("mprotect");
      exit(1);
    }
    memcpy((void*)TALENT_ADDR, talent_instructions, sizeof(talent_instructions));
  }
  else {
    printf("Don't know where to inject roller. Unknown ADOM version %i ?\n", adom_version);
    return;
  }

  // patch roll end
  *((char**)ROLLEND_ADDR) = ((char*)(&roll_end)) - ROLLEND_ADDR - 4;
  if ((adom_version >= 12021) && (adom_version <= 12059)) {
    memset((void*)(ROLLEND_ADDR - 1), 0xE8, 1);
  }

  // resume ADoM
  ((void(*)())RESUME_ADDR)();
  return;
}

static void display_roll_status(RollResult *best_rolls, int rolln, int max_rolls,
                                struct timeval begin) {
  struct timeval end;
  gettimeofday(&end, NULL);

  double tdiff = end.tv_sec-begin.tv_sec;
  tdiff += (end.tv_usec-begin.tv_usec) / 1000000.0;

  printf("\033[2J\033[0;0HRolled %d/%d characters in %.1f s at %.1f Hz as follows.\r\n\r\n",
  rolln, max_rolls, tdiff, ((double)rolln)/tdiff);

  printf("+----+----+----+----+----+----+----+----+----+------+---+---+------+\r\n");

  for (int stat=0; stat < 0x9; stat++)
    printf("| %2s ", stat_names[stat]);

  printf("| Sum  | I | T | Err  |\r\n");
  printf("+----+----+----+----+----+----+----+----+----+------+---+---+------+\r\n");

  for (int stat=0; stat < 0x9; stat++)
    printf("| %2d ", stat_requirements[stat]);

  printf("|      | %c |   |      |\r\n", (item_requirements > 0) ? 'X' : ' ');
  printf("+----+----+----+----+----+----+----+----+----+------+---+---+------+\r\n");

  for (int i=0; i < BEST_ROLLS; i++)
    if (best_rolls[i].stats[0] > 0) {
      for (int stat=0; stat < 0x9; stat++)
        printf("| %2d ", best_rolls[i].stats[stat]);

      printf("| %4d |", +best_rolls[i].stats[0]
                        +best_rolls[i].stats[1]
                        +best_rolls[i].stats[2]
                        +best_rolls[i].stats[3]
                        +best_rolls[i].stats[4]
                        +best_rolls[i].stats[5]
                        +best_rolls[i].stats[6]
                        +best_rolls[i].stats[7]
                        +best_rolls[i].stats[8]);
      printf(" %c |", (best_rolls[i].items[item_requirements-1]) ? 'X' : ' ');
      printf(" %1d |", best_rolls[i].talents);
      printf(" %4d |\r\n", sqerr(best_rolls[i].stats));
    }
  printf("+----+----+----+----+----+----+----+----+----+------+---+---+------+\r\n");

  fflush(stdout);
}

#define BETTER_THAN(i) ((!item_requirements || shm->items[item_requirements-1] == best_rolls[(i)].items[item_requirements-1]) ? sqerr(shm->stats) <= sqerr(best_rolls[(i)].stats) : shm->items[item_requirements-1])

static int process_roll_result(RollResult *best_rolls) {
  uint32_t RNG_ADDR = 0;
  int adom_version = get_version();
  if (adom_version == 111) {
    RNG_ADDR = 0x082ada40;
  }
  else if (adom_version == 12018) {
    RNG_ADDR = 0x082CEBA0;
  }
  else if (adom_version == 12020) {
    RNG_ADDR = 0x082cf6c0;
  }
  else if (adom_version == 12021) {
    RNG_ADDR = 0x082f3d40;
  }
  else if (adom_version == 12022) {
    RNG_ADDR = 0x082f5d60;
  }
  else if (adom_version == 12023) {
    RNG_ADDR = 0x082f6dc0;
  }
  else if (adom_version == 12048) {
    RNG_ADDR = 0x083316a0;
  }
  else if (adom_version == 12049) {
    RNG_ADDR = 0x08332c60;
  }
  else if (adom_version == 12051) {
    RNG_ADDR = 0x0845a000;
  }
  else if (adom_version == 12055) {
    RNG_ADDR = 0x0845af80;
  }
  else if (adom_version == 12059) {
    RNG_ADDR = 0x0846d540;
  }
  else {
    printf("Don't know where to inject roller. Unknown ADOM version %i ?\n", adom_version);
    return 2;
  }

  if (BETTER_THAN(BEST_ROLLS-1)) {
    int pos = BEST_ROLLS-1;

    while (pos-1 >= 0 && BETTER_THAN(pos-1)) {
      memcpy(best_rolls+pos, best_rolls+pos-1, sizeof(RollResult));
      pos--;
    }

    memcpy(best_rolls[pos].rng, (void*)RNG_ADDR, 0x100);
    memcpy(best_rolls[pos].stats, shm->stats, 0x9*sizeof(int));
    memcpy(best_rolls[pos].items, shm->items, (0x2b9+0x2f)*sizeof(int));
    best_rolls[pos].talents = shm->talents;
  }

  if(!best_rolls[0].items[item_requirements-1]) return 0;
  for (int i=0; i < 0x9; i++)
    if (best_rolls[0].stats[i] < stat_requirements[i])
      return 0;

  return 1;
}

static void reset_best(RollResult *best_rolls) {
  for (int i=0; i < BEST_ROLLS; i++) {
    for (int j=0; j < 0x9; j++)
      best_rolls[i].stats[j] = -1;
    for (int j=0; j < 0x2b9+0x2f; j++)
      best_rolls[i].items[j] = 0;
    }
}

void roll_start() {
  uint32_t RESUME_ADDR, RNG_ADDR = 0;
  int adom_version = get_version();

  if (adom_version == 111) {
    RNG_ADDR = 0x082ada40;
    RESUME_ADDR = 0x08073970;
  }
  else if (adom_version == 12018) {
    RNG_ADDR = 0x082CEBA0;
    RESUME_ADDR = 0x0807C7B0;
  }
  else if (adom_version == 12020) {
    RNG_ADDR = 0x082cf6c0;
    RESUME_ADDR = 0x0807c820;
  }
  else if (adom_version == 12021) {
    RNG_ADDR = 0x082f3d40;
    RESUME_ADDR = 0x0807eb70;
  }
  else if (adom_version == 12022) {
    RNG_ADDR = 0x082f5d60;
    RESUME_ADDR = 0x0807f000;
  }
  else if (adom_version == 12023) {
    RNG_ADDR = 0x082f6dc0;
    RESUME_ADDR = 0x0807f1a0;
  }
  else if (adom_version == 12048) {
    RNG_ADDR = 0x083316a0;
    RESUME_ADDR = 0x080817b0;
  }
  else if (adom_version == 12049) {
    RNG_ADDR = 0x08332c60;
    RESUME_ADDR = 0x08081840;
  }
  else if (adom_version == 12051) {
    RNG_ADDR = 0x0845a000;
    RESUME_ADDR = 0x08089310;
  }
  else if (adom_version == 12055) {
    RNG_ADDR = 0x0845af80;
    RESUME_ADDR = 0x080897c0;
  }
  else if (adom_version == 12059) {
    RNG_ADDR = 0x0846d540;
    RESUME_ADDR = 0x0808c160;
  }
  else {
    printf("Don't know where to inject roller. Unknown ADOM version %i ?\n", adom_version);
    return;
  }

  unsigned int rolln = 0;
  unsigned int max_rolls = MAX_ROLLS;
  RollResult best_rolls[BEST_ROLLS];
  unsigned char roll_failed = 0;

  shm = (RollerShared*) shm_init( sizeof(RollerShared) );
  srand(time(NULL));

  // initial roll
  reset_best(best_rolls);
  if (!try_fork()) { child(); return; }

  if (wait(NULL) == -1) {
    perror("wait");
    exit(1);
  }

  process_roll_result(best_rolls);
  roll_failed = 2;

  // main roller
  while (!config_roller(best_rolls, roll_failed, &max_rolls)) {
    rolln = 0;
    reset_best(best_rolls);

    struct timeval begin;
    gettimeofday(&begin, NULL);

    for (/*ahh*/; rolln < max_rolls; rolln++) {
      reseed_rng();
      if (!(rolln % 100)) display_roll_status(best_rolls, rolln, max_rolls, begin);

      if (!try_fork()) { child(); return; }

      if (wait(NULL) == -1) {
        perror("wait");
        exit(1);
      }

      if (process_roll_result(best_rolls)) break;
    }

    roll_failed = (rolln == max_rolls);
    memcpy((void*)RNG_ADDR, best_rolls[0].rng, 0x100);
  }

  shm_deinit(shm);

  // resume ADoM
  printf("\033[2J\033[3;3H");
  fflush(stdout);
  ((void(*)())RESUME_ADDR)();
};
