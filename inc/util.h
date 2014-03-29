/*
 * Copyright (C) 2014 - plutoo
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdint.h>
#include <stdbool.h>

typedef uint8_t u8;
typedef int8_t s8;
typedef uint16_t u16;
typedef int16_t s16;
typedef uint32_t u32;
typedef int32_t s32;
typedef uint64_t u64;
typedef int64_t s64;

// Fix for Microshit compiler
#ifndef __func__
#define __func__ __FUNCTION__
#endif

#define DEBUG(...) do {                          \
	fprintf(stdout, "%s: ", __func__);       \
	fprintf(stdout,  __VA_ARGS__);           \
    } while(0);


#define ERROR(...) do { \
	fprintf(stderr, "%s:%d: ", __FILE__, __LINE__); \
	fprintf(stderr, __VA_ARGS__);			\
    } while(0);

#if 0
#define PAUSE() fgetc(stdin);
#else
#define PAUSE()
#endif

#define ARRAY_SIZE(s) (sizeof(s)/sizeof((s)[0]))
