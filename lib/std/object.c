#pragma strict_types
#pragma save_types
void move(object a) { move_object(a); }
void selfdestruct() { destruct(); }
void create() {} 
void init() {}
string dump() { return save_object(); }
