#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/process.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "lib/kernel/stdio.h"
#include "threads/vaddr.h"

static void syscall_handler(struct intr_frame *);
static void syscall_halt(struct intr_frame *f UNUSED);
static void syscall_exit(struct intr_frame *f);
static void syscall_exec(struct intr_frame *f);
static void syscall_wait(struct intr_frame *f);
static void syscall_create(struct intr_frame *f);
static void syscall_remove(struct intr_frame *f);
static void syscall_open(struct intr_frame *f);
static void syscall_filesize(struct intr_frame *f);
static void syscall_read(struct intr_frame *f);
static void syscall_write(struct intr_frame *f);
static void syscall_seek(struct intr_frame *f);
static void syscall_tell(struct intr_frame *f);
static void syscall_close(struct intr_frame *f);

static struct file_info *find_file_by_fd(int fd);
static void terminate_process(void);
static void *check_user_pointer_read(const void *ptr, size_t size);
static void *check_user_pointer_write(void *ptr, size_t size);
static char *check_user_pointer_string(const char *ptr);
static int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);

static size_t ptr_size = sizeof(void *);

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f)
{
  int syscall_type = *(int *)check_user_pointer_read(f->esp, sizeof(int));
  /*debug*/
  // printf("syscall num: %i\n", syscall_type);
  switch (syscall_type)
  {
  case SYS_HALT:
    syscall_halt(f);
    break;
  case SYS_EXIT:
    syscall_exit(f);
    break;
  case SYS_EXEC:
    syscall_exec(f);
    break;
  case SYS_WAIT:
    syscall_wait(f);
    break;
  case SYS_CREATE:
    syscall_create(f);
    break;
  case SYS_REMOVE:
    syscall_remove(f);
    break;
  case SYS_OPEN:
    syscall_open(f);
    break;
  case SYS_FILESIZE:
    syscall_filesize(f);
    break;
  case SYS_READ:
    syscall_read(f);
    break;
  case SYS_WRITE:
    syscall_write(f);
    break;
  case SYS_SEEK:
    syscall_seek(f);
    break;
  case SYS_TELL:
    syscall_tell(f);
    break;
  case SYS_CLOSE:
    syscall_close(f);
    break;
  default:
    NOT_REACHED();
    break;
  }
}

static void
syscall_halt(struct intr_frame *f UNUSED)
{
  shutdown_power_off();
}

static void
syscall_exit(struct intr_frame *f)
{
  int exit_code = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  thread_current()->exit_code = exit_code;
  thread_exit();
}

static void
syscall_exec(struct intr_frame *f)
{
  char *cmd_line = *(char **)check_user_pointer_read(f->esp + ptr_size, ptr_size); // get the pointer of the string
  check_user_pointer_string(cmd_line);
  f->eax = process_execute(cmd_line);
}

static void
syscall_wait(struct intr_frame *f)
{
  int pid = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  f->eax = process_wait(pid);
}

static void
syscall_create(struct intr_frame *f)
{
  char *file_name = *(char **)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  check_user_pointer_string(file_name);
  unsigned int initial_size = *(unsigned int *)check_user_pointer_read(f->esp + ptr_size + ptr_size, ptr_size);

  // only one process at a time is executing file system code.
  sema_down(&sema_filesys);
  f->eax = filesys_create(file_name, initial_size);
  sema_up(&sema_filesys);
}

static void
syscall_remove(struct intr_frame *f)
{
  char *file_name = *(char **)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  check_user_pointer_string(file_name);

  sema_down(&sema_filesys);
  f->eax = filesys_remove(file_name);
  sema_up(&sema_filesys);
}

static void
syscall_open(struct intr_frame *f)
{
  char *file_name = *(char **)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  check_user_pointer_string(file_name);

  sema_down(&sema_filesys);
  struct file *file_entry = filesys_open(file_name);
  sema_up(&sema_filesys);

  if (file_entry == NULL)
  {
    f->eax = -1;
    return;
  }

  struct thread *cur = thread_current();
  struct file_info *file_info = malloc(sizeof(struct file_info));
  file_info->fd = cur->next_fd++;
  file_info->file = file_entry;
  list_push_back(&cur->file_list, &file_info->elem);
  f->eax = file_info->fd;
}

static void
syscall_filesize(struct intr_frame *f)
{
  int fd = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  struct file_info *entry = find_file_by_fd(fd);
  if (entry == NULL)
  {
    f->eax = -1;
    return;
  }

  sema_down(&sema_filesys);
  f->eax = file_length(entry->file);
  sema_up(&sema_filesys);
}

static void
syscall_read(struct intr_frame *f)
{
  int fd = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  void *buffer = *(void **)check_user_pointer_read(f->esp + 2 * ptr_size, ptr_size);
  unsigned size = *(unsigned *)check_user_pointer_read(f->esp + 3 * ptr_size, ptr_size);
  check_user_pointer_write(buffer, size);

  if (fd == 0)
  {
    for (unsigned i = 0; i < size; i++)
    {
      *(uint8_t *)buffer = input_getc();
      buffer += sizeof(uint8_t);
    }
    f->eax = size;
    return;
  }
  if (fd == 1)
  {
    terminate_process();
  }

  struct file_info *entry = find_file_by_fd(fd);
  if (entry == NULL)
  {
    f->eax = -1;
    return;
  }
  sema_down(&sema_filesys);
  f->eax = file_read(entry->file, buffer, size);
  sema_up(&sema_filesys);
}

static void
syscall_write(struct intr_frame *f)
{
  int fd = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  void *buffer = *(void **)check_user_pointer_read(f->esp + 2 * ptr_size, ptr_size);
  unsigned size = *(unsigned *)check_user_pointer_read(f->esp + 3 * ptr_size, ptr_size);
  check_user_pointer_read(buffer, size);

  if (fd == 1)
  {
    putbuf((char *)buffer,size);
    f->eax = size;
    return;
  }
  if(fd == 0){
    terminate_process();
  }

  struct file_info *entry = find_file_by_fd(fd);
  if (entry == NULL)
  {
    f->eax = -1;
    return;
  }
  sema_down(&sema_filesys);
  f->eax = file_write(entry->file,buffer,size);
  sema_up(&sema_filesys);
}

static void
syscall_seek(struct intr_frame *f)
{
  int fd = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  unsigned pos = *(unsigned *)check_user_pointer_read(f->esp + 2 * ptr_size, ptr_size);

  struct file_info *entry = find_file_by_fd(fd);
  if (entry != NULL)
  {
    sema_down(&sema_filesys);
    file_seek(entry->file,pos);
    sema_up(&sema_filesys);
  }
}

static void
syscall_tell(struct intr_frame *f)
{
  int fd = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);

  struct file_info *entry = find_file_by_fd(fd);
  if (entry != NULL)
  {
    sema_down(&sema_filesys);
    f->eax = file_tell(entry->file);
    sema_up(&sema_filesys);
  } else {
    f->eax = -1;
  }
}

static void
syscall_close(struct intr_frame *f)
{
  int fd = *(int *)check_user_pointer_read(f->esp + ptr_size, ptr_size);
  struct file_info *entry = find_file_by_fd(fd);
  if (entry != NULL)
  {
    sema_down(&sema_filesys);
    file_close(entry->file);
    sema_up(&sema_filesys);
    list_remove(&entry->elem);
    free(entry);  
  }
}

/*Find a pointer to the file_info owned by the current process by ID
  return NULL if not found.*/
static struct file_info *
find_file_by_fd(int fd)
{
  struct thread *cur = thread_current();

  for (struct list_elem *e = list_begin(&cur->file_list); e != list_end(&cur->file_list);
       e = list_next(e))
  {
    struct file_info *entry = list_entry(e, struct file_info, elem);
    if (entry->fd == fd)
      return entry;
  }
  return NULL;
}

/*terminate the current process with exit_code=-1*/
static void
terminate_process(void)
{
  thread_current()->exit_code = -1;
  thread_exit();
}

/*check if an user-provided pointer is valid to read.
  Return the pointer itself is valid.
*/
static void *
check_user_pointer_read(const void *ptr, size_t size)
{
  if (!is_user_vaddr(ptr))
    terminate_process();
  for (size_t i = 0; i < size; i++)
  {
    if (get_user(ptr + i) == -1)
    {
      terminate_process();
    }
  }
  return (void *)ptr;
}

/*check if an user-provided pointer is valid to write.
  Return the pointer itself is valid.
*/
static void *
check_user_pointer_write(void *ptr, size_t size)
{
  if (!is_user_vaddr(ptr))
    terminate_process();
  for (size_t i = 0; i < size; i++)
  {
    if (!put_user(ptr + i, 0))
    {
      terminate_process();
    }
  }
  return ptr;
}

/*check fi an user-provided sting is safe to read.
 */
static char *
check_user_pointer_string(const char *ptr)
{
  if (!is_user_vaddr(ptr))
    terminate_process();

  uint8_t *_ptr = (uint8_t *)ptr;
  while (true)
  {
    int c = get_user(_ptr);
    if (c == -1)
    {
      terminate_process();
    }
    else if (c == '\0')
    {
      return (char *)ptr;
    }
    ++_ptr;
  }
  NOT_REACHED();
}

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user(const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}