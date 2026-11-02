
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

TrieNode *g_catalog = NULL;

/* Every item that exists in the game, with how many fit in one stack. */
static const struct { const char *name; int max_stack; } CATALOG[] = {
    {"health_potion", 10}, {"bread", 20},     {"torch", 5},         {"arrows", 50},
    {"old_sword", 1},      {"dragonbane", 1}, {"dragon_scale", 10}, {"gold_key", 3}
};
#define CATALOG_SIZE ((int)(sizeof CATALOG / sizeof CATALOG[0]))


int rnd(int lo, int hi) { return lo + rand() % (hi - lo + 1); }

void banner(const char *title)
{
    printf("\n==================================================================\n");
    printf("  %s\n", title);
    printf("==================================================================\n");
}

void read_line(const char *prompt, char *buf, int n)
{
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, n, stdin)) exit(0);         /* input closed: quit */
    buf[strcspn(buf, "\r\n")] = '\0';
}

int read_int(const char *prompt, int def)
{
    char b[32];
    read_line(prompt, b, sizeof b);
    if (!isdigit((unsigned char)b[0]) && b[0] != '-') return def;   /* empty or not a number */
    return atoi(b);
}

void print_state(const GameState *s)
{
    printf("    %.23s | HP %d/%d | Gold %d\n", s->player, s->hp, MAX_HP, s->gold);
    printf("    Inventory:");
    if (s->item_count < 0 || s->item_count > MAX_ITEMS) printf(" <damaged>");
    else
        for (int i = 0; i < s->item_count; i++)
            printf(" %.23s x%d%s", s->items[i].name, s->items[i].qty,
                   i < s->item_count - 1 ? "," : "");
    printf("\n    Quests done:");
    for (int q = 0; q < NUM_QUESTS; q++)
        if (s->quest[q] == Q_DONE) printf(" [%s]", QUEST_NAME[q]);
    printf("\n");
}


static int trie_idx(char c)
{
    if (c >= 'a' && c <= 'z') return c - 'a';
    if (c == '_') return 26;
    return -1;
}

TrieNode *trie_new(void) { return calloc(1, sizeof(TrieNode)); }

void trie_insert(TrieNode *root, const char *key, int value)
{
    TrieNode *n = root;
    for (; *key; key++) {
        int i = trie_idx(*key);
        if (i < 0) return;                      /* not a valid item character */
        if (!n->ch[i]) n->ch[i] = trie_new();
        n = n->ch[i];
    }
    n->end = 1;
    n->value = value;
}

static TrieNode *walk(TrieNode *n, const char *key)
{
    for (; n && *key; key++) {
        int i = trie_idx(*key);
        if (i < 0) return NULL;
        n = n->ch[i];
    }
    return n;
}

int trie_get(TrieNode *root, const char *key, int *value)
{
    TrieNode *n = walk(root, key);
    if (!n || !n->end) return 0;
    if (value) *value = n->value;
    return 1;
}

static int print_words(TrieNode *n, char *buf, int depth)
{
    int count = 0;
    if (n->end) {
        buf[depth] = '\0';
        printf("    %-16s %d\n", buf, n->value);
        count++;
    }
    for (int i = 0; i < TRIE_K && depth < NAME_LEN - 2; i++)
        if (n->ch[i]) {
            buf[depth] = (char)(i < 26 ? 'a' + i : '_');
            count += print_words(n->ch[i], buf, depth + 1);
        }
    return count;
}

int trie_prefix(TrieNode *root, const char *prefix)
{
    char buf[NAME_LEN];
    TrieNode *n = walk(root, prefix);
    if (!n) return 0;
    strcpy(buf, prefix);
    return print_words(n, buf, (int)strlen(prefix));
}

void trie_free(TrieNode *n)
{
    if (!n) return;
    for (int i = 0; i < TRIE_K; i++) trie_free(n->ch[i]);
    free(n);
}

void catalog_init(void)
{
    g_catalog = trie_new();
    for (int i = 0; i < CATALOG_SIZE; i++)
        trie_insert(g_catalog, CATALOG[i].name, CATALOG[i].max_stack);
}

static int inv_find(const GameState *s, const char *name)
{
    for (int i = 0; i < s->item_count; i++)
        if (strcmp(s->items[i].name, name) == 0) return i;
    return -1;
}

int inv_add(GameState *s, const char *name, int qty)
{
    int max, i;
    if (qty < 1 || !trie_get(g_catalog, name, &max)) return 0;
    i = inv_find(s, name);
    if (i < 0) {
        if (s->item_count >= MAX_ITEMS) return 0;
        i = s->item_count++;
        memset(&s->items[i], 0, sizeof s->items[i]);
        strcpy(s->items[i].name, name);
    }
    s->items[i].qty += qty;
    if (s->items[i].qty > max) s->items[i].qty = max;   /* stack limit */
    return 1;
}

void search_inventory(const GameState *s, const char *prefix)
{
    TrieNode *t = trie_new();
    for (int i = 0; i < s->item_count; i++) trie_insert(t, s->items[i].name, s->items[i].qty);
    printf("  Your items starting with \"%s\" (name, quantity):\n", prefix);
    if (!trie_prefix(t, prefix)) printf("    (none)\n");
    trie_free(t);
}

void search_catalog(const char *prefix)
{
    printf("  Game items starting with \"%s\" (name, max stack):\n", prefix);
    if (!trie_prefix(g_catalog, prefix)) printf("    (none)\n");
}

static void compare_states(const GameState *a, const GameState *b)
{
    printf("    %-20s | %-12s | %-12s\n", "field", "first", "second");
    printf("    ---------------------+--------------+--------------\n");
    printf("    %-20s | %-12d | %-12d\n", "hp", a->hp, b->hp);
    printf("    %-20s | %-12d | %-12d\n", "gold", a->gold, b->gold);
    printf("    %-20s | %-12d | %-12d\n", "item types", a->item_count, b->item_count);
    printf("    %-20s | %-12d | %-12d\n", "quests done", quests_done(a), quests_done(b));
    for (int q = 0; q < NUM_QUESTS; q++)
        if (a->quest[q] != b->quest[q])
            printf("    quest %-14.14s | %-12s | %-12s\n", QUEST_NAME[q], qstat(a->quest[q]), qstat(b->quest[q]));

    printf("    %-20s | %-12s | %-12s\n", "integrity",
           validate_state(a, 0) ? "CORRUPT" : "ok", validate_state(b, 0) ? "CORRUPT" : "ok");
}

int compare_saves(const Engine *e, int ta, int tb)
{
    const GameState *a = &e->cur, *b = &e->cur;
    if (ta) {
        Checkpoint *cp = avl_find(e->cps, ta);
        if (!cp) { printf("  No checkpoint at t=%d\n", ta); return 0; }
        a = &cp->st;
    }
    if (tb) {
        Checkpoint *cp = avl_find(e->cps, tb);
        if (!cp) { printf("  No checkpoint at t=%d\n", tb); return 0; }
        b = &cp->st;
    }
    printf("  Comparing %s%d with %s%d:\n", ta ? "t=" : "live ", ta, tb ? "t=" : "live ", tb);
    compare_states(a, b);
    return 1;
}
