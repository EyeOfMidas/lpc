
int gender;
void set_gender(int g) { gender=g; }
int query_gender() { return gender; }

string query_gender_string() { return ({"neuter","male","female"})[gender]; }
string query_objective() { return ({"it","him","her"})[gender]; }
string query_possessive() { return ({"its","his","her"})[gender]; }
string query_pronoun() { return ({"it","he","she"})[gender]; }

