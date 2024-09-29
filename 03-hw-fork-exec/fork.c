
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>
int main(int argc, char *argv[]) {
	int pid;
	int pipefd[2]; 

   	if (pipe(pipefd) == -1) {
       		perror("pipe");
       	 	exit(1);
    	}

	printf("Starting program; process has pid %d\n", getpid());
	FILE *file = fopen("fork-output.txt", "w");
	fprintf(file, "BEFORE FORK (%d)\n", fileno(file));
	fflush(file);

	if ((pid = fork()) < 0) {
		fprintf(stderr, "Could not fork()");
		exit(1);
	}

	/* BEGIN SECTION A */

	printf("Section A;  pid %d\n", getpid());
//	sleep(5);

	/* END SECTION A */
	if (pid == 0) {
		/* BEGIN SECTION B */
		printf("Section B \n");
		write(pipefd[1], "hello from Section B\n", 22);
                close(pipefd[1]);
 		if (dup2(fileno(file), STDOUT_FILENO) < 0) {
            		perror("dup2 failed");
            		exit(1);
       		 }
		char *newenviron[] = { NULL };

        	printf("Program \"%s\" has pid %d. Sleeping.\n", argv[0], getpid());
//        	sleep(30);

        	if (argc <= 1) {
                	printf("No program to exec.  Exiting...\n");
                	exit(0);
        	}

        	printf("Running exec of \"%s\"\n", argv[1]);
        	execve(argv[1], &argv[1], newenviron);
        	printf("End of program \"%s\".\n", argv[0]);
		close(pipefd[0]);
		sleep(10);
		const char *message = "hello from Section B\n";
        	ssize_t result = write(pipefd[1], message, 22);
		if (result == -1) {
            		perror("write");
            		exit(1);
        	}
//        	write(pipefd[1], "hello from Section B\n", 22);
// 	      	sleep(10);
//		close(pipefd[1]);
//		sleep(5);
	//	fprintf(file,"Section B(%d)\n",fileno(file));
//		sleep(30);
//		sleep(30);
//		printf("Section B done sleeping\n");

		exit(0);

		/* END SECTION B */
	} else {
		/* BEGIN SECTION C */
		printf("Section C\n");
		close(pipefd[1]);
		char buffer[100]; // Buffer to hold the read data
       		ssize_t bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1);
        	buffer[bytesRead] = '\0'; 

        	printf("Received %ld bytes from the pipe.\n", bytesRead);
        	printf("Message: %s", buffer); 

		bytesRead = read(pipefd[0], buffer, sizeof(buffer) - 1); 
                buffer[bytesRead] = '\0';
                printf("Received %ld bytes from the pipe.\n", bytesRead);
        	close(pipefd[0]); 
        	fclose(file);
//		fprintf(file,"Section C(%d)\n",fileno(file));
//		fclose(file);
//		sleep(5);
//		wait(NULL);  
//		sleep(30);
//		printf("Section C done sleeping\n");

		exit(0);

		/* END SECTION C */
	}
	/* BEGIN SECTION D */

	printf("Section D\n");
//	sleep(30);

	/* END SECTION D */
}

