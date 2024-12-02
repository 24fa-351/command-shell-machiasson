#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <ctype.h>

void set_var(char *name, char *value) {
    setenv(name, value, 1);
}
void unset_var(char *name) {
    unsetenv(name);
}
char* echo(char *command) {
    char *echoed = malloc(strlen(command) + 1);
    int i = 0, j = 0;
    while (command[i] != '\0') {
        if (command[i] == '$' && command[i+1] != '\0') {
            char name[256];
            int length = 0;
            i++;
            while (command[i] != '\0' && (isalnum(command[i]) || command[i] == '_')) {
                name[length++] = command[i++];
            }
            name[length] = '\0';
            char *value = getenv(name);
            if (value != NULL) {
                strcpy(&echoed[j], value);
                j += strlen(value);
            }
        } else {
            echoed[j++] = command[i++];
        }
    }
    echoed[j] = '\0';
    return echoed;
}
void cd(char *path) {
    if (chdir(path) != 0) {
        perror("xsh: cd");
    }
}
void pwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        printf("%s\n", cwd);
    } else {
        perror("xsh: pwd");
    }
}
void execute_command(char *command, int background) {
    pid_t pid = fork();
    if (pid == 0) {
        char *args[100];
        char *token = strtok(command, " ");
        int i = 0;
        while (token != NULL) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;
        int input_file = -1, output_file = -1;
        for (int j = 0; args[j] != NULL; j++) {
            if (strcmp(args[j], "<") == 0) {
                input_file = open(args[j + 1], O_RDONLY);
                if (input_file == -1) {
                    perror("xsh: open input file");
                    exit(1);
                }
                args[j] = NULL;
            }
            if (strcmp(args[j], ">") == 0) {
                output_file = open(args[j + 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (output_file == -1) {
                    perror("xsh: open output file");
                    exit(1);
                }
                args[j] = NULL;
            }
        }
        if (input_file != -1) {
            dup2(input_file, STDIN_FILENO);
            close(input_file);
        }
        if (output_file != -1) {
            dup2(output_file, STDOUT_FILENO);
            close(output_file);
        }
            execvp(args[0], args);        
    } else {
        if (!background) {
            waitpid(pid, NULL, 0);
        }
    }
}
void handle_pipe(char *command) {
    char *pipe_command[255];
    int i = 0;
    char *token = strtok(command, "|");
    while (token != NULL) {
        pipe_command[i++] = token;
        token = strtok(NULL, "|");
    }
    pipe_command[i] = NULL;
    int pipe_fd[2];
    int fd_in = 0;
    for (int j = 0; pipe_command[j] != NULL; j++) {
        pipe(pipe_fd);
        pid_t pid = fork();
        if (pid == 0) {
            if (pipe_command[j+1] != NULL) {
                dup2(pipe_fd[1], STDOUT_FILENO);
            }
            dup2(fd_in, STDIN_FILENO);
            close(pipe_fd[0]);
            close(pipe_fd[1]);
            execute_command(pipe_command[j], 0);
            exit(0);
        } else {
            wait(NULL);
            close(pipe_fd[1]);
            fd_in = pipe_fd[0];
        }
    }
}
int main() {
    char command[1024];
    char *user_input;
    while (1) {
        char cwd[1024];
        getcwd(cwd, sizeof(cwd));
        printf("xsh# %s ", cwd);
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;
        if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0) {
            break;
        }
        if (strncmp(command, "set ", 4) == 0) {
            char *name = strtok(command + 4, " ");
            char *var_value = strtok(NULL, " ");
            set_var(name, var_value);
        } else if (strncmp(command, "unset ", 6) == 0) {
            unset_var(command + 6);
        } else if (strncmp(command, "cd ", 3) == 0) {
            cd(command + 3);
        } else if (strcmp(command, "pwd") == 0) {
            pwd();
        } else if (strncmp(command, "echo ", 5) == 0) {
            char *echoed = echo(command + 5);
            printf("%s\n", echoed);
            free(echoed);
        } else if (strchr(command, '|') != NULL) {
            handle_pipe(command);
        }
    }
    return 0;
}