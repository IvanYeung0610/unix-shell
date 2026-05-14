/*
 * Unix Shell
 * Ivan Yeung
 * NYU Tandon
 */

#include "../include/shell.h"

int main(void) {
    char **tokens = malloc(sizeof(char*) * INITIAL_CAP);
    char **tokens2 = malloc(sizeof(char*) * INITIAL_CAP);
    char *line = NULL;

    if (signalSetup() == -1) {
        fputs("signalSetup failed!\n", stderr);
        return 0;
    }

    if (runShell(line, &tokens, &tokens2, INITIAL_CAP)) {
        fputs("runShell failed!\n", stderr);
        free(line);
        free(tokens);
        return 1;
    }

    free(line);
    free(tokens);
    free(tokens2);
    return 0;
}

/**
 * @brief Sets up the action taken by the shell
 * for the signals SIGINT and SIGQUIT. SIGQUIT is
 * ignored while SIGINT is handled by the custom
 * sigIntHandler() function.
 *
 * @return Returns 0 on success. On failure -1 is
 * returned
 */
int signalSetup() {
    struct sigaction intAct, quitAct;

    intAct.sa_flags = 0;
    quitAct.sa_flags = 0;

    sigemptyset(&intAct.sa_mask);
    sigemptyset(&quitAct.sa_mask);

    intAct.sa_handler = sigIntHandler;
    quitAct.sa_handler = SIG_IGN;


    if (sigaction(SIGINT, &intAct, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }
    if (sigaction(SIGQUIT, &quitAct, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }

    return 0;
}

/**
 * @brief Signal handler for the SIGINT signal.
 * It wites a newline to standard output.
 *
 * @param signalNum The signal being handled 
 */
void sigIntHandler(int signalNum) {
    if(write(STDOUT_FILENO, "\n", 1) == -1) {
        perror("write failed");
        exit(1);
    }
}

/**
 * @brief Iterates infinitely until the program is ended. Parses
 * and stores the input and executes it. Also displays prompt while
 * looping and waiting for input. As an added feature, runShell will
 * also determine if there is a pipe and handle it accordingly. It
 * will split the line into two arrays of strings holding tokens
 * from the before and after the pipe character. Then it will call
 * the execPipe to properly execute both sides.
 *
 * @param line A buffer to store inputted lines
 * @param tokens A pointer to a buffer to store the tokenized lines
 * @param tokens2 A pointer to a second buffer to store tokenized lines
 * in the case that a pipe was found in the inputted command
 * @param cap Initial capacity of the tokens array
 * @return 
 */
int runShell(char *line, char ***tokens, char ***tokens2, size_t cap) {
    bool exitLoop = false;
    size_t lineLen = 0;
    size_t tokensCap = cap;
    size_t tokensCap2 = cap;
    int exitStatus = 0;

    while(!exitLoop) {
        // Display PS1 or "[pwd]> " as prompt
        char* prompt = getenv("PS1");
        if (!prompt) {
            prompt = getenv("PWD");
        }
        printf("%s> ", prompt);

        errno = 0;
        ssize_t len = getline(&line, &lineLen, stdin);
        if (len == -1) {
            if (errno == EINTR) { // handles SIGINT
                clearerr(stdin);
                exitStatus = 130;
                continue;
            } else if (errno != 0) {
                perror("getline failed");
                return 1;
            } else { // EOF case if file is used
                return 0;
            }
        }
        // removes '\0' to prevent last token from including it
        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        // Checks for pipe
        char *line2 = NULL;
        checkPipe(line, &line2);

        // create variables to store redirection files
        // 0: "<", 1: ">", 2: ">>", 3: "2>", 4: "&>"
        const char *redirectFiles[5] = {NULL};
        const char *redirectFiles2[5] = {NULL};
        int numToks = parseLine(line, tokens, &exitStatus, redirectFiles, 
                                &tokensCap);
        int numToks2 = 0;
        if (numToks == -1) {
            fputs("parseLine failed!\n", stderr);
            return 1;
        }

        if (line2 != NULL) { // There was a pipe
            numToks2 = parseLine(line2, tokens2, &exitStatus, redirectFiles2, 
                                    &tokensCap2);
            if (numToks2 == -1) {
                fputs("parseLine failed!\n", stderr);
                return 1;
            }

            if (execPipe(*tokens, *tokens2, &exitStatus, 
                     redirectFiles, redirectFiles2)) {
                fputs("execPipe failed!\n", stderr);
                return 1;
            }
        } else {
            // checks for cd and exit are skipped if a pipe was found
            if (checkCommand((const char**) (*tokens), &exitLoop,
                             numToks, &exitStatus, redirectFiles)) {
                fputs("runCommands failed!\n", stderr);
                return 1;
            }
        }

    }
    return 0;
}

/**
 * @brief Tokenizes a line with " " as the seperator
 * and stores it in the passed in array of char pointers. Will also
 * resize the array with realloc with more space is needed by doubling
 * the capacity of the array pointed to by tokens. Additionally,
 * the function checks if there are redirect tokens and adds the token
 * to the redirectFiles array.
 *
 * @param line Buffer storing the line of arguments
 * @param tokens Pointer to array of char ptrs to store the tokens in
 * @param exitStatus Stores last exitStatus to replace $? token
 * @param redirectFiles An array of char pointers that indicate whether
 * there is a specific redirect in the command
 * @param cap Pointer to capacity of the tokens array so the updates
 * to capacity can be seen by the function that called it
 * @return Returns the number of tokens parsed or -1 on error
 */
int parseLine(char *line, char ***tokens, int *exitStatus, 
              const char **redirectFiles, size_t *cap) {
    char* args = strtok(line, " ");
    size_t index = 0;
    while (args != NULL) {
        // replaces $? token with exit status
        if (strncmp(args, "$?", TOK_LEN_MAX) == 0) {
            static char statusBuf[4];
            snprintf(statusBuf, 4, "%d", *exitStatus);
            args = statusBuf;
        }
        // handles redirection commands
        if (strncmp(args, "<", TOK_LEN_MAX) == 0) {
            redirectFiles[0] = strtok(NULL, " ");
        } else if (strncmp(args, ">", TOK_LEN_MAX) == 0) {
            redirectFiles[1] = strtok(NULL, " ");
            if (createFile(redirectFiles[1]) == -1) {
                fputs("createFile failed", stderr);
                return -1;
            }
        } else if (strncmp(args, ">>", TOK_LEN_MAX) == 0) {
            redirectFiles[2] = strtok(NULL, " ");
            if (createFile(redirectFiles[2]) == -1) {
                fputs("createFile failed", stderr);
                return -1;
            }
        } else if (strncmp(args, "2>", TOK_LEN_MAX) == 0) {
            redirectFiles[3] = strtok(NULL, " ");
            if (createFile(redirectFiles[3]) == -1) {
                fputs("createFile failed", stderr);
                return -1;
            }
        } else if (strncmp(args, "&>", TOK_LEN_MAX) == 0)  {
            redirectFiles[4] = strtok(NULL, " ");
            if (createFile(redirectFiles[4]) == -1) {
                fputs("createFile failed", stderr);
                return -1;
            }
        } else {
            (*tokens)[index] = args;
            index++;

            // Resizes array if it is full
            if (index >= (*cap)) {
                (*cap) *= 2; // double capacity
                char **newArr = realloc((*tokens), (*cap) * (sizeof(char*)));
                if (newArr == NULL) {
                    perror("Memory allocation failed");
                    return -1;
                }
                (*tokens) = newArr;
            }
        }
        args = strtok(NULL, " ");
    }
    (*tokens)[index] = NULL; // null terminates array
    return index;
}

/**
 * @brief Checks the arguments to see if the built in cd or exit commands
 * were inputted to be run. Otherwise it calls execCommand to fork a child
 * and exec the command. Additionally handles stderr redirect for cd failure.
 *
 * @param tokens An array of char ptr with inputted tokens
 * @param exitStatus A ptr to boolean indicating whether shell has been exited
 * @param numToks Indicates the number of tokens stored in tokens array
 * @param exitStatus Pointer to exit status that is passed into execCommand
 * @param redirectFiles An array of char pointers that indicate whether
 * there is a specific redirect in the command
 * @return 0 is returned on success, otherwise 1 is returned
 */
int checkCommand(const char **tokens, bool *exitLoop, 
                 int numToks, int *exitStatus, const char **redirectFiles) {
    if (numToks == 0) { // Do nothing if no args
        return 0;
    }
    // cd and exit are manually checked since we can use child to exec them
    if (strncmp(tokens[0], "cd", TOK_LEN_MAX) == 0) {
        int cdErr;
        if (numToks == 1) {
            cdErr = chdir(getenv("HOME"));
        } else {
            cdErr = chdir(tokens[1]);
        }
        // set exit status based on if cd passed or failed
        *exitStatus = 0;
        if (cdErr == -1) {
            // Handles err redirection if it was specified in the line
            if (redirectFiles[3] || redirectFiles[4]) {
                const char *errFile = redirectFiles[3] ? 
                    redirectFiles[3] : redirectFiles[4];
                if (handleCdRedirect(errFile) == -1) {
                    fputs("handleCdRedirect failed!\n", stderr);
                    return 1;
                }
            } else {
                perror("chdir failed");
            }
            *exitStatus = 1;
            return 0;
        }

        // update PWD
        char cwd[TOK_LEN_MAX];
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            perror("getcwd failed");
            return 1;
        }
        if (setenv("PWD", cwd, 1) == -1) {
            perror("setenv failed");
            return 1;
        }
    } else if (strncmp(tokens[0], "exit", TOK_LEN_MAX) == 0) {
        *exitLoop = true;
    } else {
        if (execCommand((char *const *)tokens, exitStatus, redirectFiles)) {
            fputs("execCommands failed!\n", stderr);
            return 1;
        }
    }

    return 0;
}

/**
 * @brief Executes the command and arguments specified by tokens.
 * Additionally it records the exit status of the child when the child
 * terminates. It also properly handles job control when the child is
 * created. The both and shell and child process will make the child's
 * process group the child's pid to prevent a race condition. The shell
 * will then give control of the terminal to the child and take back
 * control of the terminal once the child has exited.
 *
 * @param tokens Array of strings that were inputted to the shell
 * @param exitStatus Stores the exit status of the child process
 * @param redirectFiles An array of char pointers that indicate whether
 * there is a specific redirect in the command
 * @return Returns 0 on success. Otherwise returns 1 to indicate an
 * issue occurred.
 */
int execCommand(char *const *tokens, int *exitStatus, const char **redirectFiles) {
    pid_t pid;
    int wstatus;
    pid = fork();
    if (pid == 0) { // child
        // Make child its own process group
        if (setpgid(getpid(), getpid()) < 0) {
            perror("setpgid failed");
            _exit(1);
        }

        if (resetDisposition() == -1) {
            fputs("resetDispostion failed!\n", stderr);
            _exit(1);
        }

        if (handleRedirect(redirectFiles) == -1) {
            fputs("handleRedirects failed!\n", stderr);
            _exit(1);
        }
        int execute = execvp(tokens[0], tokens);
        if (execute == -1) {
            perror("execvp failed");
            _exit(1);
        }
    } else if (pid > 0) { // parent
        //Give child its own process group and the controlling terminal
        if (setpgid(pid, pid) < 0) {
            perror("setpgid failed");
            return 1;
        }
        if (tcsetpgrp(0, pid) < 0) {
            perror("tcsetpgrp failed");
            return 1;
        }

        int waitRes;
        errno = 0;
        while ((waitRes = waitpid(pid, &wstatus, WUNTRACED)) == -1 
            && errno == EINTR) {
            // keeps waiting if SIGINT interrupted wait
        }

        if (waitRes == -1) {
            perror("wait failed");
            return 1;
        }

        // Gets and records exit status of child
        if (WIFEXITED(wstatus)) {
            *exitStatus = WEXITSTATUS(wstatus);
        } else if (WIFSIGNALED(wstatus)) {
            *exitStatus = WTERMSIG(wstatus) + 128;
        }

        // Gets back control from the child
        if (signal(SIGTTOU, SIG_IGN) == SIG_ERR) {
            perror("signal failed");
            return 1;
        }
        if (tcsetpgrp(0, getpid()) < 0) {
            perror("tcsetpgrp failed");
            return 1;
        }
        if (signal(SIGTTOU, SIG_DFL) == SIG_ERR) {
            perror("signal failed");
            return 1;
        }
    } else { // error
        perror("Fork failed");
        return 1;
    }
    return 0;
}

/**
 * @brief A function that redirects I/O based
 * on which files in the redirectFiles array are
 * populated with a file name.
 *
 * @param redirectFiles An array of char pointers that indicate whether
 * there is a specific redirect in the command
 * @return Returns 0 on success and -1 on failure.
 */
int handleRedirect(const char **redirectFiles) {
    if (redirectFiles[0]) { // <
        if (redirectIO(redirectFiles[0], O_RDONLY, 
                       0644, STDIN_FILENO, -1) == -1) {
            fputs("handleRedirect failed!\n", stderr);
            return -1;
        }
    }
    if (redirectFiles[1]) { // >
        if (redirectIO(redirectFiles[1], O_WRONLY | 
                      O_CREAT | O_TRUNC, 0644, STDOUT_FILENO, -1) == -1) {
            fputs("handleRedirect failed!\n", stderr);
            return -1;
        }
    }
    if (redirectFiles[2]) { // >>
        if (redirectIO(redirectFiles[2], O_WRONLY | 
                    O_CREAT | O_APPEND, 0644, STDOUT_FILENO, -1) == -1) {
            fputs("handleRedirect failed!\n", stderr);
            return -1;
        }
    }
    if (redirectFiles[3]) { // 2>
        if (redirectIO(redirectFiles[3], O_WRONLY | 
                    O_CREAT | O_TRUNC, 0644, STDERR_FILENO, -1) == -1) {
            fputs("handleRedirect failed!\n", stderr);
            return -1;
        }
    }
    if (redirectFiles[4]) { // &>
        if (redirectIO(redirectFiles[4], O_WRONLY | O_CREAT | O_TRUNC, 
                       0644, STDOUT_FILENO, STDERR_FILENO) == -1) {
            fputs("handleRedirect failed!\n", stderr);
            return -1;
        }
    }
    return 0;
}

/**
 * @brief A function that is called by handleRedirect to 
 * actually create the redirection.
 *
 * @param file The name of the file redirecting to
 * @param flags The flags to be passed to open()
 * @param mode The mode to be passed to open()
 * @param FILENO File descriptor of the I/O stream
 * @param FILENO2 Nonnegative if a second stream needs 
 * to be redirected otherwise it is a negative number if not needed
 * @return returns 0 on success and -1 on failure
 */
int redirectIO(const char *file, int flags, mode_t mode, 
               int FILENO, int FILENO2) {
    int fd = open(file, flags, mode);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }
    if (dup2(fd, FILENO) == -1) {
        perror("dup2 failed");
        return -1;
    }

    // Check for second file descriptor to dup
    if (FILENO2 >= 0) {
        if (dup2(fd, FILENO2) == -1) {
            perror("dup2 failed");
            return -1;
        }
    }

    if (close(fd) == -1) {
        perror("close failed");
        return -1;
    }
    return 0;
}

/**
 * @brief A function to handle the edge case of stderr redirect
 * in the case of a cd failure. It will write the error message
 * based on the errno set by chdir in the specified file. Instead
 * of actually redirecting, this function will just open and write
 * to the file that was pointed to by the redirect command.
 *
 * @param file The file to write the error message to.
 * @return 
 */
int handleCdRedirect(const char *file) {
    int fd = open(file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open failed");
        return -1;
    }
    const char *errMsg = strerror(errno);
    size_t errMsgLen = strlen(errMsg);
    if (write(fd, errMsg, errMsgLen) == -1) {
        perror("write failed");
        return -1;
    }
    if (write(fd, "\n", 1) == -1) {
        perror("write failed");
        return -1;
    }
    if (close(fd) == -1) {
        perror("close failed");
        return -1;
    }
    return 0;
}

/**
 * @brief Opens the file given the filename as the
 * parameter to ensure that a file is created. If the
 * file does not already exist it is created. Immediately
 * closes the file afterwards since we are not doing anything
 * else.
 *
 * @param file The name of the file being opened or created.
 * @return On success, 0 is returned. On failure -1 is returned.
 */
int createFile(const char *file) {
    int fd = open(file, O_RDONLY | O_CREAT, 0644);

    if (fd == -1) {
        perror("open failed");
        return -1;
    }

    if (close(fd) == -1) {
        perror("close failed");
        return -1;
    }

    return 0;
}

/**
 * @brief Checks if pipe character exists in line. Takes
 * the first occurence of the character and replaces with a
 * null terminating character. Address after this character
 * set as line2. User will know if a pipe was found by checking
 * if line2 is still NULL in the function that called checkPipe.
 *
 * @param line The line entered into the shell
 * @param line2 The address to store the second part of the pipe
 */
void checkPipe(char *line, char **line2) {
    char *pipeLoc = strchr(line, '|');
    if (pipeLoc != NULL) {
        *pipeLoc = '\0';
        // Checks case when | is last character to prevent seg fault
        if (*(pipeLoc + 1) == '\0') {
            fputs("invalid pipe usage!\n", stderr);
            exit(1);
        }
        *line2 = pipeLoc + 1;
    }
}

/**
 * @brief Creates two child processes to handle the command with
 * a pipe. One process will run the command on the left side of the
 * pipe. The other process will run the command on the right side of
 * the pipe. The parent will close both ends of the pipe and wait
 * for both children to end to collect and record the exit status
 * of the child running the command to the right of the pipe.
 *
 * @param tokens Array of strings inputted before the pipe
 * @param tokens2 Array of strings inputted after the pipe
 * @param exitStatus Stores the exit status of the child process
 * @param redirectFiles An array of char pointers that indicate whether
 * there is a specific redirect left of the pipe
 * @param redirectFiles2 An array of char pointers that indicate whether
 * there is a specific redirect right of the pipe
 * @return Returns 0 on success. Otherwise returns 1 to indicate an issue
 * occurred.
 */
int execPipe(char *const *tokens, char *const *tokens2, int *exitStatus,
                const char **redirectFiles, const char **redirectFiles2) {
    int pipefd[2];
    pid_t left, right;

    if (pipe(pipefd) == -1) {
        perror("pipe failed");
        return 1;
    }
    // fork to exec first part of the pipe input (left-side)
    left = fork();
    if (left == 0) { // child
        pipeChild(tokens, redirectFiles, pipefd, STDOUT_FILENO);
    } else if (left < 0) { // fork failed
        perror("Fork failed");
        return 1;
    }
    // fork to exec second part of the pipe input (right-side)
    right = fork();
    if (right == 0) { // child
        pipeChild(tokens2, redirectFiles2, pipefd, STDIN_FILENO);
    } else if (right < 0) { // fork failed
        perror("Fork failed");
        return 1;
    }

    if (close(pipefd[0]) == -1) {
        perror("close failed");
        return 1;
    }
    if (close(pipefd[1]) == -1) {
        perror("close failed");
        return 1;
    }

    int leftStatus, rightStatus, waitRes;
    errno = 0;

    while ((waitRes = waitpid(left, &leftStatus, 0)) ==
        -1 && errno == EINTR) {
        // keeps waiting if SIGINT interrupted wait
    }
    if (waitRes == -1) {
        perror("waitpid failed");
        return 1;
    }

    while ((waitRes = waitpid(right, &rightStatus, 0)) ==
        -1 && errno == EINTR) {
        // keeps waiting if SIGINT interrupted wait
    }
    if (waitRes == -1) {
        perror("waitpid failed");
        return 1;
    }

    // Gets and records exit status of child executing rightside of pipe
    if (WIFEXITED(rightStatus)) {
        *exitStatus = WEXITSTATUS(rightStatus);
    } else if (WIFSIGNALED(rightStatus)) {
        *exitStatus = 128 + WTERMSIG(rightStatus);
    }

    return 0;
}

/**
 * @brief Resets the disposition for SIGINT and SIGQUIT signals
 * back to default.
 *
 * @return Returns 0 on success and 1 on failure.
 */
int resetDisposition() {
    // set child's dispostion for SIGINT and SIGQUIT back to default
    struct sigaction sa_default;
    sa_default.sa_handler = SIG_DFL;
    sigemptyset(&sa_default.sa_mask);
    sa_default.sa_flags = 0;

    if (sigaction(SIGINT, &sa_default, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }
    if (sigaction(SIGQUIT, &sa_default, NULL) == -1) {
        perror("sigaction failed");
        return -1;
    }

    return 0;
}

/**
 * @brief Performs the actions expected of a subprocess created
 * to run the command of one side of a pipe. Dups stdout to write
 * end of the pipe or stdin to read end of the pipe based on the
 * the fileno parameter that was passed in. Then both ends of the
 * pipe are closed. The dispositions for signals SIGINT and SIGQUIT
 * are reset to default. Redirects in the command are handled by
 * calling handleRedirect. Finally the command is executed along
 * with parameters. Each of these steps are also checked for
 * error values and handled appropriately.
 *
 * @param tokens The tokens for the command
 * @param redirectFiles An array of char pointers taht indicate whether
 * there is a specific redirect in the command
 * @param pipefd Array storing the file descriptors for the read and write
 * end of the pipe created
 * @param fileno The file descriptor that is being duped for proper
 * communication of inputs and outputs of the piped commands
 */
void pipeChild(char *const *tokens, const char **redirectFiles, int *pipefd,
               int fileno) {
    if (fileno == STDOUT_FILENO) {
        if (dup2(pipefd[1], STDOUT_FILENO) == -1) {
            perror("dup2 failed");
            _exit(1);
        }
    } else if (fileno == STDIN_FILENO) {
        if (dup2(pipefd[0], STDIN_FILENO) == -1) {
            perror("dup2 failed");
            _exit(1);
        }
    } else {
        fputs("invalid fileno for pipeChild()\n", stderr);
        _exit(1);
    }

    if (close(pipefd[0]) == -1) {
        perror("close failed");
        _exit(1);
    }
    if (close(pipefd[1]) == -1) {
        perror("close failed");
        _exit(1);
    }

    if (resetDisposition() == -1) {
        fputs("resetDispositionfailed!\n", stderr);
        _exit(1);
    }

    if (handleRedirect(redirectFiles) == -1) {
        fputs("handleRedirects failed!\n", stderr);
        _exit(1);
    }

    int execute = execvp(tokens[0], tokens);
    if (execute == -1) {
        perror("execvp failed");
        _exit(1);
    }
}
