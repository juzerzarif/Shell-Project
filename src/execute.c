/**
 * @file execute.c
 *
 * @brief Implements interface functions between Quash and the environment and
 * functions that interpret an execute commands.
 *
 * @note As you add things to this file you may want to change the method signature
 */

#include "execute.h"

#include <stdio.h>
#include <limits.h>

#include "quash.h"

// Remove this and all expansion calls to it
/**
 * @brief Note calls to any function that requires implementation
 */
#define IMPLEMENT_ME() \
  fprintf(stderr, "IMPLEMENT ME: %s(line %d): %s()\n", __FILE__, __LINE__, __FUNCTION__)

/***************************************************************************
 * Global variables
 ***************************************************************************/

int pipes[2][2]; //two pipes for i/o redirection between children
bool currPipe = false; //current pipe being written to - used in create_process

IMPLEMENT_DEQUE_STRUCT(ProcessDeque, pid_t); //Process queue for storing 
IMPLEMENT_DEQUE(ProcessDeque, pid_t);        //PIDs of child processes

typedef struct Job
{
  int jobID;
  pid_t pid;
  char *command;
  ProcessDeque processQueue;
} Job;

IMPLEMENT_DEQUE_STRUCT(JobsDeque, Job); //Job queue for storing 
IMPLEMENT_DEQUE(JobsDeque, Job);        //background jobs

JobsDeque bgJobQueue;

// Destructor for a Job object
void destroyJob(Job job)
{
  if(job.command != NULL)
  {
    free(job.command);
  }

  destroy_ProcessDeque(&job.processQueue);
}

/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char *get_current_directory(bool *should_free)
{
  *should_free = true; //the char* returned by getcwd needs to be free'd
                       //since we didn't pass a buffer size to getcwd

  return getcwd(NULL, 0);
}

// Returns the value of an environment variable env_var
const char *lookup_env(const char *env_var)
{
  return getenv(env_var);
}

// Check the status of background jobs
void check_jobs_bg_status()
{
  size_t jobSize = length_JobsDeque(&bgJobQueue);

  for (int i=0; i<jobSize; i++)
  {
    Job tmp = pop_front_JobsDeque(&bgJobQueue);

    size_t pidSize = length_ProcessDeque(&(tmp.processQueue));

    for (int j=0; j<pidSize; j++)
    {
      int status;
      pid_t process_pid = pop_front_ProcessDeque(&(tmp.processQueue));
      pid_t return_pid = waitpid(process_pid, &status, WNOHANG); //check status of process with pid process_pid

      if (return_pid == 0) //process is still running
      {
        push_back_ProcessDeque(&(tmp.processQueue), process_pid);
      }
      else if (return_pid == -1) //something went wrong :O
      {
        perror("Error in finishing process");
      }
    }

    // if all processes have been popped, then the job is complete
    if (is_empty_ProcessDeque(&(tmp.processQueue)))
    {
      print_job_bg_complete(tmp.jobID, tmp.pid, tmp.command);
      destroyJob(tmp);
    }
    else
    {
      push_back_JobsDeque(&bgJobQueue, tmp);
    }
    
  }
}

// Prints the job id number, the process id of the first process belonging to
// the Job, and the command string associated with this job
void print_job(int job_id, pid_t pid, const char *cmd)
{
  printf("[%d]\t%8d\t%s\n", job_id, pid, cmd);
  fflush(stdout);
}

// Prints a start up message for background processes
void print_job_bg_start(int job_id, pid_t pid, const char *cmd)
{
  printf("Background job started: ");
  print_job(job_id, pid, cmd);
}

// Prints a completion message followed by the print job
void print_job_bg_complete(int job_id, pid_t pid, const char *cmd)
{
  printf("Completed: \t");
  print_job(job_id, pid, cmd);
}

/***************************************************************************
 * Functions to process commands
 ***************************************************************************/
// Run a program reachable by the path environment variable, relative path, or
// absolute path
void run_generic(GenericCommand cmd)
{
  char *exec = cmd.args[0];
  char **args = cmd.args; //NULL terminated list of arguments

  execvp(exec, args);
}

// Print strings
void run_echo(EchoCommand cmd)
{
  char **str = cmd.args; //NULL terminated list of strings

  for (int i = 0; str[i] != NULL; i++)
  {
    printf("%s ", str[i]);
  }

  printf("\n");

  // Flush the buffer before returning
  fflush(stdout);
}

// Sets an environment variable
void run_export(ExportCommand cmd)
{
  const char *env_var = cmd.env_var;
  const char *val = cmd.val;

  if (setenv(env_var, val, 1) < 0)
  {
    perror("ERROR: Unable to set environment variable");
  }
}

// Changes the current working directory
void run_cd(CDCommand cmd)
{
  // Get the directory name
  const char *dir = cmd.dir;

  // Check if the directory is valid
  if (dir == NULL)
  {
    perror("ERROR: Failed to resolve path");
    return;
  }

  // change directory
  if (chdir(dir) < 0)
  {
    perror("ERROR: Could not change working directory");
    return;
  }

  // set PWD variable
  if (setenv("PWD", dir, 1) < 0)
  {
    perror("ERROR: Unable to update PWD variable");
    return;
  }
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd)
{
  int signal = cmd.sig;
  int job_id = cmd.job;

  if (is_empty_JobsDeque(&bgJobQueue))
  {
    return;
  }

  size_t jobSize = length_JobsDeque(&bgJobQueue);

  for (int i=0; i<jobSize; i++)
  {
    Job tmp = pop_front_JobsDeque(&bgJobQueue);

    if(job_id == tmp.jobID)
    {
      size_t pidSize = length_ProcessDeque(&(tmp.processQueue));

      for (int j=0; j<pidSize; j++)
      {
        pid_t process_pid = pop_front_ProcessDeque(&(tmp.processQueue));
        
        kill(process_pid, signal);

        push_back_ProcessDeque(&(tmp.processQueue), process_pid);
      }
    } 

    push_back_JobsDeque(&bgJobQueue, tmp);   
  }
}

// Prints the current working directory to stdout
void run_pwd()
{
  bool freeDir = false;
  char* dir = get_current_directory(&freeDir);

  printf("%s\n", dir);

  if (freeDir)
  {
    free(dir);
  }
  // Flush the buffer before returning
  fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs()
{
  size_t size = length_JobsDeque(&bgJobQueue);
  
  for(int i=0; i<size; i++)
  {
    Job temp = pop_front_JobsDeque(&bgJobQueue);
    print_job(temp.jobID, temp.pid, temp.command);
    push_back_JobsDeque(&bgJobQueue, temp);
  }

  // Flush the buffer before returning
  fflush(stdout);
}

/***************************************************************************
 * Functions for command resolution and process setup
 ***************************************************************************/

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for child processes.
 *
 * This version of the function is tailored to commands that should be run in
 * the child process of a fork.
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void child_run_command(Command cmd)
{
  CommandType type = get_command_type(cmd);

  switch (type)
  {
  case GENERIC:
    run_generic(cmd.generic);
    break;

  case ECHO:
    run_echo(cmd.echo);
    break;

  case PWD:
    run_pwd();
    break;

  case JOBS:
    run_jobs();
    break;

  case EXPORT:
  case CD:
  case KILL:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief A dispatch function to resolve the correct @a Command variant
 * function for the quash process.
 *
 * This version of the function is tailored to commands that should be run in
 * the parent process (quash).
 *
 * @param cmd The Command to try to run
 *
 * @sa Command
 */
void parent_run_command(Command cmd)
{
  CommandType type = get_command_type(cmd);

  switch (type)
  {
  case EXPORT:
    run_export(cmd.export);
    break;

  case CD:
    run_cd(cmd.cd);
    break;

  case KILL:
    run_kill(cmd.kill);
    break;

  case GENERIC:
  case ECHO:
  case PWD:
  case JOBS:
  case EXIT:
  case EOC:
    break;

  default:
    fprintf(stderr, "Unknown command type: %d\n", type);
  }
}

/**
 * @brief Creates one new process centered around the @a Command in the @a
 * CommandHolder setting up redirects and pipes where needed
 *
 * @note Processes are not the same as jobs. A single job can have multiple
 * processes running under it. This function creates a process that is part of a
 * larger job.
 *
 * @note Not all commands should be run in the child process. A few need to
 * change the quash process in some way
 *
 * @param holder The CommandHolder to try to run
 *
 * @sa Command CommandHolder
 */


void create_process(CommandHolder holder, Job *currentJob)
{
  // Read the flags field from the parser
  bool p_in = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  // Setup pipes, redirects, and new process
  bool currPipeCopy = currPipe; //create a copy of the current pipe boolean so we don't lose
                                //its value when we flip the bool later

  if (p_out) //only create pipes if needed
  {
    if (pipe(pipes[currPipe]) < 0)
    {
      perror("Error creating pipes");
    }
    currPipe = !currPipe; //switch which pipe will be used next time
  }

  pid_t pid = fork();

  if (pid == 0)
  {
    if (p_in) //There was a pipe created last time
    {
      close(pipes[!currPipeCopy][1]);
      dup2(pipes[!currPipeCopy][0], STDIN_FILENO);
    }

    if (r_in) // Read standard input from a file
    {
      int in = open(holder.redirect_in, O_RDONLY);
      dup2(in, STDIN_FILENO);
      close(in);
    }

    if (p_out) //We need to ridirect standard output for this process
    {
      close(pipes[currPipeCopy][0]);
      dup2(pipes[currPipeCopy][1], STDOUT_FILENO);
    }

    if (r_out) // Write standard output to a file
    {
	    int flags = r_app ? (O_WRONLY | O_APPEND | O_CREAT) : (O_WRONLY | O_TRUNC | O_CREAT); // check append flag to append
                                                                                            // or truncate to file
      int out = open(holder.redirect_out, flags, 0664); // 0664 gives read/write permission to owner and group and read
                                                        // permission to other users if file has to be created for open()
      dup2(out, STDOUT_FILENO);
      close(out);
    }

    free(currentJob->command);
    destroy_ProcessDeque(&(currentJob->processQueue));

    child_run_command(holder.cmd); // This should be done in the child branch of a fork
    exit(EXIT_SUCCESS);
  }
  else
  {
    push_back_ProcessDeque(&(currentJob->processQueue), pid);
    parent_run_command(holder.cmd); // This should be done in the parent branch of
                                    // a fork
  }

  // close pipes used to read standard input from
  if (p_in)
  {
    close(pipes[!currPipeCopy][0]);
    close(pipes[!currPipeCopy][1]);
  }
}

// Run a list of commands
void run_script(CommandHolder *holders)
{
  // initialize the background job queue when run_script runs for the first time
  static bool firstRun = true; 

  if (firstRun)
  {
    bgJobQueue = new_destructable_JobsDeque(1, destroyJob);
    firstRun = false;
  }

  // check if holders are valid
  if (holders == NULL)
    return;

  check_jobs_bg_status();

  // if command is exit or quit
  if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC)
  {
    end_main_loop();
    return;
  }

  CommandType type;
  
  // job struct holding information about this job
  Job currentJob;

  currentJob.jobID = 0;
  currentJob.command = get_command_string();
  currentJob.processQueue = new_ProcessDeque(1);

  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
    create_process(holders[i], &currentJob);

  currentJob.pid = peek_front_ProcessDeque(&currentJob.processQueue); // job pid is the pid of the first process

  if (!(holders[0].flags & BACKGROUND))
  {
    // Not a background Job
    // Wait for all processes under the job to complete
    while (!is_empty_ProcessDeque(&currentJob.processQueue))
    {
      pid_t pid = pop_front_ProcessDeque(&currentJob.processQueue);
      int status;
      if ((waitpid(pid, &status, 0)) == -1)
      {
        perror("Process encountered an error");
        exit(EXIT_FAILURE);
      }
    }
    free(currentJob.command);
    destroy_ProcessDeque(&currentJob.processQueue);
  }
  else
  {
    // A background job.
    // Push the new job to the job queue
    if (is_empty_JobsDeque(&bgJobQueue))
    {
      currentJob.jobID = 1;
    }
    else
    {
      int lastID = peek_back_JobsDeque(&bgJobQueue).jobID;
      currentJob.jobID = lastID + 1;
    }
    
    push_back_JobsDeque(&bgJobQueue, currentJob);
    
    print_job_bg_start(currentJob.jobID, currentJob.pid, currentJob.command);
  }
}
