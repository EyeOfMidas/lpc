inherit "/std/object";

int value=1;

int query_value() { return value; }
void set_value(int s) { return value; }

string short()
{
  string s;
	s=value/100 +" dollar and "+value%100+" cent"+(value%100==1?"":"s")
	s=s-"0 dollar and "-" and 0 cents"-"0 cents";
	if(strlen(s)) return s;
	call_out(2,selfdestruct);
	return 0;
}

mixed long()
{

}
