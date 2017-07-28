// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef IBELIE_TREE_H__
#define IBELIE_TREE_H__

#include "port.h"

#ifdef __cplusplus
extern "C" {
#endif

/* IblTree_HEAD defines the initial segment of every IblTree. */
#define IblTree_HEAD \
	IBL_ALIGN(2, size_t parent); \
	struct _IblTree* right;      \
	struct _IblTree* left

/* Every pointer to a tree node can be cast to a IblTree. */
typedef struct _IblTree {
	IblTree_HEAD;
} *IblTree;

#define IblTree_Parent(n) ((IblTree)((n)->parent & ~1))

IblAPI(void)    IblTree_Erase   (IblTree*, IblTree);
IblAPI(void)    IblTree_Insert  (IblTree*, IblTree*, IblTree, IblTree);
IblAPI(IblTree) IblTree_Begin   (IblTree);
IblAPI(IblTree) IblTree_End     (IblTree);
IblAPI(IblTree) IblTree_Next    (IblTree);
IblAPI(IblTree) IblTree_Prev    (IblTree);
IblAPI(IblTree) IblTree_PrePost (IblTree);
IblAPI(IblTree) IblTree_Post    (IblTree);

#define IblTree_Search(ROOT, NODE, CMP) {      \
	NODE = ROOT;                               \
	while (NODE) {                             \
		register int SEARCH_CMP = CMP;         \
		if (SEARCH_CMP < 0) {                  \
			NODE = NODE->left;                 \
		} else if (SEARCH_CMP > 0) {           \
			NODE = NODE->right;                \
		} else {                               \
			break;                             \
		}                                      \
	}                                          \
}

#define IblTree_Vacancy(ROOT, LINK, PT, CMP) { \
	LINK = ROOT; PT = NULL;                    \
	while (*LINK) {                            \
		register int SEARCH_CMP = CMP;         \
		if (SEARCH_CMP < 0) {                  \
			PT = *LINK;                        \
			LINK = &((*LINK)->left);           \
		} else if (SEARCH_CMP > 0) {           \
			PT = *LINK;                        \
			LINK = &((*LINK)->right);          \
		} else {                               \
			break;                             \
		}                                      \
	}                                          \
}

#define IblTree_Free(ROOT, FREE) {             \
	IblTree FREE_P;                            \
	register IblTree* NODE = ROOT;             \
	while (*NODE) {                            \
		while ((*NODE)->left) {                \
			NODE = &((*NODE)->left);           \
		}                                      \
		if ((*NODE)->right) {                  \
			NODE = &((*NODE)->right);          \
			continue;                          \
		}                                      \
		FREE_P = IblTree_Parent(*NODE);        \
		{FREE;}                                \
		*NODE = NULL;                          \
		if (!FREE_P) {break;}                  \
		FREE_P = IblTree_Parent(FREE_P);       \
		NODE = FREE_P ? &(FREE_P) : ROOT;      \
	}                                          \
}


#ifdef __cplusplus
}
#endif

#endif /* IBELIE_TREE_H__ */
