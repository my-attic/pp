#include "pp.h"

boolean_t FwdDomainDefAllowed;
halfword_t LabelList;
halfword_t FwdFunc;
halfword_t CurFID;

int sLevel;

// append a node n at the end of a list defined by the head h
// and the tail t
// if h is non empty then t is assumed to be non empty too
void AppendNode(halfword_t*h, halfword_t*t, halfword_t n)
{
	if (*h==NADA)  *h=n; else Mem[*t].nh.link=n;
	if (n!=NADA) *t=n;
}

void CompountSt(halfword_t*h, halfword_t*t);


//----------------------------------------------------------
// Statements
//----------------------------------------------------------
halfword_t Statement(halfword_t* t);


// return true if list la is only a constant attribute
// if so, then returns
// used only by with statement
// returns True also if la is the empty list (no attribute == null attribute)
static boolean_t ConstAttribute(int* k, halfword_t la)
{
	halfword_t e; // expression
	boolean_t ca=False; // result to return
    if (la==NADA) return True;
	if (Mem[la].ll.info==0)
	{
		e=Mem[la+1].hh.lo;
		if (Mem[e].nh.op==opConst)
		{
			*k=Mem[e+1].i;
			ca=True;
		}
	}
	return ca;
}

// TODO : this check can be used elsewhere
// check for a variable identifier and returns it
halfword_t CheckVarID()
{
	halfword_t x;
	x=CheckID();
	if (GetXID(x)!=sVAR) ReportError(VAREXPECTED);
	return x;
}



// with statement
//--------------
//+--------------------------+
//|opWith| level |     +----------> next
//|--------------------------|
//|   variable   |           |
//|--------------------------|
//|           offset         |
//+--------------------------+
//
// syntax : WITH rec_variable DO statements
// replace the identifier field of the record variable
// by variables
//

halfword_t WithStm(halfword_t*tl)
{

    halfword_t vn;		// created variable node for each field
    halfword_t id;     	// field identifier
    halfword_t wn;     	// with node
    halfword_t lt;     	// local tail
	halfword_t v;  		// variable id
	memoryword_t t; 	// type of the record variable
	byte_t cat;  		// category of record variable
	byte_t cas;  		// cas
	int offs;			// offset of variable
    int k;				// value of constant attribute or postoffset
	memoryword_t tf; 	// type of the field
	halfword_t la; 		// attribute list of record variable
	halfword_t SPtr=HiMemMin; 	// save stack bottom
	halfword_t hd=NADA;      	// head of list
	int tvp=TmpVarIndex; 		// memorise temp offset
	do
	{
		// check for a variable identifier
		halfword_t x=CheckID();
		if (GetXID(x)!=sVAR) ReportError(VAREXPECTED);// var id expected
		// this should be of record type
		VarAccess(&t,&v,True);
		if (t.th.bt!=btRec) ReportError(RECTYPEEXPECTED); // record type expected
		// check if entiere variable
		la=Mem[v+1].hh.hi;
		cat=Mem[Mem[v+1].hh.lo].qqqq.b0; // category of var
		offs=Mem[Mem[v+1].hh.lo+2].i;    // offset
		// 1st case : entiere variable with no or constant attribute
		// -> Create a variable node with offset equal
		//    to the sum of the offset of variable of offset of the field
		//    the constant attribute
		// 2nd case : reference with no or constant attribute with
		//       eventually post offset
		//       or entiere variable with indirection + constant
		// -> Create a variable node with post offset equal to the
		//    sum of the field offset and of the constant attribute
		// 3th case others
		// -> Create a with node with temporary variable initialised with
		//    dynamic address of the variable
		//    Create variable node with offset of this tmp var
		//    and post offs et equal to the field offset
		cas=3; // default
		k=0;
		switch (cat)
		{
		case 0: // entiere var
		    if (ConstAttribute(&k,la))
			{ // Constant attribute
				cas=1;
			}
			else if (Mem[la].ll.info==1)
			{ // indirection
				la=Mem[la].ll.link;
				if (la==NADA) cas=2; // single indirection
				else if (ConstAttribute(&k,la)) cas=2; // indirection with single constant
			}
			break;
		case 1: // reference
			if (ConstAttribute(&k,la))
			{ // reference with constant offset
				cas=2;
			}
		  	break;
		case 5: // reference with post offset
			cas=2;
			k=Mem[Mem[v+1].hh.lo+3].i; // postoffset
			break;
		}
		if (cas==3)
		{
			// allocate temporary variable
			offs=TmpVarIndex;
			TmpVarIndex+=4;
			if (TmpVarIndex>LocalsIndex) LocalsIndex=TmpVarIndex;
			//if sLevel>1 then
			offs=-TmpVarIndex;
			// create with node
			wn=GetAvail(3);
			Mem[wn].nh.op=opWith;
			Mem[wn+1].hh.lo=v;
			Mem[wn+2].i=offs;
			// append to list
			if (hd==NADA) hd=wn; else Mem[*tl].ll.link=wn;
			*tl=wn; // update tail
		}
		// create a new variable for each field
		// and save previous variable value
		halfword_t fl=Mem[t.th.link].ll.link;  // field list
		while (fl!=NADA) // loop for all fields
		{
			// create variable node
			if (cas==1)
		    {
				vn=GetAvail(3);
				Mem[vn].qqqq.b0=0; // entiere var
				Mem[vn+2].i=offs+k+Mem[fl+2].i; // add offsets
		    }
			else
			{
				vn=GetAvail(4);
				Mem[vn].qqqq.b0=5; // refrence with post offset
				Mem[vn+2].i=offs;  // assign offset
				Mem[vn+3].i=k+Mem[fl+2].i; // assign postoffset
			}
			// level
			if (cas==3) Mem[vn].qqqq.b1=sLevel;
			else Mem[vn].qqqq.b1=Mem[Mem[v+1].hh.lo].qqqq.b1; // same level as record variable
			// assign type
			tf=Mem[fl+1]; // field type
			Mem[vn+1]=tf; // assign type
			Mem[vn].qqqq.b2=tf.th.s; // scalar size
			// push old identifier values
			id=Mem[fl].ll.info; // field identifier
			HiMemMin-=2; // allocate room in stack
			if (HiMemMin<LoMemMax) ReportError(OUTOFMEMORY); // check room
			Mem[HiMemMin+1].hh.lo=id;
			Mem[HiMemMin].sy=HTable[id].symb;
			// assign new values
			HTable[id].symb.s=sVAR;
			HTable[id].symb.l=sLevel;
			HTable[id].symb.val=vn;
			fl=Mem[fl].ll.link; // next field
		}
	}
	while (ScanChar(','));
	CheckKW(kDO);
	// parse statement
	wn=Statement(&lt);
	// append in list
	if (hd==NADA) hd=wn; else Mem[*tl].ll.link=wn;
	*tl=lt; // update tail
	// restaure previous symbol definitions
	while (HiMemMin<SPtr)
    {
		HTable[Mem[HiMemMin+1].hh.lo].symb=Mem[HiMemMin].sy;
		HiMemMin+=2;
    }
	// restaure old tmp var ptr
	TmpVarIndex=tvp;
	// return value
	return hd;
}



// goto node
//+------------------------+
//|opGoto|Level|      +---------->
//|------------------------|
//| lbl node   |           |
//+------------------------+
halfword_t GotoStm()
{
	halfword_t x=ScanLabel();
	halfword_t n=GetAvail(2);
	if (GetXID(x)!=sLABEL) ReportError(UNDECLAREDLABEL,x); // undeclared label
	Mem[n].nh.op=opGoto;
	Mem[n].qqqq.b1=HTable[x].symb.l; // label level
	Mem[n+1].hh.lo=IDVal; // lbl node
	return n;
}

//+----------------------+
//| opIf |    |     +----------> next
//|----------------------|
//| test expr | true stm |
//|----------------------|
//| false stm |  xxxxx   |
//+----------------------+
halfword_t IfStm()
{
	halfword_t n=GetAvail(3);  // if node
	halfword_t f;  // False statement
	Mem[n].nh.op=opIf;
	CheckBoolExpr(&Mem[n+1].hh.lo);
	CheckKW(kTHEN);
	Mem[n+1].hh.hi=Statement(NIL);
	f=NADA;
	if (ScanKW(kELSE)) f=Statement(NIL);
	Mem[n+2].hh.lo=f;
	return n;
}


// scan a constant list of a given ordinal type t
// returns the list,
// append at the end of list h
// check duplicate constants
halfword_t ConstantList(memoryword_t t, halfword_t *h)
{
	halfword_t e;    	// current constant node
    memoryword_t t1; 	// constant type
    halfword_t y,l,cl;
    int v;  			// current constant value

    ConstExpr(&t1,&e,NOIDENT);
    cl=e;  // return value = first node
    do
    {
		CheckCompatible(t,t1);
		v=Mem[e+1].i;  // constant value
		// check no duplicate const and position y to tail
		y=NADA;
		l=*h;
		while (l!=NADA)
		{
		  y=l;
		  l=Mem[l].ll.link;
		  if (Mem[y+1].i==v) ReportError(DUPLICATECASE); // duplicate case
		}
		if (y==NADA) *h=e; else Mem[y].ll.link=e; // append
		Mem[e].ll.link=NADA;  // close list
		e=NADA; // for end of list test
		if (ScanChar(',')) ConstExpr(&t1,&e,NOIDENT);
    }
	while (e!=NADA); // i.e. no comma
    return cl;
}


//+------------------------+
//|opCase|     |      +--------> Next
//|------+-----+-----------|    +------------------+
//|    expr    | case list----->|   stm   |     +----->
//|------------+-----------|    |---------+--------|
//| else stm   |           |    |      constant    |
//+------------------------+    +------------------+

halfword_t CaseStm()
{
	memoryword_t t;
	halfword_t n=GetAvail(3);  // node
    halfword_t h=NADA; // global constant list
    halfword_t l; // current constant list
    halfword_t e=NADA; // else statement
    halfword_t s; // current statement

    Mem[n].nh.op=opCase;
    OptimizedExpression(&t,&Mem[n+1].hh.lo,NOIDENT);
    CheckOrdinalType(t);
    CheckKW(kOF);

    for(;;)
    {
		l=ConstantList(t,&h);
		CheckChar(':');
		s=Statement(NIL);
		do
		{	// assign statement to all constant node
			Mem[l].ll.info=s;
			l=Mem[l].ll.link;
		}
		while (l!=NADA);
		if (ScanKW(kEND))  break;
		if (ScanKW(kELSE)) goto l11;
		CheckChar(';');
		if (ScanKW(kELSE))
		{
		l11:
			e=Statement(NIL);
			ScanChar(';');
			CheckKW(kEND);
			break;
		}
		else if (ScanKW(kEND)) break;
    }
    Mem[n+1].hh.hi=h; // assign constant list
    Mem[n+2].hh.lo=e; // assign else statement
    return n;
}

//+---------------------------+
//| opWhile |    |     +----------> next
//|---------------------------|
//|   test expr  | stmatement |
//+----+----------------------+
halfword_t WhileStm()
{
	halfword_t n=GetAvail(2); // while node
	Mem[n].nh.op=opWhile;
	CheckBoolExpr(&Mem[n+1].hh.lo);
	CheckKW(kDO);
  	Mem	[n+1].hh.hi=Statement(NIL);
  	return n;
}

//+--------------------------+
//| opRepeat|    |      +---------> next
//|--------------------------|
//|   test expr  | statement |
//+--------------------------+
halfword_t RepeatStm()
{
	halfword_t n=GetAvail(2); // repeat node
	halfword_t h=NADA; // previous statement
	halfword_t x; //current statement
	halfword_t t; // tail of statement list
	do
	{
		x=Statement(&t);
		if (x!=NADA)
		{
			if (h==NADA) Mem[n+1].hh.hi=x; // memorise first statement
			else Mem[h].ll.link=x;          // else link to previous
			h=t;  // this is now previous expression
		}
	}
	while (ScanChar(';'));
	if (h!=NADA) Mem[h].ll.link=NADA; // close list
	else Mem[n+1].hh.hi=NADA;     // else no statement
	Mem[n].nh.op=opRepeat;
	CheckKW(kUNTIL);
	CheckBoolExpr(&Mem[n+1].hh.lo); // test expression
	return n;
}

// for statement
//+-------------------------+
//|opFor | to |     +------------> next
//|-----------+-------------|
//| index var | start expr  |
//|-----------+-------------|
//| bound exp | statement   |
//|-------------------------|
//|    bound var offset     |
//+-------------------------+

// 0 : to
// 1 : downto

halfword_t ForStm()
{
	halfword_t x,v,e1,n;
	memoryword_t t,t1;
	int bof;

	n=GetAvail(4);
	Mem[n].nh.op=opFor;
	x=CheckID();
	if (GetXID(x)!=sVAR) ReportError(VAREXPECTED); // var id expected
	 x=IDVal; // memorise variable symbol
	 VarAccess(&t,&v,True);
	 CheckOrdinalType(t);
	 // check allowed variable in a for index
	 if ( (Mem[v].nh.op!=opVar)
		   || (Mem[v+1].hh.hi!=NADA)      // entiere variable
		   || (Mem[x].qqqq.b1!=sLevel) // defined in current bloc
		   || (Mem[x].qqqq.b0!=0) )      // value variable not already used in a for loop
    	ReportError(INVALIDINDEX); // invalid index variable
	 Mem[x].qqqq.b0=4;      // lock x as a for index variable
	 Mem[n+1].hh.lo=v;
	 CheckDuo(':','=');
	 OptimizedExpression(&t1,&e1,NOIDENT);
	 AssCompTypes(t,t1,&e1);
	 if (ScanKW(kTO)) Mem[n].qqqq.b1=0;
	 else if (ScanKW(kDOWNTO)) Mem[n].qqqq.b1=1;
	 else ReportError(EXPECTEDKW,kTO); // to expected
	 Mem[n+1].hh.hi=e1;
	 OptimizedExpression(&t1,&e1,NOIDENT);
	 AssCompTypes(t,t1,&e1);
	 Mem[n+2].hh.lo=e1;
	 CheckKW(kDO);
	 // allocate tmp bound variable -- this variable is always allocated in locals
	 // TODO allocate only if bound expression is non constant
	 //bof=TmpVarIndex;
	 TmpVarIndex+=4; // word size
	 if (TmpVarIndex>LocalsIndex) LocalsIndex=TmpVarIndex;
	 //if (sLevel>1) bof=-TmpVarPtr;
	 bof=-TmpVarIndex;
	 Mem[n+2].hh.hi=Statement(NIL);
	 TmpVarIndex-=4; // free tmp variable
	 Mem[n+3].i=bof;
	 Mem[x].qqqq.b0=0; // return x to assignable
	 return n;
}

// assignement node
//+--------------------------+
//| opAssign | bt  |    |---------->
//|--------------------------|
//|     var        |   expr  |
//|--------------------------|
//|sa|da|   byte  size       |
//+--------------------------+
// sa : source alignement
// da : dest alingmenment

// parse a procedural expression that may be
// either a procedural symbol
// or a procedural variable
void FuncExpr(memoryword_t *t, halfword_t * e)
{
	halfword_t x=CheckID();
	switch(GetXID(x))
	{
	case sVAR:   VarAccess(t,e,False); break;
	case sFUNC:  *e=AddrFunc(btFunc,t); break;
	case sPROC:  *e=AddrFunc(btProc,t); break;
	default: ReportError(PROCVALEXPECTED);
	}
}

// is return type tr in register
// else it is a var parameter
boolean_t RegisterValued(memoryword_t tr)
{
	return
    (tr.th.bt!=btStr) &&
    ((tr.th.bt!=btSet) || (tr.th.s<=4));
}


// assign an expression to a variable access v
// of type vt. The variable access may be either
// a true variable or a pointer return function access
halfword_t AssignTo(memoryword_t vt, halfword_t v)
{
	memoryword_t et; // expression type
	halfword_t e;    // expression node
	halfword_t n;    // assignement node

	switch(vt.th.bt)
	{
	case btFunc:
	case btProc:
		FuncExpr(&et,&e);
		break;
	default: Expression(&et,&e,NOIDENT);
	}
	AssCompTypes(vt,et,&e);
	// check if it is an function that returns a non scalar type
	switch (Mem[e].nh.op)
	{
	case opPCall: case opPPCall: case opInline:
		// check type
		if (RegisterValued(vt)) goto def;
		// if attribute list exists, then it is a normal assignement
		if (Mem[e].ll.link!=NADA) goto def;
		// else assign then variable to the return parameter
		n=Mem[e+1].hh.hi; // actual parameter list
		// search last term of the list
		while (Mem[n].ll.link!=NADA) n=Mem[n].ll.link;
		// assign variable to the expression field
		Mem[n+1].hh.lo=v;
		// node is just the function node
		n=e;
		break;
	default:
	def:
		n=GetAvail(3);
		Mem[n].nh.op=opAssign;
		Mem[n].nh.bt=vt.th.bt;
		Mem[n].ll.link=NADA;
		OptimizeExpression(&e);
		Mem[n+1].hh.lo=v;
		Mem[n+1].hh.hi=e;
		Mem[n+2].i=TypeSize(vt); // size
		// assign alignment informations
		Mem[n+2].qqqq.b3=(et.th.s << 6)+((vt.th.s & 3) << 4);
	}
	return n;
}

void CheckAssignable()
{
  	switch (Mem[IDVal].qqqq.b0)
  	{
  	case 4: case 6: ReportError(ILLEGALASSIGN);
  	}
}


// Statement that begins with a variable identifier
// this is usually a variable assignement, but it may be a procedural variable
// invocation, e.g. proc brol(procedure a); begin a end;
halfword_t VarAssign()
{
	memoryword_t vt; // variable tye
	halfword_t n;    // node to return
    CheckAssignable(); // relevant only for ordinal type
    VarAccess(&vt,&n,True);
    OptimizeExpression(&n);
    if (ScanDuo(':','='))
    {	// assignement
        n=AssignTo(vt,n);
    }
    else
    {	// Invocation Only procedure or function without parameter are allowed
    	if (Mem[n].nh.op!=opPPCall) ReportError(INVALIDSTM); // invalid statement
    }
    return n;
}


halfword_t RetVarNode; // return variable node// offset for additionnal variables for a temporary with variable
  // or a bound variable of a for loop
  // e.g. for bounds or with statemsnts
  // used to check parameter and locals size

// this procedure search in the stack the return node that
// correspond to identifier "x"
// this occurs when a sub function assign the result of a function.
// if the identifier is not found then reports error
halfword_t SearchVarNode(halfword_t x)
{
	if (x==CurFID) return RetVarNode;

    halfword_t y=HiMemMin;
    while (y<MEMSIZE) // loop for all activation blocs
    {
    	if (x==Mem[y].hh.hi)  // check the name
    	{
    		// return the return variable note field in the stack
    	    return Mem[Mem[y].hh.lo+4].hh.hi;
    	}
    	y=Mem[y].hh.lo+6;
    }

    ReportError(ILLEGALASSIGN); // function not found in stack --> illegal assignement
}

// statement that start with a function symbol
// it may be:
// - return value of a function
// - procedure call ignoring result
// - deferencing pointer result

// the assignement to the return variable node is
//+----------------------------+
//|opFAssign|  bt |     +---------->
//|---------+-----+------------|
//|     var       |   expr     |
//|----------------------------|
//|sa|da|       size           |
//+----------------------------+
// sa : source alignement (expression)
// da : destination alignement (function return type)
// x is the function identifier

halfword_t FuncStm(halfword_t x)
{
	memoryword_t t,t1;    // expr type
	halfword_t n;        // returned node
	halfword_t e;        // expression
	halfword_t vi;       // variable return node
	halfword_t vn;       // variable node
	halfword_t l;        // a list node

	if (ScanDuo(':','='))
	{ // result of current function
		vi=SearchVarNode(x);
		t1=Mem[Mem[IDVal].hh.hi]; // return type of function
		OptimizedExpression(&t,&e,NOIDENT);
		AssCompTypes(t1,t,&e);
		vn=GetAvail(2); // variable node
		// initialisation of variable node
		Mem[vn].nh.op=opVar;
		Mem[vn].nh.bt=t1.th.bt;
		Mem[vn].qqqq.b2=t1.th.s;
		Mem[vn+1].hh.hi=NADA;
		Mem[vn+1].hh.lo=vi;
		n=GetAvail(3); // instruction node
		// initialisation of instruction node
		Mem[n].nh.op=opFAssign;
		Mem[n].nh.bt=t1.th.bt;
		Mem[n+1].hh.lo=vn;  // return variable node
		Mem[n+1].hh.hi=e;   // optimised expression
		Mem[n+2].i=TypeSize(t1);
		Mem[n+2].qqqq.b3=(t.th.s << 6)+((t1.th.s & 3) << 4);
	}
	else
	{
		n=FuncCall(&t1);
		switch (Mem[n].nh.op)
		{
		case opPCall:
			if (Mem[n].ll.link!=NADA)
			{
			  // assignement to dereferenced function result
				CheckDuo(':','=');
				n=AssignTo(t1,n);
			}
			else // function call and leave the result
			{
				if (!RegisterValued(t1))
			    { // replace the result parameter by a const parameter
					// this way, the allocated room in stack will be freed
					// else, it means that the result is pushed
					l=Mem[n+1].hh.hi; // actual parameter list
					// get last
					while (Mem[l].ll.link!=NADA) l=Mem[l].ll.link;
					// replace with a const parameter
					Mem[l].qqqq.b0=4;
			    }
			}
			break;
		case opPPCall:
			if (ScanDuo(':','='))
			{ // assignement
				n=AssignTo(t1,n);
			}
		}
	}
	return n;
}


// search in a variant list l a record node that
// correspond to constant x.
halfword_t SearchField(halfword_t l,int x)
{
	while (l!=NADA)
	{
		if (Mem[l+1].i==x) return Mem[l].ll.info;
		l=Mem[l].ll.link;
	}
	ReportError(NOSUCHVARIANT); // no such variant
}

// arguments of predefined procedures 'new' and 'dispose'
// syntax : new(var_ptr[,variant_const]*)
// node
//+--------------------+
//| op |    |     +---------> next  opNew/opDipose
//|--------------------|
//| ptr var |  xxxxxx  |
//|--------------------|
//|     [ size ]       |  new only
//+--------------------+

halfword_t NewDisposeStm(halfword_t n)
{
	halfword_t x; // var ptr identifier
	halfword_t v; // variable
	halfword_t y; // created node
	halfword_t e; // constant expression
	halfword_t r; // record node
	memoryword_t t; // type of variable
	memoryword_t tc; // domaine type
	int s; // Pointee size
	CheckChar('(');
	x=CheckID();
	if (GetXID(x)!=sVAR) ReportError(PTRVAREXPECTED);
	VarAccess(&t,&v,True);
	if (t.th.bt!=btPtr) ReportError(PTRVAREXPECTED);
	if (t.th.link==NADA) goto genericptr;
	if (ScanChar(','))
	{   // list of variant constant
		tc=Mem[t.th.link]; // domain node
		tc=Mem[tc.th.link]; // domain type
		if (tc.th.bt!=btRec)  ReportError(RECTYPEEXPECTED); // record expected
		r=tc.th.link; // record node
		do
		{
			ConstExpr(&tc,&e,NOIDENT);
			CheckCompatible(tc,Mem[r+2]);
			r=SearchField(Mem[r].hh.lo,Mem[e+1].i);
		}
		while (ScanChar(','));
		s=Mem[r+1].i;
	}
	else s=TypeSize(Mem[Mem[t.th.link].th.link]); //  /!\  domain type is double indirection in node
genericptr:
	CheckChar(')');
	// Create node
	if (n==0)
	{
		y=GetAvail(3);
		Mem[y].nh.op=opNew;
		Mem[y+2].i=s;  // size to alloc
	}
	else
	{
		y=GetAvail(2);
		Mem[y].nh.op=opDispose;
	}
	Mem[y+1].hh.lo=v; // variable
	return y;
}

// check if a basic type is write able
//------------------------------------
void CheckWriteable(BasicType_t bt)
{
	switch (bt)
	{
	case btInt: case btUInt: case btBool: case btChar: case btStr: case btReal:
		return;
	}
	ReportError(NONWRITEABLE);
}


// write/writeln
//---------------
// create a list of such expressino
//+-------------------------------+
//| opWriteText | bt |    +----------> Next
//|-------------------------------|
//|      text        |    expr    |
//|-------------------------------|
//|      width       |   prec     |
//+-------------------------------+
//
//+----------------------------+
//| opWrite | xx |     +----------> Next
//|----------------------------|
//|   file       |     +----------> Expression list
//|----------------------------|  (as in read node, see below)
//| access n pos |  Nada       |
//+----------------------------+
//
// access node position is the location where
// append the file access node for assignement f^:=e
//
//+----------------------------+
//| opNewline | XX |     +----------> Next
//|----------------------------|
//|   file         |    xxxx   |
//+----------------------------+

// input : ln =1 for writeln 0 for write
// returns the generated statement list and assign the
// tail in t
halfword_t WriteStm(halfword_t*t,halfword_t ln)
{
	memoryword_t	ty;	// type of expressions
	memoryword_t	tf;	// component type of file, for non text only
	halfword_t 		e;	// expression
	halfword_t		f;	// file expression to write on
	halfword_t		h;	// head of the generated list
	halfword_t		n;	// node under ruction
	halfword_t		wd;	// width expression
	halfword_t		pr; // precision expression
	boolean_t		txt; // boolean that indicates a text
	BasicType_t		bt;	// basic type

	f=NADA; // default output
	h=NADA;
	if (ScanChar('('))
	{
		OptimizedExpression(&ty,&e,NOIDENT);
		txt=True; // default
		switch(ty.th.bt)
		{ // search for a destintation file r.g. write(f,...)
		case btFile:
			if (ln!=0) ReportError(INVALIDTYPE); // Invalid type -- cannot writeln on a file
			txt=False;
			tf=Mem[ty.th.link]; // component type of the file
			f=e;
			if (! ScanChar(',')) goto finish;
		    break;
		case btText:
	        f=e;
	        if (!ScanChar(',')) goto finish;
	        break;
		default:
			f=NADA;
			goto noexpr; // the expression has already been parsed
		}
		n=NADA;
		for(;;)
		{
			OptimizedExpression(&ty,&e,NOIDENT);
		noexpr:
			if (txt)
			{
				n=GetAvail(3);
				bt=ty.th.bt;  // type to write
				CheckWriteable(bt);  // check if this type is writeable
				wd=NADA;   // default width
				pr=NADA;   // default precision
				if (ScanChar(':'))
				{
					OptimizedExpression(&ty,&wd,NOIDENT);  // scan width expression
					CheckIntegerType(ty.th.bt);
					if (bt==btReal)  // if type to write is real, scan for a possible precision
					{
						if (ScanChar(':'))
						{
							OptimizedExpression(&ty,&pr,NOIDENT);
							CheckIntegerType(ty.th.bt);
						}
					}
				}
				Mem[n].nh.op=opWriteText;  // generate the statement node
				Mem[n].nh.bt=bt;
				Mem[n].nh.link=NADA;
				Mem[n+1].hh.lo=f;  	// file to write on
				Mem[n+1].hh.hi=e;	// expression to write
				Mem[n+2].hh.lo=wd;	// width
				Mem[n+2].hh.hi=pr;	// precision
				AppendNode(&h,t,n);	// append to statement list
			}
			else
			{   // non text file
				if (n==NADA)
				{   // first time : allocate the write node
					n=GetAvail(3);
					Mem[n].nh.op=opWrite;
					Mem[n+1].hh.lo=f;
					wd=n+1; // initialise node to assign expression
					// search last attribute node
					pr=f;
					if (Mem[f].nh.op==opVar) pr=pr+1;
					// last node of the list
					while (Mem[pr].ll.link!=NADA) pr=Mem[pr].ll.link;
					// initialise file access node
					Mem[n+2].ll.info=pr;
					Mem[n+2].ll.link=NADA;
				}
				pr=GetAvail(3);  // allocate node for expression
				Mem[wd].ll.link=pr; // append in list
				wd=pr; // this is new tail
				AssCompTypes(tf,ty,&e); // check compatibles types
				Mem[pr].nh.bt=tf.th.bt;
				Mem[pr].ll.link=NADA;
				Mem[pr+1].hh.lo=f;
				Mem[pr+1].hh.hi=e;
				Mem[pr+2].i=TypeSize(tf);
				Mem[pr+2].qqqq.b3 |= ((ty.th.s &3) << 6);
			}
			if (ScanChar(','))
			{	// next expression to write
				continue;
			}
		finish:
			CheckChar(')');
			break;
		}
	}
	if (!txt) AppendNode(&h,t,n); // for non text files, just append a single write node
	if (ln!=0)
	{	// append a newline statement
		n=GetAvail(2);
		Mem[n].nh.op=opNewLine;
		Mem[n+1].hh.lo=f;
		AppendNode(&h,t,n);
	}
	return h;  // returns the head of the list
}


// read/readln
//------------
// create a list of such expression
//+------------------------------+
//| opReadText | bt |    +----------> Next
//|------------------------------|
//|      text       |    v       |
//+------------------------------+
// v is the variable to assign
// bt is the basic type to read
//
//+----------------------------+
//| opRead | xx  |     +----------> Next
//|----------------------------|
//|    file      |     +----------> Expression list
//|----------------------------|
//|   file tail  |    Nada     |
//+----------------------------+
// file tail is the last attribute list location
// in the file variable in order to append
// the file access node for assignemenbts v:=f^
// bt is the basic type of the component type of the file

// Expression list : for assignements v:=f^
// similar to assign node
//+----------------------------+
//| xx   | bt   |    +------------> Next
//|----------------------------|
//|    variable |    expresson |  (with eventual cast node )
//|----------------------------|
//|sa|la|        size          |
//+----------------------------+
//
//+----------------------------+
//| opReadln | XX |     +----------> Next
//|----------------------------|
//|    text        |    xxxx   |
//+----------------------------+
//
// the readln node is not appended if the latest type is string


halfword_t ReadStm(halfword_t*t,halfword_t ln)
{

	halfword_t v;   // variable node
	halfword_t f;   // file
	memoryword_t ty;  // type of the variable
	memoryword_t tf;  // component type (file only)
	boolean_t txt;  // flag for text file
	halfword_t h;   // head of list
	halfword_t n;   // node under construction
	halfword_t le;  // exression list
	halfword_t te;	// tail of expression list
	halfword_t fe;   // file expression for assignement

	h=NADA;
	f=NADA;
	ty.th.bt=btVoid;  // initialise for readln; and check latest type is not string
	if (ScanChar('('))
	{
		CheckVarID();
		VarAccess(&ty,&v,True);
		txt=True; // by default
		switch (ty.th.bt)
		{
		case btFile:
			if (ln!=0) ReportError(INVALIDTYPE); // invalid type
			txt=False;
			tf=Mem[ty.th.link]; // component file
			f=v;
			if (!ScanChar(',')) goto finish;
		  	break;
		case btText:
			f=v;
			if (!ScanChar(',')) goto finish;
			break;
		default:
			goto symbolread;
		}
		n=NADA;
		do
		{
			CheckVarID();
			VarAccess(&ty,&v,True);
symbolread:
			if (txt)
			{   // text file
				n=GetAvail(2);
				switch(ty.th.bt) // check readable tyoe
				{
				case btChar:
				case btInt:
				case btUInt:
				case btReal:
				case btStr:
					break;
				default: ReportError(NONREADABLETYPE); // non readable type
				}
				Mem[n].nh.op=opReadText;
				Mem[n].nh.bt=ty.th.bt;
				Mem[n+1].hh.lo=f;
				Mem[n+1].hh.hi=v;
				AppendNode(&h,t,n);
			}
			else
			{  // not a text file
				if (n==NADA)
				{ // first time : allocate read node
					n=GetAvail(3);
					Mem[n].nh.op=opRead;
					Mem[n+1].hh.lo=f;
					// set tail to assign expression list
					te=n+1;
					// search tail of attribute list
					le=f;
					if (Mem[f].nh.op==opVar) le++;
					while (Mem[le].ll.link!=NADA) le=Mem[le].ll.link;
					Mem[n+2].hh.lo=le;
					Mem[n+2].hh.hi=NADA;
				}
				le=GetAvail(3); // allocate node for expression
				Mem[te].ll.link=le; // append to tail
				te=le; // set new tail
				// check compatible type and set cast indicator
				fe=f;
				AssCompTypes(ty,tf,&fe);
				Mem[le].nh.bt=ty.th.bt;
				Mem[le].ll.link=NADA;
				Mem[le+1].hh.lo=v;
				Mem[le+1].hh.hi=fe;
				Mem[le+2].i=TypeSize(ty);
				Mem[le+2].qqqq.b3|=((ty.th.s & 3) << 4);
			}
		}
		while (ScanChar(','));
finish:
		CheckChar(')');
	}

	if (!txt) AppendNode(&h,t,n);
	// is it readln ?
	if (ln!=0)
	{
		n=GetAvail(2);
		Mem[n].nh.op=opReadln;
		Mem[n+1].hh.lo=f;
		AppendNode(&h,t,n);
	}
	return h;
}


// put, get, close. Accept either file and text argument
//----------------
//+-------------------------+
//| op |  nf   |     +------------>
//|-------------------------|
//|  file var  |   nf       |
//+-------------------------+
// op : opFile or opClose
// val is function number. It is given by IDVal
// Close : 168
// Get   : 140
// put   : 116

halfword_t FilePStm(halfword_t*t,halfword_t nf)
{
	halfword_t v;
	halfword_t n;
	memoryword_t ty;

	CheckChar('(');
	CheckVarID();
	VarAccess(&ty,&v,True);
	switch(ty.th.bt)
	{
	case btFile:
	case btText:
		break;
	default:
		ReportError(INVALIDTYPE); // invalid type -- file text
	}
	n=GetAvail(2);
	Mem[n].nh.op=opFile;
	Mem[n].ll.link=NADA;
	Mem[n+1].hh.lo=v;
	Mem[n+1].hh.hi=nf;
	CheckChar(')');
	*t=n;
	return n;
}


// open/close
//-----------
// syntax: reset(f,name) or reset(f)
//+------------------------------+
//| opOpen | mode |      +--------------> next
//|------------------------------|
//|  file var     | expr name    |
//|------------------------------|
//|    size of component type    |
//+------------------------------+

//
// mode b0 = 0 : reset; b0 = 1 : rewrite (info is in IDVal)
//      b2 = 1 for string expression
//           else, it is a parameter
// expr is either a string valued expression
// or a parameter number

halfword_t OpenStm(halfword_t*t,byte_t rw)
{
	halfword_t n;    // node under construction
	halfword_t v;    // variable node
	halfword_t s;    // name expression
	memoryword_t ty;
	int sz;

	CheckChar('(');
	CheckVarID();
	VarAccess(&ty,&v,False);
	switch(ty.th.bt) // get size of component type
	{
	case btFile:
		sz=TypeSize(Mem[ty.th.link]);
		break;
	case btText:
		sz=1;  // file of char
		break;
	default:
		ReportError(INVALIDTYPE); // invalid type
	}
	if (ScanChar(','))
	{ // name is specified
		OptimizedExpression(&ty,&s,NOIDENT);
		if (ty.th.bt!=btStr) ReportError(INVALIDTYPE);
	}
	else
	{  // if name is not specified
       // check that f is of text type
		if (ty.th.bt!=btText) ReportError(INVALIDTYPE);
		s=NADA;
	}
	CheckChar(')');
	n=GetAvail(3);
	Mem[n].nh.op=opOpen;
	Mem[n].qqqq.b1=rw;
	Mem[n].hh.hi=NADA;

	Mem[n+1].hh.lo=v;
	Mem[n+1].hh.hi=s;

 	Mem[n+2].i=sz;
 	*t=n;
 	return n;
}

// statement that starts with an identifier
//------------------------------------------
halfword_t OtherStatement(halfword_t*tn)
{
	halfword_t id;	// identifier
	halfword_t n;	// node under construction
	halfword_t t;	// local tail
	n=NADA; 		// this is the default value
	t=NADA;     	// initialised to NADA to check if it has been changed
	id=ScanID();
	if (id!=NOIDENT) switch (GetXID(id))
	{
	case sVAR: 		n=VarAssign(); break;
	case sPROC:		n=FuncCall1(NIL); break;
	case sFUNC: 	n=FuncStm(id); break;
	case sNEW:  	n=NewDisposeStm(IDVal); break;
	case sWRITE: 	n=WriteStm(&t,IDVal); break;
	case sREAD:  	n=ReadStm(&t,IDVal); break;
	case sFILEP:	n=FilePStm(&t,IDVal); break;
	case sOPEN: 	n=OpenStm(&t,IDVal); break;
	default: 		ReportError(UNEXPECTEDID,id); // unexpected identifier
	}
	if (t==NADA) t=n; // if tail not assigned, then single node statement or empty statement
	*tn=t;
	return n;
}


// returns a statement or statement list
// and assign tail to *t if t <> nil
halfword_t Statement(halfword_t* t)
{
	halfword_t n, tn; // tn is local tail
	CheckStack();
	tn=NADA;
	if (ScanKW(kBEGIN)) CompountSt(&n, &tn);
	else if (ScanKW(kWITH))n= WithStm(&tn);
	else if (ScanKW(kGOTO)) n=GotoStm();
	else if (ScanKW(kIF)) n=IfStm();
	else if (ScanKW(kCASE)) n=CaseStm();
	else if(ScanKW(kWHILE)) n=WhileStm();
	else if (ScanKW(kREPEAT)) n=RepeatStm();
	else if (ScanKW(kFOR)) n=ForStm();
	else n=OtherStatement(&tn); // returns Nada if empty
	if (tn==NADA) tn=n; // returns NADA if empty
	if (tn!=NADA) Mem[tn].ll.link=NADA; // if not empty, then close the list
	if (t!=NIL) *t=tn;  // write tail if required
	return n;
}


//-----------------------------------------------------------
// Declarations
//------------------------------------------------------------

int MinList, MaxList; // min and max values of a valued list ( enumerated types )
//---------------------------------------------------
// Ideitifier list
//---------------------------------------------------

// scan a list of new identifier of type sVar
// x : already scaned identifier, NOIDENT elsewhere
// nodesize is the size of the generated node for each identifier
// t is the tail (last node of the list)
// v = true for scanning value tohether with identifier
// eg a,b=5,c=10...
// in this case, assign MinList and MaxList
// assign minlist and maxlist which are min and max values of an enumerated list
static halfword_t ScanIdList(halfword_t x, int nodesize, halfword_t *t1, boolean_t v)
{
	halfword_t y; // node under construction
	if (x==NOIDENT) x=CheckID();
	halfword_t l=GetAvail(nodesize); // list to return
	halfword_t t=l; // current tail of the list
	NewID(x,sVAR);
	MinList=-1;
	int n=0; // value
	for(;;)
	{
		HTable[x].symb.val=t; // set symbol value to node
		Mem[t].ll.info=x; // set info field to id
		if (v)
		{
			if (ScanChar('='))
			{
				MaxList=ConstIntExpr();
				if (n>MaxList) ReportError(INVALIDORDER);
				n=MaxList;
			}
			MaxList=n;
			if (MinList<0) MinList=n;
			Mem[t+1].i=n;
			n++;
		}
		if (!ScanChar(',')) break;
	    x=CheckID();
	    NewID(x,sVAR);
	    y=GetAvail(nodesize);
	    Mem[t].ll.link=y; // link list
	    t=y;  // new current node
	}
	// close list
	Mem[t].ll.link=NADA;
	if (t1!=NIL) *t1=t; // assign tail if needed
	return l;
}


//--------------------
//  Type declaration
//--------------------

// check for a type identifier
// assign type node to tn if non nil
// and assign type to t
void CheckTypeID(memoryword_t*t,halfword_t*tn)
{
	halfword_t x;
	if (ScanKW(kSTRING))
	{
		*t=StringType;
		if (tn!=NIL) *tn=StringTypeNode;
	}
	else
	{
		x=ScanID();
		if (x==NOIDENT) ReportError(TYPEIDEXPECTED); // type identifierr expected
        if (GetXID(x) != sTYPE) ReportError(TYPEIDEXPECTED);
        if (tn!=NIL) *tn=IDVal;
        *t=Mem[IDVal];
	}
}

// returns min value of an ordinal type
int TypeMin(memoryword_t t)
{
	if (t.th.link!=NADA) return Mem[t.th.link].i;
	if (t.th.bt==btInt) return 0x80000001;
	return 0;
}

// returns max value of an ordinal type
static int TypeMax(memoryword_t t)
{
	if (t.th.link!=NADA) return Mem[t.th.link+1].i;
	switch (t.th.bt)
	{
	case btBool: return 1;
	case btChar: return 255;
	case btInt: return 0x7fffffff;
	default: ReportError(INTERNAL,"TypeMax");
	}
}

/*
 TODO Chech subrange bounds
 min <= max whatever the sign
 check that max <= maxint if min is negative
*/
//
// subrange type, for example type byte = 0..255
// read min and max, check constants, type compatibility
//
//+-------------------+
//| bt |  s |   +---------,
//+-------------------+   |
//                        |
// subrange node          |
//+-------------------+   |
//|       min         |<--'
//|-------------------|
//|       max         |
//+-------------------+

void SubRangeType(memoryword_t*t, halfword_t x)
{
	int Mini, Maxi;
	memoryword_t t2;
	Mini=ConstOrdExpr(t,x);
	CheckDuo('.','.');
	Maxi=ConstOrdExpr(&t2,NOIDENT);
	CheckCompatible(*t,t2);
	if (Mini>Maxi) ReportError(INVALIDORDER);
	// define size
	if (IsIntegerT(*t))
	{
		if (Mini>=0)
		{
			t->th.bt=btUInt;
			if (Maxi<256) t->th.s=1;
			else if (Maxi<65536) t->th.s=2;
			else t->th.s=4;
		}
		else
		{
			t->th.bt=btInt;
			if ((Mini>=-128)&&(Maxi<128)) t->th.s=1;
			else if ((Mini>=-32768)&&(Maxi<32768)) t->th.s=2;
			else t->th.s=4;
		}

	}
	x=GetAvail(2);
	Mem[x].i=Mini;
	Mem[x+1].i=Maxi;
	t->th.link=x;
}

// enumerated
//+---------------------+
//| bt | s  |    +---------> Range node (Min,Max)
//+--------------------+

BasicType_t EnumCount;

void EnumeratedType(memoryword_t*t)
{
	HEntry_t *h;
	if (EnumCount==btMax) ReportError(TOOMUCHENUM); // too much enumerate
	halfword_t x=ScanIdList(NOIDENT,2,NIL,True);
	CheckChar(')');


	t->th.bt=EnumCount;
	// compute size
	if (MaxList<256) t->th.s=1;
	else if (MaxList<65536) t->th.s=2;
	else t->th.s=4;
    // createb range node
	halfword_t y=GetAvail(2);
	Mem[y].i=MinList;
	Mem[y+1].i=MaxList;
	t->th.link=y;

	// create const nodes
	while (x!=NADA)
	{
		y=Mem[x].ll.link;
		h=HTable+Mem[x].ll.info;
		h->symb.s=sCONST;
		h->symb.val=x;
		Mem[x]=*t;
		x=y;
	}
	// initialise canonical set of this enum
	//if (MaxList>=MAXSET) MaxList=MAXSET-1;
	// TODO ???
	EnumCount++;
}

// pointer type
//+------------------+              +-----------+
//| btPtr | s=4 |  +--> Domain node |    |  +----> domain type
//+------------------+              +-----------+

// The indirection to domain node is a way to simply
// solve the problem of forward domain type definition
// Doing so, domain type assignement is valid for
// all instances of the defined type
//
// nt = node where to write type
// if n=Nada

halfword_t StringTypeNode; // node that contains the string type

boolean_t FwdDomainDefAllowed; // indicates that forward domain type definition
		// of pointer type is allowed.
		// default is false.
		// set to true at the begining of a type declaration
		// reset to false at the end of the same type declaration

halfword_t FwdType; // list of forward defined domain type for pointers
// initialised to NADA at beginning of type def
// checked at the end, used while denote a pointer type


void PointerType(memoryword_t*t)
{
	halfword_t x; // identifier
	halfword_t z; // domain type
	halfword_t dn=GetAvail(1); // domain type

	if (ScanKW(kSTRING))
	{
		z=StringTypeNode;

	}
	else
	{  // pointer type may be forward defined
		x=CheckID();
		if (!FwdDomainDefAllowed)
		{
			if (HTable[x].symb.s!=sTYPE) ReportError(TYPEIDEXPECTED);
			z=HTable[x].symb.val;
		}
		else
		{
			// if forward type definition allowed, then append in FwdType list
			z=GetAvail(2);
			Mem[z].ll.link=FwdType;
			FwdType=z;
			Mem[z].ll.info=x;  // symbol
			Mem[z+1].hh.lo=dn; // domain node
			z=NADA; // indicates that domain type is not defined
		}
	}
	Mem[dn].ll.link=z;
	t->th.bt=btPtr;
	t->th.s=4;
	t->th.link=dn;
}

// alignement depends on the size
// real : double word
// 7 : byte alignement
// 6 : halfword alignement
// 4 : word alignement
// 0 : dword alignement
// the more sever is the and wise of all alignements
byte_t Alignement(memoryword_t t)
{
	if (t.th.bt==btStr) return 7; // string is byte aligned
	byte_t a=t.th.s&7;
	if (a&1)a=7;
	else if (a&2) a=6;
	return a;
}


// multiplication of a short by a long
// and check overflow
static word_t LSMul(halfword_t a, word_t b)
{
	word_t c0, c1;
	c0=((word_t)a)*(b&0xffff);
	c1=((word_t)a)*(b >> 16) + (c0>>16);
	if (c1>0x7fff) ReportError(STRUCTURETOOLARGE);
	return (c0&0xffff)+(c1<<16);
}

// multiply two longs
// and check overflow
static word_t LLMul(word_t a, word_t b)
{
	if (a<=0xffff) return LSMul(a,b);
	else if (b<=0xffff) return LSMul(b,a);
	else ReportError(STRUCTURETOOLARGE);

}

// initialise size and alignement of array
//-------------------------------
void ArrayInitSize(memoryword_t *t)
{
	memoryword_t tc,ti;
	int s;

	tc=Mem[t->th.link+1];
	if (tc.th.bt==btArray)
	{
		CheckStack();
		ArrayInitSize(&tc);
	}
	// compute size
	ti=Mem[t->th.link];
	s=TypeSizeOf(tc);
	s=LLMul(s,TypeMax(ti)-TypeMin(ti)+1);
	if (s>=(1 << 24)) ReportError(STRUCTURETOOLARGE); // structure too large
	Mem[t->th.link+2].i=s;
	// propagate alignement
	// the alignement of an array is the alignement of the components
	t->th.s=tc.th.s & 7; //TypeAlign(tc);
}


// type denoter forward declaration
void TypeDen(memoryword_t*t);

//+-----------------------+
//|    index type         |
//|-----------------------|
//|    component type     |
//|-----------------------|
//|     size              |
//+-----------------------+

void ArrayType(memoryword_t*t, boolean_t pk)
{
	halfword_t x;
	memoryword_t  ta, ti, tc;  // array type, index type and component type
	CheckChar('[');
	ta.th.bt=btArray;
	ta.th.link=GetAvail(3);
	*t=ta;
	for (;;)
	{

		x=ScanID();
		if ((x!=NOIDENT) && (GetXID(x)==sTYPE)) ti=Mem[IDVal];
		else SubRangeType(&ti,x);
		CheckOrdinalType(ti);
		Mem[ta.th.link]=ti;
		if (!ScanChar(',')) break;
		tc.th.bt=btArray;
		tc.th.link=GetAvail(3);
		Mem[ta.th.link+1]=tc;
		ta=tc;
	}
	CheckChar(']');
	CheckKW(kOF);
	TypeDen(&Mem[ta.th.link+1]);
	// Conmpute sizes and alignements
	ArrayInitSize(t);
	// reduce packed array[1..n] of char to string type
	if (pk)  // packed
		if (Mem[t->th.link+1].i==TCHAR) // component type is char
		if (IsIntegerT(ti))  // index type is integer
		if (ti.th.link!=NADA)   // is it subrange
		if (Mem[ti.th.link].i==1) // lower bound is 1
		{
			*t=StringType;  // new type is string type
			if (Mem[ti.th.link+1].i>=StringType.th.link)
				ReportError(CONSTOUTOFBOUND);
			x=Mem[ti.th.link+1].i; // set size
			FreeNode(t->th.link,3);
			t->th.link=x;
		}
}

// Record node
//    +----> variant list
//+---|-----------+             +--------------+
//|   +   |   +---------------->|  id   |   +-----> next
//|---------------| Field list  |--------------|
//|     size      |             |     type     |
//|---------------|             |--------------|
//|(variant type) |             |    offset    |
//+---------------+             +--------------+
// variant type is present only if variant list is non empty
// variant list:
//
//    +-----> record node (without field list)
//+---|-----------+
//|   +   |  +----------> next const
//|---------------|
//|   constant    |
//+---------------+


// return address alignement according to type T
int FieldAlign(int*offset, memoryword_t t)
{
	int x;
	x=Align(*offset,t);
	*offset=x+TypeSize(t);
	if (*offset>1000000) ReportError(STRUCTURETOOLARGE);
	return x;
}




// scan a field identifier list and append the new node at the end
// h : head of the list
// x : already scanned identifier
// list true for a list, false for a single identifier
// returns the list of newly allicated nodes
halfword_t ScanFieldIDs(halfword_t*h, halfword_t x, boolean_t list)
{
	halfword_t y,n,m;
	m=NADA;
	do
	{
		if (x==NOIDENT) x=CheckID();
		y=*h;
		if (ScanFields(x,&y)!=NADA) ReportError(DUPLICATEID,x);
		n=GetAvail(3);
		if (m==NADA) m=n;  // memorise first one
		if (y==NADA) *h=n; else Mem[y].ll.link=n;
		// initialise first node
		Mem[n].ll.info=x;
		Mem[n].ll.link=NADA;
		x=NOIDENT; // force check on next iteration
	}
	while (list && ScanChar(','));
	return m;
}

// fuction that scan end of field list according to e
boolean_t ScanEnd(boolean_t e)
{
	if (e) return ScanKW(kEND);
	return ScanChar(')');
}

// vl : variant list
// if variant list is non nada, vt is assigned with the variant type
void FieldList(halfword_t* vl, halfword_t*fl, memoryword_t*vt, int*offset, boolean_t e)
{

	halfword_t 		l;
	memoryword_t 	t;
	halfword_t 		x;
	int 			VarOffset;
	halfword_t 		VarVL;

	*vl=NADA;  // initialise variant list
	goto loopentry;
fixedpart:
	// fixed part
	l=ScanFieldIDs(fl,NOIDENT,True);
	CheckChar(':');
//  TypeDen(t,l+1); // in case of forward defined pointer
      // write on the node of the first list
	TypeDen(&t);
	while (l!=NADA)
	{
		Mem[l+2].i=FieldAlign(offset,t);
		Mem[l+1]=t;
		l=Mem[l].ll.link;
	}
	if (ScanEnd(e)) return;
	CheckChar(';');
loopentry:
	if (ScanEnd(e)) return;
	if (!ScanKW(kCASE)) goto fixedpart;
	// variant part
	x=CheckID();
    // check if x is the identifier of an ordinal type

    if (HTable[x].symb.s==sTYPE)
    {
       t=Mem[HTable[x].symb.val];
       if (IsOrdinalType(t))  goto varianttype;
    }

    l=ScanFieldIDs(fl,x,False);
    CheckChar(':');
    CheckTypeID(&t,NIL);
    CheckOrdinalType(t);
    // if a variant variable is declared, then include it in field list

    Mem[l+2].i=FieldAlign(offset,t);
    Mem[l+1]=t;
varianttype: // here, t is ordinal type
	*vt=t;               // variant type
	CheckKW(kOF);

	int maxo=0;       // initialise for maximum
	for(;;)
	{
		l=ConstantList(*vt,vl); // append to end of variant list
		CheckChar(':');
		CheckChar('(');
		CheckStack();
		VarOffset=*offset;
		FieldList(&VarVL,fl,&t,&VarOffset,False); // recursive call
		if (VarOffset>maxo) maxo=VarOffset;
		// generate record node
		if (VarVL!=NADA)
		{
			x=GetAvail(3);
			Mem[x+2]=t;
		}
		else x=GetAvail(2);
		Mem[x].hh.lo=VarVL;
		Mem[x+1].i=VarOffset;
		// assign record node to all latest of const list
		while (l!=NADA)
		{
			Mem[l].ll.info=x;
			l=Mem[l].ll.link;
		}
		if (ScanEnd(e)) break;
		CheckChar(';');//CheckSemicolon;
		if (ScanEnd(e)) break;
	}
	*offset=maxo;
}


void RecordType(memoryword_t*t, boolean_t pk)
{
	halfword_t vl;  // variant list
    halfword_t fl;  // field list
    memoryword_t vt;  // variant type
    int	offset;
    halfword_t n;
    byte_t al;

    t->th.bt=btRec;
    fl=NADA;  		// initialise field list
    offset=0; 		// initialise offset
    FieldList(&vl,&fl,&vt,&offset,True);
    if ((offset>=(1 << 24) )) ReportError(STRUCTURETOOLARGE); // structure too large
    if ( vl!=NADA)
    {
    	n=GetAvail(3);
    	Mem[n+2]=vt; // variant type
    }
    else n=GetAvail(2);
    Mem[n].hh.lo=vl;
    Mem[n].hh.hi=fl;
    Mem[n+1].i=offset; // byte size
    t->th.link=n;
    // compute alignement
    al=7;
    while (fl!=NADA)
    {
    	al &= Alignement(Mem[fl+1]); // field alignement
    	fl=Mem[fl].ll.link;
    }
    t->th.s=al;
}


// retunrs bit size of set of component type tc
// tc is assumed to be a subrange of ordinal type
int SetSize(memoryword_t tc)
{
	int s;
	switch (tc.th.bt)
	{
	case btChar:
		return 32;
	case btUInt: case btInt:
		// TODO : if case of subrange, set the minimal size
		// e.g. set of 0..31 should be 1 word only
		return (MAXSET+7) >> 3;
	case btBool: return 1;
	default:
		if (tc.th.link==NADA) ReportError(INTERNAL);
		s=(Mem[tc.th.link+1].u+8)>>3; // range is 0 .. Max
		return (s+3)&0xfffffffc; // word alignement
	}
}


// set type
//+--------------------------+
//| btSet | s  |      +----------> Component type
//+--------------------------+


void SetType(memoryword_t*t, boolean_t pk)
{
	memoryword_t tc; // component type
	halfword_t n;    // component type node
	CheckKW(kOF);
	t->th.bt=btSet;
	TypeDen(&tc);
	CheckOrdinalType(tc);
	if ( (TypeMin(tc)<0) || (TypeMax(tc)>=MAXSET) ) ReportError(STRUCTURETOOLARGE);
	n=GetAvail(1);
	t->th.link=n;
	t->th.s=SetSize(tc);
	Mem[n]=tc;
}

//+--------------------------+
//|btFile| 0/1 |      +----------+
//+--------------------------+   |
// 0:file,  1:text               |
//+--------------------------+   |
//|      component type      |<--+
//|--------------------------|
//| component type size + 16 |
//+--------------------------+

void FileType(memoryword_t*t, boolean_t pk)
{
	memoryword_t tc;
	int s;
	int sz;
	halfword_t n;
	CheckKW(kOF);
	t->th.bt=btFile;
	TypeDen(&tc);
	FilePermissible(tc);
	n=GetAvail(2);
	Mem[n]=tc;
	sz=TypeSize(tc);
	if (sz>32768) ReportError(STRUCTURETOOLARGE); // structure too large
	Mem[n+1].i=TypeSize(tc)+12; // MOTOS 12 is the NaPP size -- file descriptor size
	t->th.link=n;
	t->th.s=0;  // not a text
}

// type denoter
//---------------
// return type in t
// if a forward domain definition is allowed for a pointer type,
// the nodes the type is written must be linked through their
// domain field after the call to TDen
// Once the domain is defined, the domains are written


void TypeDen(memoryword_t*t)
{
	CheckStack();
	if (ScanKW(kSTRING))
	{
		*t=StringType;
		if (ScanChar('['))
		{
			int n=ConstIntExpr();
			if (n>StringType.hh.hi) ReportError(OUTOFBOUND);
			t->hh.hi=(n+1);
			CheckChar(']');
		}
		return;
	}
	halfword_t id = ScanID();
	if (id!=NOIDENT)
	{
		if (GetXID(id)==sTYPE)
		{
			*t=Mem[HTable[id].symb.val];
			return;
		}
		SubRangeType(t,id);
		return;
	}
	if (ScanChar('(')) EnumeratedType(t);
	else if (ScanChar('^')) PointerType(t);
	else
	{
		boolean_t pk=ScanKW(kPACKED);
		if (ScanKW(kARRAY)) ArrayType(t, pk);
		else if (ScanKW(kRECORD)) RecordType(t,pk);
		else if (ScanKW(kSET)) SetType(t,pk);
		else if (ScanKW(kFILE)) FileType(t,pk);
		else
		{
			// if packed is scasnned and none of previous then error !
			if (pk) ReportError(UNEXPECTEDKW,kPACKED);
			SubRangeType(t,NOIDENT);
		}
	}
}

// returns byte size of a type
//-----------------------------

// A text variable contains : (3 words = 12 bytes)
// TODO, see if it is simple to place the buffer variable at address base + 5 to save a word
// +-------------------------+
// |  handle in host system  |
// +-------------------------+
// | mode |  spare...        |
// +-------------------------+
// | c    |  spare...        |
// +-------------------------+
//
// a file variables hase 2 words + buffer variable
// +-------------------------+
// |  handle in host system  |
// +-------------------------+
// | mode |  spare...        |
// +-------------------------+
// | buffer variable.        |
// |  ...                    |



int TypeSize(memoryword_t t)
{
	switch(t.th.bt)
	{
	case btStr:
		return t.th.link;
	case btRec:
		return Mem[t.th.link+1].i;
	case btArray:
		return Mem[t.th.link+2].i;
	case btSet:
		if (t.th.link==NADA) return 0;
		else return t.th.s;
	case btText:
		return 12;
	case btFile:
		CheckStack();
		return 8+TypeSize(Mem[t.th.link]);
	default:
		return (int)(t.th.s);
	}
}

// Labels
//----------
// the cycle of life of a label has 4 phases
// - declaration : label 99;
// - definition  : 99:
// - activation  : goto 99;
// - link i.e. addresse resolution while generating code
//
// a goto is local if the destination is in the same bloc
// as the activation
// Local goto are simple branch instruction
// Non local goto require to restaure the frame of the
// destination
//
// label symbol node
//+--------------------------+
//|  func num |     +------------> next
//|--------------------------|
//|    h index / local link  |
//|--------------------------|
//|            @             |
//|--------------------------|
//|      Global link         |
//+--------------------------+


// func num is set by declaration
// declaration initialise h index with the symbol in hash table.
// this is required for error message in case of non definition
// @ and global links are initialised to  0
//
// Label definition set @ to -1 to check double definitions
//
// At the end of the bloc parsing, the label definition is checked and
// local link field is initialised to 0
//
// - definition creates an opLabel node
// - invocation creates a goto node
//
// during the code generation :
// - opLabel initialises @ with the position in code
// - opGoto link the position of local branch local link
//   and position of non local in global link
// - when function is closed, the local link are solved
//   by generating the branch to the destination
//   The global links are initialised to the displacement
//   from the starting point of the function.
// - the resolution of non local goto is performed during
//   the global link. They are managed like subfunctions.


// this function returns the label list
halfword_t LabelDec()
{
	halfword_t l=NADA;
	halfword_t x; // label id
	halfword_t n;
	halfword_t nf;
	// get the function number
	// it is given by the current function node except
	// for main where it is in a global variable
	if (sLevel>1)
	{
		nf=Mem[HTable[CurFID].symb.val+1].hh.lo;
	}
	else nf=PMAIN;
	do
	{
		x=ScanLabel();
		if (x==NOIDENT) ReportError(LABELEXPECTED);  // label expected
		NewID(x,sLABEL);
		n=GetAvail(4);
		HTable[x].symb.val=n;
		Mem[n].ll.info=nf; // function number
		Mem[n].ll.link=l;
		Mem[n+1].hh.lo=x;
		Mem[n+2].i=0; // @ set to zero
		Mem[n+3].i=0;
		// check that it is defined in epilog
		l=n;
	}
	while (ScanChar(','));
	CheckChar(';');
	return l;
}

// label definition 99:
// creates a label definition node
//+-----------------------------+
//| opLabel |      |     +---------> Next
//|-----------------------------|
//|   Lbl Dec Node |            |
//+-----------------------------+
// assign the current function node to the label declration node
// to indicate that le label is defined
halfword_t LblDef(halfword_t x)
{
	halfword_t n;

	CheckChar(':');
	if (GetXID(x)!=sLABEL) ReportError(UNDECLAREDLBL,x); // undeclared label
	if (Mem[IDVal+2].i<0)  ReportError(LBLALREADYDEF,x); // already defined
	Mem[IDVal+2].i=-1; // mark as defined
	// create label node
	n=GetAvail(2);
	Mem[n].nh.op=opLabel;
	Mem[n+1].hh.lo=IDVal; // i.e. label definition node
	return n;
}

// check that all declared labels has been defined
// and initialise local link list
void CheckLabels()
{
	halfword_t l;
	l=LabelList;
	while (l!=NADA)
	{
		if (Mem[l+2].i==0) ReportError(UNDEFINEDID,Mem[l+1].hh.lo); // undefined id
		// when label is defined this field is set to -1
		// hash table data no longer needed now.
		// this field is now used for link address with the
		// same destination
		Mem[l+1].i=0; // initialise link
		l=Mem[l].ll.link;
	}
}

////////////////////////////////////////////////////////////////////////////////////

// constant declaration
//====================
// const nodes of enumerated types are
//+-----------------------+
//|       type            |
//|-----------------------|
//|        value          |
//+-----------------------+
//|    ( hi value )       |
//+-----------------------+
void ConstDec()
{
	halfword_t x=CheckID();  // identifier
	halfword_t e; // expression
	memoryword_t t; // type
	do
	{
		NewID(x,sCONST);
		CheckChar('=');

		ConstExpr(&t,&e,NOIDENT);
		HTable[x].symb.val=e;
		Mem[e]=t;
		eofAllowed=(sLevel==1); // and (not IsProgram);
		CheckChar(';');
		eofAllowed=False;
		x=ScanID();

	}
	while (x!=NOIDENT);
}

//==================
// type declaration
//==================

void ProceduralType(memoryword_t* t,BasicType_t bt);


// procedural type declaration
//----------------------------
// bt=btProc or btFunc
// assign new procedural type node to identifier z.
void ProcTypeDec(BasicType_t bt, halfword_t z)
{
	halfword_t y=GetAvail(1);
	ProceduralType(Mem+y,bt);
	HTable[z].symb.s=sTYPE;
	HTable[z].symb.val=y;
}




void TypeDec()
{
	halfword_t x,y,z;
	FwdType=NADA;
	z=CheckID();
	FwdDomainDefAllowed=True;
	do
	{
		CheckChar('=');
		if (ScanKW(kPROCEDURE))  ProcTypeDec(btProc,z);
		else if (ScanKW(kFUNCTION)) ProcTypeDec(btFunc,z);
		else
		{
			y=GetAvail(1);  // room for the type
			if ( (HTable[z].symb.s!=sTYPE) ||
				 (HTable[z].symb.l!=sLevel) ||
				 (HTable[z].symb.val!=NADA)    )
			{	// allow forward definition
				NewID(z,sTYPE);
			}
			TypeDen(Mem+y);
			HTable[z].symb.s=sTYPE;
			HTable[z].symb.val=y;
		}
		eofAllowed=(sLevel==1); // and (not IsProgram);

		CheckChar(';');
		eofAllowed = False;
		z=ScanID();
	}
	while (z!=NOIDENT);
	FwdDomainDefAllowed=False;
	// check forward type definition
	while (FwdType!=NADA)
	{	// scan list of forward defined domain types
		y=Mem[FwdType].ll.info; // symbol
		z=HTable[y].symb.val;   // value
		if (z==NADA) ReportError(UNDEFFWDTYPE,y); // undefined forward domain type
		// set domain type to each node of the list
		x=Mem[FwdType+1].hh.lo;
		do
		{	// assign domain type to each pointer type of the list
			y=Mem[x].ll.link;
			Mem[x].ll.link=z;
			x=y;
		}
		while (x!=NADA);
		x=Mem[FwdType].ll.link;
		FreeNode(FwdType,2);
		FwdType=x;
	}
}



//=====================
// VARIABLE DECLARATION
//=====================

// size of the variables
// it is initialized to à at the program level
// and initialisze to the value of the return type or a function at higher level
// parsing some statements such as with statement or for statement may add local variables
// to precompute some values
int GlobalsIndex;
int PrmIndex;
int LocalsIndex;

// offset for additionnal variables for a temporary with variable
// or a bound variable of a for loop
// e.g. for bounds or with statemsnts
// used to check parameter and locals size
int TmpVarIndex;

// Align address x according to type t
int Align(int x, memoryword_t t)
{
	int a=Alignement(t);  // odd : byte alignement
	if (a&1) return x; 	// halfword
	if (a&0x3) return x+(x&1); // word
	if (a&0x7) return (x+3)&0xfffffffc; // double word
	return (x+7)&0xfffffff8;
}

// symbol variable node
// +----------------------------+
// | cat  | level | size |      |
// |----------------------------|
// |            type            |
// |----------------------------|
// |           offset           |
// +----------------------------+

// size is the size for scalar variable
// it may be byte, half, or word, but it is always
// word for parameters

// cat :
// 0 = value
// 1 = reference (var parameter)
// 2 = procedural parameter
// 3 = const in code (str, set)
// 4 = index variable of a for loop
// 5 = temporary variable of a with statement
//     variable is  address. offset of the pointer
//     post offset is additionnal offset on dereferenced value
// 6 = non assignable reference

void VarDec()
{
	halfword_t x=NOIDENT;
	memoryword_t t;	// type of each variable
	int s;   		// Type size
	do
	{
		// generate the list of identifiers with 3 nodes
		halfword_t l=ScanIdList(x,3,NIL,False);
		CheckChar(':');
		TypeDen(&t);
		s=TypeSize(t);
		// initialise nodes of list
		do
		{
			 x=l;
			 l=Mem[l].ll.link;  // next
			 Mem[x+1]=t;      // initialise type
			 Mem[x].qqqq.b0=0;       // value Variable
			 Mem[x].qqqq.b1=sLevel;
			 Mem[x].qqqq.b2=t.th.s;  // scalar size
			 // compute offset
			 if (/*IsProgram and*/ (sLevel<=1))  // global variables, post incrementation of addresses
			 {
				 GlobalsIndex=Align(GlobalsIndex,t);
				 Mem[x+2].i=GlobalsIndex;
				 GlobalsIndex+=s;
				 if (GlobalsIndex>MAXGLOBALS) ReportError(TOOMUCHGLOBALS); // too much globals
			 }
			 else // local variable or non program mode : pre incrementation of negative offset
			 {
				 LocalsIndex+=s;  // preincrementation
				 LocalsIndex=Align(LocalsIndex,t);
				 if (LocalsIndex>MAXLOCALS) ReportError(TOOMUCHLOCALS);
			     Mem[x+2].i=-LocalsIndex; // negative offset
			 }
		}
		while(l!=NADA);
//emitString("offset:"); emitInt(Mem[x+2].i);
		eofAllowed=(sLevel==1); // and (not IsProgram);
		CheckChar(';');
		eofAllowed=False;
		x=ScanID();  // next identifier
	}
	while (x!=NOIDENT);
}

// some information about activation bloc
// the bloc stack is in the upper part of pMem array,
// in the range [HiMemMin..MemSize[
// When entering in a function definition, after
// scaning the function name, the level is increased
// by one (sLevel variable) and an activation bloc
// is created
//    +---------------------+
//    | Fwd func | xxxxxxxx |
//    +---------------------+
//    | Lbl lst  | RetNode  | Labe list/return variable node
//    |---------------------|
//    |   Param size        |
//    |---------------------|
//    |   Var Size          |
//    |---------------------|
//    | LoMemMax |  Avail   | mark memory
//    |---------------------|
// +->| Id 1     |  xxxxxxx | <-  Id1 Index
// |  |---------------------|
// |  | symbol1             |
// |  |---------------------|
// :  :      ...            :
// :  :                     :
// |  |---------------------|
// +---+Id1 Index |  CurFID | <-  HiMemMin
//    +---------------------+


halfword_t TypeNode;   // node to type defined by identfier
// TODO -- make this variable local to procheader



// create variable nodes and append to parameter list
// fpl is the formal parameter list
// it correspond to address of result passed as first parameter
// Initialise PrmIndex.
void PrmNodes(halfword_t fpl)
{
	memoryword_t ty;
    int s;
    halfword_t t;  // variable node
    halfword_t h1;
    h1=Mem[HiMemMin].hh.lo; // first parameter
    while (fpl!=NADA)
    {
    	ty=Mem[Mem[fpl+1].hh.lo]; // type of the formal parameter
    	// size depends on kind of parameter
		switch(Mem[fpl].ll.info)
		{
		case 0:
		case 4:
			s=(TypeSize(ty)+3) & 0xfffffffc;
			break;
		default:
			s=4; // for var or const, size of pointer
		}
		t=GetAvail(3);
		Mem[t].qqqq.b0=Mem[fpl].ll.info;
		Mem[t].qqqq.b1=sLevel;
		Mem[t].qqqq.b2=4; // always word alignment
		Mem[t+1]=ty; // type
		// compute offset
		Mem[t+2].i=PrmIndex;
		PrmIndex+=s;
		if (PrmIndex>STACKSIZE) ReportError(TOOMUCHPARAMS); // too much parameters
		// set new symbol value
		HTable[Mem[h1].hh.lo].symb.s=sVAR;
		HTable[Mem[h1].hh.lo].symb.l=sLevel;
		HTable[Mem[h1].hh.lo].symb.val=t;
		h1-=2;  // next symbol
		fpl=Mem[fpl].ll.link;
    }
    // adjust PrmIndex with additionnal lexical scope frame pointer parameters
    // minus room of LR and FP to get true parameter size
    PrmIndex+=((sLevel-2) << 2)-8;
}


// scan a parameter list section
// returns type in ty
// h,t : head and tail of the generated list
// au=true if untyped parameter is allowed
// if so, assign void type to ty
// TypeNode is assigned
void ParamIdList(memoryword_t * ty, halfword_t *h, halfword_t*t, boolean_t au)
{
	*h=ScanIdList(NOIDENT,2,t,False);
	if (au)
	{
		if (ScanChar(':')) CheckTypeID(ty,&TypeNode);
		else
		{
			ty->i=TVOID;
			TypeNode=NADA;
		}
	}
	else
	{
		CheckChar(':');//Colon;
		CheckTypeID(ty,&TypeNode);
	}
}


// restaure old definitions of local symbols
// and other data

void PopBloc()
{

	halfword_t x,y;
	y=HiMemMin;

	x=Mem[y].hh.lo;
	CurFID=Mem[y].hh.hi;

	while (y!=x)
	{ // loop for pushed symbols
		y+=2;
		HTable[Mem[y].hh.lo].symb=Mem[y-1].sy;
	}
	// restaure context
	LocalsIndex=Mem[x+2].i;
	PrmIndex=Mem[x+3].i;
	LabelList=Mem[x+4].hh.lo; // label list
	RetVarNode=Mem[x+4].hh.hi;
	FwdFunc=Mem[x+5].hh.lo;
	HiMemMin=x+6;
	sLevel--;
}


void ProcHeader(boolean_t f, halfword_t y);

// scan procedural type ane assign it to t
// bt=btProc or btFunc
void ProceduralType(memoryword_t* t,BasicType_t bt)
{
	t->th.link=GetAvail(1); // type info link
	ProcHeader(bt==btFunc,t->th.link);
	t->th.bt=bt;
	t->th.s=4;
	PopBloc();
}

// scan a procedural parameter
// output ty is the type
// input bt is the basic type btProc or btFunc
// output h,t are the generated node (one term list)
void ProceduralParameter(memoryword_t*ty,BasicType_t bt,halfword_t* h,halfword_t*t)
{
	halfword_t x=CheckID();
	NewID(x,sVAR);
	ProceduralType(ty,bt);
	TypeNode=GetAvail(1);
	Mem[TypeNode]=*ty;
	*h=GetAvail(2);
	Mem[*h].ll.info=x;
	Mem[*h].ll.link=NADA;
  // type is assigned by caller
	*t=*h; // One node list
}

// analyse a conformant array parameter
// output ty is the conformant array type
// intput pk is true if this analysis comes after a "packed" key word
// output h,t are head and tail of the generated nodes

void ConformantArrayPrms(memoryword_t*ty,halfword_t* h, halfword_t*t,boolean_t pk)
{
	if (pk) CheckKW(kARRAY);
	ReportError(NOTYETIMPLEM);
}

// Formal parameter list
//======================
//+---------------------+
//|    v     |   next ----->
//|---------------------|
//| T Node   | xxxx     |
//+---------------------+
// v = 0 : value parameter
// v = 1 : address parameter
// v = 2 : procedural parameter
// v = 3 : const non simple parameter (set or string)
// v = 4 : non assignable value parameter
// v = 5 : reserved
// v = 6 : non assignable reference
// v = 7 : conformant array parameter

// assign the list to fpl

void FormalPrmList(halfword_t* fpl)
{
	byte_t v;      		// parameter category
	halfword_t h1,t1;   // current head/tail
	halfword_t h,t;     // head/tail of parameter list
	memoryword_t ty; 	// parameter type
	halfword_t l;

	h=NADA;
	if (ScanChar('('))
	{
		for(;;)
		{
			// read and build the list of parameter
			if (ScanKW(kVAR))
			{
				v=1; // vv:=4; // var prm
				ParamIdList(&ty,&h1,&t1,True);
			}
			else if (ScanKW(kCONST))
			{
				ParamIdList(&ty,&h1,&t1,False);
				if ((TypeSize(ty)<=4) || (ty.th.bt==btReal)) v=4; // indicate non assignable
				else if ((ty.th.bt==btStr) || (ty.th.bt==btSet))  v=3;
				else v=6; // non assigable reference
			}
			else if (ScanKW(kPROCEDURE))
			{ // procedural parameter
				v=2;
				ProceduralParameter(&ty,btProc,&h1,&t1);
			}
			else if (ScanKW(kFUNCTION))
			{ // function parameter
				v=2;
				ProceduralParameter(&ty,btFunc,&h1,&t1);
			}
			else if (ScanKW(kPACKED)) ConformantArrayPrms(&ty,&h1,&t1,True);
			else if (ScanKW(kARRAY)) ConformantArrayPrms(&ty,&h1,&t1,False);
			else
			{// value paraneter
				ParamIdList(&ty,&h1,&t1,False);
				v=0;
			}
			// force value for procedual types
			if ( (v==0) && ( (ty.th.bt==btProc)||(ty.th.bt==btFunc) ) ) v=2;
			if (v!=0) FilePermissible(ty);
			// assign type to nodes
			l=h1;
			do
			{
				Mem[l+1].hh.lo=TypeNode;
				Mem[l].ll.info=v;
				l=Mem[l].ll.link;
			}
			while (l!=NADA);
			// Append Current list to the new one
			if (h==NADA) h=h1; // save head of list
			else Mem[t].ll.link=h1;
			t=t1;
			if (ScanChar(';')) continue;
			CheckChar(')');//RPar
			break;
		}
	}
	*fpl=h;
}


// Initialise an activation bloc that push data on
// the stack
void ActivateBloc()
{
	// save data of previsous bloc on the top of recursion stack
	Room(6); // check out of memory
	HiMemMin-=6;
	Mem[HiMemMin].hh.lo=HiMemMin;
	Mem[HiMemMin].hh.hi=CurFID;
	Mem[HiMemMin+2].i=LocalsIndex;
	Mem[HiMemMin+3].i=PrmIndex;
	Mem[HiMemMin+4].hh.lo=LabelList;
	Mem[HiMemMin+4].hh.hi=RetVarNode;
	Mem[HiMemMin+5].hh.lo=FwdFunc;
}

// procedure header
//=================


//+------------------------------+
//|       fpl    | ret type node |
//|------------------------------|
//| func number  |     infos     |
//+------------------------------+
// func number is <0 for inline functions
// predefined inlines are numbered for optimisation
// (e.g. odd is number -1)
// else it is the index in Func table
// infos : list of parameters identifier (forward)
//   or list of code for inline functions
//   or Nada for "normal" procedures
// ret type is the return type node
//   (nada for procedure)
// The first word corresponds to procedural type link

// Parse procedure header
// f : true for function, false for procedure
// x : hash index of Procedure identifier
// y is the function node
// It assign the formal parameter list and the return
// type node fields of the function node
// Memory is marked *after* creation of the formal
// parameter list so that it remains valid while
// the funtion may be invoqued.
// additionnal nodes for parameters, locals and return
// value are cleared when bloc is poped
void ProcHeader(boolean_t f, halfword_t y)
{
	halfword_t rtn; // return type node
	memoryword_t t; // return type
	// start activation bloc
	sLevel++;
	ActivateBloc();
	// do not mark memory now so that the parameter list and the
	// variable nodes remains valid for the current bloc
	// formal parameter list
	FormalPrmList(&Mem[y].hh.lo);

	rtn=NADA; // default return var node
	PrmIndex=8; // room for LR and FP
	RetVarNode=NADA;
	if (f)
	{ // return value of a function
		CheckChar(':');//Colon;
		CheckTypeID(&t,&TypeNode);
		rtn=TypeNode;
		// check if type is allowed as return function bype
		if ((t.th.bt>btStr) && (t.th.bt<btEnum)) ReportError(6,13);
		// create variable node for return value
		RetVarNode=GetAvail(3);
		Mem[RetVarNode+1]=t; // type
		switch(t.th.bt)
		{
		case btSet:
		    if (t.th.s<4) goto regret;
		case  btStr: // node as additionnal var parameter
            Mem[RetVarNode+2].i=8; // offset : first parameter
            PrmIndex=12;
            Mem[RetVarNode].qqqq.b0=1;       // var parameter
            Mem[RetVarNode].qqqq.b1=sLevel;
            Mem[RetVarNode].qqqq.b2=4;
            break;
		default:
		regret:
			LocalsIndex=TypeSize(t);
			Mem[RetVarNode+2].i=-TypeSize(t);
			Mem[RetVarNode].qqqq.b0=0;
			Mem[RetVarNode].qqqq.b1=sLevel;
			Mem[RetVarNode].qqqq.b2=t.th.s;
		}
	}
	Mem[y].hh.hi=rtn; // return type node
}

// scan inline code
// n = function node
// assign code list and function number to func node
void InlineCode(halfword_t n)
{
	halfword_t h,t,e; // head tail and expression
	CheckChar('(');//LPar;
	h=NADA;
	do
	{
		e=ConstNode(btInt,ConstIntExpr());
		if (h==NADA) h=e; else Mem[t].ll.link=e;
		t=e;
	}
	while (ScanChar(','));
	CheckChar(')');//RPar;
	Mem[t].ll.link=NADA; // close list
	Mem[n+1].ii.lo=-32768; // value lower than any predefined function
	Mem[n+1].hh.hi=h; // set code list
}


halfword_t ParseBloc();


// code for forward declaration
// n  : function node
// id : function identifier
//
// FwdList : list of forward defined functions
//+-----------------------+
//|    id     |   next -------->
//|-----------------------|
//|   node    |   xxxxxx  |
//+-----------------------+
//
// Identifier list :
//
void ForwardFuncDec(halfword_t n, halfword_t id)
{
	halfword_t x;
    halfword_t y;
    halfword_t z;
    halfword_t l;
    halfword_t u;

    Mem[n+1].ii.lo=NewFunction(); // allocate new number
    // build list of id parameters and restaure bloc
    l=NADA;    // list initialisation
    x=HiMemMin;              // first parameter
    y=Mem[x].hh.lo; // last
    CurFID=Mem[x].hh.hi;
    ReleaseMem(Mem[y+1]); // *** caution ***
      // restaure heap *before* nodes allocation for list.
    while (x<y)
    {
    	z=GetAvail(2);
    	Mem[z].ll.link=l;
    	u=Mem[x+2].hh.lo;     // id
    	Mem[z].ll.info=u;     // memorise it
    	Mem[z+1].sy=HTable[u].symb; // and current value
    	HTable[u].symb=Mem[x+1].sy; // restaure pushed value
    	x+=2;  // next symbol
    	l=z;
    }
    // save PrmSize and RetVarNode in the first node
    z=GetAvail(2);
    Mem[z].ll.link=l;
    Mem[z+1].i=PrmIndex;
    Mem[z].ll.info=RetVarNode;
    // restaure others pushed values
    LocalsIndex=Mem[y+2].i;
    PrmIndex=Mem[y+3].i;
    LabelList=Mem[y+4].hh.lo; // label list
    RetVarNode=Mem[y+4].hh.hi;
    FwdFunc=Mem[y+5].hh.lo;
    HiMemMin=y+6;

    Mem[n+1].hh.hi=z; // set identifier list in function node
    // append this function in the forward list
    y=GetAvail(2);
    Mem[y].ll.link=FwdFunc;
    Mem[y].ll.info=id;  // function identifier
    Mem[y+1].hh.lo=n;   // function node
    FwdFunc=y;
    sLevel--;
}


// Forward function definition
// n is the function node
halfword_t ForwardFuncDef(halfword_t id)
{
	halfword_t n; // function node
	halfword_t l;
	halfword_t x;
	halfword_t y;
	halfword_t m;

	CheckChar(';');
	// get function node
	n=HTable[id].symb.val;
	// rebuild activation bloc
	sLevel++;
	ActivateBloc();
	// restaure parameters values
	y=Mem[HiMemMin].hh.lo; // bottom
	l=Mem[n+1].hh.hi;  // parameter list
	PrmIndex=Mem[l+1].i;   // restaure PrmSize
	RetVarNode=Mem[l].ll.info; // and RetVarNode
	m=Mem[l].ll.link;
	FreeNode(l,2);
	l=m;
	while (l!=NADA)
	{
		Room(2);
		HiMemMin-=2;  // make room in stack
		x=Mem[l].ll.info;   // identifier
		Mem[HiMemMin+2].hh.lo=x; // push it
		Mem[HiMemMin+1].sy=HTable[x].symb; // save old value
		HTable[x].symb=Mem[l+1].sy;  // restaure parameter value
		m=Mem[l].ll.link;  // next term of list
		FreeNode(l,2);        // free current term
		l=m;
	}
	Mem[HiMemMin].hh.lo=y;      // restaure bottom
	Mem[HiMemMin].hh.hi=CurFID; // and current function identifier
	MarkMem(&Mem[Mem[HiMemMin].hh.lo+1]); // Mark heap
	CurFID=id;
	return n;
}

void Assembler(halfword_t nf);


// Procedure Declaration
//======================
// f : true for function declaration, false for procedure
void ProcDec(boolean_t is_func)
{
	halfword_t x;
	byte_t l;
	memoryword_t tr; // return  type, tVoid for procedures
	halfword_t n;     // function node
    halfword_t h;
    IDEnum_t idt;
    x=CheckID();
    if (is_func) idt=sFUNC; else idt=sPROC;

    HEntry_t *he = HTable+x;
    if (he->symb.s==sUNDEF)  goto newfunc; // undefined symbol : new function
    l=he->symb.l;   // get level
    if (l<sLevel) goto newfunc; // symbol defined in previous bloc : new function
    if (he->symb.s!=idt) ReportError(DUPLICATEID,x); // duplicate identifier
    // check that it is a forward definition
    if (Mem[he->symb.val].ii.hi<0) ReportError(DUPLICATEID,x); // inline
    if (Mem[he->symb.val+1].ii.hi==NADA) ReportError(DUPLICATEID,x); // already defined function

    // forward functions processing
    n=ForwardFuncDef(x);
    goto definefunc;
newfunc: // definition of a new function
	n=GetAvail(2);    // the function node
	NewID(x,idt);
	ProcHeader(is_func,n); // parse procedure header
	CurFID=x;
	HTable[x].symb.val=n; // assign new symbol value
	CheckChar(';');//Semicolon;
	if (ScanKW(kFORWARD))
	{
		// creates variable nodes for each formal parameter identifier
		PrmNodes(Mem[n].hh.lo);
		// Mark now memory for bloc
		MarkMem(&Mem[Mem[HiMemMin].hh.lo+1]); // Mark heap
		ForwardFuncDec(n,x);
	}
	else if (ScanKW(kINLINE))
	{
		//    Mark(pMem^[pMem^[HiMemMin].hh.lo+1]); // Mark heap
		PopBloc(); // restaure bloc before allocating code nodes
		InlineCode(n);
	}
    else
    {  // normal case : user definition
    	PrmNodes(Mem[n].hh.lo);
    	MarkMem(&Mem[Mem[HiMemMin].hh.lo+1]); // Mark heap
    	Mem[n+1].hh.lo=NewFunction(); // get function number
    definefunc:
		Mem[n+1].hh.hi=NADA;     // clear link info
		if (ScanKW(kASM))
		{
			Assembler(Mem[n+1].ii.lo);
		}
		else
		{
			// get return type
			if (is_func) tr=Mem[Mem[n].hh.hi]; else tr.i=TVOID;
			// initialise LocalsIndex accordint to return type
			if (RegisterValued(tr)) LocalsIndex=TypeSize(tr);
    		else LocalsIndex=0;
			h=ParseBloc();  // parse bloc

			// Code generation
			OpenFunction(Mem[n+1].ii.lo);  // prolog

			StmCodeAndLink(h);             // code generation

			CloseFunction(tr);   // epilog
		}
		ReleaseMem(Mem[Mem[HiMemMin].hh.lo+1]);
		PopBloc();      // bloc restauration
    }
	eofAllowed=(sLevel==1); //(Not IsProgram) and (sLevel=1);
	CheckChar(';');//Semicolon;
	eofAllowed=False;
}



// compount statement
//-------------------
// assign t to tail of list
// assign h to head of list

void CompountSt(halfword_t*h, halfword_t*t)
{
	halfword_t x,tn,tx;
	*h=NADA;
	for(;;)
	{
		x=ScanLabel();
		if (x!=NOIDENT)
		{
			x=LblDef(x); // mono node statement
			if (*h==NADA) *h=x;
			else Mem[tn].ll.link=x;
			tn=x; // This is new tail
		}
		x=Statement(&tx);
		if (*h==NADA) *h=x;
		else Mem[tn].ll.link=x;
		if (x!=NADA)  tn=tx;
		if (ScanKW(kEND)) break;
		if (!ScanChar(';')) ReportError(UNEXPECTEDTOKEN);
	}
	if(t!=NIL) *t=tn;  // write tail if required
}




halfword_t ProgPrmList;

// scan list of  program parameter identifiers
// initialise them of type sParam
// when they are defined as a variable, then the value is sVar
// Thus, identifier of type sParam may be accepted as new
// variable identifier



void ScanProgPrms()
{
	// flag to check if input and output are declared in the program parameter section
	boolean_t InputPrm=False, OutputPrm=False;
	halfword_t x; //identifier
    halfword_t n; // list node under construction
    halfword_t t; // tail of the list
    do
    {
		x=CheckID();
		if (x==InputVar)
		{
			if (InputPrm) ReportError(DUPLICATEID,x);
			InputPrm=True;
			HTable[x].symb.l=1; // set level 1 so that
			// the defaut definition is valid at global level
		}
		else if (x==OutputVar)
		{
			if (OutputPrm) ReportError(DUPLICATEID,x);
			OutputPrm=True;
			HTable[x].symb.l=1;
		}
		else
		{
			NewID(x,sPARAM);
			// link to program parameters list
			n=GetAvail(1);
			Mem[n].ll.info=x;
			AppendNode(&ProgPrmList,&t,n);
		}
    }
    while (ScanChar(','));
    CheckChar(')');
    Mem[t].ll.link=NADA;
}

// bloc parsing
//-------------
// returns statement list
halfword_t ParseBloc()
{
	halfword_t n;

	FwdDomainDefAllowed=False;
	LabelList=NADA;
	FwdFunc=NADA;
	if (ScanKW(kLABEL)) LabelList=LabelDec();
	TmpVarIndex=0; // by default, set to LocalsIndex when level>1

	for(;;)
	{
		if (ScanKW(kCONST))  { ConstDec(); continue; }
		if (ScanKW(kTYPE))  { TypeDec(); continue; }
		if (ScanKW(kVAR)) { VarDec(); continue; }
		if (ScanKW(kPROCEDURE)) { ProcDec(False); continue; }
		if (ScanKW(kFUNCTION)) { ProcDec(True); continue; }
		break;
	}


	// check that all program parameters are defined
	if (sLevel==1)
	{
		halfword_t x=ProgPrmList;
		while (x!=NADA)
		{
			n=Mem[x].ll.info;
			if (HTable[n].symb.s!=sVAR) ReportError(7,n);
			x=Mem[x].ll.link;
		}
	}

	// check that all forward functions are defined
	while (FwdFunc!=NADA)
	{
		// check link info is Nada
		if (Mem[Mem[FwdFunc+1].hh.lo+1].hh.hi!=NADA)
		{
			//ReportError(INTERNAL,"forward");
			ReportError(UNDEFINEDID,Mem[FwdFunc].ll.info);
		}
		n=Mem[FwdFunc].ll.link; // next
		FreeNode(FwdFunc,2);   // free the list node
		FwdFunc=n;
	}
	n=NADA;


	if /*IsProgram or */(sLevel>1)
	{	// VarSize is word aligned
		LocalsIndex=(LocalsIndex+3) & 0xfffffffc;
		TmpVarIndex=LocalsIndex; // initialisation of temporary additionnal variables pointer
	}

    CheckKW(kBEGIN);

    CompountSt(&n,NIL);

    OptimizeStatements(&n);

    // Check all labels has been defined
    CheckLabels();

    return n;
}

// compilation du fichier f
void do_compile()
{

	halfword_t h;
	halfword_t MainNum;  // function number of the main function -- normally LINKSIZE-2
	                     // generated as forward in /StartupCode/ and used for main code generation

	// various initialisations
	eofAllowed=0;  // end of file not allowed while parsing
	InputState=ScanAlways;

	NestedIf=0;
	DefinesPtr=0;  // clear all defines

	CurFID=NOIDENT;  // set current function identifier to NOIDENT

	// prepare input
	nb_lines=0;
	PushInput();
	NextToken();

	CheckKW(kPROGRAM);

	StartupCode(); // startup code generation

	sLevel=1;
	ProgPrmList=NADA;
	ScanProgName();
	if (ScanChar('('))
	{   // scan program parameters
		ScanProgPrms();
	}

	CheckChar(';');

	// main bloc

	GlobalsIndex=0; // initialisation of global index
	LocalsIndex=0;  // initialisation of locals index, needed for temporary variables
	PrmIndex=0;     //

	h=ParseBloc();

	// check final dot
	if (Pool[TkPtr++]!='.') ReportError(EXPECTEDCHAR,'.'); // '.' expected

    // open the function of the main section
    OpenFunction(PMAIN);

    // generate the code
    StmCodeAndLink(h);

    // close the function
    CloseFunction((memoryword_t)TVOID);

}

void InitParse()
{
	// initialize string type node
	StringType.u=TSTRING;
	StringTypeNode=GetAvail(1);
	Mem[StringTypeNode]=StringType;
	EnumCount=btEnum;
}

//***********************************
// Assembler
//***********************************

int CodeOffset;
   // initialised to 0 at the beginning of a ASM bloc
   // and incremented on each asm item.
   // it is used to know the label address during the node
   // generation process

/*
typedef enum {
    mnADC,
    mnADD,
    mnAND,
    mnB,
    mnBIC,
    mnBL,
    mnBX,
    mnCDP,
    mnCMN,
    mnCMP,
    mnDCB,
    mnDCW,
    mnDCD,
    mnEOR,
    mnLDC,
    mnLDM,
    mnLCR,
    mnMCR,
    mnMLA,
    mnMOV,
    mnMRC,
    mnMRS,
    mnMSR,
    mnMUL,
    mnMVN,
    mnORR,
    mnRSB,
    mnRSC,
    mnSBC,
    mnSTC,
    mnSTM,
    mnSTR,
    mnSUB,
    mnSWI,
    mnSWP,
    mnTEQ,
    mnTST
} MnemonicEnum_t;

*/



// Scan a mnemonic and the mnemonic number
// assign to i the index of the first non scaned character
static int ScanMnemonic(int*i)
{
	int k;
	int mn;
	k=IdPtr;
	switch(Pool[k])
	{
	case 'a':
		switch(Pool[++k])
		{
		case 'd':
			switch(Pool[++k])
			{
			case 'c': mn=0; break;
			case 'd': mn=1; break;
			default: goto err;
			}
			break;
		case 'n':
			if (Pool[++k]!='d') goto err;  // AND
			mn=2;
			break;
		default: goto err;
		}
		break;
	case 'b':
		switch(Pool[++k])
		{
		case 'i':
			if (Pool[++k]!='c') goto err; // BIC
			mn=4;
			break;
		case 'k':
			if (Pool[++k]!='p') goto err;
			if (Pool[++k]!='t') goto err;  // BKPT
			mn=50;
			break;
		case 'l':
			switch(Pool[++k])
			{
			case 'x': mn=43; break; // BLX
			case 't':
			case 's':
			case 'e':
				k-=2; // allow to recognise blt bls and ble as conditional branches
				mn=3;
				break;
			default:
				k--;
				mn=5; // BL
			}
			break;
        case 'x':
          mn=6;
          break;
        default: // B alone
          k--;
          mn=3;
		}
		break;
	case 'c':
		switch(Pool[++k])
		{
		case 'd':
			if (Pool[++k]!='p') goto err; // CDP
			mn=7;
			break;
		case 'l':
			if (Pool[++k]!='z') goto err; // CLZ
			mn=42;
			break;
		case 'm':
			switch (Pool[++k])
			{
			case 'n': mn=8;	break; // CMN
			case 'p': mn=9;	break; // CMP
			default: goto err;
			}
			break;
		default: goto err;
		}
		break;
	case 'd':
		if (Pool[++k]!='c') goto err;
		switch(Pool[++k])
		{
		case 'b': mn=51; break; // DCB
		case 'w': mn=52; break; // DCW
		case 'd': mn=53; break; // DCD
		case 'f': mn=54; break; // DCF
		default: goto err;
		}
		break;
	case 'e':
        if (Pool[++k]!='o') goto err;
        if (Pool[++k]!='r') goto err; // EOR
        mn=10;
        break;
	case 'l':
		switch(Pool[++k])
		{
		case 'd':
			switch(Pool [++k])
			{
			case 'c': mn=11; break; // LDC
			case 'm': mn=12; break; // LDM
			case 'r': mn=13; break; // LDR
			default: goto err;
			}
			break;
		default:
			goto err;
		}
		break;
	case 'm':
		switch (Pool[++k])
		{
		case 'c':
			if (Pool[++k]!='r') goto err; // MCR
			mn=14;
			break;
		case 'l':
			if (Pool[++k]!='a') goto err; // MLA
			mn=15;
			break;
		case 'o':
			if (Pool[++k]!='v') goto err; // MOV
            mn=16;
            break;
		case 'r':
			switch (Pool[++k])
			{
			case 'c': mn=17; break; // MRC
			case 's': mn=18; break; // MRS
			default: goto err;
			}
			break;
		case 's':
			if (Pool[++k] != 'r') goto err; // MSR
			mn=19;
			break;
		case 'u':
			if (Pool[++k]!='l') goto err; // MUL
			mn=20;
			break;
		case 'v':
			if (Pool[++k]!='n') goto err; // MVN
			mn=21;
			break;
		default:
			goto err;
		}
		break;
	case 'o':
		if (Pool[++k]!='r') goto err;
		if (Pool[++k]!='r') goto err; // ORR
		mn=22;
		break;
	case 'r':
		if (Pool[++k]!='s') goto err;
		switch(Pool[++k])
		{
		case 'b': mn=23; break; // RSB
		case 'c': mn=24; break; // RSC
		default: goto err;
		}
		break;
	case 's':
		switch(Pool[++k])
		{
		case 'b':
			if (Pool[++k]!='c') goto err; // SBC
			mn=25;
			break;
		case 'm':
			switch(Pool[++k])
			{
			case 'u':
				if (Pool[++k]!='l') goto err;
				if (Pool[++k]!='l') goto err; // SMULL
				mn=36;
				break;
			case 'l':
				if (Pool[++k]!='a') goto err;
		        if (Pool[++k]!='l') goto err; // SMLAL
		        mn=37;
		        break;
			default: goto err;
			}
			break;
		case 't':
			switch(Pool[++k])
			{
			case 'c': mn=26; break; // STC
			case 'm': mn=27; break; // STM
			case 'r': mn=28; break; // STR
			default: goto err;
			}
			break;
		case 'u':
			if (Pool [++k]!='b') goto err; // SUB
			mn=29;
			break;
		case 'w':
			switch(Pool[++k])
			{
			case 'i': mn=30; break; // SWI
			case 'p': mn=31; break; // SWP
			default: goto err;
			}
			break;
			default: goto err;
		}
		break;
	case 't':
		switch(Pool[++k])
		{
		case 'e':
			if (Pool[++k]!='q') goto err; // TEQ
			mn=32;
			break;
		case 's':
			if (Pool[++k]!='t') goto err; // TST
			mn=33;
			break;
		default: goto err;
		}
		break;
	case 'u':
		if (Pool[++k]!='m') goto err;
		switch(Pool[++k])
		{
		case 'l':
			if (Pool[++k]!='a') goto err;
			if (Pool[++k]!='l') goto err; // UMLAL
			mn=35;
			break;
		case 'u':
            if (Pool[++k]!='l') goto err;
            if (Pool[++k]!='l') goto err; // UMULL
            mn=34;
            break;
		default: goto err;
		}
		break;
	default: goto err;
	}
	*i=k+1;
	return mn;
err:
	ReportError(UNKNOWNMNEMONIC);
}


// scan condition code and return asm code
// change *k to *k+2 if condition code is correctly scanned
static int ScanConditionCode(int*k)
{
	int cc;
	char c=Pool[*k+1]; // next char -- it has been already lowcased
	cc=0xe0000000;  // this is default doncition code
	switch(Pool[*k])
	{
	case 'a':
		if (c!='l') return cc; // AL
		break;
	case 'c':
		switch (c)
		{
		case 'c': cc=0x30000000; break; // CC
		case 's': cc=0x20000000; break; // CS
		default: return cc;
		}
		break;
	case 'e':
		if (c!='q') return cc;  // EQ
		cc=0;
		break;
	case 'g':
		switch(c)
		{
		case 'e': cc=0xa0000000; break; // GE
		case 't': cc=0xc0000000; break;  //GT
		default: return cc;
		}
		break;
	case 'h':
		switch (c)
		{
		case 'i': cc=0x80000000; break; // HI
		case 's': cc=0x20000000; break; // HS
		default: return cc;
		}
		break;
	case 'l':
		switch(c)
		{
		case 'e': cc=0xd0000000; break; // LE
		case 'o': cc=0x30000000; break; // LO
		case 's': cc=0x90000000; break; // LS
		case 't': cc=0xb0000000; break; // LT
		default: return cc;
		}
		break;
	case 'm':
		if (c!='i') return cc; // MI
		cc=0x40000000;
		break;
	case 'n':
		if (c!='e') return cc; // NE
		cc=0x10000000;
		break;
	case 'p':
        if (c!='l') return cc; // PL
		cc=0x50000000;
		break;
	case 'v':
		switch(c)
		{
		case 'c': cc=0x70000000; break; // VC
		case 's': cc=0x60000000; break; // VS
		default: return cc;
		}
        break;
    default:
       return cc;
	}
	(*k)+=2;  // here a 2 letter condition code has been scanned, so adjust input index
	return cc;
}


// scan the "set condition code" condition
static int ScanSetCC(int *k)
{
	int s=0;
	if (LowCase(Pool[*k])=='s')
	{
		(*k)++;
		s=1<<20;
	}
	return s;
}


// 0  word
// 1: unsigned byte B
// 2: unsigned half H
// 3: signed half   SH
// 4: signed byte   SB
static int  ScanLDRFormat(int*k)
{
	int r=0;
	switch (Pool[*k])
	{
	case 'b':
		(*k)++;
		r=1;
		break;
	case 'h':
		(*k)++;
		r=2;
		break;
	case 's':
		switch(Pool[*k+1])
		{
		case 'h': r=3; break;
		case 'b': r=4; break;
		default: ReportError(INVALIDOPCODE,0);
		}
		(*k)+=2;
	}
	return r;
}

static int ScanLetter(int* k, char l)
{
	int r=0;
	if (LowCase(Pool[*k])==l)
	{
		(*k)++;
		r=1;
	}
	return r;
}


static int ScanBlocInfo(int*k,boolean_t l)
{
	int x;
	switch(Pool[*k])
	{
	case 'e':
		x=l << 24;
		goto lll;
	case 'f':
		x=(l^1)<<24;
	lll:
		switch(Pool[*k+1])
		{
		case 'a': x|=((l^1)<<23); break;
		case 'd': x|=(l << 23); break;
		default:ReportError(INVALIDOPCODE);
		}
		break;
	case 'i':
		x=1 << 23;
		goto lln;
	case 'd':
		x=0;
	lln:
		switch(Pool[*k+1])
		{
		case 'b': x|=(1<<24);
		case 'a' : break;
		default: ReportError(INVALIDOPCODE);
		}
		break;
	default: ReportError(INVALIDOPCODE);
	}
	(*k)+=2;
	return x;
}
// declare a new assembler symbol
// returns label node
halfword_t NewAsmLabel(halfword_t x)
{
	halfword_t n;
	PushSymbol(x);
	n=GetAvail(2);
	Mem[n].ll.link=LabelList; // link to list
	LabelList=n;
	return n;
}

void DefineASMLabel()
{
	halfword_t x,v;

	x=LookUp();
	if (HTable[x].symb.s==sLABEL) ReportError(DUPLICATEID,x);
	v=HTable[x].symb.val;
	if (v==NADA)
	{
		v=NewAsmLabel(x);
		HTable[x].symb.val=v;
	}
    Mem[v].ii.lo=-1; // mark as declared
    Mem[v+1].i=CodeOffset; // write address
    HTable[x].symb.s=sLABEL;
}


// returns the instruction type
// assign code data to c
// values
// 0 = @dcb
// 1 = @dcw
// 2 = @dcd
// 3 = BX
// 4 = Branch and Branch wth link
// 5 = Data Processing MOV MVN
// 6 = Data processing CMP,CMN,TEQ,TST
// 7 = Data Processing AND,EOR,SUB,RSB,ADD,ADC,SBC,RSC,ORR,BIC
// 8 = multiply MUL
// 9 = multiply MLA
// 10 = long multiply UMULL,UMLAL,SMULL,SMLAL
// 11 = data transfert LDR LDRB...
// 12 = halfword ans signed data transfert
// 13 = Block Data Transfert LDM STM
// 14 = SWP
// 15 = Software interrupt SWI
// 16 = PSR transfert (MRS)
// 17 = MSR

// 18 = Coprocessor data operation CDP
// 19 = Coprocessor data transfert LDC STC
// 20 = Coprocessor register transfert MRC MCR

// 21 = label
// 22 = dcf  define double precision floting point
// 23 = clz
// 24 = bkpt

int ScanOpCode(int*x)
{
	char c; // current character
	int mn; // mnemonic number
	int k ; // index of analysed character in pool
	int r; // returned value

	NextToken();
	IdPtr=TkPtr;
	c=LowCase(Pool[TkPtr]);
	// allow local label definition
	if (c=='@')
	{
		c=LowCase(Pool[++TkPtr]);
	}
	while (IsIDChar(c))
	{
		Pool[TkPtr++]=c;
	    c=LowCase(Pool[TkPtr]);
	}
	LenID=TkPtr-IdPtr;
	if (Pool[IdPtr]=='@')
	{
		DefineASMLabel();
		NextToken();
		return 21;
	}
	mn=ScanMnemonic(&k);
	// mnemonics >=50 have no conditionnal field
    // scan additionnal op code parameters
	if (mn<50) *x=ScanConditionCode(&k);

    switch(mn)
    {
    case 3: // b
        (*x)|=0x0a000000;
        r=4;
        break;
    case 5: // bl
    	(*x)|=0x0b000000;
        r=4;
        break;
    case 6: // bx
        (*x)^=0x012fff10;
        r= 3;
        break;
    case 43: // blx
    	(*x)|=0x012fff30;
        r= 3;
        break;
    case 0: // adc
    	(*x)|= (5 << 21) | ScanSetCC(&k);
    	r=7;
    	break;
    case 1: // add
        (*x)|=(4 << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 2: // and
 //   emitString("AND...");
    	(*x)|= ScanSetCC(&k);
        r=7;
        break;
    case 4: // bic
    	*x|=(0xe << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 8: // cmn
    	(*x)|=(0xb << 21) | (1 << 20) | ScanSetCC(&k);
        r=6;
        break;
    case 9: // cmp
        (*x)|= (0xa << 21) | (1 << 20) | ScanSetCC(&k);
        r=6;
        break;
    case 10: // eor
    	(*x)|= (1 << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 15: // mla
    	(*x)|= 0x00200090 | ScanSetCC(&k);
        r=9;
        break;
    case 16: // mov
    	(*x)|= (0xd << 21) | ScanSetCC(&k);
		r=5;
		break;
    case 21: // mvn
    	(*x)|= (0xf << 21) | ScanSetCC(&k);
        r=5;
        break;
    case 22: // orr
    	(*x)|=(0xc << 21) | ScanSetCC(&k);
    	r=7;
    	break;
    case 23: // rsb
        (*x)|=(3 << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 24: // rsc
    	(*x)|=(7 << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 25: // sbc
    	(*x)|= (6 << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 29: // sub
    	(*x)|= (2 << 21) | ScanSetCC(&k);
        r=7;
        break;
    case 32: // teq
    	(*x)|= (9 << 21) | (1 << 20) | ScanSetCC(&k);
        r=6;
        break;
    case 33: // tst
    	(*x)|=(8 << 21) | (1 << 20) | ScanSetCC(&k);
        r=6;
        break;
    case 20: // mul
    	(*x)|= 0x00000090 | ScanSetCC(&k);
         r=8;
         break;
    case 34: //umull
    	(*x)|=0x00800090 | ScanSetCC(&k);
        r=10;
        break;
    case 35: //umlal
    	(*x)|= 0x00a00090 | ScanSetCC(&k);
        r=10;
        break;
    case 36: //smull
    	(*x)|= 0x00c00090 | ScanSetCC(&k);
        r=10;
        break;
    case 37: //smlal
    	(*x)|=0x00e00090 | ScanSetCC(&k);
        r=10;
        break;
    case 13:
		(*x)|=(1<<20); // set load bit
    case 28: // ldr
        switch(ScanLDRFormat(&k))
        {
        case 0: // word
            (*x) |= 0x04000000 |(ScanLetter(&k,'t') << 21);
            r=11;
            break;
        case 1: // unsigned byte B
            (*x)|=0x04400000 | (ScanLetter(&k,'t') << 21);
            r=11;
            break;
        case 2: // unsigned half H
            (*x)|=0x000000b0;
            r=12;
            break;
        case 3: // signed half   SH
            (*x)|=0x000000f0;
            r=12;
            break;
        case 4: // signed byte   SB
            (*x)|= 0x000000d0;
            r=12;
            break;
        }
        break;
    case 12:
    	(*x)|=ScanBlocInfo(&k,1)|(1<<20)|0x08000000;
    	r=13;
    	break;
    case 27: // ldm,stm
    	(*x)|=ScanBlocInfo(&k,0) | 0x08000000;
        r=13;
        break;
    case 31: // swp
    	(*x)|=0x01000090 | (ScanLetter(&k,'b')<<22);
        r=14;
        break;
    case 30: // swi
    	(*x)|=0x0F000000;
        r=15;
        break;
    case 18: // MRS
    	(*x)|=0x010f0000;
        r=16;
        break;
    case 19: // MSR
    	(*x)|=0x0128f000;
        r=17;
        break;
    case 50:
    	*x=0xe1200070;
        r=24; // bkpt
        break;
    case 51: r=0; break; // dcb
    case 52: r=1; break; // dcw
    case 53: r=2; break; // dcd
    case 54: r=22; break; // dcf
    case 42: // clz
    	(*x)|=0x016f0f10;
        r=23;
        break;
    case 17:
    	(*x)|=(1<<20);
    case 14:
        (*x)|=0x0e000010 | ScanSetCC(&k);
        r=20; // MCR/MRC
        break;
    case 7: // cdp
    	(*x)|=0x0e000000 | ScanSetCC(&k);
        r=18; // CDP
        break;
    case  11: // ldc/stc
    	(*x)|= (1 << 20);
    case 26: // ldc/stc
    	(*x)|= 0x0c000000 | ScanSetCC(&k) | (ScanLetter(&k,'l')<<22);
        r=19;
        break;
    default:
    	ReportError(INTERNAL, "ScanOpCode");
    }
    if (k!=TkPtr) ReportError(INVALIDOPCODE); // unknown opcode
    return r;
}


// Assembler node format
//+-----------------------+
//| op  | c   |     o------------> Next
//|-----+-----+-----------|
//|        code           |
//+-----------------------+
// op = opAsm
// c : operation to perform
//     0 : brut code
//     1 : local link
//     2 : external call, code contains function number
//     3 : pc relative LDR/STR
//     4 : pc relative signed data transfert
//     5 : pc relative mov instruction (replaced by add/sub rd,pc#offset)
//     6 : pc relaive LDC/STC (coprocessor data transfert)

static void AppendAsmNode(halfword_t *h, halfword_t *t, byte_t c, int x)
{
	halfword_t n=GetAvail(2);
	Mem[n].nh.op=opAsm;
	Mem[n].qqqq.b1=c;
	Mem[n+1].i=x;
	if (*h==NADA) *h=n; else Mem[*t].ll.link=n;
	*t=n;
	CodeOffset++;
}


// Scan absolute number 0..15 for register number
int ScanNible(int*k)
{
	int n;
	char c;
	c=Pool[*k+1];
	if ((c<'0')||(c>'9')) ReportError(UNEXPECTEDTOKEN);
	n=c-'0';
	(*k)+=2;
	if (n==1)
	{
		c=Pool[*k];
		if ((c>='0')&&(c<='5'))
		{
			n=10+c-'0';
			(*k)++;
		}
	}
	return n;
}


// Scan Coprocessor Information 0..7
static int ScanCoInfo()
{
    char c;
    c=Pool[TkPtr++];
    if ((c<'0') || (c>='8')) ReportError(UNEXPECTEDTOKEN);
    NextToken();
    return c-'0';
}

int ScanCoOpCode4()
{
    int n;
    TkPtr--; // as ScanNible exect in previous char
    n=ScanNible(&TkPtr);
    NextToken();
    return n;
}


// scan corprocessor register cr0..cr15
//------------------------------------
static int ScanCoReg()
{
    int n;
    if (LowCase(Pool[TkPtr++])!='c') { err: ReportError(UNEXPECTEDTOKEN); }
    if (LowCase(Pool[TkPtr])!='r')  goto err;
    n=ScanNible(&TkPtr);
    NextToken();
    return n;
}


static int ScanCoProc()
{
    int n;
    if (LowCase(Pool[TkPtr])!='p') ReportError(UNEXPECTEDTOKEN);
    n=ScanNible(&TkPtr);
    NextToken();
    return n;
}

// scan a register name
// returns register number and update index in pool
static int ScanRegister()
{
	int r;
	char c;
	int k;

	NextToken();
	k=TkPtr;
	switch(LowCase(Pool[k]))
	{
	case 'l':  // LR
		if (LowCase(Pool[k+1])!='r') goto err;
		r=14;
		k+=2;
		break;
	case 'p': // PC
		if (LowCase(Pool[k+1])!='c') goto err;
		r=15;
		k+=2;
		break;
	case 'r': // r0 .. r16
		r=ScanNible(&k);
		break;
	case 's': // SP
		if (LowCase(Pool[k+1])!='p') goto err;
		r=13;
		k+=2;
		break;
	default: err: ReportError(UNEXPECTEDTOKEN);
	}
	TkPtr=k;
	NextToken();
	return r;
}


// scan shift name and returns
// lsl, asl : 0
// lsr        1
// asr        2
// ror        3
// rrx        4
int ScanShiftName()
{
    int k; // input pool index
    int r; // returned value
    NextToken();
    k=TkPtr;
    switch(LowCase(Pool[k]))
    {
    case 'a': // ASL ASR
        if (LowCase(Pool[k+1])!='s') goto err;
        k+=2;
        switch (LowCase(Pool[k]))
        {
        case 'l': r=0; break;
        case 'r': r=2; break;
        default: goto err;
        }
        break;
    case 'l': // LSL LSR
        if (LowCase(Pool[k+1])!='s') goto err;
        k+=2;
        switch (LowCase(Pool[k]))
        {
        case 'l': r=0; break;
        case 'r': r=1; break;
        default: goto err;
        }
        break;
    case 'r':
        switch(LowCase(Pool[k+1]))
        {
        case 'o':
            if (LowCase(Pool[k+2])!='r') goto err;
            k+=2;
            r=3;
            break;
        case 'r':
            if (LowCase(Pool[k+2])!='x') goto err;
            k+=2;
            r=4;
            break;
        default: goto err;
        }
    default: err: ReportError(UNEXPECTEDTOKEN); // unexpected token
    }
    TkPtr=k+1;
    NextToken();
    return r;
}

// scan an assembler symbol and return it
static halfword_t ScanAsmSymbol()
{
    char c;
    IdPtr=TkPtr;
    do
    {
        TkPtr++;
        c=LowCase(Pool[TkPtr]);
        Pool[TkPtr]=c; // force low case label
    }
    while (IsIDChar(c));
    LenID=TkPtr-IdPtr;
    return LookUp();
}

// check if k fit as operand 2 of an instruction
int CheckFitOp2(word_t k)
{
    int i,r;

    r=0;
    // check if one rotation is 8 bits
    for(i=0;i<16;i++)
    {
        if (k<256) return k+(r<<8);
        r++;
        k=(k<<2)+(k>>30);
    }
    ReportError(CONSTOUTOFBOUND);
}

// scan shift operand and return the correspoonding code
static int ScanShift()
{
    int k,sn;
    sn=ScanShiftName();
    if (sn==4) return 3<<5; // RRX
    if (ScanChar('#'))  // constant value
    {
          k=ConstIntExpr();
          if (sn!=0)
          {
              if ((k>32) || (k<1)) err: ReportError(CONSTOUTOFBOUND); // constant oput of bound
          }
          else if (k>31) goto err; // lsl is 0..31
          k&=31;
          return (sn << 5)+(k << 7);
    }
    // else register value
    k=ScanRegister();
    return (1 << 4)+(sn << 5)+(k << 8);
}

static int ScanOperand2()
{
    int o2;

    NextToken();
    if (ScanChar('#'))
    {
        o2=CheckFitOp2(ConstOrdExpr(NIL,NOIDENT))+(1 << 25);
    }
    else
    {
        o2=ScanRegister();
        if (ScanChar(',')) o2|=ScanShift();
    }

    return o2;
}



// scan operand of a dcd dirctive
static void ScanDCD(halfword_t*h,halfword_t*t)
{
	int i;

	NextToken();
	do
	{  // list of integer valued tokens
		i=ConstOrdExpr(NIL,NOIDENT);
		AppendAsmNode(h,t,0,i);
	}
	while (ScanChar(','));
}


// scan half word constant list
static void ScanDCH(halfword_t*h, halfword_t*t)
{
	long unsigned int x,y;
	NextToken();
	do
	{
		x=ConstOrdExpr(NIL,NOIDENT);
		if (x>65535) ReportError(CONSTOUTOFBOUND);
		if (!ScanChar(','))
		{
			AppendAsmNode(h,t,0,x);
			return;
		}
		y=ConstOrdExpr(NIL,NOIDENT);
		if (y>65535) ReportError(CONSTOUTOFBOUND);
		AppendAsmNode(h,t,0,x+(y<<16));
	}
	while (ScanChar(','));
}



// scan byte constant
// either litteral string or list of integer
static void ScanDCB(halfword_t*h, halfword_t*t)
{
	long unsigned b,x;
	char c;
	int i;


	NextToken();
	i=0;
	x=0;
	do
	{
		if (ScanChar('\''))
		{ // litteral string
			for(;;)
			{
				c=Pool[TkPtr++];
				if (c=='\'')
				{
					c=Pool[TkPtr];
					if (c!='\'') break;
					TkPtr++;
				}
				else if (c<' ') ReportError(UNEXPECTEDEOL);
				x|=(((long unsigned)c)<<(i<<3));
				i++;
				if (i==4)
				{
					i=0;
					AppendAsmNode(h,t,0,x);
					x=0;
				}
			}
		}
		else
		{
			b=ConstOrdExpr(NIL,NOIDENT);
			if (b>255) ReportError(CONSTOUTOFBOUND); // out of bound
			x|=(b<<(i<<3));
			i++;
			if (i==4)
			{
				i=0;
				AppendAsmNode(h,t,0,x);
				x=0;
			}
		}
 	}
	while (ScanChar(','));
	if (i>0) AppendAsmNode(h,t,0,x);
}

// scan list of floating point constant
static void ScanDCF(halfword_t*h, halfword_t*t)
{
	number_t x;
	NextToken();
	do
	{
	    ConstRealExpr(&x);
	    AppendAsmNode(h,t,0,x.uu.lo);
	    AppendAsmNode(h,t,0,x.uu.hi);

	}
	while (ScanChar(','));
}

// scan operand of a MOV instruction and update code x
static byte_t OperandMov(int*x)
{
    halfword_t y,v;

    *x|=ScanRegister()<<12;

    CheckChar(',');
    if ((Pool[TkPtr]=='@') && ((*x & 0x01e00000)==0x01a00000))
    {  // mov Rn,@xxx is PC relative mov
        y=ScanAsmSymbol();  // get label identifier
        v=HTable[y].symb.val;
        if (v==NADA)
        {
            v=NewAsmLabel(y);
            HTable[y].symb.val=v;
            Mem[v].ll.info=y;
        }
      // mark value in room for operand 2 and Rn and set immediate operand
        *x|=(1 << 25) | (v & 0xfff) | ((v << 4) & 0xf0000);
        return 5;
    }
    else
    {
        *x|=ScanOperand2();
        return 0;
    }
}


static void OperandCmp(int*x)
{

  (*x)|=(ScanRegister() << 16);
  CheckChar(',');
  (*x)|=ScanOperand2();
}


static void OperandAnd(int*x)
{
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    (*x)|=(ScanRegister() << 16);
    CheckChar(',');
    (*x)|=ScanOperand2();
}

static void OperandMul(int*x)
{
    (*x)|=(ScanRegister() << 16);
    CheckChar(',');
    (*x)|=ScanRegister();
    CheckChar(',');
    (*x)|=(ScanRegister() << 8);
}

static void OperandMla(int*x)
{
    (*x)|=(ScanRegister() << 16);
    CheckChar(',');
    (*x)|= ScanRegister();
    CheckChar(',');
    (*x)|=(ScanRegister() << 8);
    CheckChar(',');
    (*x)|= (ScanRegister() << 12);
}

static void OperandUMull(int*x)
{
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    (*x)|=(ScanRegister() << 16);
    CheckChar(',');
    (*x)|=ScanRegister();
    CheckChar(',');
    (*x)|=(ScanRegister() << 8);
}

static void PCRelativePreindexed(int*x)
{
    halfword_t y,v;
    (*x)|= (1 << 24); // pre-indexed
    if (Pool[TkPtr]!='@') ReportError(EXPECTEDCHAR,'[');
    y=ScanAsmSymbol();
    v=HTable[y].symb.val;
    if (v==NADA)
    {
        v=NewAsmLabel(y);
        HTable[y].symb.val=v;
        Mem[v].ll.info=y;
    }
    // symbol data in 16 bits : 12 bits operand and 4 bits base register
    (*x)|=(v & 0xfff) | ((v << 4) & 0xf0000);
}


// PC relative preindexed address
static void LDCPCRelative(int*x)
{
    halfword_t y,v;
    (*x)&=0xf0ffffff; // erase code info
    if (Pool[TkPtr]!='@') ReportError(EXPECTEDCHAR,'[');
    y=ScanAsmSymbol();
    v=HTable[y].symb.val;
    if (v==NADA)
    {
        v=NewAsmLabel(y);
        HTable[y].symb.val=v;
        Mem[v].ll.info=y;
    }
// 16 bits of the symbol are in bits 0..7 (offset), 16..19 (base register) and 24..27 (0xb)
    (*x)|=((v & 0xff) | ((v << 8) & 0xf0000)| ((v<<12) & 0x0f000000));
}


static byte_t OperandDT(int*x)
{
    int k;
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    if (ScanChar('['))
    {
        (*x)|= (ScanRegister() << 16);
        if (ScanChar(','))
        {
            (*x)|=(1 << 24); // pre-indexed
            if (ScanChar('#'))
            {
                k=ConstIntExpr();
                if ((k>0xfff) || (k<-0xfff))  ReportError(CONSTOUTOFBOUND);
                if (k>=0) (*x)|=(1<<23); else k=-k;
                (*x)|=k;
                CheckChar(']');
            }
            else
            {
                (*x)|=(1 << 25);
                if (!ScanChar('-'))
                {
                    ScanChar('+');
                    (*x)|=(1 << 23);
                }
                (*x)|=ScanRegister(); // rm
                if (ScanChar(',')) (*x)|=ScanShift();
                CheckChar(']');
            }
            if (ScanChar('!')) (*x)|=(1 << 21);
        }
        else
        {
            CheckChar(']');
            if (ScanChar(','))// post indexed
            {
                if (ScanChar('#'))
                {
                    k=ConstIntExpr();
                    if ((k>0xfff)||(k<-0xfff)) ReportError(CONSTOUTOFBOUND);
                    if (k>=0) (*x)|=(1 << 23); else k=-k;
                    (*x)|=k;
                }
                else
                {
                    (*x)|=(1 << 25); // register offset
                    if (!ScanChar('-') )
                    {
                        ScanChar('+');
                        (*x)|=(1 << 23);
                    }
                    (*x)|=ScanRegister(); // rm
                    if (ScanChar(',')) (*x)|= ScanShift();
                }
            }
            else (*x)|=(1 << 24);
        }
        return 0;
    }
    // PC relative pre-indexed
    PCRelativePreindexed(x);
    return 3;
}

static byte_t OperandHSDT(int*x)
{
    int  k;
    halfword_t y,v;
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    if (ScanChar('['))
    {
        (*x)|=(ScanRegister() << 16);
        if (ScanChar(','))
        { // preindexed
            (*x)|= (1 << 24);
            if (ScanChar('#'))
            {
                (*x)|=(1 << 22);
                k=ConstIntExpr();
                if (k>=0) (*x)|=(1<<23); else k=-k;
                if (k>0xff) ReportError(CONSTOUTOFBOUND);
                (*x)|=(k & 0xf) | ((k << 4) & 0xf00);
                CheckChar(']');
            }
            else
            {
                if (!ScanChar('-'))
                {
                    ScanChar('+');
                    (*x)|=(1 << 23);
                }
                (*x)|=ScanRegister(); // rm
                CheckChar(']');
            }
            if (ScanChar('!')) (*x)|=(1 << 21);
        }
        else
        {
            CheckChar(']');
            if (ScanChar(','))
            {  // post indexed
                (*x)|=(1 << 21); // write back
                if (ScanChar('#'))
                {
                    (*x)|=(1 << 22);  // immediate offset
                    k=ConstIntExpr();
                    if (k>=0) (*x)|=(1 << 23); else k=-k;
                    if (k>0xff) ReportError(CONSTOUTOFBOUND);
                    (*x)|=(k & 0xf) | ((k << 4) & 0xf00);
                }
                else
                {
                    if (! ScanChar('-'))
                    {
                        ScanChar('+');
                        (*x)|=(1 << 23);
                    }
                    (*x)|=ScanRegister(); // rm
                }
            }
            else  (*x)|=(1 << 24) | (1 << 22);
        }
        return 0;
    }
    // PC relative expression
    (*x)|=(1 << 24) | (1 << 22); // pre-indexed and immediate offset
    if (Pool[TkPtr]!='@') ReportError(EXPECTEDCHAR,'[');
    y=ScanAsmSymbol();
    v=HTable[y].symb.val;
    if (v==NADA)
    {
        v=NewAsmLabel(y);
        HTable[y].symb.val=v;
        Mem[v].ll.info=y;
    }
    (*x)&=0xf1f0f060; // clear bits used for data
    (*x)|=(v & 0x1f) | ((v << 2) & 0xf80) |
                  ((v << 6) & 0xf0000) | ((v <<11) & 0x0e000000);
    return 4;
}

static void OperandLDM(int*x)
{
    int rlo, rhi;
    int k;
    (*x)|=(ScanRegister() << 16);
    if (ScanChar('!')) (*x)|=(1 << 21);
    CheckChar(',');
    CheckChar('(');
    do
    { // register list
        rlo=ScanRegister();
        rhi=rlo;
        if (ScanChar('-')) rhi=ScanRegister();
        if (rhi<rlo) ReportError(INVALIDORDER); // invalid ordering
        k=(1 << (rhi+1))-(1 << rlo);
        (*x)|=k;
    }
    while(ScanChar(','));
    CheckChar(')');
    if (ScanChar('^')) (*x)|=(1 << 22);
}

static void OperandSWP(int*x)
{
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    (*x)|=ScanRegister();
    CheckChar(',');
    CheckChar('[');
    (*x)|=(ScanRegister() << 16);
    CheckChar(']');
}

static void OperandSWI(int*x)
{
    word_t k;
    NextToken();
    k=ConstIntExpr();
    if (k>0x00ffffff)  ReportError(CONSTOUTOFBOUND); // out of bound
    (*x)|=k;
}

// scan psr or psrf
// recognise spsr, cpsr, spsr_lfg, cpsr_flg, spsr_all
// ans cpsr_all
static int ScanPsr(boolean_t f)
{
    int x;

    NextToken();
    switch(LowCase(Pool[TkPtr]))
    {
        case 'c': x=0; break;
        case 's': x=1<<22; break;
        default: err: ReportError(UNEXPECTEDTOKEN);

    }
    TkPtr++;
    if (LowCase(Pool[TkPtr++])!='p') goto err;
    if (LowCase(Pool[TkPtr++])!='s') goto err;
    if (LowCase(Pool[TkPtr++])!='r') goto err;
    if (Pool[TkPtr]=='_')
    {
        TkPtr++;
        switch(LowCase(Pool[TkPtr]))
        {
        case 'f':
            if (!f) goto err;
            TkPtr++;
            if (LowCase(Pool[TkPtr++])!='l') goto err;
            if (LowCase(Pool[TkPtr++])!='g') goto err;
            break;
        case 'a':
            TkPtr++;
            if (LowCase(Pool[TkPtr++])!='l') goto err;
            if (LowCase(Pool[TkPtr++])!='l') goto err;
            x|=(f << 16);
            break;
        default: goto err;
        }
    }
    else x|= (f << 16);
    return x;
}

static void OperandMRS(int*x)
{
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    (*x)|=ScanPsr(False);
}

static void OperandMSR(int*x)
{
    int k;
    (*x)|= ScanPsr(True);
    CheckChar(',');
    if ((((*x >> 16)&1)==0) && ScanChar('#'))
    {
        (*x)|=CheckFitOp2(ConstIntExpr()) | (1 << 25);
    }
    else
    {
        (*x)|=ScanRegister();
    }
}

static void OperandCDP(int*x)
{
    NextToken();
    (*x)|=(ScanCoProc() << 8);
    CheckChar(',');
    (*x)|=(ScanCoOpCode4() << 20); // opcode 1
    CheckChar(',');
    (*x)|=(ScanCoReg() << 12);  // crd
    CheckChar(',');
    (*x)|=(ScanCoReg() << 16); // crn
    CheckChar(',');
    (*x)|=(ScanCoReg()); // crm
    if (ScanChar(','))
       (*x)|=(ScanCoInfo() << 5);
}

void OperandCLZ(int*x)
{
    (*x)|=(ScanRegister() << 12);
    CheckChar(',');
    (*x)|= ScanRegister();
}


void OperandBKPT(int*x)
{
    word_t k;
    NextToken();
    k=ConstIntExpr();
    if (k>0x0000ffff) ReportError(CONSTOUTOFBOUND); // out of bound
    (*x)|=(k & 0xf) | ((k << 4) & 0x000fff0f);
}


static byte_t OperandB(int*x)
{
    halfword_t y,v;
    NextToken();
    if (LowCase(Pool[TkPtr])=='@')
    {
        y=ScanAsmSymbol();
        v=HTable[y].symb.val;
        if (v==NADA)
        {
            v=NewAsmLabel(y);
            HTable[y].symb.val=v;
            Mem[v].ll.info=y;
        }
        *x|=v;
        return 1; // Local link B BL
    }

    y=CheckID();
    switch (GetXID(y))
    {
    case sFUNC:
    case sPROC:
        break;
    default:
    err:
        ReportError(UNKNOWNID,y);
    }
    if (Mem[IDVal+1].ii.lo<0) goto err;
    *x|=Mem[IDVal+1].hh.lo; // function number
    return 2; // external call
}



void OperandMCRMRC(int*x)
{
    NextToken();
    (*x)|=(ScanCoProc() << 8); // co processor number
    CheckChar(',');
    (*x)|=(ScanCoInfo() << 21); // op code 1
    CheckChar(',');
    (*x)|=(ScanRegister() << 12); // destination register
    CheckChar(',');
    (*x)|=(ScanCoReg() << 16);
    CheckChar(',');
    (*x)|=ScanCoReg(); // additionnal coprocessor register
    if (ScanChar(','))
    {
        (*x)|=(ScanCoInfo() << 5);
    }
}


byte_t OperandLDCSTC(int*x)
{
    int k;
    NextToken();
    (*x)|=(ScanCoProc() << 8);  // copro number
    CheckChar(',');
    (*x)|=(ScanCoReg() << 12); // copro register
    CheckChar(',');
    if (ScanChar('['))
    {
        (*x)|=(ScanRegister() << 16); // [Rn
        if (ScanChar(','))
        { // [Rn,#exp]  pre indexed
            (*x)|=(1 << 24);
            CheckChar('#');
            k=ConstIntExpr();
            if ((k<-256) || (k>=256))  ReportError(CONSTOUTOFBOUND);
            if (k<0)  k&=255;  else (*x)|=(1 << 23); // up bit
            (*x)|=k;
            CheckChar(']');
            if (ScanChar('!')) (*x)|=(1 << 21); // write back
        }
        else
        {
            CheckChar(']');
            if (ScanChar(',')) // post indexed
            { // [rn],#expr
                (*x)|=(1 << 21); // write back mandatory
                CheckChar('#');
                k=ConstIntExpr();
                if ((k<-256) || (k>=256)) ReportError(CONSTOUTOFBOUND);
                if (k<0) k&=255; else (*x)|=(1 << 23); // up bit
                (*x)|=k;
            }
            else // [Rn]
            {
                (*x)|=(1 << 24); // preindexed with offset zero
            }
        }
        return 0;
    }
    // special PC relative pre-indexed
    LDCPCRelative(x);
    return 6;
}



// assemble a instruction line
// this is the elementary iteration of the whole
// assembly procedure
// h and t are the head of tail of sequence of
// instruction to update
void AssemblyLine(halfword_t*h, halfword_t*t)
{
	int x;
	int opcode;
	byte_t b;
	opcode=ScanOpCode(&x);

	switch(opcode)
	{
	case 0: ScanDCB(h,t); break; // DCB
	case 1: ScanDCH(h,t); break; // DCH
	case 2: ScanDCD(h,t); break;
	case  3: // bx,blx
	    x|=ScanRegister();
	    AppendAsmNode(h,t,0,x);
	    break;
    case 4: // B BL
        b=OperandB(&x);
        AppendAsmNode(h,t,b,x);
        break;
    case 5: // MOV
        b=OperandMov(&x);
        AppendAsmNode(h,t,b,x);
        break;
    case 6: // CMP
        OperandCmp(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 7: // AND
        OperandAnd(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 8: // MUL
        OperandMul(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 9:
        OperandMla(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 10:
        OperandUMull(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 11: // data transfert
        b=OperandDT(&x);
        AppendAsmNode(h,t,b,x);
        break;
    case 12: // halfword or signed data transfert
        b=OperandHSDT(&x);
        AppendAsmNode(h,t,b,x);
        break;
    case 13: // bloc data transfert
        OperandLDM(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 14:
        OperandSWP(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 15:
        OperandSWI(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 16:
        OperandMRS(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 17:
        OperandMSR(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 18: // CDP
        OperandCDP(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 19:
        b=OperandLDCSTC(&x);
        AppendAsmNode(h,t,b,x);
        break;
    case 20: // MCR/MRC
        OperandMCRMRC(&x);
        AppendAsmNode(h,t,0,x);
        break;
	case 21: break; // label, do noting
	case 22: ScanDCF(h,t); break;
    case 23:
        OperandCLZ(&x);
        AppendAsmNode(h,t,0,x);
        break;
    case 24:
        OperandBKPT(&x);
        AppendAsmNode(h,t,0,x);
        break;
	}
	ScanChar(';');
	NextToken();

}




// Assembler
//----------
// this procedure analyses a sequence of ARM
// assembly code until the end keywords

void Assembler(halfword_t nf)
{
	halfword_t h;
	halfword_t t;

	h=NADA;
	CodeOffset=0; // initialisation of the code pointer
	LabelList=NADA;
	while (!ScanKW(kEND))
	{
		AssemblyLine(&h,&t);
	}
	// check that all labels have been defined
	while (LabelList!=NADA)
	{
		if (Mem[LabelList].ii.lo!=-1)
		{
			ReportError(UNDEFINEDID,Mem[LabelList].hh.lo);
		}
		LabelList=Mem[LabelList].ll.link;
	}

	if (h!=NADA) Mem[t].ll.link=NADA; // close list
	asmProlog(nf);
	StatementCode(h);
	asmEpilog();
}


// end of file "parse.c"
