#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include <threads/fixed_point.h>
#include <threads/synch.h>
#include <filesys/filesys.h>
#include <filesys/file.h>

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */

    int exit_code;                      /* for userprog ,may be moved to USERPROG*/
    struct semaphore wait_success;      /* for userprog, wait for start_process*/
    bool exec_success;                  /* for userprog, true if child process excute successfully.*/

    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */
    int64_t block_ticks;         /*ticks of block time*/

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /*for priority donate*/
    int original_priority;          /* Original Priority before donate. */
    struct list locks;                  /*Locks held by the thread*/
    struct lock* lock_waiting;      /*the lock this thread is waiting*/

    /*for mlfqs*/
    fixed_point recent_cpu;
    fixed_point load_avg;
    int nice;

    /*for userprog waiting*/
    struct list children;              /*list of children process*/
    struct thread *parent;          /*parent of the thread, initially NULL*/
    struct child_info *child_entry;  /*entry of child_info itself*/

    /*process open file*/
    struct list file_list;             /*FIles opened by the thread. Member type is file_info*/
    int next_fd;                       /*Next file descriptor to be allocated.*/
    struct file *exec_file;            /*The file that the thread is excuting*/

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/*struct to record the info of child
*/
struct child_info
{
   tid_t tid;                       /*child's thread id*/
   struct thread *t;                /*child's entry struct*/
   int exit_code;                   /*child's exit status*/
   bool is_alive;                   /*is child thread alive*/
   bool is_waiting;                 /*true if parent is wating on this child*/
   struct semaphore semaphore;      /*sema for waiting on a child*/
   struct list_elem elem;           /*element of child list*/
};

/* struct of a opened file */
struct file_info
{
   int fd;                          /*file descriptor*/
   struct file *file;               /*Pointer to file*/
   struct list_elem elem;
};


/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;
extern struct list ready_list;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void check_blocked_thread(struct thread* thread, void* aux UNUSED);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);
bool prio_a_less_b(const struct list_elem* a, const struct list_elem* b, void* aux UNUSED);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

void mlfqs_increase_recent_cpu(void);
void mlfqs_update_load_avg_and_recent_cpu(void);
void mlfqs_update_thread_priority(struct thread* t);
#endif /* threads/thread.h */
