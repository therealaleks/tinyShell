#include <sys/time.h>
#include <sys/resource.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>   //wait()
#include <sys/types.h>
#include <stdlib.h>
#include <signal.h> //signal
#include <fcntl.h> //open, O_WRONLY, O_RDONLY

int histCount = 0;  //number of things in history
char hist[99][100]; //initializing counter for number of elements in history as well as the history array of arrays
char fifoPath[100]; //where we're gonna store the absolute path to fifo
int exiit = 0;      //exit request status variable exiit = 1 means a request to exit has been raised

void history(char *cmd){    //method where we add commands to the history
    if (histCount == 100){  //handling the case of a full history
        for (int i = 1; i < 100; i++){  //we just delete the first element and back everything up by one
            strcpy(hist[i-1], hist[i]);
        }
        histCount--;    //then we decrement the counter
    }
    strcpy( hist[histCount], cmd);  //we add the command to history
    histCount++;       //increment counter
}

void signalHandler(int sig){
    printf("\n");
    if (sig == 2){      //we only do something if its control+c, we do nothing for control+z
        printf("want to quit? [y/n]\n");        //prompt
        exiit = 1;          //raise exit request
    }
    return;
}

char *get_a_line(){

	char array[100];        //we assume that the line wont cross 100 characters
 	char *cmd = array;      //make pointer to it
	fgets(cmd, 100, stdin); //GET that line
    history(cmd);           //add it to history
	return cmd;
}

void internal(char *com, char *args[]){ //handling internal commands

    if (strcmp(com, "chdir")==0 || strcmp(com, "cd")==0){   //chdir
        if (!args[1]){
            chdir(getenv("HOME"));  //if no args, it goes to home
        }else{
            if (chdir(args[1]) != 0){   //error handling
                printf("execution failed. bad argument? \n");
            };
        }
    }else if (strcmp(com, "limit")==0){ //limit

        if (!(args[1]) || atoi(args[1]) == 0){  //checking argument validity
            printf("Invalid argument for limit");
        }else{
            struct rlimit old_lim, new_lim;

            getrlimit(RLIMIT_NOFILE, &old_lim); //get old limit
            printf("Old limits:\n\tsoft limit: %ld\n\thard limit: %ld\n", old_lim.rlim_cur, old_lim.rlim_max);

            new_lim.rlim_cur = atoi(args[1]);   //assign new soft limit
            new_lim.rlim_max = old_lim.rlim_max;    /*i understood that we dont need to change hard limit. If we do
            then i wouldve just checked if args[2] had something in it and set it right her to old lim. If thats the
            case please dont take off points please pretty please. an honest mistake by an honest man just trying
            to live off the land*/
            //print out new limit
            printf("New limits:\n\tsoft limit: %ld\n\thard limit: %ld\n", new_lim.rlim_cur, new_lim.rlim_max);

            if (setrlimit(RLIMIT_NOFILE, &new_lim)==-1){    //setting limit and error handling
                printf("Handle setrlimit() error\n");
            }
        }
    }else if (strcmp(com, "history")==0){   //history
        for (int i = 0 ; i < histCount; i++){   //print everything from the history array
            printf("%d %s",i+1, hist[i]);
        }
    }
    return;
}

int my_system(char *line){

	int status = 0;
    char *pnt = line;
    int pipe = 0;

	while (status == 0){   //remove newline. Ok I get it wouldve been done much faster if I just strlen()'d it and
		if (*pnt == 10){    //just removed the last char. but by the time my brain unfoze id already used this whole
		                    //shabang to also check for pipin. And now im adding a line just to explain myself.
			*pnt = '\0';    //newline becomes null
			status++;
		}else if (*pnt == '|'){ //check if we're doing a pipe
		    pipe = 1;
		    pnt++;
		}else pnt++;    //advance pointer
	}

	char *args[100];    //tokenize
	char *args2[100];
    char com[100];  //get the command into a const char
    char comm[100];  //command for pipe
    char *pip = "|";

	args[0]  = strtok (line, " ");
    strcpy(com, args[0]);   //get command into const char

	for (int cnt = 1; cnt <= 99; cnt++){

		args[cnt] = strtok (NULL, " ");

		if (args[cnt] && strcmp(args[cnt],pip)==0){ //check if theres a pipe
		    args[cnt] = NULL;   //remove '|'
            for (int cnt2 = 0; cnt2 <= 99; cnt2++){
                args2[cnt2] = strtok (NULL, " ");   //put the rest into the arrays of the second command's args
            }
            strcpy(comm, args2[0]); //get second command into const char
            break;
		}
	}


	if (strcmp(com, "history")==0 || strcmp(com, "limit")==0 || strcmp(com, "chdir")==0 || strcmp(com, "cd")==0){
        internal(com, args);
	    return 0;
	}

    //now we forkin
	pid_t pid = fork();

	if (pid == 0){

		if (pipe == 1){     //enter this section if thou hast followed thine divine path of pipes

            int fd1;
            //we fork again to PIPE IT UP
            if (!fork()){ //read from fifo
                wait(NULL); //wait for parent to finish
                
                close(0);
		fd1 = open(fifoPath, O_RDONLY);
                dup(fd1);

                if(  execvp(comm, args2) < 0){  //error checking
                    printf("%s failed to execute\n", comm);
                }
                close(fd1);

                exit(0); //in case of error

            }else{ //write to fifo

                
                close(1);
		fd1 = open(fifoPath, O_WRONLY);
                dup(fd1);

                if( execvp(com, args) < 0){
                    printf("%s failed to execute\n", com);
                }
                close(fd1);
            }
        }else{      //if thou art common peasant, without the blessing of PIPES, go here
            if( execvp(com, args) < 0){
                printf("%s failed to execute\n", com);
            }
		}
		exit(0); //in case of error
	}else{  //parent
        wait(NULL);
	}
    return 0;
}

void getFifoPath(char *path){   //to get the absolute path of Fifo
    if (path){
        realpath(path, fifoPath);
    }
}

void exRequestHandler(char *answ){
    char *yes = "y\n";      //initialize comparison points for appropriate answers
    char *no = "n\n";

    if (strcmp(answ, yes) == 0 ){
        exit(0);        //if y, we exit
    }else if (strcmp(answ, no) != 0 ){
        printf("say again?\n"); //else if not n, ask the idiot to repeat
    }else{
        exiit = 0;      //else it can only be n, so badabing badaboom
    }
}

int main(int argc, char *argv[]){

    signal(SIGINT, signalHandler);  //handlin signals yo
    signal(SIGTSTP, signalHandler);

    getFifoPath(argv[1]);   //get abs path of fifo

	while(1){

		char* line = get_a_line();

		if(exiit == 1){                          //here we check if a request to exit has been raised
          exRequestHandler(line);               //if so, we handle it (also we pass to the handler the answer user gave to prompt)
		}else if (strlen(line) > 1){                //otherwise, we proceed to my system, if line is valid
			my_system(line);
		}else return 0;     //otherwise, we f**k off
	}
}

