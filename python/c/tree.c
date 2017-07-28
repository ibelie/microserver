// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#include "tree.h"

#define _IblTree_Color(n)           ((n)->parent & 1)
#define _IblTree_IsRed(n)           (!_IblTree_Color(n))
#define _IblTree_IsBlack(n)         _IblTree_Color(n)
#define _IblTree_SetRed(n)          do { (n)->parent &= ~1; } while (0)
#define _IblTree_SetBlack(n)        do { (n)->parent |= 1; } while (0)
#define _IblTree_SetParent(n, p)    do { (n)->parent = ((n)->parent & 1) | (size_t)(p); } while (0)
#define _IblTree_SetColor(n, color) do { (n)->parent = ((n)->parent & ~1) | (color); } while (0)

#ifdef __cplusplus
extern "C" {
#endif

IblTree IblTree_Begin(IblTree node) {
	if (!node) { return NULL; }
	while (node->left) { node = node->left; }
	return node;
}

IblTree IblTree_End(IblTree node) {
	if (!node) { return NULL; }
	while (node->right) { node = node->right; }
	return node;
}

#define IblTree_Rotate(HAND1, HAND2) \
__rotate_##HAND1(IblTree* root, IblTree node) { \
	IblTree HAND2 = node->HAND2;                \
	IblTree parent = IblTree_Parent(node);      \
                                                \
	if ((node->HAND2 = HAND2->HAND1)) {         \
		_IblTree_SetParent(HAND2->HAND1, node); \
	}                                           \
	HAND2->HAND1 = node;                        \
                                                \
	_IblTree_SetParent(HAND2, parent);          \
                                                \
	if (parent) {                               \
		if (node == parent->HAND1) {            \
			parent->HAND1 = HAND2;              \
		} else {                                \
			parent->HAND2 = HAND2;              \
		}                                       \
	} else {                                    \
		*root = HAND2;                          \
	}                                           \
	_IblTree_SetParent(node, HAND2);            \
}

static void IblTree_Rotate(left, right);
static void IblTree_Rotate(right, left);
#undef IblTree_Rotate

void __insert(IblTree* root, IblTree node) {
	IblTree parent, gparent;

	while ((parent = IblTree_Parent(node)) && _IblTree_IsRed(parent)) {
		gparent = IblTree_Parent(parent);

		if (parent == gparent->left) {
			{
				register IblTree uncle = gparent->right;
				if (uncle && _IblTree_IsRed(uncle)) {
					_IblTree_SetBlack(uncle);
					_IblTree_SetBlack(parent);
					_IblTree_SetRed(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->right == node) {
				register IblTree tmp;
				__rotate_left(root, parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			_IblTree_SetBlack(parent);
			_IblTree_SetRed(gparent);
			__rotate_right(root, gparent);
		} else {
			{
				register IblTree uncle = gparent->left;
				if (uncle && _IblTree_IsRed(uncle)) {
					_IblTree_SetBlack(uncle);
					_IblTree_SetBlack(parent);
					_IblTree_SetRed(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->left == node) {
				register IblTree tmp;
				__rotate_right(root, parent);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			_IblTree_SetBlack(parent);
			_IblTree_SetRed(gparent);
			__rotate_left(root, gparent);
		}
	}

	_IblTree_SetBlack(*root);
}

static void __erase(IblTree* root, IblTree parent, IblTree node) {
	IblTree other;

	while ((!node || _IblTree_IsBlack(node)) && node != *root) {
		if (parent->left == node) {
			other = parent->right;
			if (_IblTree_IsRed(other)) {
				_IblTree_SetBlack(other);
				_IblTree_SetRed(parent);
				__rotate_left(root, parent);
				other = parent->right;
			}
			if ((!other->left || _IblTree_IsBlack(other->left)) &&
			    (!other->right || _IblTree_IsBlack(other->right))) {
				_IblTree_SetRed(other);
				node = parent;
				parent = IblTree_Parent(node);
			} else {
				if (!other->right || _IblTree_IsBlack(other->right)) {
					_IblTree_SetBlack(other->left);
					_IblTree_SetRed(other);
					__rotate_right(root, other);
					other = parent->right;
				}
				_IblTree_SetColor(other, _IblTree_Color(parent));
				_IblTree_SetBlack(parent);
				_IblTree_SetBlack(other->right);
				__rotate_left(root, parent);
				node = *root;
				break;
			}
		} else {
			other = parent->left;
			if (_IblTree_IsRed(other)) {
				_IblTree_SetBlack(other);
				_IblTree_SetRed(parent);
				__rotate_right(root, parent);
				other = parent->left;
			}
			if ((!other->left || _IblTree_IsBlack(other->left)) &&
			    (!other->right || _IblTree_IsBlack(other->right))) {
				_IblTree_SetRed(other);
				node = parent;
				parent = IblTree_Parent(node);
			} else {
				if (!other->left || _IblTree_IsBlack(other->left)) {
					_IblTree_SetBlack(other->right);
					_IblTree_SetRed(other);
					__rotate_left(root, other);
					other = parent->left;
				}
				_IblTree_SetColor(other, _IblTree_Color(parent));
				_IblTree_SetBlack(parent);
				_IblTree_SetBlack(other->left);
				__rotate_right(root, parent);
				node = *root;
				break;
			}
		}
	}
	if (node) {
		_IblTree_SetBlack(node);
	}
}

void IblTree_Erase(IblTree* root, IblTree node) {
	int color;
	IblTree child, parent, old;

	if (!node->left) {
		child = node->right;
	} else if (!node->right) {
		child = node->left;
	} else {
		old = node;
		node = node->right;
		while (node->left) { node = node->left; }

		if (IblTree_Parent(old)) {
			if (IblTree_Parent(old)->left == old) {
				IblTree_Parent(old)->left = node;
			} else {
				IblTree_Parent(old)->right = node;
			}
		} else {
			*root = node;
		}

		child = node->right;
		parent = IblTree_Parent(node);
		color = _IblTree_Color(node);

		if (parent == old) {
			parent = node;
		} else {
			if (child) {
				_IblTree_SetParent(child, parent);
			}
			parent->left = child;

			node->right = old->right;
			_IblTree_SetParent(old->right, node);
		}

		node->parent = old->parent;
		node->left = old->left;
		_IblTree_SetParent(old->left, node);

		goto color;
	}

	parent = IblTree_Parent(node);
	color = _IblTree_Color(node);

	if (child) {
		_IblTree_SetParent(child, parent);
	}
	if (parent) {
		if (parent->left == node) {
			parent->left = child;
		} else {
			parent->right = child;
		}
	} else {
		*root = child;
	}

color:
	if (color) { /* black */
		__erase(root, parent, child);
	}
}

void IblTree_Insert(IblTree* root, IblTree* link, IblTree parent, IblTree node) {
	node->parent = (size_t)parent;
	node->left = node->right = NULL;
	*link = node;
	__insert(root, node);
}

IblTree IblTree_Next(IblTree node) {
	if (IblTree_Parent(node) == node) { return NULL; }

	/* If we have a right-hand child, go down and then left as far as we can. */
	if (node->right) {
		node = node->right;
		while (node->left) {
			node = node->left;
		}
		return (IblTree)node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	register IblTree parent;
	while ((parent = IblTree_Parent(node)) && node == parent->right) {
		node = parent;
	}
	return parent;
}

IblTree IblTree_Prev(IblTree node) {
	if (IblTree_Parent(node) == node) { return NULL; }

	/* If we have a left-hand child, go down and then right as far as we can. */
	if (node->left) {
		node = node->left;
		while (node->right) {
			node = node->right;
		}
		return (IblTree)node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	register IblTree parent;
	while ((parent = IblTree_Parent(node)) && node == parent->left) {
		node = parent;
	}
	return parent;
}

IblTree IblTree_PrePost(IblTree node) {
	if (!node) { return NULL; }
	while (node->left || node->right) {
		while (node->left) {
			node = node->left;
		}
		if (node->right) {
			node = node->right;
		}
	}
	return node;
}

IblTree IblTree_Post(IblTree node) {
	register IblTree parent = IblTree_Parent(node);
	if (!parent || parent == node) { return NULL; }

	/* If parent has a right-hand child, go down and then left as far as we can. */
	if (parent->right && parent->right != node) {
		node = parent->right;
		while (node->left || node->right) {
			while (node->left) {
				node = node->left;
			}
			if (node->right) {
				node = node->right;
			}
		}
		return (IblTree)node;
	} else {
		return parent;
	}
}

#ifdef __cplusplus
}
#endif
