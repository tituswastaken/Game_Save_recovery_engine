
#include "game.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

Graph g_graph;

const char *QUEST_NAME[NUM_QUESTS] = {
    "Wake in Riverwood",     /* 0 */
    "Find the Old Sword",    /* 1 */
    "Clear the Goblin Cave", /* 2 */
    "Consult the Oracle",    /* 3 */
    "Forge Dragonbane",      /* 4 */
    "Slay the Dragon"        /* 5 */
};

static const int QUEST_EDGES[][2] = {
    {0, 1}, {0, 2}, {1, 3}, {2, 3}, {1, 4}, {3, 5}, {4, 5}
};
#define NUM_EDGES ((int)(sizeof QUEST_EDGES / sizeof QUEST_EDGES[0]))


static void add_to_list(Adj **head, int v)
{
    Adj *a = malloc(sizeof *a);
    a->v = v;
    a->next = *head;
    *head = a;
}

void graph_init(void)
{
    for (int i = 0; i < NUM_EDGES; i++) {
        int u = QUEST_EDGES[i][0], v = QUEST_EDGES[i][1];
        add_to_list(&g_graph.out[u], v);
        add_to_list(&g_graph.in[v], u);
    }
}

void graph_free(void)
{
    for (int u = 0; u < NUM_QUESTS; u++) {
        for (Adj *a = g_graph.out[u], *n; a; a = n) { n = a->next; free(a); }
        for (Adj *a = g_graph.in[u],  *n; a; a = n) { n = a->next; free(a); }
    }
}


static int dfs_cycle(int u, int *color)
{
    color[u] = 1;
    for (Adj *a = g_graph.out[u]; a; a = a->next) {
        if (color[a->v] == 1) return 1;
        if (color[a->v] == 0 && dfs_cycle(a->v, color)) return 1;
    }
    color[u] = 2;
    return 0;
}

int graph_has_cycle(void)
{
    int color[NUM_QUESTS] = {0};
    for (int u = 0; u < NUM_QUESTS; u++)
        if (color[u] == 0 && dfs_cycle(u, color)) return 1;
    return 0;
}

static int topo_order(int *order)
{
    int indeg[NUM_QUESTS] = {0}, queue[NUM_QUESTS], head = 0, tail = 0, k = 0;
    for (int u = 0; u < NUM_QUESTS; u++)
        for (Adj *a = g_graph.out[u]; a; a = a->next) indeg[a->v]++;
    for (int u = 0; u < NUM_QUESTS; u++)
        if (indeg[u] == 0) queue[tail++] = u;
    while (head < tail) {
        int u = queue[head++];
        order[k++] = u;
        for (Adj *a = g_graph.out[u]; a; a = a->next)
            if (--indeg[a->v] == 0) queue[tail++] = a->v;
    }
    return k;
}

int missing_prereq(const GameState *s, int q)
{
    for (Adj *a = g_graph.in[q]; a; a = a->next)
        if (s->quest[a->v] != Q_DONE) return a->v;
    return -1;
}

void show_quest_map(const GameState *s)
{
    int order[NUM_QUESTS];
    int k = topo_order(order);
    printf("  Quest graph: %d quests, %d edges, %s\n", NUM_QUESTS, NUM_EDGES,
           graph_has_cycle() ? "HAS A CYCLE" : "no cycles");
    printf("  A valid order to play (topological sort):\n");
    for (int i = 0; i < k; i++) {
        int q = order[i];
        printf("    %d. %-22s [%-6s] needs: ", i + 1, QUEST_NAME[q], qstat(s->quest[q]));
        if (!g_graph.in[q]) printf("-");
        for (Adj *a = g_graph.in[q]; a; a = a->next)
            printf("%s%s", QUEST_NAME[a->v], a->next ? ", " : "");
        printf("\n");
    }
}


int quests_done(const GameState *s)
{
    int c = 0;
    for (int q = 0; q < NUM_QUESTS; q++)
        if (s->quest[q] == Q_DONE) c++;
    return c;
}

const char *qstat(int st)
{
    if (st == Q_LOCKED) return "locked";
    if (st == Q_ACTIVE) return "active";
    if (st == Q_DONE)   return "done";
    return "INVALID";
}


void engine_init(Engine *e, const char *name)
{
    memset(e, 0, sizeof *e);
    GameState *s = &e->cur;
    snprintf(s->player, NAME_LEN, "%s", name);
    s->hp = MAX_HP;
    s->gold = 50;
    s->quest[0] = Q_ACTIVE;
    inv_add(s, "health_potion", 2);
    seal(s);
}

static void tick(Engine *e) { e->clock += rnd(5, 20); }

void act_explore(Engine *e)
{
    GameState *s = &e->cur;
    int gold = rnd(10, 50), dmg = rnd(0, 20);
    s->gold += gold;
    s->hp -= dmg;
    if (s->hp < 1) s->hp = 1;
    printf("  Explored: +%d gold, -%d HP\n", gold, dmg);
    seal(s);
    tick(e);
}

int act_pickup(Engine *e, const char *name, int qty)
{
    if (!inv_add(&e->cur, name, qty)) {
        printf("  Could not pick up '%s' (unknown item or bag full)\n", name);
        return 0;
    }
    printf("  Picked up %d x %s\n", qty, name);
    seal(&e->cur);
    tick(e);
    return 1;
}


int act_quest(Engine *e, int q)
{
    GameState *s = &e->cur;
    if (q < 0 || q >= NUM_QUESTS || s->quest[q] != Q_ACTIVE) {
        printf("  That quest is not available right now.\n");
        return 0;
    }
    s->quest[q] = Q_DONE;
    s->gold += 50;
    printf("  Quest complete: %s (+50 gold)\n", QUEST_NAME[q]);
    for (Adj *a = g_graph.out[q]; a; a = a->next) {
        int v = a->v;
        if (s->quest[v] == Q_LOCKED && missing_prereq(s, v) < 0) {
            s->quest[v] = Q_ACTIVE;
            printf("  >> New quest unlocked: %s\n", QUEST_NAME[v]);
        }
    }
    seal(s);
    tick(e);
    return 1;
}

static void run_demo(void)
{
    Engine e;
    srand(7);

    banner("STEP 1 - NEW GAME, PLAY, CREATE CHECKPOINTS");
    engine_init(&e, "Aria");
    print_state(&e.cur);
    act_explore(&e);
    act_quest(&e, 0);
    int tA = create_checkpoint(&e, "Left the village");
    act_quest(&e, 1);
    act_explore(&e);
    int tB = create_checkpoint(&e, "Found the sword");
    act_quest(&e, 2);
    act_pickup(&e, "torch", 2);
    int tC = create_checkpoint(&e, "Cleared the cave");
    act_quest(&e, 3);
    act_explore(&e);
    int tD = create_checkpoint(&e, "Met the oracle");
    e.cur.hp = 10;                                  /* a monster attack leaves Aria badly hurt */
    seal(&e.cur);
    printf("  !! A monster attack leaves Aria at %d/%d HP\n", e.cur.hp, MAX_HP);
    int tE = create_checkpoint(&e, "Hurt by a monster");
    act_quest(&e, 4);
    int tF = create_checkpoint(&e, "Forged Dragonbane");
    printf("\n");
    list_checkpoints(&e);

    banner("STEP 2 - SAVE GAME");
    save_game(&e);

    banner("STEP 3 - TRIE: inventory + catalog search");
    search_inventory(&e.cur, "");
    search_catalog("dr");

    banner("STEP 4 - GRAPH: quest dependencies");
    show_quest_map(&e.cur);

    banner("STEP 5 - CORRUPT THE SAVE FILE, then LOAD GAME");
    corrupt_file(C_BYTES);
    if (!load_game(&e)) printf("  -> the save file is unusable; checkpoints will be used instead.\n");

    banner("STEP 6 - CORRUPT THREE CHECKPOINTS");
    corrupt_checkpoint(&e, tB, C_QUEST);
    corrupt_checkpoint(&e, tC, C_ITEM);
    corrupt_checkpoint(&e, tF, C_BYTES);

    banner("STEP 7 - DETECT CORRUPTION");
    detect_all(&e);

    banner("STEP 8 - COMPARE SAVES");
    compare_saves(&e, tA, tD);
    printf("\n");
    compare_saves(&e, tC, tD);

    banner("STEP 9 - ROLLBACK to the latest valid checkpoint");
    rollback_to(&e, e.clock);
    printf("  (Rollback takes the most RECENT valid state - even if it is a bad spot.)\n");

    banner("STEP 10 - RECOVER BEST STATE (max-heap)");
    recover_best(&e);
    printf("  (The heap prefers t=%d over t=%d: same progress, but not nearly dead.)\n", tD, tE);

    banner("STEP 11 - SAVE GUARD: a corrupted live state is never saved");
    corrupt_state(&e.cur, C_STATS);
    save_game(&e);
    recover_best(&e);
    save_game(&e);

    avl_free(e.cps);
    printf("\nDemo complete.\n");
}


static void play_menu(Engine *e)
{
    char name[NAME_LEN];
    printf("  1 Explore   2 Pick up item   3 Complete a quest\n");
    switch (read_int("  Action: ", 0)) {
    case 1: act_explore(e); break;
    case 2:
        read_line("  Item name (e.g. health_potion): ", name, sizeof name);
        if (trie_get(g_catalog, name, NULL)) act_pickup(e, name, read_int("  Quantity [1]: ", 1));
        else { printf("  No such item. Did you mean:\n"); search_catalog(name); }
        break;
    case 3:
        for (int q = 0; q < NUM_QUESTS; q++)
            if (e->cur.quest[q] == Q_ACTIVE) printf("    %d. %s\n", q, QUEST_NAME[q]);
        act_quest(e, read_int("  Quest number: ", -1));
        break;
    }
}

static void interactive(void)
{
    Engine e;
    char buf[64];
    read_line("Player name [Hero]: ", buf, sizeof buf);
    engine_init(&e, buf[0] ? buf : "Hero");

    for (;;) {
        const GameState *s = &e.cur;
        banner("GAME SAVE CORRUPTION & RECOVERY ENGINE");
        printf("  %.23s  HP %d/%d  Gold %d  Quests %d/%d  t=%d  checkpoints %d  [%s]\n",
               s->player, s->hp, MAX_HP, s->gold, quests_done(s), NUM_QUESTS,
               e.clock, avl_count(e.cps), validate_state(s, 0) ? "STATE CORRUPT" : "state ok");
        printf("   1. Play                       7. Quest map (Graph)\n");
        printf("   2. SAVE GAME                  8. CORRUPT SAVE\n");
        printf("   3. LOAD GAME                  9. DETECT CORRUPTION\n");
        printf("   4. CREATE CHECKPOINT         10. ROLLBACK\n");
        printf("   5. List checkpoints (AVL)    11. COMPARE SAVES\n");
        printf("   6. Search items (Trie)       12. RECOVER BEST STATE\n");
        printf("   0. Exit\n");
        int choice = read_int("  Choice: ", -1);
        printf("\n");
        switch (choice) {
        case 0: avl_free(e.cps); return;
        case 1: play_menu(&e); break;
        case 2: save_game(&e); break;
        case 3:
            if (!load_game(&e)) {
                read_line("  Recover the best checkpoint instead? (y/n): ", buf, sizeof buf);
                if (buf[0] == 'y' || buf[0] == 'Y') recover_best(&e);
            }
            break;
        case 4:
            read_line("  Label: ", buf, sizeof buf);
            create_checkpoint(&e, buf[0] ? buf : "Manual checkpoint");
            break;
        case 5: list_checkpoints(&e); break;
        case 6: {
            char name[NAME_LEN];
            int where = read_int("  1 my inventory  2 all game items [1]: ", 1);
            read_line("  Starts with (empty = all): ", name, sizeof name);
            if (where == 2) search_catalog(name); else search_inventory(&e.cur, name);
            break;
        }
        case 7: show_quest_map(&e.cur); break;
        case 8: corrupt_menu(&e); break;
        case 9: detect_all(&e); break;
        case 10:
            list_checkpoints(&e);
            rollback_to(&e, read_int("  Roll back to time t= [latest]: ", e.clock));
            break;
        case 11: {
            list_checkpoints(&e);
            int a = read_int("  First  (t=, 0 = live game): ", 0);
            int b = read_int("  Second (t=, 0 = live game): ", 0);
            compare_saves(&e, a, b);
            break;
        }
        case 12: recover_best(&e); break;
        default: printf("  Unknown option.\n"); break;
        }
    }
}



int main(int argc, char **argv)
{
    srand((unsigned)time(NULL));
    catalog_init();
    graph_init();
    if (graph_has_cycle()) {
        printf("Quest graph has a cycle - the game could never be finished.\n");
        return 1;
    }
    if (argc > 1 && strcmp(argv[1], "--demo") == 0) run_demo();
    else interactive();
    trie_free(g_catalog);
    graph_free();
    return 0;
}
