inherit "/std/object";
#define GENDER_NEUTER 0
#define GENDER_MALE 1
#define GENDER_FEMALE 2

void create(string str)
{
  ::create(str);
}

int gender;
int set_gender(int g)
{ if (g > -1 && g < 3 ) return gender = g; }
int query_gender() { return gender; }
int set_neuter() { return gender = GENDER_NEUTER; }
int set_male() { return gender = GENDER_MALE; }
int set_female() { return gender = GENDER_FEMALE; }
int query_neuter() { return gender == GENDER_NEUTER; }
int query_male() { return gender == GENDER_MALE; }
int query_female() { return gender == GENDER_FEMALE; }


string query_gender_string()
{
  switch(gender)
  {
    case GENDER_NEUTER:  return "neuter";
    case GENDER_MALE:    return "male";
    case GENDER_FEMALE:  return "female";
  }
}

string query_pronoun()
{
  switch(gender)
  {
    case GENDER_NEUTER:  return "it";
    case GENDER_MALE:    return "he";
    case GENDER_FEMALE:  return "she";
  }
}

string query_possessive()
{
  switch(gender)
  {
    case GENDER_NEUTER:  return "its";
    case GENDER_MALE:    return "hes";
    case GENDER_FEMALE:  return "her";
  }
}

string query_objective()
{
  switch(gender)
  {
    case GENDER_NEUTER:  return "it";
    case GENDER_MALE:    return "him";
    case GENDER_FEMALE:  return "her";
  }
}
