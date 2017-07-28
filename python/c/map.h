// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef IBELIE_MAP_H__
#define IBELIE_MAP_H__

#include "tree.h"

#ifdef __cplusplus
extern "C" {
#endif

/* IblMap_KEY defines the initial segment of every IblMap_Item. */
#define IblMap_KEY(TYPE)  \
	IblTree_HEAD;         \
	IBL_ALIGN(8, TYPE key)

/* Every pointer to a map item can be cast to a IblMap_Item. */
typedef struct _IblMap_Item {
	IblMap_KEY(byte);
} *IblMap_Item;

typedef void* IblMap_Key;
typedef size_t      (*IblMap_Hash)    (IblMap_Key);
typedef IblMap_Item (*IblMap_NewItem) (IblMap_Key);
typedef int         (*IblMap_Compare) (IblMap_Key, IblMap_Key);
typedef void        (*IblMap_Dealloc) (IblMap_Item);

typedef struct _IblMap {
	size_t size;
	size_t capacity;
	IblMap_Item*   table;
	IblMap_Hash    hash;
	IblMap_NewItem alloc;
	IblMap_Dealloc dealloc;
	IblMap_Compare compare;
} *IblMap;

#define IblMap_Size(m) ((m) ? (m)->size : 0)
#define IblMap_New(HASH, ALLOC, DEALLOC, COMPARE) \
	{0, 0, NULL, HASH, ALLOC, DEALLOC, COMPARE}
#define IblMap_Free(m) do { IblMap_Clear(m); free(m); (m) = NULL; } while (0)

IblAPI(void)        IblMap_Clear (IblMap);
IblAPI(bool)        IblMap_Del   (IblMap, IblMap_Key);
IblAPI(IblMap_Item) IblMap_Get   (IblMap, IblMap_Key);
IblAPI(IblMap_Item) IblMap_Set   (IblMap, IblMap_Key);

IblAPI(IblMap_Item) IblMap_Begin (IblMap);
IblAPI(IblMap_Item) IblMap_End   (IblMap);
IblAPI(IblMap_Item) IblMap_Next  (IblMap, IblMap_Item);
IblAPI(IblMap_Item) IblMap_Prev  (IblMap, IblMap_Item);


/* ============================================================================= */
#define IblMap_KEY_STRING(NAME, FIELDS)                                           \
	typedef struct _##NAME {                                                      \
		IblMap_KEY(string);                                                       \
		FIELDS                                                                    \
	} *NAME;                                                                      \
                                                                                  \
	static inline NAME _##NAME##_New(string* key) {                               \
		NAME item = (NAME)calloc(1, sizeof(struct _##NAME));                      \
		if (item) {                                                               \
			register size_t length = strlen(*key);                                \
			item->key = (string)calloc(length + 1, sizeof(char));                 \
			if (item->key) {                                                      \
				memcpy(item->key, *key, length);                                  \
			}                                                                     \
		}                                                                         \
		return item;                                                              \
	}                                                                             \
                                                                                  \
	static inline void _##NAME##_Free(NAME item) {                                \
		if (item) {                                                               \
			if (item->key) { free(item->key); }                                   \
			free(item);                                                           \
		}                                                                         \
	}                                                                             \
                                                                                  \
	static inline IblMap NAME##_New(void) {                                       \
		IblMap map = (IblMap)calloc(1, sizeof(struct _IblMap));                   \
		if (map) {                                                                \
			map->hash    = (IblMap_Hash)    IblStringHash;                        \
			map->alloc   = (IblMap_NewItem) _##NAME##_New;                        \
			map->dealloc = (IblMap_Dealloc) _##NAME##_Free;                       \
			map->compare = (IblMap_Compare) IblStringCompare;                     \
		}                                                                         \
		return map;                                                               \
	}                                                                             \
/* ============================================================================= */

/* ============================================================================= */
#define IblMap_KEY_BYTES(NAME, FIELDS)                                            \
	typedef struct _##NAME {                                                      \
		IblMap_KEY(struct _bytes);                                                \
		FIELDS                                                                    \
	} *NAME;                                                                      \
                                                                                  \
	static inline NAME _##NAME##_New(bytes key) {                                 \
		NAME item = (NAME)calloc(1, sizeof(struct _##NAME));                      \
		if (item) {                                                               \
			item->key.length = key->length;                                       \
			item->key.data = (byte*)calloc(key->length, sizeof(byte));            \
			if (item->key.data) {                                                 \
				memcpy(item->key.data, key->data, key->length);                   \
			}                                                                     \
		}                                                                         \
		return item;                                                              \
	}                                                                             \
                                                                                  \
	static inline void _##NAME##_Free(NAME item) {                                \
		if (item) {                                                               \
			if (item->key.data) { free(item->key.data); }                         \
			free(item);                                                           \
		}                                                                         \
	}                                                                             \
                                                                                  \
	static inline IblMap NAME##_New(void) {                                       \
		IblMap map = (IblMap)calloc(1, sizeof(struct _IblMap));                   \
		if (map) {                                                                \
			map->hash    = (IblMap_Hash)    IblBytesHash;                         \
			map->alloc   = (IblMap_NewItem) _##NAME##_New;                        \
			map->dealloc = (IblMap_Dealloc) _##NAME##_Free;                       \
			map->compare = (IblMap_Compare) IblBytesCompare;                      \
		}                                                                         \
		return map;                                                               \
	}                                                                             \
/* ============================================================================= */

/* ============================================================================= */
#define IblMap_KEY_NUMERIC(NAME, TYPE, FIELDS)                                    \
	typedef struct _##NAME {                                                      \
		IblMap_KEY(TYPE);                                                         \
		FIELDS                                                                    \
	} *NAME;                                                                      \
                                                                                  \
	static inline NAME _##NAME##_New(TYPE* key) {                                 \
		NAME item = (NAME)calloc(1, sizeof(struct _##NAME));                      \
		if (item) { item->key = *key; }                                           \
		return item;                                                              \
	}                                                                             \
                                                                                  \
	static inline size_t _##NAME##_Hash(TYPE* key) { return (size_t)(*key); }     \
	static inline void   _##NAME##_Free(NAME item) { if (item) { free(item); } }  \
	static inline int    _##NAME##_Compare(TYPE* k1, TYPE* k2) {                  \
		return (int)((*k1) - (*k2));                                              \
	}                                                                             \
                                                                                  \
	static inline IblMap NAME##_New(void) {                                       \
		IblMap map = (IblMap)calloc(1, sizeof(struct _IblMap));                   \
		if (map) {                                                                \
			map->hash    = (IblMap_Hash)    _##NAME##_Hash;                       \
			map->alloc   = (IblMap_NewItem) _##NAME##_New;                        \
			map->dealloc = (IblMap_Dealloc) _##NAME##_Free;                       \
			map->compare = (IblMap_Compare) _##NAME##_Compare;                    \
		}                                                                         \
		return map;                                                               \
	}                                                                             \
/* ============================================================================= */

#ifdef __cplusplus
}
#endif

#endif /* IBELIE_MAP_H__ */
