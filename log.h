#include <string.h>
#include <stdio.h>
// Returns the local date/time formatted as 2014-03-19 11:11:52

char* getFormattedTime(void);
float getClockTime(void);

// Remove path from filename

#define __SHORT_FILE__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

// Main log macro

#define __LOG__(format, loglevel, ...) printf("%s %f %-5s[%s][%s:%d] " format, getFormattedTime(), getClockTime(), loglevel, __SHORT_FILE__, __func__ , __LINE__, ## __VA_ARGS__)

// Specific log macros with
#define LOG_DEBUG(format, ...) __LOG__(format, "DEBUG", ## __VA_ARGS__)
#define LOG_WARN(format, ...) __LOG__(format, "WARN", ## __VA_ARGS__)
#define LOG_ERROR(format, ...) __LOG__(format, "ERROR", ## __VA_ARGS__)
#define LOG_INFO(format, ...) __LOG__(format, "INFO", ## __VA_ARGS__)
