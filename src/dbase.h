#define MAGIC1 0x42F00BA
#define MAGIC2 0xDAFABB0

/* save some memory, 512 is probably a better value though */
#define BLOCK_SIZE 256

/* sync database every 16 modification */
#define BUFFER 16

#define MAX(X,Y) ((X)<(Y)?(Y):(X))
#define MIN(X,Y) ((X)>(Y)?(Y):(X))

struct DBT_s
{
  char *data;
  int len;
};

typedef struct DBT_s DBT;

struct file_s
{
  int len;
  int ptr;
};

typedef struct file_s file;

struct block0
{
  int magic1;
  int magic2;
  file links;
  file hashtable;
};

/* a free hash entry has key.len==-1 */

struct hash_entry_s
{
  unsigned int hashval;
  file key;
  file data;
};

typedef struct hash_entry_s hash_entry;
typedef char block[BLOCK_SIZE];

struct dbase_s
{
  int fd;
  int flags;
  struct block0 block0;
  int hashsize;
  hash_entry *hashtable;
  int *links;
  int no_links;
  int no_files_to_free;
  int counter;
  int files_to_free[BUFFER+10];
};

typedef struct dbase_s dbase;

int sync_database(dbase *d);
int set_database_entry(dbase *d,DBT key,DBT data);
int remove_database_entry(dbase *d,DBT key);
DBT get_database_entry(dbase *d,DBT key);
int count_database_entries(dbase *d);
int map_database_keys(dbase *d,void (*fun)(DBT));
dbase *open_database(char *name);
dbase *create_database(char *name);
int close_database(dbase *d);
