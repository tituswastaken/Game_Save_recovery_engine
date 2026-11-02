

#ifndef GAME_H
#define GAME_H

#include <stdio.h>

#define NAME_LEN    24      /* names are printed with "%.23s" so a damaged
                               name can never be read past its end */
#define LABEL_LEN   40
#define MAX_ITEMS    8
#define NUM_QUESTS   6
#define MAX_HP     100
#define MAX_GOLD   999999
#define TRIE_K      27      /* a-z and '_' */

/* quest status */
enum { Q_LOCKED = 0, Q_ACTIVE = 1, Q_DONE = 2 };

/* corruption types */
enum { C_BYTES = 1, C_STATS, C_QUEST, C_ITEM };


typedef struct {
    char name[NAME_LEN];
    int  qty;
} Item;

typedef struct {
    char         player[NAME_LEN];
    int          hp, gold;
    int          item_count;
    Item         items[MAX_ITEMS];
    int          quest[NUM_QUESTS];   /* Q_LOCKED / Q_ACTIVE / Q_DONE */
    unsigned int checksum;            /* hash of everything else */
} GameState;

typedef struct {
    int       ts;                     /* key: in-game time */
    char      label[LABEL_LEN];
    GameState st;
} Checkpoint;

typedef struct AVLNode {
    Checkpoint cp;
    int h;                            /* height of this subtree */
    struct AVLNode *l, *r;
} AVLNode;

typedef void (*CpVisit)(Checkpoint *cp, void *ctx);

/* Member 2: Trie node */
typedef struct TrieNode {
    struct TrieNode *ch[TRIE_K];
    int end;        /* 1 if a word ends here */
    int value;      /* catalog: max stack size, inventory: quantity */
} TrieNode;

/* Member 3: Graph (adjacency lists) */
typedef struct Adj { int v; struct Adj *next; } Adj;

typedef struct {
    Adj *out[NUM_QUESTS];   /* u -> quests it unlocks */
    Adj *in[NUM_QUESTS];    /* v <- its prerequisites */
} Graph;

/* The whole running game */
typedef struct {
    GameState cur;      /* live game */
    AVLNode  *cps;      /* checkpoint timeline */
    int       clock;    /* in-game time, always increasing */
} Engine;


extern TrieNode   *g_catalog;                  /* inventory.c */
extern Graph       g_graph;                    /* game.c */
extern const char *QUEST_NAME[NUM_QUESTS];     /* game.c */


AVLNode    *avl_insert(AVLNode *n, const Checkpoint *cp);
Checkpoint *avl_find(AVLNode *n, int ts);
Checkpoint *avl_floor(AVLNode *n, int ts);
void        avl_inorder(AVLNode *n, CpVisit fn, void *ctx);
int         avl_count(const AVLNode *n);
int         avl_height(const AVLNode *n);
void        avl_free(AVLNode *n);

int  create_checkpoint(Engine *e, const char *label);   /* CREATE CHECKPOINT */
void list_checkpoints(const Engine *e);
int  rollback_to(Engine *e, int t);                     /* ROLLBACK */

int  rnd(int lo, int hi);
void banner(const char *title);
void read_line(const char *prompt, char *buf, int n);
int  read_int(const char *prompt, int def);
void print_state(const GameState *s);

TrieNode *trie_new(void);
void      trie_insert(TrieNode *root, const char *key, int value);
int       trie_get(TrieNode *root, const char *key, int *value);
int       trie_prefix(TrieNode *root, const char *prefix);
void      trie_free(TrieNode *n);
void      catalog_init(void);

int  inv_add(GameState *s, const char *name, int qty);
void search_inventory(const GameState *s, const char *prefix);
void search_catalog(const char *prefix);

int  compare_saves(const Engine *e, int ta, int tb);    /* COMPARE SAVES */


void graph_init(void);
void graph_free(void);
int  graph_has_cycle(void);
int  missing_prereq(const GameState *s, int q);
void show_quest_map(const GameState *s);

int         quests_done(const GameState *s);
const char *qstat(int st);

void engine_init(Engine *e, const char *name);
void act_explore(Engine *e);
int  act_pickup(Engine *e, const char *name, int qty);
int  act_quest(Engine *e, int q);


unsigned int state_hash(const GameState *s);
void         seal(GameState *s);
int          validate_state(const GameState *s, int verbose);

int  save_game(Engine *e);                              /* SAVE GAME */
int  load_game(Engine *e);                              /* LOAD GAME */
void corrupt_state(GameState *s, int type);             /* CORRUPT SAVE */
int  corrupt_file(int type);
int  corrupt_checkpoint(Engine *e, int ts, int type);
void corrupt_menu(Engine *e);
void detect_all(const Engine *e);                       /* DETECT CORRUPTION */
int  recover_best(Engine *e);                           /* RECOVER BEST STATE */

#endif 
