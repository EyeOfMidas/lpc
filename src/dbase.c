/* A simple database manager,
 * Goals: crashproof, small, simple and not GPL, the idea is that 
 *        only the last writen entries can disappear in a crash.
 * Possible future goals: speed, locking and shrinking
 * Written by Fredrik Hubinette.
 */

#include "global.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include "dbase.h"

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#define BLOCK0_NEEDS_SAVING 1
#define LINKTABLE_NEEDS_SAVING 2
#define HASHTABLE_NEEDS_SAVING 4
#define LINKS_ALLOCATED_BUT_NOT_SAVED 8
#define FAILED_TO_WRITE_HASHTABLE 16
#define FAILED_TO_SAVE_BLOCK0 32

#define SET(X,Y) (X)=(int)htonl((unsigned long)(Y))
#define GET(X) ((int)ntohl((unsigned long)(X)))

/* We can't use the normal hashmem, because it's machine dependant. */
unsigned int hashkey(const DBT t)
{
  unsigned int ret;
  int mlen=t.len;
  char *a=t.data;
  ret=9248339*mlen;
  switch(mlen&7)
  {
    case 7: ret^=*(a++);
    case 6: ret^=(ret<<4)+*(a++);
    case 5: ret^=(ret<<7)+*(a++);
    case 4: ret^=(ret<<6)+*(a++);
    case 3: ret^=(ret<<3)+*(a++);
    case 2: ret^=(ret<<7)+*(a++);
    case 1: ret^=(ret<<5)+*(a++);
  }

  for(mlen>>=3;--mlen>=0;)
  {
    ret^=(ret<<7)+((((((*(a++)<<3)+*(a++))<<4)+*(a++))<<5)+*(a++));
    ret^=(ret>>6)+((((((*(a++)<<3)+*(a++))<<4)+*(a++))<<5)+*(a++));
  }
  return ret;
}

static block *read_block(dbase *d,int blockno,int ignore,block *whereto)
{
  block *tmp=NULL;
  int pos;

  pos=blockno*BLOCK_SIZE;
  if(pos!=lseek(d->fd,pos,SEEK_SET)) return NULL;

  if(!whereto)
  {
    tmp=whereto=(block *)calloc(1,sizeof(block));
    if(!whereto) return NULL;
  }

  if(BLOCK_SIZE!=read(d->fd,(char *)whereto,BLOCK_SIZE) && !ignore)
  {
    if(tmp) free((char *)tmp);
    return NULL;
  }
  return whereto;
}

static int write_block(dbase *d,int blockno,block *b)
{
  int pos;

  pos=blockno*BLOCK_SIZE;
  if(pos!=lseek(d->fd,pos,SEEK_SET)) return 0;

  if(BLOCK_SIZE!=write(d->fd,(char *)b,BLOCK_SIZE)) return 0;
  return 1;
}

static int more_links(dbase *d)
{
  int *q,tmp;
  q=(int *)realloc((char *)d->links,2*d->no_links*sizeof(int));
  if(!q) return 0;
  for(tmp=d->no_links;tmp<d->no_links*2;tmp++) SET(q[tmp],0);
  d->no_links*=2;
  d->links=q;
  return 1;
}

static void free_file(dbase *d,int ptr,int immidiate)
{
  if(immidiate)
  {
    int *last;
    while(ptr!=-1)
    {
      last=d->links+ptr;
      ptr=GET(*last);
      SET(*last,0);
    }
  }else{
    d->files_to_free[d->no_files_to_free++]=ptr;
  }
}

static int save_linktable2(dbase *d)
{
  int pos,len;
  d->flags|=LINKS_ALLOCATED_BUT_NOT_SAVED;
  pos=GET(d->block0.links.ptr)*BLOCK_SIZE;

  if(lseek(d->fd,pos,SEEK_SET)!=pos)
    return 0;

  len=d->no_links*sizeof(int);
  if(write(d->fd,(char *)d->links,len)!=len)
    return 0;

  d->flags|=BLOCK0_NEEDS_SAVING;
  d->flags&=~LINKS_ALLOCATED_BUT_NOT_SAVED;
  d->flags&=~LINKTABLE_NEEDS_SAVING;

  return 1;
}

static int save_linktable(dbase *d)
{
  int e,q,needed_links;

  if(d->flags & LINKS_ALLOCATED_BUT_NOT_SAVED)
    return save_linktable2(d);

  free_file(d,GET(d->block0.links.ptr),0);

  needed_links=d->no_links*sizeof(int)/BLOCK_SIZE;
  while(1)
  {
    for(e=0;e<d->no_links-needed_links;e++)
    {
      for(q=0;q<needed_links;q++)
        if(GET(d->links[e+q]))
          break;
      if(q==needed_links)
        break;
    }
    if(e==d->no_links-needed_links)
    {
      if(!more_links(d))
        return 0;
    }else{
      break;
    }
  }

  /* allocate links */
  for(q=0;q<needed_links-1;q++)
    SET(d->links[e+q],e+q+1);
  SET(d->links[e+q],-1);

  /* gc */
  for(q=0;q<d->no_files_to_free;q++)
    free_file(d,d->files_to_free[q],1);

  d->no_files_to_free=0;
  SET(d->block0.links.ptr,e);
  SET(d->block0.links.len,d->no_links*sizeof(int));

  return save_linktable2(d);
}

static int *read_linktable(dbase *d)
{
  int pos;
  if(d->links) return d->links;
  pos=GET(d->block0.links.ptr)*BLOCK_SIZE;

  if(lseek(d->fd,pos,SEEK_SET)!=pos)
    return 0;

  d->links=(int *)malloc(MAX(GET(d->block0.links.len),1));
  if(!d->links) return NULL;
  
  if(GET(d->block0.links.len)!=
     read(d->fd,(char *)d->links,GET(d->block0.links.len)))
  {
    free((char *)d->links);
    return d->links=NULL;
  }
  d->no_links=GET(d->block0.links.len)/sizeof(int);
  return d->links;
}

static char *read_file(dbase *d,file f)
{
  block b;
  int blockno,tmp;
  char *ret,*p;

  if(!read_linktable(d))
    return NULL;

  blockno=GET(f.ptr);
  ret=p=(char *)malloc(MAX(GET(f.len),1));
  if(!ret) return NULL;

  while(p<ret+GET(f.len))
  {
    tmp=MIN(ret+GET(f.len)-p,BLOCK_SIZE);
    if(tmp==BLOCK_SIZE)
    {
      if(!read_block(d,blockno,0,(block *)p))
      {
        free(ret);
        return NULL;
      }
    }else{
      if(!read_block(d,blockno,0,&b))
      {
        free(ret);
        return NULL;
      }
      MEMCPY(p,(char *)&b,tmp);
    }
    p+=tmp;
    blockno=GET(d->links[blockno]);
  }
  return ret;
}

/* don't forget to save the linktable afterwards */
static int write_file(dbase *d,DBT a)
{
  block b;
  int tmp,e,blockno;
  int needed_links,first_link,*last;
  char *p;

  needed_links=(a.len+BLOCK_SIZE-1)/BLOCK_SIZE;

  while(1)
  {
    for(tmp=e=0;e<d->no_links && tmp<needed_links;e++)
      if(!GET(d->links[e]))
	tmp++;

    if(tmp<needed_links)
      more_links(d);
    else
      break;
  }

  last=&first_link;
  for(e=0;e<d->no_links && needed_links;e++)
  {
    if(GET(d->links[e])) continue;
    SET(*last,e);
    last=d->links+e;
    needed_links--;
  }
  SET(*last,-1); /* used */
  MEMSET((char *)&b,0,sizeof(b));

  /* blocks allocated, let's try to save into them */
  blockno=GET(first_link);
  p=a.data;
  while(p<a.data+a.len)
  {
    tmp=MIN(a.data+a.len-p,BLOCK_SIZE);
    if(tmp==BLOCK_SIZE)
    {
      if(!write_block(d,blockno,(block *)p))
      {
        free_file(d,first_link,1);
        return 0;
      }
    }else{
      MEMCPY((char *)&b,p,tmp);
      if(!write_block(d,blockno,&b))
      {
        free_file(d,first_link,1);
        return 0;
      }
    }
    blockno=GET(d->links[blockno]);
    p+=tmp;
  }
  d->flags|=LINKTABLE_NEEDS_SAVING;

  return GET(first_link);
}

static int write_hashtable(dbase *d)
{
  DBT htable;
  int f;

  d->flags|=FAILED_TO_WRITE_HASHTABLE;

  htable.len=d->hashsize*sizeof(hash_entry);
  htable.data=(char *)d->hashtable;

  f=write_file(d,htable);
  if(!f) return 0;

  free_file(d,GET(d->block0.hashtable.ptr),0);
  SET(d->block0.hashtable.ptr,f);
  SET(d->block0.hashtable.len,htable.len);

  d->flags&=~FAILED_TO_WRITE_HASHTABLE;
  d->flags&=~HASHTABLE_NEEDS_SAVING;
  d->flags|=BLOCK0_NEEDS_SAVING;

  return 1;
}

static int save_block0(dbase *d)
{
  d->flags|=FAILED_TO_SAVE_BLOCK0;
  if(lseek(d->fd,0,SEEK_SET))
    return 0;

  if(sizeof(struct block0)!=write(d->fd,(char *)&(d->block0),sizeof(struct block0)))
    return 0;

  d->flags&=~BLOCK0_NEEDS_SAVING;
  d->flags&=~FAILED_TO_SAVE_BLOCK0;
  return 1;
}

static int read_block0(dbase *d)
{
  if(lseek(d->fd,0,SEEK_SET))
    return 0;

  if(sizeof(struct block0)!=read(d->fd,(char *)&(d->block0),sizeof(struct block0)))
    return 0;

  if(GET(d->block0.magic1)!=MAGIC1 || GET(d->block0.magic2)!=MAGIC2)
    return 0;

  return 1;
}

int sync_database(dbase *d)
{
  if(d->flags & HASHTABLE_NEEDS_SAVING)
    if(!write_hashtable(d))
      return 0;

  if(d->flags & LINKTABLE_NEEDS_SAVING)
    if(!save_linktable(d))
      return 0;

  if(d->flags & BLOCK0_NEEDS_SAVING)
    if(!save_block0(d))
      return 0;

  d->counter=0;
  return 1;
}

static int spurious_sync(dbase *d)
{
  d->counter++;
  if(d->no_files_to_free>BUFFER || d->counter>BUFFER)
    return sync_database(d);
  return 1;
}

static hash_entry *read_hashtable(dbase *d)
{
  if(d->flags &
      (FAILED_TO_WRITE_HASHTABLE |
       LINKS_ALLOCATED_BUT_NOT_SAVED |
       FAILED_TO_SAVE_BLOCK0))
  {
    /* no further operations can be done when syncing fails */
    if(!sync_database(d))
      return NULL;
  }
  if(d->hashtable) return d->hashtable;
  d->hashsize=GET(d->block0.hashtable.len)/sizeof(hash_entry);
  d->hashtable=(hash_entry *)read_file(d,d->block0.hashtable);
  if(d->hashtable) return d->hashtable;
  return 0;
}

static int find_key(dbase *d,DBT key,unsigned int hashval)
{
  int p,q,tmp;
  q=p=hashval%(d->hashsize);
  do
  {
    if(GET(d->hashtable[p].hashval)==hashval
       && GET(d->hashtable[p].key.len)==key.len)
    {
      char *k;
      k=read_file(d,d->hashtable[p].key);
      if(!k) return -2;
      tmp=MEMCMP(k,key.data,key.len);
      free(k);
      if(!tmp) return p;
    }
 
    p++;
    if(p>=d->hashsize) p=0;
  }while(p!=q);
  return -1;
}

static int add_key(dbase *d,DBT key,unsigned int hashval)
{
  int p,q,f;
  hash_entry *h;

  q=p=hashval % (d->hashsize);
  do
  {
    if(GET(d->hashtable[p].key.len)==-1)
    {
      f=write_file(d,key);
      if(!f) return -1;
      SET(d->hashtable[p].key.ptr,f);
      SET(d->hashtable[p].key.len,key.len);
      SET(d->hashtable[p].hashval,hashval);
      return p;
    }
    p++;
    if(p>=d->hashsize) p=0;

  }while(p!=q);
  /* rehash !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  h=(hash_entry *)malloc(sizeof(hash_entry)*d->hashsize*2);
  if(!h) return -1;

  MEMSET((char *)h,-1,d->hashsize*2*sizeof(hash_entry));

  for(f=0;f<d->hashsize;f++)
  {
    p=q=GET(d->hashtable[f].hashval) % (d->hashsize*2);
    do
    {
      if(GET(h[p].key.len)==-1)
      {
        h[p]=d->hashtable[f];
        break;
      }
      p++;
      if(p>=d->hashsize*2) p=0;
    }while(p!=q);
  }
#ifdef MALLOC_DEBUG
  check_sfltable();
#endif
  free((char *)d->hashtable);
  d->hashsize*=2;
  d->hashtable=h;

  return add_key(d,key,hashval);
}

int set_database_entry(dbase *d,DBT key,DBT data)
{
  file dataf;
  int keyindex;
  unsigned int hashval;

  if(!read_hashtable(d)) return 0;

  SET(dataf.len,data.len);
  SET(dataf.ptr,write_file(d,data));
  if(!dataf.ptr) return 0;

  hashval=hashkey(key);
  keyindex=find_key(d,key,hashval);
  if(keyindex==-2) return 0;
  if(keyindex<0)
  {
    keyindex=add_key(d,key,hashval);
    if(keyindex<0)
    {
      free_file(d,GET(dataf.ptr),1);
      return 0;
    }
  }else{
    free_file(d,GET(d->hashtable[keyindex].data.ptr),0);
  }
  d->hashtable[keyindex].data=dataf;
  d->flags|=HASHTABLE_NEEDS_SAVING;
  if(!spurious_sync(d)) return 0;
  return 1;
}

int remove_database_entry(dbase *d,DBT key)
{
  unsigned int hashval;
  int keyindex;

  if(!read_hashtable(d)) return 0;

  hashval=hashkey(key);
  keyindex=find_key(d,key,hashval);
  if(keyindex==-2) return 0;
  if(keyindex<0)
  {
    return 1;
  }else{
    free_file(d,GET(d->hashtable[keyindex].data.ptr),0);
    free_file(d,GET(d->hashtable[keyindex].key.ptr),0);
    SET(d->hashtable[keyindex].key.ptr,-1);
    SET(d->hashtable[keyindex].key.len,-1);
    d->flags|=HASHTABLE_NEEDS_SAVING;
    if(!spurious_sync(d)) return 0;
    return 1;
  }
}

DBT get_database_entry(dbase *d,DBT key)
{
  DBT ret;
  unsigned int hashval;
  int keyindex;

  ret.data=NULL;
  ret.len=-1;

  if(!read_hashtable(d)) return ret;

  hashval=hashkey(key);
  keyindex=find_key(d,key,hashval);
  if(keyindex>=0)
  {
    ret.data=read_file(d,d->hashtable[keyindex].data);
    ret.len=GET(d->hashtable[keyindex].data.len);
  }
  return ret;
}

int count_database_entries(dbase *d)
{
  int ret, e;
  ret=0;
  if(!read_hashtable(d)) return -1;
  for(e=0;e<d->hashsize;e++)
    if(GET(d->hashtable[e].key.len)>=0)
      ret++;

  return ret;
}

int map_database_keys(dbase *d,void (*fun)(DBT))
{
  int e;
  DBT tmp;

  if(!read_hashtable(d)) return 0;
  for(e=0;e<d->hashsize;e++)
  {
    if(GET(d->hashtable[e].key.len)>=0)
    {
      tmp.data=read_file(d,d->hashtable[e].key);
      if(!tmp.data) return 0;
      tmp.len=GET(d->hashtable[e].key.len);
      fun(tmp);
    }
  }

  return 1;
}

dbase *open_database(char *name)
{
  int fd;
  dbase *d;

#if 0
  fd=open(name,O_RDWR | O_SYNC);
#else
  fd=open(name,O_RDWR);
#endif

  if(fd<0) return NULL;

  d=(dbase *)malloc(sizeof(dbase));
  d->fd=fd;
  d->hashtable=NULL;
  d->links=NULL;
  d->counter=0;
  d->no_files_to_free=0;
  d->flags=0;

  if(!read_block0(d))
  {
    close(fd);
    free((char *)d);
    return NULL;
  }

  if(!read_linktable(d))
  {
    close(fd);
    free((char *)d);
    return NULL;
  }
  return d;
}

dbase *create_database(char *name)
{
  char bb[BLOCK_SIZE];
  int fd,e;
  struct block0 b;

  fd=open(name,O_WRONLY | O_CREAT | O_EXCL,(6<<6)|(6<<3)|6);
  
  SET(b.magic1,MAGIC1);
  SET(b.magic2,MAGIC2);
  SET(b.links.ptr,1);
  SET(b.links.len, BLOCK_SIZE-BLOCK_SIZE%sizeof(int) );
  SET(b.hashtable.ptr,2);
  SET(b.hashtable.len, BLOCK_SIZE-BLOCK_SIZE%sizeof(hash_entry) );

  for(e=0;e<BLOCK_SIZE;e++) bb[e]=0;

  /* header */
  if(write(fd,(char *)&b,sizeof(b))!=sizeof(b))
  {
    close(fd);
    return NULL;
  }

  /* fill block0 */
  if(write(fd,bb,BLOCK_SIZE-sizeof(b))!=BLOCK_SIZE-sizeof(b))
  {
    close(fd);
    return NULL;
  }

  /* phony links */
  ((int *)(&bb[0]))[0]=-1;
  ((int *)(&bb[0]))[1]=-1;
  ((int *)(&bb[0]))[2]=-1;
  if(write(fd,bb,BLOCK_SIZE)!=BLOCK_SIZE)
  {
    close(fd);
    return NULL;
  }

  /* hashtable */
  for(e=0;e<BLOCK_SIZE;e++) bb[e]=-1;
  if(write(fd,bb,BLOCK_SIZE)!=BLOCK_SIZE)
  {
    close(fd);
    return NULL;
  }
  close(fd);

  return open_database(name);
}

int close_database(dbase *d)
{
  if(!sync_database(d))
    return 0;

  if(d->hashtable) free((char *)d->hashtable);
  if(d->links) free((char *)d->links);
  close(d->fd);
  free((char *)d);
  return 1;
}
