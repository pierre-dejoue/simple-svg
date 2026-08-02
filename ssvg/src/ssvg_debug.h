#ifndef SVG_SSVG_DEBUG_H
#define SVG_SSVG_DEBUG_H

#ifndef SSVG_CONFIG_DEBUG
#	define SSVG_CONFIG_DEBUG 0
#endif

#if SSVG_CONFIG_DEBUG
#include <cassert>
#define SSVG_DEBUG_BREAK assert(0)

#include <cstdio>
#define SSVG_DEBUG_PRINTF(_format, ...) std::printf(_format, ##__VA_ARGS__)

#define SSVG_STRINGIZE_(_x) #_x
#define SSVG_STRINGIZE(_x) SSVG_STRINGIZE_(_x)
#define SSVG_FILE_LINE_LITERAL "" __FILE__ "(" SSVG_STRINGIZE(__LINE__) "): "

#define SSVG_TRACE(_format, ...) \
	do { \
		SSVG_DEBUG_PRINTF(SSVG_FILE_LINE_LITERAL "ssvg " _format "\n", ##__VA_ARGS__); \
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
#include <cassert>
#define SSVG_TRACE(_format, ...)
#define SSVG_WARN(_condition, _format, ...)
#define SSVG_CHECK(_condition, _format, ...) \
	do { \
		assert((_condition)); \
	} while(0)
#endif

#endif
