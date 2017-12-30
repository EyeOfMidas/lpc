
void do_lvalue(node *n)
{
  switch(n->token)
  {
  case F_LOCAL:
    ins_f_byte(F_PUSH_LOCAL_LVALUE);
    ins_byte(n->u.s.a.number);
    break;

  case F_GLOBAL:
    ins_f_byte(F_PUSH_GLOBAL_LVALUE);
    ins_byte(n->u.s.a.number);
    break;

  case F_INDEX:
    docode(n->u.s.a.node);
    docode(n->u.s.b.node);
    ins_f_byte(F_PUSH_INDEXED_LVALUE);
    break;

  default:
    error("Illegal lvalue.\n");
    ins_int(0);
    break;
  }
}

void do_assign(node *to,node *from)
{
  switch(n->token)
  {
  case F_LOCAL:
    do_docode(from);
    ins_f_byte(F_ASSIGN_LOCAL);
    ins_byte(to->u.s.a.number);
    break;
    
  case F_GLOBAL:
    do_docode(from);
    ins_f_byte(F_ASSIGN_GLOBAL);
    ins_byte(to->u.s.a.number);
    break;

  case F_INDEX:
    do_lvalue(to->u.s.a.node);
    do_docode(to->u.s.b.node);
    do_docode(from);
    ins_f_byte(F_ASSIGN_INDEX);
    break;

  default:
    error("Illegal lvalue.\n");
    ins_int(0);
    break;
  }
}





void foo()
{
  switch()
  {
  case F_PUSH_LOCAL_LVALUE:
    sp++;
    sp->type=T_LVALUE_POINTER;
    sp->subtype=T_ANY;
    sp->u.svalue=fp+EXTRACT_UCHAR(pc);
    pc++;
    break;

  case F_RESOLV_INDEXED_LVALUE:
  {
    struct vector *v;
    int i;
    switch(sp[-1].type)
    {
    default:
      error("Indexing on illigal type.\n");

    case T_POINTER:
      if(sp[0].type!=T_NUMBER) error("Index is not an integer.\n");
      i=sp[0].u.number;
      if(i<0) i+=sp[-1].u.vec->size;
      if(i<0 || i>=sp[-1].u.vec->size) error("Index out of range.\n");
      sp[0].u.number=i;
      sp[0].type=T_LVALUE_VECTOR_INDEX;
      break;

    case T_MAPPING:
      sp++;
      sp->type=T_LVALUE_MAPPING_INDEX:
    }
  }

  case F_LVALUE_TO_SVALUE:
  {
    switch(sp[-1].type)
    {
    case T_LVALUE_POINTER:
      assign_svalue_no_free(sp,sp->u.svalue);
      break;

    case T_LVALUE_VECTOR_INDEX:
      assign_svalue(sp,sp[-1].u.vec->item+sp->u.number);
      free_svalue(sp-1);
      sp[-1]=*sp;
      sp--;
      break;

    case T_LVALUE_MAPPING_INDEX:
      i=assoc(sp[-2].u.vec->item[0].u.vec,sp[-1]);
      if(i==-1)
      {
	assign_svalue(
      }else{
      }
    }
  }
  }
}
  
