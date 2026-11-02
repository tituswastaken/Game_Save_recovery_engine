
#include "game.h"
#include <stdlib.h>
#include <string.h>


unsigned int state_hash(const GameState *s)
{
    GameState copy = *s;
    copy.checksum = 0;
    const unsigned char *p = (const unsigned char *)&copy;
    unsigned int h = 7;
    for (size_t i = 0; i < sizeof copy; i++) h = h * 31 + p[i];
    return h;
}

void seal(GameState *s) { s->checksum = state_hash(s); }


/* 1 if the name is only a-z and '_' (all the trie can store) and has an end marker. */
static int clean_name(const char *name)
{
    for (int i = 0; i < NAME_LEN; i++) {
        if (name[i] == '\0') return i > 0;
        if (!((name[i] >= 'a' && name[i] <= 'z') || name[i] == '_')) return 0;
    }
    return 0;
}


int validate_state(const GameState *s, int verbose)
{
    int bad = 0, max;

    /* 1. checksum */
    if (state_hash(s) != s->checksum) {
        bad++;
        if (verbose) printf("      [X] checksum mismatch (bytes were damaged)\n");
    }

    /* 2. stats must be possible */
    if (s->hp < 0 || s->hp > MAX_HP) {
        bad++;
        if (verbose) printf("      [X] HP %d outside 0-%d\n", s->hp, MAX_HP);
    }
    if (s->gold < 0 || s->gold > MAX_GOLD) {
        bad++;
        if (verbose) printf("      [X] gold %d is impossible\n", s->gold);
    }

    /* 3. every item must exist (Trie lookup) and fit its stack */
    if (s->item_count < 0 || s->item_count > MAX_ITEMS) {
        bad++;
        if (verbose) printf("      [X] item count %d is impossible\n", s->item_count);
    } else {
        for (int i = 0; i < s->item_count; i++) {
            const Item *it = &s->items[i];
            if (!clean_name(it->name)) {
                bad++;
                if (verbose) printf("      [X] item %d has a damaged name\n", i + 1);
            } else if (!trie_get(g_catalog, it->name, &max)) {
                bad++;
                if (verbose) printf("      [X] unknown item '%s'\n", it->name);
            } else if (it->qty < 1 || it->qty > max) {
                bad++;
                if (verbose) printf("      [X] %s x%d is outside 1-%d (duplication glitch?)\n",
                                    it->name, it->qty, max);
            }
        }
    }

    /* 4. quests must follow the dependency graph */
    for (int q = 0; q < NUM_QUESTS; q++) {
        if (s->quest[q] < Q_LOCKED || s->quest[q] > Q_DONE) {
            bad++;
            if (verbose) printf("      [X] '%s' has an invalid status\n", QUEST_NAME[q]);
        } else if (s->quest[q] != Q_LOCKED && missing_prereq(s, q) >= 0) {
            bad++;
            if (verbose) printf("      [X] '%s' is %s but '%s' is not done\n",
                                QUEST_NAME[q], qstat(s->quest[q]), QUEST_NAME[missing_prereq(s, q)]);
        }
    }
    return bad;
}


#define SAVE_FILE "save.sav"

int save_game(Engine *e)
{
    if (validate_state(&e->cur, 0)) {
        printf("  SAVE REFUSED: the live state is corrupted - saving it would spread the damage.\n");
        validate_state(&e->cur, 1);
        return 0;
    }
    FILE *f = fopen(SAVE_FILE, "wb");
    if (!f) { printf("  Could not open " SAVE_FILE "\n"); return 0; }
    fwrite(&e->cur, sizeof(GameState), 1, f);
    fclose(f);
    printf("  Game saved to " SAVE_FILE " (checksum %08X)\n", e->cur.checksum);
    return 1;
}

int load_game(Engine *e)
{
    GameState st;
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) { printf("  " SAVE_FILE " not found\n"); return 0; }
    int ok = fread(&st, sizeof(GameState), 1, f) == 1;
    fclose(f);
    if (!ok) { printf("  " SAVE_FILE " is too short - the file is damaged\n"); return 0; }
    if (validate_state(&st, 0)) {
        printf("  " SAVE_FILE " is CORRUPTED:\n");
        validate_state(&st, 1);
        return 0;
    }
    e->cur = st;
    printf("  Loaded " SAVE_FILE ":\n");
    print_state(&e->cur);
    return 1;
}


static const char *CORRUPT_NAME[] = {
    "", "damaged bytes", "impossible stats", "quest done out of order", "item glitch"
};

void corrupt_state(GameState *s, int type)
{
    if (type == C_BYTES) {
        /* a disk error: 3 bytes in a row get all their bits flipped,
         * and the checksum is NOT updated */
        unsigned char *p = (unsigned char *)s;
        int start = rand() % ((int)sizeof *s - 3);
        for (int i = start; i < start + 3; i++) p[i] = (unsigned char)~p[i];
        printf("    bytes %d-%d damaged\n", start, start + 2);
        return;
    }
    /* The other three are a buggy game writing wrong data WITH a fresh
     * checksum - so only the rule checks can catch them. */
    if (type == C_STATS) {
        s->hp = MAX_HP * 3;
        printf("    HP set to %d (max is %d)\n", s->hp, MAX_HP);
    } else if (type == C_QUEST) {
        for (int q = NUM_QUESTS - 1; q >= 0; q--)
            if (s->quest[q] != Q_DONE && missing_prereq(s, q) >= 0) {
                s->quest[q] = Q_DONE;
                printf("    '%s' marked done before its prerequisites\n", QUEST_NAME[q]);
                break;
            }
    } else if (type == C_ITEM && s->item_count > 0 && s->item_count < MAX_ITEMS) {
        s->items[0].qty = 9999;
        printf("    '%.23s' duplicated to x9999\n", s->items[0].name);
        Item *fake = &s->items[s->item_count++];
        memset(fake, 0, sizeof *fake);
        strcpy(fake->name, "debug_sword_of_doom");
        fake->qty = 1;
        printf("    fake item 'debug_sword_of_doom' added\n");
    }
    seal(s);
}

int corrupt_file(int type)
{
    GameState st;
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) { printf("  " SAVE_FILE " not found\n"); return 0; }
    int ok = fread(&st, sizeof st, 1, f) == 1;
    fclose(f);
    if (!ok) { printf("  " SAVE_FILE " is already damaged\n"); return 0; }
    printf("  Corrupting " SAVE_FILE " with %s\n", CORRUPT_NAME[type]);
    corrupt_state(&st, type);
    f = fopen(SAVE_FILE, "wb");
    fwrite(&st, sizeof st, 1, f);
    fclose(f);
    return 1;
}

int corrupt_checkpoint(Engine *e, int ts, int type)
{
    Checkpoint *cp = avl_find(e->cps, ts);
    if (!cp) { printf("  No checkpoint at t=%d\n", ts); return 0; }
    printf("  Corrupting checkpoint t=%d \"%s\" with %s\n", ts, cp->label, CORRUPT_NAME[type]);
    corrupt_state(&cp->st, type);
    return 1;
}

void corrupt_menu(Engine *e)
{
    int target = read_int("  Corrupt: 1 a checkpoint  2 a save file  3 the live game: ", 0);
    if (target < 1 || target > 3) return;
    for (int t = C_BYTES; t <= C_ITEM; t++) printf("    %d. %s\n", t, CORRUPT_NAME[t]);
    int type = read_int("  Type: ", C_BYTES);
    if (type < C_BYTES || type > C_ITEM) type = C_BYTES;
    if (target == 1) {
        list_checkpoints(e);
        corrupt_checkpoint(e, read_int("  Checkpoint time t=", -1), type);
    } else if (target == 2) {
        corrupt_file(type);
    } else {
        printf("  Corrupting the live game with %s\n", CORRUPT_NAME[type]);
        corrupt_state(&e->cur, type);
    }
}


static void report_row(Checkpoint *cp, void *ctx)
{
    int *corrupt = ctx;
    int bad = validate_state(&cp->st, 0);
    printf("    t=%-5d %-24.24s %s\n", cp->ts, cp->label, bad ? "CORRUPT" : "ok");
    if (bad) { (*corrupt)++; validate_state(&cp->st, 1); }
}

void detect_all(const Engine *e)
{
    int corrupt = 0;
    printf("  Checkpoints:\n");
    avl_inorder(e->cps, report_row, &corrupt);
    printf("  -> %d of %d checkpoints corrupted\n", corrupt, avl_count(e->cps));

    printf("\n  Live game: %s\n", validate_state(&e->cur, 0) ? "CORRUPT" : "ok");
    validate_state(&e->cur, 1);

    GameState st;
    FILE *f = fopen(SAVE_FILE, "rb");
    if (!f) return;                                    /* nothing saved yet */
    int ok = fread(&st, sizeof st, 1, f) == 1;
    fclose(f);
    if (!ok) { printf("\n  Save file: CORRUPT (too short)\n"); return; }
    printf("\n  Save file: %s\n", validate_state(&st, 0) ? "CORRUPT" : "ok");
    validate_state(&st, 1);
}


typedef struct { int score; int ts; } HeapItem;
typedef struct { HeapItem a[4096]; int n; } Heap;

static void swap(HeapItem *x, HeapItem *y) { HeapItem t = *x; *x = *y; *y = t; }

static void heap_push(Heap *h, HeapItem it)
{
    int i = h->n++;
    h->a[i] = it;
    while (i > 0 && h->a[(i - 1) / 2].score < h->a[i].score) {   /* sift up */
        swap(&h->a[i], &h->a[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}

static HeapItem heap_pop(Heap *h)
{
    HeapItem top = h->a[0];
    h->a[0] = h->a[--h->n];
    int i = 0;
    for (;;) {                                                     /* sift down */
        int l = 2 * i + 1, r = 2 * i + 2, big = i;
        if (l < h->n && h->a[l].score > h->a[big].score) big = l;
        if (r < h->n && h->a[r].score > h->a[big].score) big = r;
        if (big == i) break;
        swap(&h->a[i], &h->a[big]);
        i = big;
    }
    return top;
}

/* How good is this checkpoint to resume from? Progress counts, danger costs. */
static int recovery_score(const GameState *s)
{
    int score = quests_done(s) * 1000 + s->gold + s->item_count * 20;
    if (s->hp < MAX_HP / 4) score -= 1000;             /* nearly dead */
    return score;
}


static void push_if_valid(Checkpoint *cp, void *ctx)
{
    Heap *h = ctx;
    if (validate_state(&cp->st, 0) == 0 && h->n < 4096) {
        HeapItem it = { recovery_score(&cp->st), cp->ts };
        heap_push(h, it);
    }
}

int recover_best(Engine *e)
{
    static Heap h;                       /* static: too big for the stack */
    h.n = 0;
    avl_inorder(e->cps, push_if_valid, &h);
    printf("  %d of %d checkpoints are valid and go into the max-heap\n", h.n, avl_count(e->cps));
    if (h.n == 0) { printf("  Nothing to recover from.\n"); return -1; }

    printf("  Ranking (score = 1000*quests + gold + 20*items, -1000 if HP is below 25):\n");
    HeapItem best = heap_pop(&h);
    HeapItem it = best;
    for (int rank = 1; rank <= 3; rank++) {
        Checkpoint *cp = avl_find(e->cps, it.ts);
        printf("   %s#%d  t=%-5d %-24.24s score %d\n", rank == 1 ? ">" : " ", rank, cp->ts, cp->label, it.score);
        if (h.n == 0) break;
        it = heap_pop(&h);
    }
    Checkpoint *cp = avl_find(e->cps, best.ts);
    e->cur = cp->st;
    printf("  Recovered best state: t=%d \"%s\"\n", cp->ts, cp->label);
    print_state(&e->cur);
    return best.ts;
}
