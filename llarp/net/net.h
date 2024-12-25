#pragma once
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <sys/types.h>

bool
llarp_getifaddr(const char* ifname, int af, struct sockaddr* addr);
