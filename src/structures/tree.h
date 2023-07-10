#pragma once

#ifndef TOMOSRV_TREE_H
#define TOMOSRV_TREE_H


/** @brief Generic binary search tree for your future mapping pleasure */
typedef struct tomo_tree {
    struct tomo_tree *next[2];
} TOMO_TREE;


/** @brief Comparison function between tree nodes. Note that they are not const,
 *      just in case you get any wacky ideas later
 *  @returns EXACTLY -1 if n1 < n2, EXACTLY +1 if n1 > n2, and EXACTLY 0 if eq
 */
typedef int TOMO_TREE_CMPPROC(struct tomo_tree *n1, struct tomo_tree *n2);


/** @brief Insert @p node into the tree rooted at @p root, using @p cmpfn to
 *      impose a total ordering on the nodes
 *  @param root
 *      Pointer to root pointer, updated if need be
 *  @param node
 *      Node to be inserted. The memory for this is externally allocated. Write
 *      a thunk! Unless you figure out some wacky shit l8ter, this should
 *      contain NULL pointers as its children
 *  @param cmpfn
 *      Comparison function between two nodes
 *  @returns Nonzero if @p node was already in the tree
 */
int tomo_tree_insert(TOMO_TREE        **root,
                     TOMO_TREE         *node,
                     TOMO_TREE_CMPPROC *cmpfn);


/** @brief Remove @p node from the tree rooted at @p root, using @p cmpfn to
 *      establish a total ordering
 *  @param root
 *      Root node pointer
 *  @param node
 *      Node to be removed. Note that the ordering is NOT established by the
 *      values of the pointers, so this node does not have to be the same object
 *      as the node being removed
 *  @param cmpfn
 *      Comparison function
 *  @returns A "unique" (not in the tree anymore) pointer to the node that was
 *      removed. Remember that this memory is externally managed, so you likely
 *      will want to free(3) this
 */
TOMO_TREE *tomo_tree_remove(TOMO_TREE        **root,
                            TOMO_TREE         *node,
                            TOMO_TREE_CMPPROC *cmpfn);


#endif /* TOMOSRV_TREE_H */
