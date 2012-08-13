/*
	dio_shark.c
	the main project source file
	it contains 'main' function

	dio-shark is disk block tracing and analysis tool
	it will loosing sharks to each cpu
*/

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/time.h>

#include "dio_shark.h"


