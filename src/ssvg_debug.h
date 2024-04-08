#ifndef SVG_SVG_DEBUG_H
#define SVG_SVG_DEBUG_H

#ifndef SSVG_CONFIG_DEBUG
#	define SSVG_CONFIG_DEBUG 0
#endif

#ifndef SSVG_CONFIG_BX_DEBUG
#	define SSVG_CONFIG_BX_DEBUG 1
#endif

#if SSVG_CONFIG_DEBUG
#if SSVG_CONFIG_BX_DEBUG
#include <bx/debug.h>
#define SSVG_DEBUG_PRINTF(_format, ...) bx::debugPrintf(_format, ##__VA_ARGS__)
#define SSVG_DEBUG_BREAK bx::debugBreak()
#else
#include <cassert>
#include <cstdio>
#define SSVG_DEBUG_PRINTF(_format, ...) std::printf(_format, ##__VA_ARGS__)
#define SSVG_DEBUG_BREAK assert(0)
#endif

#define SSVG_TRACE(_format, ...) \
	do { \
		SSVG_DEBUG_PRINTF(BX_FILE_LINE_LITERAL "ssvg " _format "\n", ##__VA_ARGS__); \
	} while(0)

#define SSVG_WARN(_condition, _format, ...) \
	do { \
		if (!(_condition) ) { \
			SSVG_TRACE(_format, ##__VA_ARGS__); \
		} \
	} while(0)

#define SSVG_CHECK(_condition, _format, ...) \
	do { \
		if (!(_condition) ) { \
			SSVG_TRACE(_format, ##__VA_ARGS__); \
			SSVG_DEBUG_BREAK; \
		} \
	} while(0)
#else
#define SSVG_TRACE(_format, ...)
#define SSVG_WARN(_condition, _format, ...)
#define SSVG_CHECK(_condition, _format, ...)
#endif

#endif
