/**
 * @file common.h
 * @author Kai Dewey
 * @brief A list of common includes and defines for the project.
 * @version 0.1
 */

#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L

 // stdlib
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/types.h> 

// networking
#include <sys/socket.h>
#include <netdb.h> 
#include <arpa/inet.h>

#endif // COMMON_H
