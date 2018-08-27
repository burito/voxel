



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
