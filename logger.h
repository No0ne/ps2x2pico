#ifndef LOGGER_H
#define LOGGER_H
#include <stdio.h>

#ifndef DEBUG
#define DEBUG 0
#endif

#ifndef TRACE
#define TRACE 0
#endif

#ifndef ERROR
#define ERROR 1
#endif


#define debug_print(fmt, ...) do { if (DEBUG) { printf(fmt, ##__VA_ARGS__ ); } } while (0)

#define trace_print(fmt, ...) do { if (TRACE) { printf(fmt, ##__VA_ARGS__ ); } } while (0)

#define error_print(fmt, ...) do { if (ERROR) { printf(fmt, ##__VA_ARGS__ ); } } while (0)

#endif
