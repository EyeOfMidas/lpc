#include "efuns.h"
#include "simulate.h"
#include "array.h"
#include "stralloc.h"
#include "dbase.h"

static dbase *dbf=NULL;
char *last_file=NULL;

void do_sync_database()
{
  if(dbf) sync_database(dbf);
}

void do_close_database()
{
  if(dbf)
  {
    close_database(dbf);
    dbf=NULL;
    free_string(last_file);
    last_file=NULL;
  }
}

void f_db_flush(int num_arg,struct svalue *argp)
{
  do_close_database();
}

void fix_dbase_file(char *file)
{
  if(!last_file || strcmp(last_file,file))
  {
    do_close_database();

    dbf = open_database ( file );
    if(!dbf)
      dbf = create_database ( file );

    if(dbf)
      last_file=make_shared_string(file);
  }
}

void f_db_get(int num_arg,struct svalue *argp)
{
  char *file;
  DBT key,content;
  
  file = simple_check_valid_path(argp,"db_get", 0);
  if(!file)
  {
    pop_n_elems(2);
    push_zero();
    return;
  }
  fix_dbase_file(file);

  if(dbf)
  {
    key.data=strptr(argp+1);
    key.len=my_strlen(argp+1)+1;
    content=get_database_entry(dbf, key);
    if(content.data)
    {
      pop_n_elems(2);
      push_shared_string(
		 make_shared_binary_string(content.data,content.len-1));
      free((char *)(content.data));
    }else{
      pop_n_elems(2);
      push_zero();
    }
  }else{
    pop_n_elems(2);
    push_number(-1);
  }
}

void f_db_set(int num_arg,struct svalue *argp)
{
  char *file;
  DBT key,content;
  int ret;
  
  check_argument(2,T_STRING,F_DB_SET);
  file = simple_check_valid_path(argp,"db_set", 1);
  if(!file)
  {
    pop_n_elems(3);
    push_zero();
    return;
  }
  fix_dbase_file(file);

  if(dbf)
  {
    key.len=my_strlen(argp+1)+1;
    key.data=strptr(argp+1);
    content.data=strptr(argp+2);
    content.len=my_strlen(argp+2)+1;
    ret=set_database_entry(dbf, key, content);
    pop_n_elems(3);
    push_number(ret);
  }else{
    pop_n_elems(3);
    push_number(-1);
  }
}

void f_db_delete(int num_arg,struct svalue *argp)
{
  char *file;
  DBT key;
  int ret;
  
  file = simple_check_valid_path(argp,"db_delete", 1);
  if(!file)
  {
    pop_n_elems(2);
    push_zero();
    return;
  }
  fix_dbase_file(file);

  if(dbf)
  {
    key.data=strptr(argp+1);
    key.len=my_strlen(argp+1)+1;
    ret=remove_database_entry(dbf, key);
    pop_n_elems(2);
    push_number(ret);
  }else{
    pop_n_elems(2);
    push_number(-1);
  }
}

static struct vector *foovector;
static int foopos;

static void do_map_key(DBT key)
{
  SET_STR(foovector->item+foopos,make_shared_string(key.data));
  free(key.data);
  foopos++;
}

void f_db_indices(int num_arg,struct svalue *argp)
{
  char *file;
  int size;
  
  file = simple_check_valid_path(argp, "db_indices", 0);
  if(!file)
  {
    pop_n_elems(1);
    push_zero();
    return;
  }
  fix_dbase_file(file);

  if(dbf)
  {
    size=count_database_entries(dbf);
    if(size>=0)
    {
      foovector=allocate_array(size);
      if(foovector)
      {
	foopos=0;
	if(map_database_keys(dbf,do_map_key))
	{
	  pop_stack();
	  push_vector(foovector);
	  foovector->ref--;
	  return;
	}
	free_vector(foovector);
      }
    }
  }
  pop_stack();
  push_number(-1);
}

