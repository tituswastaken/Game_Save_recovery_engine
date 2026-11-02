
#include "game.h"
#include <stdlib.h>
#include <string.h>


int avl_height(const AVLNode *n) { return n ? n->h : 0; }

static void update_height(AVLNode *n)
{
    int a = avl_height(n->l), b = avl_height(n->r);
    n->h = 1 + (a > b ? a : b);
}

static AVLNode *rotate_right(AVLNode *y)
{
    AVLNode *x = y->l;
    y->l = x->r;
    x->r = y;
    update_height(y);
    update_height(x);
    return x;
}

static AVLNode *rotate_left(AVLNode *x)
{
    AVLNode *y = x->r;
    x->r = y->l;
    y->l = x;
    update_height(x);
    update_height(y);
    return y;
}

/* Fix the node if its two subtrees differ in height by more than 1. */
static AVLNode *balance(AVLNode *n)
{
    update_height(n);
    int bf = avl_height(n->l) - avl_height(n->r);
    if (bf > 1) {                                       /* left heavy */
        if (avl_height(n->l->l) < avl_height(n->l->r))
            n->l = rotate_left(n->l);                   /* LR case */
        return rotate_right(n);                         /* LL case */
    }
    if (bf < -1) {                                      /* right heavy */
        if (avl_height(n->r->r) < avl_height(n->r->l))
            n->r = rotate_right(n->r);                  /* RL case */
        return rotate_left(n);                          /* RR case */
    }
    return n;
}

AVLNode *avl_insert(AVLNode *n, const Checkpoint *cp)
{
    if (!n) {
        AVLNode *node = calloc(1, sizeof *node);
        node->cp = *cp;
        node->h = 1;
        return node;
    }
    if (cp->ts < n->cp.ts)      n->l = avl_insert(n->l, cp);
    else if (cp->ts > n->cp.ts) n->r = avl_insert(n->r, cp);
    else                        n->cp = *cp;            /* same time: overwrite */
    return balance(n);
}

Checkpoint *avl_find(AVLNode *n, int ts)
{
    while (n) {
        if (ts == n->cp.ts) return &n->cp;
        n = ts < n->cp.ts ? n->l : n->r;
    }
    return NULL;
}

/* Latest checkpoint with timestamp <= ts (NULL if none). */
Checkpoint *avl_floor(AVLNode *n, int ts)
{
    Checkpoint *best = NULL;
    while (n) {
        if (n->cp.ts == ts) return &n->cp;
        if (n->cp.ts < ts) { best = &n->cp; n = n->r; }  /* candidate, look for later */
        else n = n->l;
    }
    return best;
}

/* Visit every checkpoint in time order, calling fn on each. */
void avl_inorder(AVLNode *n, CpVisit fn, void *ctx)
{
    if (!n) return;
    avl_inorder(n->l, fn, ctx);
    fn(&n->cp, ctx);
    avl_inorder(n->r, fn, ctx);
}

int avl_count(const AVLNode *n) { return n ? 1 + avl_count(n->l) + avl_count(n->r) : 0; }

void avl_free(AVLNode *n)
{
    if (!n) return;
    avl_free(n->l);
    avl_free(n->r);
    free(n);
}

/* Print the tree sideways: right subtree on top, root on the left. */
static void avl_draw(const AVLNode *n, int depth)
{
    if (!n) return;
    avl_draw(n->r, depth + 1);
    printf("    %*s%s t=%d%s\n", depth * 8, "", depth ? "+--" : "ROOT",
           n->cp.ts, validate_state(&n->cp.st, 0) ? " [CORRUPT]" : "");
    avl_draw(n->l, depth + 1);
}


int create_checkpoint(Engine *e, const char *label)
{
    Checkpoint cp;
    if (validate_state(&e->cur, 0)) {
        printf("  Refusing to checkpoint: the live state is corrupted.\n");
        return -1;
    }
    memset(&cp, 0, sizeof cp);
    cp.ts = ++e->clock;
    snprintf(cp.label, LABEL_LEN, "%s", label);
    cp.st = e->cur;
    e->cps = avl_insert(e->cps, &cp);
    printf("  [+] Checkpoint t=%d \"%s\" stored  (AVL: %d nodes, height %d)\n",
           cp.ts, cp.label, avl_count(e->cps), avl_height(e->cps));
    return cp.ts;
}

static void list_row(Checkpoint *cp, void *ctx)
{
    (void)ctx;
    printf("    t=%-5d %-24.24s HP %4d  gold %-5d quests %d/%d  %s\n",
           cp->ts, cp->label, cp->st.hp, cp->st.gold,
           quests_done(&cp->st), NUM_QUESTS,
           validate_state(&cp->st, 0) ? "CORRUPT" : "ok");
}

void list_checkpoints(const Engine *e)
{
    if (!e->cps) { printf("  No checkpoints yet.\n"); return; }
    printf("  Checkpoints (AVL in-order = time order):\n");
    avl_inorder(e->cps, list_row, NULL);
    printf("  %d checkpoints, tree height %d\n", avl_count(e->cps), avl_height(e->cps));
    printf("  Tree structure:\n");
    avl_draw(e->cps, 0);
}


/* Restore the latest VALID checkpoint at or before time t.
 * Each step back is one O(log n) floor search. */
int rollback_to(Engine *e, int t)
{
    Checkpoint *cp = avl_floor(e->cps, t);
    while (cp) {
        if (validate_state(&cp->st, 0) == 0) {
            e->cur = cp->st;
            printf("  Rolled back to t=%d \"%s\"\n", cp->ts, cp->label);
            print_state(&e->cur);
            return cp->ts;
        }
        printf("  skip t=%d \"%s\": corrupted\n", cp->ts, cp->label);
        cp = avl_floor(e->cps, cp->ts - 1);             /* the one before it */
    }
    printf("  No valid checkpoint at or before t=%d.\n", t);
    return -1;
}
