
/* 
 * tsh - A tiny shell program with job control
 * <The line above is not a sufficient documentation.
 *  You will need to write your program documentation.
 *  Follow the 15-213/18-213/15-513 style guide at
 *  http://www.cs.cmu.edu/~213/codeStyle.html.>
 *
*/

#include "tsh_helper.h"

/*
 * If DEBUG is defined, enable contracts and printing on dbg_printf.
 */
#ifdef DEBUG
/* When debugging is enabled, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(...) assert(__VA_ARGS__)
#define dbg_assert(...) assert(__VA_ARGS__)
#define dbg_ensures(...) assert(__VA_ARGS__)
#else
/* When debugging is disabled, no code gets generated for these */
#define dbg_printf(...)
#define dbg_requires(...)
#define dbg_assert(...)
#define dbg_ensures(...)
#endif

/* Function prototypes */
void eval(const char *cmdline);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);
void sigquit_handler(int sig);

/* Helper function */
pid_t get_argv_pid(char *argv, struct job_t *jl);
struct job_t *get_bg_jobs(struct job_t *jl);
void print_msg(pid_t pid, int jid, char *msg, long sid);


/*
 * <Write main's function header documentation. What does main do?>
 * "Each function should be prefaced with a comment describing the purpose
 *  of the function (in a sentence or two), the function's arguments and
 *  return value, any error cases that are relevant to the caller,
 *  any pertinent side effects, and any assumptions that the function makes."
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE_TSH];  // Cmdline for fgets
    bool emit_prompt = true;    // Emit prompt (default)

    // Redirect stderr to stdout (so that driver will get all output
    // on the pipe connected to stdout)
    Dup2(STDOUT_FILENO, STDERR_FILENO);

    // Parse the command line
    while ((c = getopt(argc, argv, "hvp")) != EOF)
    {
        switch (c)
        {
        case 'h':                   // Prints help message
            usage();
            break;
        case 'v':                   // Emits additional diagnostic info
            verbose = true;
            break;
        case 'p':                   // Disables prompt printing
            emit_prompt = false;  
            break;
        default:
            usage();
        }
    }

    // Install the signal handlers
    Signal(SIGINT,  sigint_handler);   // Handles ctrl-c
    Signal(SIGTSTP, sigtstp_handler);  // Handles ctrl-z
    Signal(SIGCHLD, sigchld_handler);  // Handles terminated or stopped child

    Signal(SIGTTIN, SIG_IGN);
    Signal(SIGTTOU, SIG_IGN);

    Signal(SIGQUIT, sigquit_handler); 

    // Initialize the job list
    initjobs(job_list);

    // Execute the shell's read/eval loop
    while (true)
    {
        if (emit_prompt)
        {
            printf("%s", prompt);
            fflush(stdout);
        }

        if ((fgets(cmdline, MAXLINE_TSH, stdin) == NULL) && ferror(stdin))
        {
            app_error("fgets error");
        }

        if (feof(stdin))
        { 
            // End of file (ctrl-d)
            printf ("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }
        
        // Remove the trailing newline
        cmdline[strlen(cmdline)-1] = '\0';
        
        // Evaluate the command line
        eval(cmdline);
        
        fflush(stdout);
    } 
    
    return -1; // control never reaches here
}


/* Handy guide for eval:
 *
 * If the user has requested a built-in command (quit, jobs, bg or fg),
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.
 * Note: each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.
 */

void eval(const char *cmdline) 
{
    parseline_return parse_result;     
    struct cmdline_tokens token;
    sigset_t ourmask, pre_mask;
    int out_file = -1, in_file = -1, jid;
    pid_t pid, cur_pid;
    struct job_t *cur_job, *bg_jobs;

    // Parse command line
    parse_result = parseline(cmdline, &token);

    // Ignore empty or error parseline
    if (parse_result == PARSELINE_ERROR || parse_result == PARSELINE_EMPTY)
    {
        return;
    }

    // Block SIGCHLD, SIGINT, SIGTSTP, and copy
    Sigemptyset(&ourmask);
    Sigemptyset(&pre_mask);
    Sigaddset(&ourmask, SIGCHLD);
    Sigaddset(&ourmask, SIGINT);
    Sigaddset(&ourmask, SIGTSTP);
    Sigprocmask(SIG_BLOCK, &ourmask, &pre_mask);

    // Different behavior for command line
    switch (token.builtin)
    {
        // Build-in command, executes it in current process
        // List running and stopped background jobs
        case BUILTIN_JOBS:
            printf("BUILTIN_JOBS\n");
            bg_jobs = get_bg_jobs(job_list);
            if (token.outfile == NULL)
            {
                listjobs(bg_jobs, STDOUT_FILENO);
            }
            else
            {
                out_file = Open(token.outfile, O_RDWR, DEF_MODE);
                listjobs(bg_jobs, out_file);
            }
            //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
            Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
            break;
        // Terminates the shell
        case BUILTIN_QUIT:
            printf("BUILTIN_QUIT\n");
            //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
            Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
            exit(0);
            break;
        // Change a stopped job or a running bg job into a running fg job
        case BUILTIN_FG :
            printf("BUILTIN_FG\n");
            cur_pid = get_argv_pid(token.argv[1], job_list);
            jid = pid2jid(job_list, cur_pid);
            cur_job = getjobjid(job_list, jid);
            cur_job->state = FG;
            Kill(-cur_pid, SIGCONT);
            // int status;
            // if (waitpid(pid, &status, 0) < 0)
            //     unix_error("waitfg: waitpid error");
            while(fgpid(job_list) == cur_pid)
            {
                Sigsuspend(&pre_mask);    
            } 
            //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
            Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
            break;
        // Changed a stopped job into a running bg job
        case BUILTIN_BG:
            printf("BUILTIN_BG\n");
            cur_pid = get_argv_pid(token.argv[1], job_list);
            jid = pid2jid(job_list, cur_pid);
            cur_job = getjobjid(job_list, jid);
            cur_job->state = BG;
            Kill(-cur_pid, SIGCONT);
            printf("[%d] (%d) %s\n", jid, cur_pid, cur_job->cmdline);
            //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
            Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
            break;

        // Forks a child process, loads and runs in context of child
        case BUILTIN_NONE:
            printf("BUILTIN_NONE\n");
            // Child process
            if ((pid = Fork()) == 0)
            {
                printf("Child\n");
                // Unblock
                Setpgid(0, 0);
                Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
                // redirect
                if (token.infile != NULL)
                {
                    in_file = Open(token.infile, O_RDONLY, DEF_MODE);
                    Dup2(in_file, STDIN_FILENO);
                }
                if (token.outfile != NULL)
                {
                    out_file = Open(token.infile, O_WRONLY, DEF_MODE);
                    Dup2(out_file, STDOUT_FILENO);
                }
                Execve(token.argv[0], token.argv, environ);
            }
            else
            {
                // Parent process
                printf("Parent\n");
                Sigprocmask(SIG_BLOCK, &ourmask, &pre_mask);
                // Background process
                if (parse_result == PARSELINE_BG)
                {
                    printf("PARSELINE_BG\n");
                    addjob(job_list, pid, BG, cmdline);
                    jid = pid2jid(job_list, pid);
                    cur_job = getjobjid(job_list, jid);
                    printf("[%d] (%d) %s\n", jid, pid, cur_job->cmdline);
                }
                // Foreground process
                else
                {
                    printf("PARSELINE_FG\n");
                    addjob(job_list, pid, FG, cmdline);
                    //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
                    // waits for SIGCHLD
                    // int status;
                    // while (waitpid(pid, &status, 0) > 0)
                    // {
                    //     printf("wait\n");
                    //     deletejob(job_list, pid);
                    // }
                    // if (errno != ECHILD)
                    // {
                    //     unix_error("waitfg: waitpid error\n");
                    // }
                   while(pid == fgpid(job_list))
                   {
                       Sigsuspend(&pre_mask);
                   }
                }
                //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
                Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
            }
            break;
        default:
            //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
            Sigprocmask(SIG_UNBLOCK, &ourmask, NULL);
            break;
    }
    // close file
    if (in_file >= 0)
    {
        Close(in_file);
        Dup2(dup(STDIN_FILENO), STDIN_FILENO);
    }
    if (out_file >= 0)
    {
        Close(out_file);
        Dup2(dup(STDOUT_FILENO), STDOUT_FILENO);
    }
    return;
}

/*
 * Get pid from args
 */
pid_t get_argv_pid(char *argv, struct job_t *jl)
{
    pid_t pid;
    if(argv[0] != '%')
    {
        pid = (pid_t)atoi(argv);
    }
    else
    {
        int jid = atoi(++argv);
        struct job_t *job = getjobjid(jl, jid);
        pid = job->pid;
    }
    return pid;
}

/*
 * Get running and stopped background jobs
 */
struct job_t *get_bg_jobs(struct job_t *jl)
{
    int i = 0;
    int j = 0;
    struct job_t *bg_list = malloc(MAXJOBS * sizeof(struct job_t));
    if (bg_list == NULL)
    {
        fprintf(stderr, "Error: malloc fails\n");
        return bg_list;
    }
    for (i = 0; i < MAXJOBS; i++)
    {
        if (jl[i].state == BG  || jl[i].state == ST )
        {
            bg_list[j] = jl[i];
            j += 1;
        }

    }
    return bg_list;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * <What does sigchld_handler do?>
 */
void sigchld_handler(int sig) 
{
    Sio_puts("sigchld_han");
    pid_t pid;
    int status, jid;
    struct job_t *cur_job;
    int olderr = errno;
    sigset_t mask, pre_mask;
    Sigemptyset(&mask);
    Sigemptyset(&pre_mask);
    Sigaddset(&mask, SIGCHLD);
    Sigaddset(&mask, SIGINT);
    Sigaddset(&mask, SIGTSTP);
    Sigprocmask(SIG_BLOCK, &mask, &pre_mask);

    // No child/stopped child return immediately
    while((pid = waitpid(-1, &status, WNOHANG|WUNTRACED)) > 0)
    {
        // Sigprocmask(SIG_BLOCK, &mask, &pre_mask);
        // child terminated normally
        if (WIFEXITED(status))
        {
            cur_job = getjobpid(job_list, pid);
            jid = pid2jid(job_list, pid);
            cur_job->state = ST;
            char msg[] = "stopped by signal ";
            print_msg(pid, jid, msg, (long)(SIGTSTP));
        }
        // terminated by uncaught signal
        else if (WIFSIGNALED(status) && WTERMSIG(status) == SIGINT)
        {
            jid = pid2jid(job_list, pid);
            char msg[] = "terminated by signal ";
            print_msg(pid, jid, msg, (long)(SIGINT));
            deletejob(job_list, pid);
        }
        else
        {
            deletejob(job_list, pid);
        }
        // Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
    }
    if (errno != ECHILD)
    {
        Sio_error("waitpid error in sigchld_handler\n");
    }
    //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
    errno = olderr;
    return;
}

/*
 * Safe print "Job [jid] (pid) msg sid" to screen
 */
void print_msg(pid_t pid, int jid, char *msg, long sid)
{
    Sio_puts("Job [");
    Sio_putl((long)(jid));
    Sio_puts("] (");
    Sio_putl((long)(pid));
    Sio_puts(") ");
    Sio_puts(msg);
    Sio_putl(sid);
    Sio_puts("\n");
}

/* 
 * <What does sigint_handler do?>
 */
void sigint_handler(int sig) 
{
    Sio_puts("sigint_han");
    int olderr = errno;
    sigset_t mask, pre_mask;
    Sigemptyset(&mask);
    Sigemptyset(&pre_mask);
    Sigaddset(&mask, SIGCHLD);
    Sigaddset(&mask, SIGTSTP);
//    Sigaddset(&mask, SIGINT);
    Sigprocmask(SIG_BLOCK, &mask, &pre_mask);
    pid_t pid = fgpid(job_list);
    if (pid != 0)
    {
        Kill(-pid, SIGINT);
    }
    char msg[] = "terminated by signal ";
    print_msg(pid, pid2jid(job_list, pid), msg, (long)(sig));
    //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
    errno = olderr;
    return;
}

/*
 * <What does sigtstp_handler do?>
 */
void sigtstp_handler(int sig)
{
    Sio_puts("sigstp_han");
    int olderr = errno;
    sigset_t mask, pre_mask;
    Sigemptyset(&mask);
    Sigemptyset(&pre_mask);
    Sigaddset(&mask, SIGCHLD);
//    Sigaddset(&mask, SIGTSTP);
    Sigaddset(&mask, SIGINT);
    Sigprocmask(SIG_BLOCK, &mask, &pre_mask);
    pid_t pid = fgpid(job_list);
    if (pid != 0)
    {
        Kill(-pid, SIGTSTP);
    }
    char msg[] = "stopped by signal ";
    print_msg(pid, pid2jid(job_list, pid), msg, (long)(sig));
    //Sigprocmask(SIG_SETMASK, &pre_mask, NULL);
    Sigprocmask(SIG_UNBLOCK, &mask, NULL);
    errno = olderr;
    return;
}

