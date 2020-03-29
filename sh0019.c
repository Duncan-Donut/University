#include "sh0019.h"
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>


// struct command
//    Data structure describing a command. Add your own stuff.

typedef struct command command;
struct command* head = NULL; 
int pipefd[2];
int temp[2];
int firstrun = 0;
                   
struct command {
    int argc;      // number of arguments
    int background; //background
    char** argv;   // arguments, terminated by NULL
    pid_t pid;     // process ID running this command, -1 if none
    int conditional;
    int pipe;
    struct command* next; 
};
 

// command_alloc()
//    Allocate and return a new command structure.

static command* command_alloc(void) {
    command* c = (command*) malloc(sizeof(command));
    c->argc = 0;
    c->argv = NULL;
    c->background = 0;
    c->pid = -1;
    c->next = NULL;
    c->conditional = 0;
    c->pipe = 0;
    return c;
}

// command_free(c)
//    Free command structure `c`, including all its words.

static void command_free(command* c) {
    for (int i = 0; i != c->argc; ++i) {
        free(c->argv[i]);
    }
    free(c->argv);
    free(c);
}

// command_append_arg(c, word)
//    Add `word` as an argument to command `c`. This increments `c->argc`
//    and augments `c->argv`.

static void command_append_arg(command* c, char* word) {
    c->argv = (char**) realloc(c->argv, sizeof(char*) * (c->argc + 2));
    c->argv[c->argc] = word;
    c->argv[c->argc + 1] = NULL;
    ++c->argc;
}


// COMMAND EVALUATION

// start_command(c, pgid)
//    Start the single command indicated by `c`. Sets `c->pid` to the child
//    process running the command, and returns `c->pid`.
//
//    PART 1: Fork a child process and run the command using `execvp`.
//    PART 5: Set up a pipeline if appropriate. This may require creating a
//       new pipe (`pipe` system call), and/or replacing the child process's
//       standard input/output with parts of the pipe (`dup2` and `close`).
//       Draw pictures!
//    PART 7: Handle redirections.
//    PART 8: The child process should be in the process group `pgid`, or
//       its own process group (if `pgid == 0`). To avoid race conditions,
//       this will require TWO calls to `setpgid`.


pid_t start_command(command* c, pid_t pgid) {
    (void) pgid;
  
    //Piped command
    if(c->pipe == 1){

        c->pid=fork();

        //printf("This is child and this is firstrn %d \n",firstrun);

        if (c->pid == 0) {
            if(firstrun == 1){
                dup2(temp[0],0);
            }
            dup2(pipefd[1],1);  //Child's pipefd[1] now contains stdout pipe 1
            // close fds
            close(pipefd[0]);
            close(pipefd[1]);
            execvp(*(c->argv),c->argv);  //Command executes
        }
        firstrun = 1;
    }
    else if(c->pipe == 2){
        
        c->pid=fork();
        
        if (c->pid == 0) {  
            if(firstrun == 1){
                dup2(temp[0],0);
            }else{
                dup2(pipefd[0],0); //Child reading from pipefd[0]
            }  
           

            close(pipefd[0]);
            close(pipefd[1]);
            execvp(*(c->argv),c->argv);  //Command executes
        } 
        firstrun = 0; 
    }
    else{
        //Forks a child process
        c->pid = fork();
    
        //If we are the child process, we execute
        if(c->pid == 0){
            int x  = execvp(*(c->argv),c->argv);
    
            if(x == -1){
                _exit(1);
            }
        }
    }

    return c->pid;
}


// run_list(c)
//    Run the command list starting at `c`.
//
//    PART 1: Start the single command `c` with `start_command`,
//        and wait for it to finish using `waitpid`.
//    The remaining parts may require that you change `struct command`
//    (e.g., to track whether a command is in the background)
//    and write code in run_list (or in helper functions!).
//    PART 2: Treat background commands differently.
//    PART 3: Introduce a loop to run all commands in the list.
//    PART 4: Change the loop to handle conditionals.
//    PART 5: Change the loop to handle pipelines. Start all processes in
//       the pipeline in parallel. The status of a pipeline is the status of
//       its LAST command.
//    PART 8: - Choose a process group for each pipeline.
//       - Call `claim_foreground(pgid)` before waiting for the pipeline.
//       - Call `claim_foreground(0)` once the pipeline is complete.

void run_list(command* c) {
    
    struct command *checker = c; 
    struct command *checker_prev = c;

    int status;
    int wifexit;
    int wexitstatus;
    int nothing = 0; 

    pipe(pipefd);
    while (checker != NULL) { 
        if(checker->argv != NULL){

            pid_t returned_pid;
            
            //printf("This is the command %s and the cond value %d background %d and pipe %d\n",*(checker->argv),checker->conditional,checker->background,checker->pipe);

            //Checks if command needs to be piped
            if(checker->pipe == 2){  

                if(firstrun == 1){
                    temp[0] = pipefd[0];
                    temp[1] = pipefd[1];
                    pipe(pipefd);
                }               
                //printf("This is the last child and command %s\n",*(checker->argv));
                if(nothing == 0){


                    returned_pid = start_command(checker, 0);
                    checker->pid = returned_pid;
                    if(checker->background == 0){
                        waitpid(returned_pid,&status,0);
                        wifexit = WIFEXITED(status);
                        wexitstatus = WEXITSTATUS(status);
                    }
                }
                nothing = 0;
            }
            else if(checker->pipe == 1){

                if(checker->conditional == 0){
                    //printf("Piped conditional 0 \n");
                    if(firstrun == 1){
                        temp[0] = pipefd[0];
                        temp[1] = pipefd[1];
                        pipe(pipefd);
                    }
                    returned_pid = start_command(checker, 0);
                    checker->pid = returned_pid;
                    //printf("These are pipe values %d %d \n",pipefd[0],pipefd[1]);
                    close(pipefd[1]);
                } 
                else if(checker->conditional == 2){
                    if(checker->background == 1){
                        //Forks a child process -> backgrounded
                        pid_t child_child_fork = fork();
                        
                        //If we are the child process, we execute
                        if(child_child_fork == 0){
                            //printf("Spawned child process\n");
                            returned_pid = start_command(checker_prev, 0);
                            checker->pid = returned_pid;
                            close(pipefd[1]);
                            waitpid(returned_pid,&status,0);
                                        
                            wifexit = WIFEXITED(status);
                            wexitstatus = WEXITSTATUS(status);

                            if(wexitstatus == 0){
                                returned_pid = start_command(checker, 0);
                                checker->pid = returned_pid;
                                close(pipefd[1]);
                            }
                        _exit(1);  
                        }
                    }
                    else if(wexitstatus == 0){
                        //printf("Conditional 1 second part run normal\n");
                        returned_pid = start_command(checker, 0);
                        checker->pid = returned_pid;
                        close(pipefd[1]);       
                    }
                    else{
                        nothing = 1;
                    } 
                }
                else if(checker->conditional == 4){
                    if(checker->background == 1){
                        //Forks a child process -> backgrounded
                        pid_t child_child_fork = fork();
                        //If we are the child process, we execute
                        if(child_child_fork == 0){
                            //printf("Spawned child process\n");
                            returned_pid = start_command(checker_prev, 0);
                            checker->pid = returned_pid;
                            close(pipefd[1]);
                            waitpid(returned_pid,&status,0);
                                        
                            wifexit = WIFEXITED(status);
                            wexitstatus = WEXITSTATUS(status);

                            if(wexitstatus != 0){
                                returned_pid = start_command(checker, 0);
                                checker->pid = returned_pid;
                                close(pipefd[1]);
                            }
                            _exit(1);  
                        }
                    }
                    else if(wexitstatus != 0){
                        //printf("Conditional 2 second part run normal\n");

                        returned_pid = start_command(checker, 0);
                        checker->pid = returned_pid;
                        close(pipefd[1]);      
                    } 
                    else{
                        nothing = 1;
                    }
                }


            }
            else if(checker->conditional == 0){
                //printf("Conditional 0 \n");
                returned_pid = start_command(checker, 0);
                checker->pid = returned_pid;
                
                if(checker->background == 0){
                    waitpid(returned_pid,&status,0);
                }

                wifexit = WIFEXITED(status);
                wexitstatus = WEXITSTATUS(status);
            } 
            else if(checker->conditional == 1 && checker->background == 0){
                //printf("Conditional 1 first part run normal\n");
                
                returned_pid = start_command(checker, 0);
                checker->pid = returned_pid;

                waitpid(returned_pid,&status,0);
                            
                wifexit = WIFEXITED(status);
                wexitstatus = WEXITSTATUS(status);        
            }
            else if(checker->conditional == 2){
                if(checker->background == 1){
                    //Forks a child process -> backgrounded
                    pid_t child_fork = fork();
                    //If we are the child process, we execute
                    if(child_fork == 0){
                        //printf("Spawned child process\n");
                        returned_pid = start_command(checker_prev, 0);
                        checker->pid = returned_pid;

                        waitpid(returned_pid,&status,0);
                        
                        wifexit = WIFEXITED(status);
                        wexitstatus = WEXITSTATUS(status);

                        if(wexitstatus == 0){
                            returned_pid = start_command(checker, 0);
                            checker->pid = returned_pid;

                            waitpid(returned_pid,&status,0);
                            
                            wifexit = WIFEXITED(status);
                            wexitstatus = WEXITSTATUS(status);
                        }
                        _exit(1);  
                    }
                }
                else if(wexitstatus == 0){
                    //printf("Conditional 1 second part run normal\n");
                    returned_pid = start_command(checker, 0);
                    checker->pid = returned_pid;

                    waitpid(returned_pid,&status,0);
                            
                    wifexit = WIFEXITED(status);
                    wexitstatus = WEXITSTATUS(status);
                    
                }
            }
            else if(checker->conditional == 3 && checker->background == 0){
                //printf("Conditional 2 first part run normal\n");
                
                returned_pid = start_command(checker, 0);
                checker->pid = returned_pid;

                waitpid(returned_pid,&status,0);
                            
                wifexit = WIFEXITED(status);
                wexitstatus = WEXITSTATUS(status);        
            }
            else if(checker->conditional == 4){
                if(checker->background == 1){
                    //Forks a child process -> backgrounded
                    pid_t child_fork = fork();
                    //If we are the child process, we execute
                    if(child_fork == 0){
                        //printf("Spawned child process\n");
                        returned_pid = start_command(checker_prev, 0);
                        checker->pid = returned_pid;

                        waitpid(returned_pid,&status,0);
                        
                        wifexit = WIFEXITED(status);
                        wexitstatus = WEXITSTATUS(status);

                        if(wexitstatus != 0){
                            returned_pid = start_command(checker, 0);
                            checker->pid = returned_pid;

                            waitpid(returned_pid,&status,0);
                            
                            wifexit = WIFEXITED(status);
                            wexitstatus = WEXITSTATUS(status);
                        }
                        _exit(1);  
                    }
                }
                else if(wexitstatus != 0){
                    //printf("Conditional 2 second part run normal\n");

                    returned_pid = start_command(checker, 0);
                    checker->pid = returned_pid;

                    waitpid(returned_pid,&status,0);
                            
                    wifexit = WIFEXITED(status);
                    wexitstatus = WEXITSTATUS(status);
                }
            }

            //printf("This is commands wifexit %d this is wexitstatus %d for command %d\n\n",wifexit,wexitstatus,checker->pid);
        }
        checker_prev = checker;
        checker = checker->next;
    }
}


// eval_line(c)
//    Parse the command list in `s` and run it via `run_list`.

void eval_line(const char* s) {
    int type;
    char* token;

    // build the command
    command* c = command_alloc();
    head = c;

    while ((s = parse_shell_token(s, &type, &token)) != NULL) {

        struct command* temp = command_alloc();

        //Token is |
        if(type == TOKEN_PIPE){
            ////printf("Found |\n");
            c->pipe = 1;
            c->next = temp;

            temp->pipe = 2;
        }
        //Token is &&
        if(type == TOKEN_AND){
            ////printf("Found &&\n");
            if(c->conditional == 2){
                c->conditional = 2;
            }
            else if(c->conditional == 0){
                c->conditional = 1;
            }
            
            c->next = temp;
            
            temp->conditional = 2;
        }
        //Token is ||
        if(type == TOKEN_OR){
            ////printf("Found ||\n");
            if(c->conditional == 4){
                c->conditional = 4;
            }
            else if(c->conditional == 0){
                c->conditional = 3;
            }            
            c->next = temp;
            
            temp->conditional = 4;
        }
        //Token is &
        if(type == TOKEN_BACKGROUND){
            ////printf("Found &\n");
            c->background = 1;
            c->next = temp;
            
        }
        //Token is ;
        if(type == TOKEN_SEQUENCE){
            ////printf("Found ;\n");
            c->next = temp;   
        }
        
        if(type == TOKEN_NORMAL){
            ////printf("This is the command %x and token %s \n",c,token);
            command_append_arg(c, token);
        }

        if(type != TOKEN_NORMAL){
            ////printf("next command %x and token %s \n",c,token);
            c = temp;
        }
    }
    // execute it
    if (head->argc) {
        run_list(head);
    }
    command_free(head);
}

int main(int argc, char* argv[]) {
    FILE* command_file = stdin;
    int quiet = 0;

    // Check for '-q' option: be quiet (print no prompts)
    if (argc > 1 && strcmp(argv[1], "-q") == 0) {
        quiet = 1;
        --argc, ++argv;
    }

    // Check for filename option: read commands from file
    if (argc > 1) {
        command_file = fopen(argv[1], "rb");
        if (!command_file) {
            perror(argv[1]);
            exit(1);
        }
    }

    // - Put the shell into the foreground
    // - Ignore the SIGTTOU signal, which is sent when the shell is put back
    //   into the foreground
    claim_foreground(0);
    set_signal_handler(SIGTTOU, SIG_IGN);

    char buf[BUFSIZ];
    int bufpos = 0;
    int needprompt = 1;

    while (!feof(command_file)) {
        // Print the prompt at the beginning of the line
        if (needprompt && !quiet) {
            //printf("sh0019[%d]$ ", getpid());
            fflush(stdout);
            needprompt = 0;
        }

        // Read a string, checking for error or EOF
        if (fgets(&buf[bufpos], BUFSIZ - bufpos, command_file) == NULL) {
            if (ferror(command_file) && errno == EINTR) {
                // ignore EINTR errors
                clearerr(command_file);
                buf[bufpos] = 0;
            } else {
                if (ferror(command_file)) {
                    perror("sh0019");
                }
                break;
            }
        }

        // If a complete command line has been provided, run it
        bufpos = strlen(buf);
        if (bufpos == BUFSIZ - 1 || (bufpos > 0 && buf[bufpos - 1] == '\n')) {
            eval_line(buf);
            bufpos = 0;
            needprompt = 1;
        }

        // Handle zombie processes and/or interrupt requests
        // Your code here!
    }

    return 0;
}
