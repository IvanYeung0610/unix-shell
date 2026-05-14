/*
 * Unix Shell
 * Ivan Yeung
 * NYU Tandon
 */

#ifndef SHELL_H
#define SHELL_H

#define  _POSIX_C_SOURCE 200809L
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define INITIAL_CAP 8
#define TOK_LEN_MAX 4096

int signalSetup();
void sigIntHandler(int signalNum);
int resetDisposition();
int runShell(char *line, char ***tokens, char ***tokens2, size_t cap);
int parseLine(char *line, char ***tokens, int *exitStatus, 
              const char **redirectFiles, size_t *cap);
int checkCommand(const char **tokens, bool *exitLoop, int numToks,
                 int *exitStatus, const char **redirectFiles);
int execCommand(char *const *tokens, int *exitStatus, 
                const char **redirectFiles);
int handleRedirect(const char **redirectFiles);
int redirectIO(const char *file, int flags, mode_t mode, 
               int FILENO, int FILENO2);
int handleCdRedirect(const char *file);
int createFile(const char *file);
void checkPipe(char *line, char **line2);
int execPipe(char *const *tokens, char *const *tokens2, int *exitStatus,
                const char **redirectFiles, const char **redirectFiles2);
void pipeChild(char *const *tokens, const char **redirectFiles, int *pipefd,
               int fileno);

#endif
