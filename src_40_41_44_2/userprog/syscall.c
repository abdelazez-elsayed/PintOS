#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "devices/shutdown.h" // for SYS_HALT
#include "threads/vaddr.h"
#include "threads/synch.h"
#include "userprog/process.h"
#include "userprog/pagedir.h"
#include "devices/block.h"
#include "filesys/free-map.h"
#include "filesys/directory.h"
#include "filesys/inode.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "threads/palloc.h"
#include "lib/kernel/list.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);
int read_phys_mem(unsigned char *addr);
struct lock sync_file_lock;
bool check_validate(void *addr);
void validate_byte_by_byte(void * addr, size_t size);
void read_mem(void *f, unsigned char *esp, int num);
bool write_mem(unsigned char *addr, unsigned char byte);
void sys_exit(int , struct intr_frame * UNUSED);
void sys_wait(int , struct intr_frame *);
void sys_write(int , void*, int, struct intr_frame *);
void sys_exec(char *, struct intr_frame *);
void sys_create(char *, size_t, struct intr_frame *);
void sys_open(char *, struct intr_frame *);
void sys_close(int , struct intr_frame * UNUSED);
void sys_read(int, void*,int, struct intr_frame *);
void sys_filesize(int , struct intr_frame*);
void sys_remove(char *, struct intr_frame*);
void sys_seek(int, int, struct intr_frame * UNUSED);
void sys_tell(int , struct intr_frame *f);
void sys_halt(void);
void sys_halt_wrapper(void);
void sys_exit_wrapper(struct intr_frame *f UNUSED );
void sys_wait_wrapper(struct intr_frame *f);
void sys_exec_wrapper(struct intr_frame *f );
void sys_create_wrapper(struct intr_frame *f );
void sys_write_wrapper(struct intr_frame *f);
void sys_open_wrapper(struct intr_frame *f UNUSED);
void sys_close_wrapper(struct intr_frame *f);
void sys_read_wrapper(struct intr_frame *f);
void sys_filesize_wrapper(struct intr_frame *f );
void sys_remove_wrapper(struct intr_frame *f );
void sys_seek_wrapper(struct intr_frame *f UNUSED );
void sys_tell_wrapper(struct intr_frame *f);
static int fd_num=2;
void
syscall_init (void)
{
  lock_init(&sync_file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

void validate_byte_by_byte(void * addr, size_t size)
{
  unsigned i;
  unsigned char *_cmd = addr;
  for(i=0; i<size; i++)
  {
    if(!check_validate((void *)(_cmd+i)))
    {
      sys_exit(-1,NULL);
    }
  }
}

int read_phys_mem(unsigned char *addr)
{
  /*Reads a byte at user virtual address UADDR.
          UADDR must be below PHYS_BASE.
          Returns the byte value if successful, -1 if a segfault
          occurred. */
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:": "=&a" (result) : "m" (*addr));
  return result;
}

bool write_mem(unsigned char *addr, unsigned char byte)
{
  if(check_validate(addr))
  {
    int error_code;
    /* Writes BYTE to user address UDST.
    UDST must be below PHYS_BASE.
    Returns true if successful, false if a segfault occurred. */
    asm ("movl $1f, %0; movb %b2, %1; 1:"
        : "=&a" (error_code), "=m" (*addr) : "q" (byte));
    return error_code != -1;
  }
  else
    sys_exit(-1,NULL);
}

bool check_validate(void *addr)
{
  if((addr != NULL) && (((unsigned int)addr) < ((unsigned int)PHYS_BASE)))
  {
    if((pagedir_get_page(thread_current()->pagedir, addr)) != NULL)
    {
      return true;
    }
    else
    {
      return false;
    }
  }
  return false;
}

void read_mem(void *f, unsigned char *esp, int num)
{
  int i;
  for(i=0; i<num; i++)
  {
    if(check_validate(esp + i))
      *(char *)(f+i) = read_phys_mem((esp + i)) & 0xff;
    else
      sys_exit(-1,NULL);
  }
}

static void
syscall_handler (struct intr_frame *f)
{
  int syscall_number;

  void *esp = f->esp;
  if(!check_validate(esp) && !check_validate(esp+4) && ! check_validate(esp+8) && !check_validate(esp+12))
  {
    sys_exit(-1,NULL);
  }
  read_mem(&syscall_number, esp, sizeof(syscall_number));
  switch(syscall_number)
  {
    case SYS_HALT :
    {
      sys_halt_wrapper() ;
      break ;
    }
    case SYS_EXIT :
    {
      sys_exit_wrapper(f) ;
      break ;
    }
    case SYS_EXEC :
    {
      sys_exec_wrapper(f) ;
      break;
    }
    case SYS_WAIT:
    {
      sys_wait_wrapper(f) ;
      break;
    }
    case SYS_CREATE :
    {
      sys_create_wrapper(f) ;
      break;
    }
    case SYS_REMOVE:
    {
      sys_remove_wrapper(f) ;
      break;
    }
    case SYS_OPEN :
    {
      sys_open_wrapper(f) ;
      break;
    }
    case  SYS_FILESIZE :
    {
      sys_filesize_wrapper(f) ;
      break;
    }
    case SYS_READ :
    {
      sys_read_wrapper(f) ;
      break;
    }
    case SYS_WRITE :
    {
      sys_write_wrapper(f) ;
      break;
    }
    case SYS_SEEK :
    {
      sys_seek_wrapper(f) ;
      break;
    }
    case SYS_TELL :
    {
      sys_tell_wrapper(f) ;
      break;
    }
    case SYS_CLOSE :
    {
      sys_close_wrapper(f) ;
      break;

    }
    defult :
     sys_exit_wrapper(NULL) ;
     break;
  }
}
void
sys_halt_wrapper(){
  sys_halt();
}

void
sys_halt(){
  shutdown_power_off();
}
void
sys_exit_wrapper(struct intr_frame *f UNUSED ){
   int status;
   void* esp= f->esp ;
   read_mem(&status, esp+4, sizeof(status));
   sys_exit(status , f);
}
void
sys_exit(int exit_code, struct intr_frame *f UNUSED)
{
  struct tcb * tcb = thread_current()->tcb;
  printf("%s: exit(%d)\n", thread_current()->name, exit_code);
  if(tcb) tcb->exit_code = exit_code;
  thread_exit();
}

void
sys_wait_wrapper(struct intr_frame *f){
  int tid;
  void* esp = f->esp;
  read_mem(&tid,esp+4,sizeof(tid));
  sys_wait(tid , f);
}

void
sys_wait(int tid, struct intr_frame *f)
{
  f->eax = process_wait(tid);
}

void
sys_exec_wrapper(struct intr_frame *f ){
  void * cmd;
  void* esp = f->esp;
  read_mem(&cmd, esp+4, sizeof(cmd));
  sys_exec(cmd,f);
}

void
sys_exec(char *cmd, struct intr_frame *f)
{
  validate_byte_by_byte(cmd,sizeof(cmd));
  lock_acquire(&sync_file_lock);
  f->eax = process_execute((const char*)cmd);
  lock_release(&sync_file_lock);
}

void
sys_create_wrapper(struct intr_frame *f ){
  char * name;
  size_t size;
  void* esp = f->esp;
  read_mem(&name, esp+4, sizeof(name));
  read_mem(&size, esp+8, sizeof(size));
  sys_create(name,size,f);
}

void
sys_create(char *name, size_t size, struct intr_frame *f)
{
  validate_byte_by_byte(name,sizeof(name));
  lock_acquire(&sync_file_lock);
  f->eax = filesys_create((const char*)name,size);
  lock_release(&sync_file_lock);
}

void
sys_write_wrapper(struct intr_frame *f){
      int fd, size;
      void *buffer;
      void* esp = f->esp ;
      read_mem(&fd,esp+4,sizeof(fd));
      read_mem(&buffer, esp+8, sizeof(buffer));
      read_mem(&size, esp+12, sizeof(size));
      sys_write(fd,buffer,size,f);
}

void
sys_write(int fid, void * buffer, int size, struct intr_frame *f)
{
  if(buffer == NULL)
    sys_exit(-1,NULL);
  validate_byte_by_byte(buffer,sizeof(buffer));
  if(!check_validate(buffer) || !check_validate(buffer+size))
    sys_exit(-1,NULL);

  lock_acquire(&sync_file_lock);
  if(fid == STDOUT)
  {
    putbuf(buffer, size);
    lock_release(&sync_file_lock);
    f->eax=size;
  }
  else if(fid == STDIN) // negative area
  {
    f->eax=-1;
    lock_release(&sync_file_lock);
    return;
  }
  else
  {
    struct list_elem *tmp;
    struct filedescriptor * fd=NULL;
    if(!list_empty(&thread_current()->fd))
    {
      for(tmp=list_front(&thread_current()->fd); ; tmp=list_next(tmp))
      {
        fd = list_entry(tmp, struct filedescriptor, elem);
        if(fd->fd_num == fid)
         break;
        if(tmp == list_tail(&thread_current()->fd) && (fd->fd_num != fid))
        {
          lock_release(&sync_file_lock);
          f->eax=-1;
          return;
        }
      }
    }
    if(fd !=NULL)
    {
      f->eax = file_write(fd->f,buffer,size);
      lock_release(&sync_file_lock);
      return;
    }
    else
    {
      lock_release(&sync_file_lock);
      f->eax=-1;
      return;
    }
  }
}

void
sys_open_wrapper(struct intr_frame *f){
  char *name;
  void* esp = f->esp;
  read_mem(&name, esp+4, sizeof(name));
  sys_open(name , f);
}

void
sys_open(char * name, struct intr_frame *f)
{

  struct file * open=NULL;
  struct filedescriptor * fd;
  validate_byte_by_byte(name,sizeof(name));
  lock_acquire(&sync_file_lock);
  fd = palloc_get_page(0);
  if(fd == NULL)
  {
    palloc_free_page(fd);
    goto malicious_ending;
  }
  else
  {
    open = filesys_open(name);
    if(open == NULL)
      goto malicious_ending;
    fd->f = open;
    fd->fd_num = ++fd_num;
    fd->master = thread_current();
    list_push_back(&(thread_current()->fd),&(fd->elem));
    f->eax=fd->fd_num;
    lock_release(&sync_file_lock);
    return;
  }
  malicious_ending:
  f->eax=-1;
  lock_release(&sync_file_lock);
}

void
sys_close_wrapper(struct intr_frame *f UNUSED ){
  int fd;
  void* esp = f->esp;
      read_mem(&fd, esp+4, sizeof(fd));
      sys_close(fd,f);
}

void
sys_close(int fid, struct intr_frame *f UNUSED)
{
    struct list_elem *tmp;
    struct filedescriptor *fd;
    if(!list_empty(&thread_current()->fd))
    {
      lock_acquire(&sync_file_lock);
      for(tmp=list_front(&thread_current()->fd); ; tmp=list_next(tmp))
      {
        fd = list_entry(tmp, struct filedescriptor, elem);
        if(fd->fd_num == fid) //found
         break;
        if(tmp == list_tail(&thread_current()->fd) && (fd->fd_num != fid)) //not found
        {
          lock_release(&sync_file_lock);
          return;
        }
      }
      if(thread_current()->tid == fd->master->tid) // check master thread.
      {
        lock_release(&sync_file_lock);
        file_close(fd->f);
        list_remove(&(fd->elem));
        palloc_free_page(fd);
      }
    }
}

void
sys_read_wrapper(struct intr_frame *f){
      int fd, size;
      void *buffer;
      void* esp = f->esp;
      read_mem(&fd,esp+4,sizeof(fd));
      read_mem(&buffer, esp+8, sizeof(fd));
      read_mem(&size, esp+12, sizeof(fd));
      sys_read(fd,buffer,size,f);
}

void
sys_read(int fid, void * buffer, int size, struct intr_frame *f)
{
  unsigned i;
  validate_byte_by_byte(buffer,sizeof(buffer));
  validate_byte_by_byte(buffer,sizeof(buffer)+size-1);
  lock_acquire(&sync_file_lock);
  if(fid <= 2 || fid > fd_num)
  {
    f->eax=-1;
    lock_release(&sync_file_lock);
    return;
  }
  if(fid==STDIN)
  {
    for(i=0; i<(unsigned)size; i++)
      write_mem((unsigned char *)(buffer+i),input_getc());
    lock_release(&sync_file_lock);
    f->eax = size;
  }
  else if(fid == STDOUT)  // negative area
  {
    f->eax=-1;
    lock_release(&sync_file_lock);
    return;
  }
  else
  {
    struct list_elem *tmp;
    struct filedescriptor *fd;
    if(!list_empty(&thread_current()->fd))
    {
      for(tmp=list_front(&thread_current()->fd); ; tmp=list_next(tmp))
      {
        fd = list_entry(tmp, struct filedescriptor, elem);
        if(fd->fd_num == fid) //found
         break;
        if(tmp == list_tail(&thread_current()->fd) && (fd->fd_num != fid)) //not found
        {
          lock_release(&sync_file_lock);
          f->eax=-1;
          return;
        }
      }
      f->eax = file_read(fd->f,buffer,size);
      lock_release(&sync_file_lock);
      return ;
    }
  }
}

void
sys_filesize_wrapper(struct intr_frame *f ){
  int fid;
  void* esp = f->esp;
  read_mem(&fid,esp+4,sizeof(fid));
  sys_filesize(fid,f);
}

void
sys_filesize(int fid, struct intr_frame *f)
{
  struct list_elem *tmp;
  struct filedescriptor *fd=NULL;
  if(!list_empty(&thread_current()->fd))
  {
    lock_acquire(&sync_file_lock);
    for(tmp=list_front(&thread_current()->fd); ; tmp=list_next(tmp))
    {
      fd = list_entry(tmp, struct filedescriptor, elem);
      if(fd->fd_num == fid)
       break;
      if(tmp == list_tail(&thread_current()->fd) && (fd->fd_num != fid))
      {
        lock_release(&sync_file_lock);
        f->eax=-1;
        return;
      }
    }
  }
  lock_release(&sync_file_lock);
  if(fd == NULL)
    f->eax =-1;
  else
  {
    f->eax = file_length(fd->f);
  }
}

void
sys_remove_wrapper(struct intr_frame *f ){
  char * name;
  void* esp = f->esp;
  read_mem(&name, esp+4, sizeof(name));
  sys_remove(name , f);
}

void
sys_remove(char *name, struct intr_frame *f)
{
  validate_byte_by_byte(name,sizeof(name));
  lock_acquire(&sync_file_lock);
  f->eax = filesys_remove(name);
  lock_release(&sync_file_lock);
}

void
sys_seek_wrapper(struct intr_frame *f UNUSED ){
  int fd, cnt;
  void* esp = f->esp;
  read_mem(&fd,esp+4,sizeof(fd));
  read_mem(&cnt,esp+8,sizeof(cnt));
  sys_seek(fd, cnt, f);
}

void
sys_seek(int fid, int cnt, struct intr_frame *f UNUSED)
{
  struct list_elem *tmp;
  struct filedescriptor *fd=NULL;
  if(!list_empty(&thread_current()->fd))
  {
    lock_acquire(&sync_file_lock);
    for(tmp=list_front(&thread_current()->fd); ; tmp=list_next(tmp))
    {
      fd = list_entry(tmp, struct filedescriptor, elem);
      if(fd->fd_num == fid)
       break;
      if(tmp == list_tail(&thread_current()->fd) && (fd->fd_num != fid))
      {
        lock_release(&sync_file_lock);
        return;
      }
    }
  }
  if(fd->f != NULL)
  {
    file_seek(fd->f,cnt);
    lock_release(&sync_file_lock);
    return;
  }
  return;
}

void
sys_tell_wrapper(struct intr_frame *f){
  int fd;
  void* esp = f->esp;
  read_mem(&fd, esp+4, sizeof(fd));
  sys_tell(fd,f);
}

void
sys_tell(int fid,struct intr_frame *f)
{
  struct list_elem *tmp;
  struct filedescriptor *fd=NULL;
  lock_acquire(&sync_file_lock);
  if(!list_empty(&thread_current()->fd))
  {
    for(tmp=list_front(&thread_current()->fd); ; tmp=list_next(tmp))
    {
      fd = list_entry(tmp, struct filedescriptor, elem);
      if(fd->fd_num == fid)
       break;
      if(tmp == list_tail(&thread_current()->fd) && (fd->fd_num != fid))
      {
        f->eax=-1;
        lock_release(&sync_file_lock);
        return;
      }
    }
  }
  if(fd->f != NULL)
  {
    f->eax=file_tell(fd->f);
    lock_release(&sync_file_lock);
    return;
  }
  f->eax=-1;
  lock_release(&sync_file_lock);
}
