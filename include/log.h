#ifndef LOG_H
#define LOG_H

#include <stdio.h>

#ifdef LOGGING_IMPL
int LOG_VERBOSE = 0;
#else
extern int LOG_VERBOSE;
#endif

#define INFO    "\033[34m[INFO]\033[0m"
#define ERROR   "\033[31m[ERROR]\033[0m"
#define WARNING "\033[33m[WARNING]\033[0m"
#define SUCCESS "\033[32m[SUCCESS]\033[0m"

#define LOG_WARNING(fmt, ...) \
  do { \
    fprintf(stdout, WARNING " " fmt "\n", ##__VA_ARGS__); \
  } while (0)
#define LOG_ERROR(fmt, ...) \
  do { \
    fprintf(stderr, ERROR " " fmt "\n", ##__VA_ARGS__); \
  } while (0)
#define LOG_INFO(fmt, ...) \
  do { \
    if (LOG_VERBOSE) { \
      fprintf(stdout, INFO " " fmt "\n", ##__VA_ARGS__); \
    } \
  } while (0)
#define LOG_SUCCESS(fmt, ...) \
  do { \
    fprintf(stdout, SUCCESS " " fmt "\n", ##__VA_ARGS__); \
  } while (0)

#endif
