int size; /* meter */
int xpos,ypos;
object get_entrance_room(string from,int obj_size)
{
  object o;
  if(parted && obj_size/3<=size)
  {
    obj_size/=3;
    if(o=get_subroom(from)->get_entrance_room(from,obj_size)) return o;
    if(from=="n" || from=="s")
    {
      if(o=get_subroom(from+"w")->get_entrance_room(from,obj_size)) return o;
      return get_subroom(from+"e")->get_entrance_room(from,obj_size);
    }else{
      if(o=get_subroom(from+"n")->get_entrance_room(from,obj_size)) return o;
      return get_subroom(from+"s")->get_entrance_room(from,obj_size);
    }
  }else{
    if(exit_blocked(from)) return 0;
    return this_object();
  }
}

object get_room(int x,int y)
{
  if(x<xpos || y<ypos || x>=xpos+size || y>=ypos+size)
    return environment()->get_room(x,y);
  if(!parted) return this_object();
  if(x-xpos<size/3)
  {
    if(y-ypos<size/3)
    {
      return get_subroom("sw")->get_room(x,y);
    }else if(y-ypos<size/3*2){
      return get_subroom("w")->get_room(x,y);
    }else{
      return get_subroom("nw")->get_room(x,y);
    }
  }else if(x-xpos<size/3*2){
    if(y-ypos<size/3)
    {
      return get_subroom("s")->get_room(x,y);
    }else if(y-ypos<size/3*2){
      return get_subroom("")->get_room(x,y);
    }else{
      return get_subroom("n")->get_room(x,y);
    }
  }else{
    if(y-ypos<size/3)
    {
      return get_subroom("se")->get_room(x,y);
    }else if(y-ypos<size/3*2){
      return get_subroom("e")->get_room(x,y);
    }else{
      return get_subroom("ne")->get_room(x,y);
    }
  }
}

object get_room_to(string dir)
{
  switch(dir)
  {
  case "n": return get_room(x,y+1);
  case "s": return get_room(x,y-1);
  case "e": return get_room(x+1,y);
  case "w": return get_room(x-1,y);
  }
}
