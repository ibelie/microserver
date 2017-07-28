// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "map.h"

#define MIN_TABLE_SIZE (8)
#define MAX_CHAIN_LENGTH (8)
#define MAX_LOAD_TIMES_16 (12)

#define _IblMap_Hash(MAP, KEY) (MAP->hash(KEY) + (size_t)MAP) & (MAP->capacity - 1)

#ifdef __cplusplus
extern "C" {
#endif

#define _KEY(item) ((IblMap_Key)&(((IblMap_Item)item)->key))

static void _IblMap_BuildTree(IblMap map, size_t hash) {
	register uint8 i;
	IblTree root = NULL;
	for (i = 0; i < 2; i++) {
		register IblMap_Item item;
		register IblTree next;
		for (item = map->table[hash ^ i]; item; item = (IblMap_Item)next) {
			next = item->right;
			register IblTree *link, parent;
			IblTree_Vacancy(&root, link, parent, SINGLE_ARG(map->compare(_KEY(item), _KEY(*link))));
			if (*link) {
				IblPrint_Err("[Map] Duplicate key when converting list to tree.\n");
				continue;
			}
			IblTree_Insert(&root, link, parent, (IblTree)item);
		}
	}
	map->table[hash] = map->table[hash ^ 1] = (IblMap_Item)root;
}

static void _IblMap_Rehash(IblMap map, IblMap_Item new) {
	IblTree root;
	register size_t hash = _IblMap_Hash(map, _KEY(new)), i;
	register IblMap_Item item = map->table[hash];
	if (!item) {
		new->parent = 1;
		new->left = new->right = NULL;
		map->table[hash] = new;
		return;
	}
	if (item->parent + 1 >= MAX_CHAIN_LENGTH) {
		_IblMap_BuildTree(map, hash);
		item = map->table[hash];
	}
	if (map->table[hash ^ 1] == item) {
		root = (IblTree)item;
		register IblTree *link, parent;
		IblTree_Vacancy(&root, link, parent, SINGLE_ARG(map->compare(_KEY(new), _KEY(*link))));
		if (*link) {
			IblPrint_Err("[Map] Duplicate key in tree when expanding map.\n");
			return;
		}
		IblTree_Insert(&root, link, parent, (IblTree)new);
		map->table[hash] = map->table[hash ^ 1] = (IblMap_Item)root;
		return;
	}
	for (i = 0; i < MAX_CHAIN_LENGTH && item; i++) {
		register int cmp = map->compare(_KEY(new), _KEY(item));
		if (cmp == 0) {
			IblPrint_Err("[Map] Duplicate key in list when expanding map.\n");
			break;
		} else if (cmp < 0) {
			new->left = item->left;
			new->right = (IblTree)item;
			if (item->left) {
				((IblMap_Item)(item->left))->right = (IblTree)new;
			}
			item->left = (IblTree)new;
			if (i == 0) {
				new->parent = item->parent + 1;
				map->table[hash] = new;
			} else {
				map->table[hash]->parent++;
			}
			break;
		} else if (!item->right) {
			new->left = (IblTree)item;
			new->right = NULL;
			item->right = (IblTree)new;
			map->table[hash]->parent++;
			break;
		} else {
			item = (IblMap_Item)item->right;
		}
	}
}

static inline void _IblMap_TreeRehash(IblMap map, IblMap_Item item) {
	register IblTree next;
	for (item = (IblMap_Item)IblTree_PrePost((IblTree)item); item; item = (IblMap_Item)next) {
		next = IblTree_Post((IblTree)item);
		_IblMap_Rehash(map, item);
	}
}

static inline void _IblMap_ListRehash(IblMap map, IblMap_Item item) {
	register IblTree next;
	for (; item; item = (IblMap_Item)next) {
		next = item->right;
		_IblMap_Rehash(map, item);
	}
}

static inline void _IblMap_Expand(IblMap map) {
	register size_t i, capacity = map->capacity;
	register IblMap_Item* old_table = map->table;
	map->capacity <<= 1;
	map->table = (IblMap_Item*)calloc(map->capacity, sizeof(IblMap_Item));
	if (!map->table) {
		map->table = old_table;
		map->capacity >>= 1;
		return;
	}
	for (i = 0; i < capacity; i += 2) {
		register IblMap_Item item = old_table[i];
		if (!item) {
			item = old_table[i ^ 1];
			if (item) { _IblMap_ListRehash(map, item); }
		} else if (old_table[i ^ 1] == item) {
			_IblMap_TreeRehash(map, item);
		} else {
			_IblMap_ListRehash(map, item);
			item = old_table[i ^ 1];
			_IblMap_ListRehash(map, item);
		}
	}
	free(old_table);
}

static IblMap_Item _IblMap_Set(IblMap map, IblMap_Key key) {
	IblTree root;
	register size_t hash = _IblMap_Hash(map, key), i;
	register IblMap_Item item = map->table[hash], new;
	if (!item) {
		new = map->alloc(key);
		if (new) { new->parent = 1; map->size++; }
		map->table[hash] = new;
		return new;
	}
	if (item->parent + 1 >= MAX_CHAIN_LENGTH) {
		_IblMap_BuildTree(map, hash);
		item = map->table[hash];
	}
	if (map->table[hash ^ 1] == item) {
		root = (IblTree)item;
		register IblTree *link, parent;
		IblTree_Vacancy(&root, link, parent, SINGLE_ARG(map->compare(key, _KEY(*link))));
		if (*link) { return (IblMap_Item)(*link); }
		new = map->alloc(key);
		if (!new) { return NULL; }
		map->size++;
		IblTree_Insert(&root, link, parent, (IblTree)new);
		map->table[hash] = map->table[hash ^ 1] = (IblMap_Item)root;
		return new;
	}
	for (i = 0; i < MAX_CHAIN_LENGTH && item; i++) {
		register int cmp = map->compare(key, _KEY(item));
		if (cmp == 0) {
			return item;
		} else if (cmp > 0 && item->right) {
			item = (IblMap_Item)item->right;
			continue;
		}
		new = map->alloc(key);
		if (!new) { return NULL; }
		map->size++;
		if (cmp < 0) {
			new->left = item->left;
			new->right = (IblTree)item;
			if (item->left) {
				((IblMap_Item)(item->left))->right = (IblTree)new;
			}
			item->left = (IblTree)new;
			if (i == 0) {
				new->parent = item->parent + 1;
				map->table[hash] = new;
			} else {
				map->table[hash]->parent++;
			}
			return new;
		}
		new->left = (IblTree)item;
		item->right = (IblTree)new;
		map->table[hash]->parent++;
		return new;
	}
	IblPrint_Err("[Map] List length out of MAX_CHAIN_LENGTH.\n");
	return NULL;
}

static inline void _IblMap_ListFree(IblMap map, IblMap_Item item) {
	register IblTree next;
	for (; item; item = (IblMap_Item)next) {
		next = item->right;
		map->dealloc(item);
	}
}

void IblMap_Clear(IblMap map) {
	register size_t i;
	for (i = 0; i < map->capacity; i += 2) {
		register IblMap_Item item = map->table[i];
		if (!item) {
			item = map->table[i ^ 1];
			if (item) { _IblMap_ListFree(map, item); }
		} else if (map->table[i ^ 1] == item) {
			IblTree_Free((IblTree*)(map->table + i), map->dealloc((IblMap_Item)(*NODE)));
		} else {
			_IblMap_ListFree(map, item);
			item = map->table[i ^ 1];
			_IblMap_ListFree(map, item);
		}
	}
	if (map->table) { free(map->table); map->table = NULL; }
	map->size = 0;
	map->capacity = 0;
}

bool IblMap_Del(IblMap map, IblMap_Key key) {
	IblTree root;
	if (!map->table) { return false; }
	register size_t i, hash = _IblMap_Hash(map, key);
	register IblMap_Item item = map->table[hash];
	if (!item) {
		return false;
	} else if (map->table[hash ^ 1] == item) {
		register IblTree node;
		IblTree_Search((IblTree)item, node, SINGLE_ARG(map->compare(key, _KEY(node))));
		if (!node) { return false; }
		map->size--;
		root = (IblTree)item;
		IblTree_Erase(&root, node);
		map->dealloc((IblMap_Item)node);
		map->table[hash] = map->table[hash ^ 1] = (IblMap_Item)root;
		return true;
	}
	for (i = 0; i < MAX_CHAIN_LENGTH && item; i++) {
		register int cmp = map->compare(key, _KEY(item));
		if (cmp == 0) {
			if (item->left) {
				((IblMap_Item)(item->left))->right = item->right;
				map->table[hash]->parent--;
			} else if (item->right) {
				map->table[hash] = (IblMap_Item)item->right;
				map->table[hash]->parent = item->parent - 1;
			} else {
				map->table[hash] = NULL;
			}
			if (item->right) {
				((IblMap_Item)(item->right))->left = item->left;
			}
			map->size--;
			map->dealloc(item);
			return true;
		} else if (cmp < 0 || !item->right) {
			return false;
		} else {
			item = (IblMap_Item)item->right;
		}
	}
	return false;
}

IblMap_Item IblMap_Get(IblMap map, IblMap_Key key) {
	if (!map->table) { return NULL; }
	register size_t i = _IblMap_Hash(map, key);
	register IblMap_Item item = map->table[i];
	if (!item) {
		return NULL;
	} else if (map->table[i ^ 1] == item) {
		register IblTree node;
		IblTree_Search((IblTree)item, node, SINGLE_ARG(map->compare(key, _KEY(node))));
		return (IblMap_Item)node;
	}
	for (i = 0; i < MAX_CHAIN_LENGTH && item; i++) {
		register int cmp = map->compare(key, _KEY(item));
		if (cmp == 0) {
			return item;
		} else if (cmp < 0 || !item->right) {
			return NULL;
		} else {
			item = (IblMap_Item)item->right;
		}
	}
	return NULL;
}

IblMap_Item IblMap_Set(IblMap map, IblMap_Key key) {
	if (!map->table) {
		map->table = (IblMap_Item*)calloc(MIN_TABLE_SIZE, sizeof(IblMap_Item));
		map->capacity = MIN_TABLE_SIZE;
		if (!map->table) {
			IblPrint_Err("[Map] Alloc map table out of memory %zu.\n", MIN_TABLE_SIZE * sizeof(IblMap_Item));
		}
	} else if (map->size + 1 > map->capacity * MAX_LOAD_TIMES_16 / 16 &&
		map->capacity < (size_t)1 << (sizeof(size_t) >= 8 ? 59 : 27)) {
		_IblMap_Expand(map);
	}
	return _IblMap_Set(map, key);
}

IblMap_Item IblMap_Begin(IblMap map) {
	register size_t i;
	if (!map->table) { return NULL; }
	for (i = 0; i < map->capacity; i += 2) {
		register IblMap_Item item = map->table[i];
		if (!item) {
			item = map->table[i ^ 1];
			if (item) { return item; }
		} else if (map->table[i ^ 1] == item) {
			return (IblMap_Item)IblTree_Begin((IblTree)item);
		} else {
			return item;
		}
	}
	return NULL;
}

IblMap_Item IblMap_End(IblMap map) {
	register size_t i;
	if (!map->table) { return NULL; }
	for (i = map->capacity - 1; i >= 1; i -= 2) {
		register IblMap_Item item = map->table[i];
		if (!item) {
			item = map->table[i ^ 1];
			if (!item) { continue; }
			while (item->right) {
				item = (IblMap_Item)item->right;
			}
			return item;
		} else if (map->table[i ^ 1] == item) {
			return (IblMap_Item)IblTree_End((IblTree)item);
		} else {
			while (item->right) {
				item = (IblMap_Item)item->right;
			}
			return item;
		}
	}
	return NULL;
}

IblMap_Item IblMap_Next(IblMap map, IblMap_Item it) {
	if (!map->table) { return NULL; }
	register size_t i = _IblMap_Hash(map, _KEY(it));
	register IblMap_Item item = map->table[i];
	if (!item) {
		return NULL;
	} else if (map->table[i ^ 1] == item) {
		item = (IblMap_Item)IblTree_Next((IblTree)it);
		if (item) { return item; }
	} else if (it->right) {
		return (IblMap_Item)it->right;
	} else if (!(i & 1) && map->table[i ^ 1]) {
		return map->table[i ^ 1];
	}
	for (i = (i & ~1) + 2; i < map->capacity; i += 2) {
		item = map->table[i];
		if (!item) {
			item = map->table[i ^ 1];
			if (item) { return item; }
		} else if (map->table[i ^ 1] == item) {
			return (IblMap_Item)IblTree_Begin((IblTree)item);
		} else {
			return item;
		}
	}
	return NULL;
}

IblMap_Item IblMap_Prev(IblMap map, IblMap_Item it) {
	if (!map->table) { return NULL; }
	register size_t i = _IblMap_Hash(map, _KEY(it));
	register IblMap_Item item = map->table[i];
	if (!item) {
		return NULL;
	} else if (map->table[i ^ 1] == item) {
		item = (IblMap_Item)IblTree_Prev((IblTree)it);
		if (item) { return item; }
	} else if (it->left) {
		return (IblMap_Item)it->left;
	} else if ((i & 1) && map->table[i ^ 1]) {
		item = map->table[i ^ 1];
		while (item->right) {
			item = (IblMap_Item)item->right;
		}
		return item;
	}
	for (i = (i & ~1) - 1; i >= 1 && i < map->capacity; i -= 2) {
		item = map->table[i];
		if (!item) {
			item = map->table[i ^ 1];
			if (item) {
				while (item->right) {
					item = (IblMap_Item)item->right;
				}
				return item;
			}
		} else if (map->table[i ^ 1] == item) {
			return (IblMap_Item)IblTree_End((IblTree)item);
		} else {
			while (item->right) {
				item = (IblMap_Item)item->right;
			}
			return item;
		}
	}
	return NULL;
}

#ifdef __cplusplus
}
#endif
