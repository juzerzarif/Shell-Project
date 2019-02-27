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

int pipes[2][2];
bool currPipe = false;

IMPLEMENT_DEQUE_STRUCT(ProcessDeque, pid_t);
IMPLEMENT_DEQUE(ProcessDeque, pid_t);

typedef struct Job
{
  int jobID;
  char *command;
  ProcessDeque processQueue;
} Job;

IMPLEMENT_DEQUE_STRUCT(JobsDeque, Job);
IMPLEMENT_DEQUE(JobsDeque, Job);

JobsDeque bgJobQueue;

Job currentJob;
int jobCount = 1;

/***************************************************************************
 * Interface Functions
 ***************************************************************************/

// Return a string containing the current working directory.
char *get_current_directory(bool *should_free)
{

  // Change this to true if necessary
  *should_free = true;

  return getcwd(NULL, 0);
}

// Returns the value of an environment variable env_var
const char *lookup_env(const char *env_var)
{
  // TODO: Lookup environment variables. This is required for parser to be able
  // to interpret variables from the command line and display the prompt
  // correctly
  // HINT: This should be pretty simple
  // IMPLEMENT_ME();

  // TODO: Remove warning silencers
  return getenv(env_var); // Silence unused variable warning

  // return "???";
}

// Check the status of background jobs
void check_jobs_bg_status()
{
  // TODO: Check on the statuses of all processes belonging to all background
  // jobs. This function should remove jobs from the jobs queue once all
  // processes belonging to a job have completed.
  IMPLEMENT_ME();

  // TODO: Once jobs are implemented, uncomment and fill the following line
  // print_job_bg_complete(job_id, pid, cmd);
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
  // Execute a program with a list of arguments. The `args` array is a NULL
  // terminated (last string is always NULL) list of strings. The first element
  // in the array is the executable
  char *exec = cmd.args[0];
  char **args = cmd.args;

  execvp(exec, args);
}

// Print strings
void run_echo(EchoCommand cmd)
{
  // Print an array of strings. The args array is a NULL terminated (last
  // string is always NULL) list of strings.
  char **str = cmd.args;

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
  // Write an environment variable
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

  // TODO: Change directory
  if (chdir(dir) < 0)
  {
    perror("ERROR: Could not change working directory");
    return;
  }

  // TODO: Update the PWD environment variable to be the new current working
  // directory and optionally update OLD_PWD environment variable to be the old
  // working directory.
  if (setenv("PWD", dir, 1) < 0)
  {
    perror("ERROR: Unable to update PWD variable");
    return;
  }
  // IMPLEMENT_ME();
}

// Sends a signal to all processes contained in a job
void run_kill(KillCommand cmd)
{
  int signal = cmd.sig;
  int job_id = cmd.job;

  // TODO: Remove warning silencers
  (void)signal; // Silence unused variable warning
  (void)job_id; // Silence unused variable warning

  // TODO: Kill all processes associated with a background job
  IMPLEMENT_ME();
}

// Prints the current working directory to stdout
void run_pwd()
{
  // TODO: Print the current working directory
  // IMPLEMENT_ME();
  printf("%s\n", lookup_env("PWD"));
  // Flush the buffer before returning
  fflush(stdout);
}

// Prints all background jobs currently in the job list to stdout
void run_jobs()
{
  // TODO: Print background jobs
  IMPLEMENT_ME();
  size_t size = length_JobsDeque(&bgJobQueue);
  
  for(int i=0; i<size; i++)
  {
    Job temp = pop_front_JobsDeque(&bgJobQueue);
    pid_t pid = peek_front_ProcessDeque(&temp.processQueue);
    print_job(temp.jobID, pid, temp.command);
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

void setup_execute()
{
  bgJobQueue = new_JobsDeque(1);
}

void create_process(CommandHolder holder)
{
  // Read the flags field from the parser
  bool p_in = holder.flags & PIPE_IN;
  bool p_out = holder.flags & PIPE_OUT;
  bool r_in = holder.flags & REDIRECT_IN;
  bool r_out = holder.flags & REDIRECT_OUT;
  bool r_app = holder.flags & REDIRECT_APPEND; // This can only be true if r_out
                                               // is true

  // TODO: Setup pipes, redirects, and new process
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

    if (r_in)
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

    if (r_out)
    {
      int out = r_app ?  open(holder.redirect_out, O_WRONLY | O_APPEND | O_CREAT) : open(holder.redirect_out, O_WRONLY | O_CREAT);
      dup2(out, STDOUT_FILENO);
      close(out);
    }

    child_run_command(holder.cmd); // This should be done in the child branch of a fork
    exit(EXIT_SUCCESS);
  }
  else
  {
    push_back_ProcessDeque(&currentJob.processQueue, pid);
    parent_run_command(holder.cmd); // This should be done in the parent branch of
                                    // a fork
  }

  if (p_in)
  {
    close(pipes[!currPipeCopy][0]);
    close(pipes[!currPipeCopy][1]);
  }
}

// Run a list of commands
void run_script(CommandHolder *holders)
{
  if (holders == NULL)
    return;

  check_jobs_bg_status();

  if (get_command_holder_type(holders[0]) == EXIT &&
      get_command_holder_type(holders[1]) == EOC)
  {
    end_main_loop();
    return;
  }

  CommandType type;

  currentJob.jobID = jobCount;
  currentJob.command = get_command_string();
  currentJob.processQueue = new_ProcessDeque(1);

  jobCount++;

  // Run all commands in the `holder` array
  for (int i = 0; (type = get_command_holder_type(holders[i])) != EOC; ++i)
    create_process(holders[i]);

  if (!(holders[0].flags & BACKGROUND))
  {
    // Not a background Job
    // TODO: Wait for all processes under the job to complete
    // IMPLEMENT_ME();
    while (!is_empty_ProcessDeque(&currentJob.processQueue))
    {
      pid_t pid = pop_front_ProcessDeque(&currentJob.processQueue);
      int status;
      if ((waitpid(pid, &status, 0)) == -1)
      {
        perror("Process encountered an error");
      }
    }
  }
  else
  {
    // A background job.
    // TODO: Push the new job to the job queue
    IMPLEMENT_ME();
    push_back_JobsDeque(&bgJobQueue, currentJob);

    // TODO: Once jobs are implemented, uncomment and fill the following line
    // print_job_bg_start(job_id, pid, cmd);
  }
}
