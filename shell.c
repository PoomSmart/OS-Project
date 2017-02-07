#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAXH 2000
#define MAXSHOW 100
#define MAXL 500
#define MAXARG 100
#define COMMAND_FAILED "Command execution failed, exiting."
#define min(a, b) (a < b ? a : b)
#define max(a, b) (a > b ? a : b)

char *trim(char *);
void parse(char *, char **);
int hisc = 0;

int main()
{
	char *line;
	char history[MAXH][MAXL];
	char *argv[MAXARG];
	int len;
	while (1) {
		fflush(NULL);
		printf("shell.c$ ");
		if (line) free(line);
		line = (char *)malloc(sizeof(char) * MAXL);
		if (fgets(line, MAXL, stdin)) { // wait for user to input
			strcpy(line, trim(line)); // trim the input
			if ((len = strlen(line)) == 0) continue;
			if (strcmp(line, "!!") == 0)
				strcpy(line, history[min(hisc, MAXSHOW) - 1]);
			if (len > 1 && *line == '!') {
				int i = 1;
				while (i < len && isdigit(line[i])) i++;
				if (i == len) {
					// retrieve n-th history command line by !n
					line++;
					i = atoi(line);
					if (i > hisc) {
						puts("Command at this line not found in the history.");
						continue;
					}
					line--;
					strcpy(line, history[(hisc > MAXSHOW) ? (i - 1 - hisc + MAXSHOW) : (i - 1)]);
				}
			}
			if (hisc >= MAXSHOW) { // history limit
				for (int i = 0; i < MAXSHOW - 1; i++)
					strcpy(history[i], history[i + 1]);
				strcpy(history[MAXSHOW - 1], line);
				hisc++;
			} else
				strcpy(history[hisc++], line); // save line to history
			if (strcmp(line, "exit") == 0)
				exit(EXIT_SUCCESS); // user exit
			else if (strcmp(line, "history") == 0) {
				// print history
				for (int i = 0; i < min(hisc - 1, MAXSHOW - 1); i++)
					printf("%d. %s\n", max(i + 1, i + 1 + hisc - MAXSHOW), history[i]);
			} else {
				int pid = fork(); // fork a child
				if (pid == 0) {
					// let the to-be-target-binary child process arguments
					parse(line, argv);
					execvp(*argv, argv);
					// command not found in search path, exiting
					exit(EXIT_FAILURE);
				} else if (pid > 0) {
					int status;
					wait(&status);
					if (WEXITSTATUS(status)) {
						// exit because faulty command
						puts(COMMAND_FAILED);
						exit(EXIT_FAILURE);
					}
				} else {
					puts("fork() failed.");
					exit(EXIT_FAILURE);
				}
			}
		}
	}
 	return EXIT_SUCCESS;
}

char *trim(char *s)
{
	size_t size = strlen(s);
	if (!size) return s;
	if (s[size - 1] == '\n') s[size - 1] = '\0';
	char *end = s + size - 1;
	while (end >= s && isspace(*end))
		end--; // remove trailing space
	*(end + 1) = '\0';
	while (*s && isspace(*s))
		s++; // remove leading space
	return s;
}

void parse(char *line, char **argv)
{
	char *p = strtok(line, " \t\n");
	while (p) {
		*argv++ = p; // save each token
		p = strtok(NULL, " \t\n");
	}
}
