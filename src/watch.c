#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/inotify.h>
#include <dirent.h>
#include <limits.h>

#include "watch.h"
#include "log.h"

static const size_t EVENT_SIZE = sizeof(struct inotify_event);
static const size_t BUF_LEN = 1024 * (EVENT_SIZE + 16);

static volatile sig_atomic_t got_signal = 0;

static void signal_handler(int sig)
{
		(void)sig;
		got_signal = 1;
}

static int add_watch_recursive(int fd, const char *path)
{
		int wd = inotify_add_watch(fd, path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_DELETE);
		if (wd == -1) return -1;

		DIR *dir = opendir(path);
		if (!dir) return wd;

		struct dirent *entry;
		char subpath[PATH_MAX];

		while ((entry = readdir(dir)) != NULL) {
				if (entry->d_type != DT_DIR) continue;
				if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;

				snprintf(subpath, PATH_MAX, "%s/%s", path, entry->d_name);
				add_watch_recursive(fd, subpath);
		}

		closedir(dir);
		return wd;
}

int watch_init(const char *path)
{
		struct sigaction sa;
		sa.sa_handler = signal_handler;
		sa.sa_flags = 0;
		sigemptyset(&sa.sa_mask);
		sigaction(SIGINT, &sa, NULL);
		sigaction(SIGTERM, &sa, NULL);

		int fd = inotify_init();
		if (fd == -1) {
				LOG_ERROR("Failed to initialize inotify");
				return -1;
		}

		if (add_watch_recursive(fd, path) == -1) {
				LOG_ERROR("Failed to add watch on %s", path);
				close(fd);
				return -1;
		}

		return fd;
}

int watch_wait(int fd)
{
		if (got_signal) return 0;

		char buffer[BUF_LEN];
		int length = read(fd, buffer, BUF_LEN);

		if (got_signal) return 0;

		if (length < 0) {
				if (errno == EINTR) return 0;
				LOG_ERROR("Error reading inotify events");
				return 0;
		}

		usleep(100000);

		return 1;
}

void watch_close(int fd)
{
		close(fd);
}
