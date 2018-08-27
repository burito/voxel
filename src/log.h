/*
Copyright (c) 2018 Daniel Burke

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/


#define log_trace(...) log_out(__FILE__, __LINE__, LOG_TRACE, __VA_ARGS__)
#define log_debug(...) log_out(__FILE__, __LINE__, LOG_DEBUG, __VA_ARGS__)
#define log_verbose(...) log_out(__FILE__, __LINE__, LOG_VERBOSE, __VA_ARGS__)

// we always want these messages
#define log_info(...) log_out(__FILE__, __LINE__, LOG_INFO, __VA_ARGS__)
#define log_warning(...) log_out(__FILE__, __LINE__, LOG_WARNING, __VA_ARGS__)
#define log_error(...) log_out(__FILE__, __LINE__, LOG_ERROR, __VA_ARGS__)
#define log_fatal(...) log_out(__FILE__, __LINE__, LOG_FATAL, __VA_ARGS__)

enum LOG_LEVEL {
	LOG_TRACE = 1,
	LOG_DEBUG = 2,
	LOG_VERBOSE = 4,
	LOG_INFO = 8,
	LOG_WARNING = 16,
	LOG_ERROR = 32,
	LOG_FATAL = 64,
};


void log_init(void);
void log_out(char* file, int line, enum LOG_LEVEL level, char* fmt, ...);
