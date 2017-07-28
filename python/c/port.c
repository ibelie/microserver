// Copyright 2017 ibelie, Chen Jie, Joungtao. All rights reserved.
// Use of this source code is governed by The MIT License
// that can be found in the LICENSE file.

#ifndef _CRT_SECURE_NO_WARNINGS
#	define _CRT_SECURE_NO_WARNINGS
#endif

#include "port.h"
#include "sys/stat.h"

#ifdef _MSC_VER
#	define fileno _fileno
#	define S_ISREG(m) ((m) & S_IFREG)
#	include "windows.h"
#	include "io.h"
#else
#	include "unistd.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

size_t IblBytesHash(bytes buffer) {
	register size_t hash = 0;
	register uint8 *i, *n = buffer->data + buffer->length;
	for (i = buffer->data; i < n; i++) {
		hash = (hash << 5) - hash + *i;
	}
	return hash;
}

int IblBytesCompare(bytes k1, bytes k2) {
	if (k1->length == k2->length) {
		return memcmp(k1->data, k2->data, k1->length);
	} else {
		return k1->length - k2->length;
	}
}

size_t IblStringHash(string* str) {
	register size_t hash = 0;
	register char* i;
	for (i = *str; *i != '\0'; i++) {
		hash = (hash << 5) - hash + *i;
	}
	return hash;
}

int IblStringCompare(string* s1, string* s2) {
	return strcmp(*s1, *s2);
}

uint64 IblUint64(byte* b) {
	return ((uint64)(b[7])) | ((uint64)(b[6])<<8) | ((uint64)(b[5])<<16) | ((uint64)(b[4])<<24) |
		((uint64)(b[3])<<32) | ((uint64)(b[2])<<40) | ((uint64)(b[1])<<48) | ((uint64)(b[0])<<56);
}

void IblPutUint64(byte* b, uint64 v) {
	b[0] = (byte)(v >> 56);
	b[1] = (byte)(v >> 48);
	b[2] = (byte)(v >> 40);
	b[3] = (byte)(v >> 32);
	b[4] = (byte)(v >> 24);
	b[5] = (byte)(v >> 16);
	b[6] = (byte)(v >> 8);
	b[7] = (byte)(v);
}

size_t IblSizeVarint(uint64 x) {
	register size_t n = 0;
	do { n++; x >>= 7; } while (x);
	return n;
}

int IblUvarint(byte* buffer, size_t buf_len, uint64* x) {
	register uint64 y = 0;
	register size_t s = 0, i;
	if (buf_len > MaxVarintLen) { buf_len = MaxVarintLen; }
	for (i = 0; i < buf_len; i++) {
		register byte b = buffer[i];
		if (b < 0x80) {
			if (i > 9 || (i == 9 && b > 1)) {
				/* overflow */
				*x = 0;
				return -((int)i + 1);
			}
			*x = y | (uint64)(b) << s;
			return i + 1;
		}
		y |= (uint64)(b & 0x7F) << s;
		s += 7;
	}
	*x = 0;
	return 0;
}

size_t IblPutUvarint(byte* buffer, uint64 x) {
	register size_t i = 0;
	while (x >= 0x80) {
		buffer[i] = (byte)x | 0x80;
		x >>= 7;
		i++;
	}
	buffer[i] = (byte)x;
	return i + 1;
}

FILE* IblFileOpen(const string file) {
	struct stat file_stat = {0};
	stat(file, &file_stat);
	return fopen(file, S_ISREG(file_stat.st_mode) ? "r+b" : "w+b");
}

bool IblFileWriteUint64At(FILE* file, uint64 x, size_t offset) {
	byte buffer[8] = {
		(byte)(x >> 56),
		(byte)(x >> 48),
		(byte)(x >> 40),
		(byte)(x >> 32),
		(byte)(x >> 24),
		(byte)(x >> 16),
		(byte)(x >> 8),
		(byte)(x),
	};
	if (fseek(file, offset, SEEK_SET)) { return false; }
	return fwrite(buffer, sizeof(byte), 8, file) == 8;
}

bool IblFileTruncate(FILE* file, size_t length) {
	if (!file) { return false; }
	if (fflush(file)) { goto file_truncate_error; }

#ifdef _MSC_VER
	/* MS _chsize doesn't work if newsize doesn't fit in 32 bits,
	   so don't even try using it. */
	{
		/* Have to move current pos to desired endpoint on Windows. */
		if (fseek(file, length, SEEK_SET))  { goto file_truncate_error; }
		/* Truncate.  Note that this may grow the file! */
		register HANDLE hFile = (HANDLE)_get_osfhandle(fileno(file));
		if ((int)hFile != -1) {
			if (!SetEndOfFile(hFile)) { goto file_truncate_error; }
		} else {
			goto file_truncate_error;
		}
	}
#else
	if (ftruncate(fileno(file), length)) { goto file_truncate_error; }
#endif /* !_MSC_VER */

	return true;

file_truncate_error:
	clearerr(file);
	return false;
}

IblBitmap IblBitmap_New(size_t bit) {
	size_t size = bit / IblBitmap_BITCOUNT + ((bit % IblBitmap_BITCOUNT) ? 1 : 0);
	IblBitmap bitmap = (IblBitmap)calloc(size, sizeof(size_t));
	bitmap[size - 1] = IblBitmap_HEADMASK;
	return bitmap;
}

bool IblBitmap_Get(IblBitmap bitmap, size_t bit) {
	IblBitmap ptr = bitmap;
	for (; bit > IblBitmap_BITCOUNT && IblBitmap_HASNEXT(ptr); bit -= IblBitmap_BITCOUNT, ptr++);
	return bit > IblBitmap_BITCOUNT ? false : IblBitmap_GET(ptr, bit);
}

bool IblBitmap_Set(IblBitmap bitmap, size_t bit, bool flag) {
	IblBitmap ptr = bitmap;
	for (; bit > IblBitmap_BITCOUNT && IblBitmap_HASNEXT(ptr); bit -= IblBitmap_BITCOUNT, ptr++);
	if (bit > IblBitmap_BITCOUNT) {
		return false;
	} else if (flag) {
		IblBitmap_SET1(ptr, bit);
		return true;
	} else {
		IblBitmap_SET0(ptr, bit);
		return true;
	}
}

#ifdef __cplusplus
}
#endif
