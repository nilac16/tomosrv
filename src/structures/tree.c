#include <stddef.h>
#include "tree.h"


/** @brief Find @p node in the tree, or the pointer to where it should go
 *  @param root
 *      Root node
 *  @param node
 *      Node to find
 *  @param cmpfn
 *      Total ordering thunk
 *  @returns The pointer to the unique pointer to the node itself if found, or
 *      the edge where it should be placed if not
 */
static TOMO_TREE **tomo_tree_find(TOMO_TREE        **root,
                                  TOMO_TREE         *node,
                                  TOMO_TREE_CMPPROC *cmpfn)
{
    int cmp;

    while (*root) {
        cmp = cmpfn(node, *root);
        if (!cmp) {
            break;
        }
        root = &(*root)->next[(cmp + 1) / 2];
    }
    return root;
}


int tomo_tree_insert(TOMO_TREE        **root,
                     TOMO_TREE         *node,
                     TOMO_TREE_CMPPROC *cmpfn)
{
    root = tomo_tree_find(root, node, cmpfn);
    if (*root) {
        return 1;
    }
    *root = node;
    return 0;
}


/** @brief Finds the successor to @p root using the implicit total ordering
 *      given by the tree's TOPOLOGY (no comparison function req'd)
 *  @param root
 *      Node to find the successor to. This pointer's target may not be NULL
 *  @returns The successor to @p root, or NULL if it does not exist. Do not
 *      attempt to dereference this result until checking its single-indirection
 *      value!
 */
static TOMO_TREE **tomo_tree_succ(TOMO_TREE **root)
{
    TOMO_TREE **succ;

    if (!(*root)->next[1]) {
        return NULL;
    }
    succ = &(*root)->next[1];
    while ((*succ)->next[0]) {
        succ = &(*succ)->next[0];
    }
    return succ;
}


TOMO_TREE *tomo_tree_remove(TOMO_TREE        **root,
                            TOMO_TREE         *node,
                            TOMO_TREE_CMPPROC *cmpfn)
{
    TOMO_TREE *res, **succ;

    root = tomo_tree_find(root, node, cmpfn);
    if (!*root) {
        return NULL;
    }
    res = *root;
    succ = tomo_tree_succ(root);
    if (succ) {
        *root = *succ;
        (*root)->next[0] = res->next[0];
        *succ = NULL;
    } else {
        *root = res->next[0];
    }
    res->next[0] = res->next[1] = NULL;
    return res;
}
