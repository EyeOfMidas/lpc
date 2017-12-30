/*dummy file */
#undef malloc
dump_malloc_data() { return; }
char *debugmalloc(int a) { return malloc(a); }
