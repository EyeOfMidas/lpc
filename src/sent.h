struct sentence
{
  short num_arg;
  short pri;
  char *match;
  struct svalue function;
};


struct sentences
{
  int slots;
  int used_slots;
  struct sentence sent[1];  
};

