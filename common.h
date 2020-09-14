#ifndef COMMON_H
#define COMMON_H

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <sys/socket.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/uio.h>
#include <unistd.h>

#include <cassert>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <fcntl.h>
#include <string.h>
#include <stdarg.h>

#include <map>
#include <string>
#include <unordered_map>
#include <list>

#include "log.h"

#endif