#include "pp.h"


// return a constant node
// --------------------
// | op | bt |  ----  |
// --------------------
// |      value       |
// --------------------
halfword_t ConstNode(BasicType_t bt, word_t v)
{
	halfword_t e=GetAvail(2);
	Mem[e].nh.bt=bt;
	Mem[e].nh.op=opConst;
	Mem[e].ll.link=NADA;
	Mem[e+1].u=v;
	return e;
}

// Operator node
//  +------------------+
//  | op | bt |  lnk   |
//  --------------------
//  |    e1   |   e2   |
//  +------------------+

// binary op node
halfword_t OpNode2(Op_t op, BasicType_t bt, halfword_t e1, halfword_t e2)
{
	halfword_t x;
	x=GetAvail(2);
	Mem[x].nh.op=op;
	Mem[x].nh.bt=bt;
	Mem[x+1].hh.lo=e1;
	Mem[x+1].hh.hi=e2;
}

// unary op node
//+---------------------+
//| op | bt |     e     |
//+---------------------+

halfword_t OpNode1(Op_t op, BasicType_t bt, halfword_t e)
{
	halfword_t x=GetAvail(1);
	Mem[x].nh.op=op;
	Mem[x].nh.bt=bt;
	Mem[x].nh.link=e;
	return x;
}

//  a real 64 node
// --------------------
// | op | bt |  ---   |
// --------------------
// |       low        |
// --------------------
// |       high       |
// --------------------

halfword_t RealConstNode(word_t lo, word_t hi)
{
	halfword_t e=GetAvail(3);
	Mem[e].nh.op=opConst;
	Mem[e].nh.bt=btReal;
	Mem[e+1].u=lo;
	Mem[e+2].u=hi;
	return e;
}

// scan for a number
// if a number is scanned, assign a node to e and returns the basic type
boolean_t ScanNumber(memoryword_t* t, halfword_t*e)
{
	number_t x;
	switch(GetNumber(&x))
	{
	case btUInt:
		*e=ConstNode(btUInt,x.u);
		t->u=TUINTEGER;
		return True;
	case btReal:
		*e=RealConstNode(x.uu.lo, x.uu.hi);
		t->u=TREAL;
		return True;
	default:
		return False;
	}

}

// scan for a string or a char
// returns true if a string is actually processed
// returns type in t and const node in e
// +-----------------------+
// | op |  bt |   unused   |
// |-----------------------|
// |   string   number     |
// +-----------------------+
//
boolean_t ScanString(memoryword_t*t, halfword_t*e)
{
	halfword_t s;
	if(Pool[TkPtr]=='\'')
	{
		s=GetString();
		if (s<256) t->u=TCHAR; else *t=StringType;
		*e=ConstNode(t->th.bt,s);
		/*
				GetAvail(2);
		Mem[*e].nh.op=opConst;
		if (s<MAXCHAR)
		{
			t->u=TCHAR;
			Mem[*e].nh.bt=btChar;
		}
		else
		{
			*t=StringType;
			Mem[*e].nh.bt=btStr;
		}
		Mem[*e].nh.link=NADA; // indicates not written in code
		Mem[*e+1].u=s;*/
		return True;
	}
	return False;
}


// scan the key word "nil"
// if so, returns True, assign TPOINTER to *t and the corresponding
// constant node to *e
boolean_t ScanNil(memoryword_t*t, halfword_t*e)
{
	if (ScanKW(kNIL))
	{
		*e=ConstNode(btPtr,0);/*GetAvail(2);
		Mem[*e].nh.op=opConst;
		Mem[*e].nh.bt=btPtr;
		Mem[*e+1].u=0;*/
		t->u=TPOINTER;
		return True;
	}
	return False;
}

// Expression parsing
//-------------------



//===================================================
// type routines
//===================================================
/*
 *



 */


// numeric type ? (integer or real)
boolean_t IsNumType(memoryword_t t)
{
	return (t.th.bt>=btInt) && (t.th.bt<=btReal);
}

// simple type ?
boolean_t IsSimpleType(memoryword_t t)
{
	return (t.th.bt<=btReal)||(t.th.bt>=btEnum);
}

// check if given type is numerical (integer or real)
void CheckNumType(memoryword_t t)
{
	if (IsNumType(t)==0) ReportError(NONNUMERICALTYPE);
}

// signed or unsigned integer ?
boolean_t IsIntegerT(memoryword_t t)
{
  return (t.th.bt==btInt) || (t.th.bt==btUInt);
}


// ordinal type ?
boolean_t IsOrdinalType(memoryword_t t)
{
	return (t.th.bt<=btUInt)||(t.th.bt>=btEnum);
}

// Ordinal type ?
void CheckOrdinalType(memoryword_t t)
{
	if (!IsOrdinalType(t)) ReportError(ORDINALTEXP);
}

//+--------------------------+
//| opCast | bt   |   expr   |
//+--------------------------+

// if t is an integer type, add the suitable cast node to cast to real type
// and assigh the resulting type to t
void ForceRealType(memoryword_t*t, halfword_t*e)
{
	if (IsIntegerT(*t))
	{
		if (e!=NIL)
		{
			*e=OpNode1(opCast,btReal,*e);
			t->u=TREAL;
		}
	}
}

memoryword_t StringType;

void ForceStringType(memoryword_t*t, halfword_t*e)
{
	if (t->th.bt==btChar)
	{
		*e=OpNode1(opCast,btStr,*e);
		*t=StringType;
	}
}

void CheckCompatible(memoryword_t t1, memoryword_t t2);


// check parameters congruency of procedural types
// x and y are two formal parameters lists
void CheckParamCongruency(halfword_t x, halfword_t y)
{

	halfword_t v; //,w,r,s;
	// loop for each formal parameter list
	while ((x!=NADA) && (y!=NADA))
	{
		memoryword_t tx=Mem[Mem[x+1].hh.lo]; // type of the parameter
		memoryword_t ty=Mem[Mem[y+1].hh.lo];

		// check parameter category
		v=Mem[x].hh.lo;
		if (v!=Mem[y].hh.lo) { ReportError(TYPEMISMATCH); }
		if (v==2)
		{ // procedural parameter
		  // check both procedural of same type
// for debug
//			if ((tx.th.bt!=btProc)&&(tx.th.bt!=btFunc)) ReportError(INTERNAL,"Congruency x");
//			if ((ty.th.bt!=btProc)&&(ty.th.bt!=btFunc)) ReportError(INTERNAL,"Congruency y");
			CheckStack();
			CheckCompatible(tx,ty);
/*
			// get infos on types
			v=tx.th.link;
			w=ty.th.link;
			// get return value node
			r=Mem[v].hh.hi;
			s=Mem[w].hh.hi;
			if (r!=NADA)
			{
				if (s==NADA)  { emitString("cas 2"); ReportError(TYPEMISMATCH); }
				// check same return type
				if (Mem[r].i!=Mem[s].i) { emitString("cas 3"); ReportError(TYPEMISMATCH); }
			}
			  // recursive call on formal parameter list
			CheckParamCongruency(Mem[v].hh.lo,Mem[w].hh.lo);
*/
		}
		else  // any non procedural parameter
		{ // check same types
			if (tx.i!=ty.i)  { ReportError(TYPEMISMATCH); }
		}
		// next parameters
		x=Mem[x].ll.link;
		y=Mem[y].ll.link;
	}
	if ((x!=NADA) || (y!=NADA)) { ReportError(TYPEMISMATCH); }; // non compatible types
}


// check if t1 and t2 are compatible
void CheckCompatible(memoryword_t t1, memoryword_t t2)
{
again:
	// equal types
	if (t1.u==t2.u) return;
	// both integer types
	if (IsIntegerT(t1))
	{
		if (IsIntegerT(t2)) return;
		ReportError(TYPEMISMATCH);
	}
	// same ordinal type
	if (IsOrdinalType(t1))
	{
		if (t1.th.bt==t2.th.bt) return;
		ReportError(TYPEMISMATCH);
	}
	switch (t1.th.bt)
	{
	case btStr: // all string types are compatibles
		if (t2.th.bt==btStr) return;
		break;
	case btPtr:
		if (t1.u==TPOINTER)
		{
			// nil compatible with procedural types
			if ((t2.th.bt==btProc)||(t2.th.bt==btFunc)) return;
		}
		// now, check the other type is a pointer type
		if (t2.th.bt!=btPtr) ReportError(TYPEMISMATCH);
		// nil compatible with all pointer types
		if ((t2.th.link==NADA)||(t1.th.link==NADA)) return;
		// recursive terminal call to check compatible domain types
		t1=Mem[t1.th.link];
		t2=Mem[t2.th.link];
		goto again;
	case btSet: case btFile: case btText:
		if (t1.th.bt!=t2.th.bt) break;
		// empty set compatible with all types
		if (t1.th.bt==btSet)
		{
			if ((t1.th.link==NADA)||(t2.th.link==NADA)) return;
		}
		t1=Mem[t1.th.link];
		t2=Mem[t2.th.link];
		goto again;
	case btFunc:
	case btProc:
		// nil compatible with all procedural types
		if (t2.u==TPOINTER) return;
		if (t1.th.bt!=t2.th.bt) break;
		// if both are functions, check same return type
		if (t1.th.bt==btFunc)
		{
			if ( Mem[Mem[t1.th.link].hh.hi].i!=Mem[Mem[t2.th.link].hh.hi].i ) break;
		}
		// check congruency of formal parameter list
		CheckParamCongruency(Mem[t1.th.link].hh.lo,Mem[t2.th.link].hh.lo);
		return;
	}
	ReportError(TYPEMISMATCH);
}

// check if type is file permissible
void FilePermissible(memoryword_t t)
{
	halfword_t l; // list of fields in a record
again:
	switch(t.th.bt)
	{
	case btFile:
		ReportError(TYPEMISMATCH);
	case btArray:
		t=Mem[t.th.link+2];
		goto again;
	case btRec:
		CheckStack();
		l=Mem[t.th.link].ll.link;  // field list
		while (l!=NADA)
		{
	        FilePermissible(Mem[l+2]);
	        l=Mem[l].ll.link;
		}
	}
}

// Assignement compatibles types
// check if t2 type is assignable to t1 type
// generates cast node to e if necessary
void AssCompTypes(memoryword_t t1, memoryword_t t2, halfword_t*e)
{
	FilePermissible(t1);
	switch (t1.th.bt)
	{
	case btReal:
		if (IsIntegerT(t2)) ForceRealType(&t2,e);
		break;
	case btStr:
		ForceStringType(&t2,e);
		break;
	}
	CheckCompatible(t1,t2);
}

// type byte size returned by sizeof function
// incread in order to alignement
int TypeSizeOf(memoryword_t t)
{
	int s=TypeSize(t);
	return Align(s,t);
}



/*
procedure CanonicalType(var t:MemoryWord;t1:MemoryWord);
begin
  if @t<>nil then
  begin
    case t1.th.bt of
      btInt  : t1.u:=tInteger;
      btUInt : t1.u:=tUInteger;
      btStr  : t1:=StringType;
    end;
    t:=t1;
  end;
end;
*/


// make types t1 and t2 compatible by adding eventually a cast node if it makes sense
// - add cast integer to real if the other type is real
// - add cast char to string if the other type is string
// returns the resulting type in *t
void MakeCompatible(memoryword_t t1, memoryword_t t2, memoryword_t*t, halfword_t*e1, halfword_t*e2)
{
	switch(t1.th.bt)
	{
	case btReal:
		ForceRealType(&t2,e2);
		break;
	case btStr:
		ForceStringType(&t2,e2);
		break;
	}
	switch(t2.th.bt)
	{
	case btReal:
		ForceRealType(&t1,e1);
		break;
	case btStr:
		ForceStringType(&t1,e1);
		break;
	}
	CheckCompatible(t1,t2);
	// assign the type resulting from t1 and t2
	switch(t1.th.bt)
	{
	case btInt: // force other btInt also
		Mem[*e2].nh.bt=btInt;
		*t=t1;
		break;
	case btUInt: // unsigned only if both are unsigned
		if (t2.th.bt==btUInt) t->u=TUINTEGER;
		else
		{
			t->u=TINTEGER;
			Mem[*e1].nh.bt=btInt;  // cast node to int
		}
		break;
	case btStr:  // canonical string type for all string types
		*t=StringType;
		break;
	case btPtr:  // if one pointer is nil, the result is the other pointer
		if (t1.u==TPOINTER) *t=t2;
		else if (t2.u==TPOINTER) *t=t1;
		else *t=t1;
		break;
	default:
		*t=t1;
	}
}



// this procedure check types t1 and t2 for binary operator op
// eventually add cast nodes to expressions e1 and e2
// return resulting type in *t
void CheckTypes(memoryword_t t1, memoryword_t t2, Op_t op,
				memoryword_t *t, halfword_t *e1, halfword_t *e2)
{
	memoryword_t tr;
	switch (op)
	{
	case opAdd:
		ForceStringType(&t1,e1);
		ForceStringType(&t2,e2);
		MakeCompatible(t1,t2,t,e1,e2);
		switch(t->th.bt)
		{
		case btSet: case btInt: case btUInt: case btReal: case btStr: return;
		}
		break;
	case opSub: case opMul:
		MakeCompatible(t1,t2,t,e1,e2);
		switch(t->th.bt)
		{
		case btSet: case btInt: case btUInt: case btReal: return;
		}
		break;
	case opDiv: // apply only on real types
		ForceRealType(&t1,e1);
		MakeCompatible(t1,t2,t,e1,e2);
		if (t->th.bt==btReal) return;
		break;
	case opIDiv: case opMod:
		MakeCompatible(t1,t2,t,e1,e2);
		switch(t->th.bt)
		{
		case btInt: case btUInt: return;
		}
		break;
	case opShl: case opShr:
		if ((IsIntegerT(t1))&&(IsIntegerT(t2))) // Apply only on integer types
		{
			*t=t1; // result type is t1
			return;
		}
		break;
	case opAnd: case opOr:
		MakeCompatible(t1,t2,t,e1,e2);
		switch(t->th.bt)
		{
		case btBool: case btInt: case btUInt: return;
		}
		break;
	case opXor:
		MakeCompatible(t1,t2,t,e1,e2);
		switch(t->th.bt)
		{
		case btSet: case btBool: case btInt: case btUInt: return;
		}
		break;
	case opEq: case opNe:
		MakeCompatible(t1,t2,t,e1,e2);
		tr=*t;
		t->u=TBOOLEAN;
		if (IsSimpleType(tr)) return;
		switch(tr.th.bt)
		{
		case btSet: case btStr: case btPtr: return;
		}
		break;
	case opLe: case opGe:
		MakeCompatible(t1,t2,t,e1,e2);
		tr=*t;
		t->u=TBOOLEAN;
		if (IsSimpleType(tr)) return;
		switch(tr.th.bt)
		{
		case btSet: case btStr: return;
		}
		break;
	case opLt: case opGt:
		MakeCompatible(t1,t2,t,e1,e2);
		tr=*t;
		t->u=TBOOLEAN;
		if (IsSimpleType(tr)) return;
		if (tr.th.bt==btStr) return;
		break;
	case opIn:   // check left operand compatible with base type of right operand
		if (t2.th.bt!=btSet) break;
		CheckCompatible(t1,Mem[t2.th.link]);
		t->u=TBOOLEAN;
		return;
	}
	ReportError(OPDONOTAPPLY);

}

// scan the actual parameter list and returns
// the list nodes of expressions
// CAVEAT the list is builded in reverse ordre as the
// first pushed is the last parameter
// +--------------------+
// |  v | bt | Next +------>
// |--------------------|
// |  expr   | type +------>
// +--------------------+
// v=0 value,
// v=1 : variable,
// v=2 : code link (procedural)
// v=3 : typed const
// v=4 : stack evaluated nonscalar expression (set, string)
// v=5 : retun value parameter (string or large set)
//       if expr=nada, then result is in stack
//       else, expr is the destination variable
// bt : basic type

// function node :
//+------------------------+
//| op  |  l  |  link      |
//|------------------------|
//|     nb    |  alp       |
//+------------------------+
// op = opPCall  -> nb = Function number, l=level
// op = opInline -> nb = code list, l=func number
// link contains attribute list id function call
// returns node


halfword_t VarAttributesList(halfword_t*nt);
halfword_t VarProcCall(halfword_t *nt, halfword_t v, memoryword_t t);
halfword_t ActualPrmList(halfword_t fpl, halfword_t tr);


// parse an expression that start with a procedure or a function identifier
// output tr : return type node.
// input tr is nil for procedures

halfword_t FuncCall1(halfword_t*tr)
{

	memoryword_t ty; // return type of the function
	halfword_t n;
	short signed int nf;
	halfword_t nt; // node of the return type
	Op_t op;
	n=GetAvail(2);

	nf=Mem[IDVal+1].hh.lo; // function number
	if (nf<0)
	{	// inline function
		op=opInline;
		Mem[n+1].hh.lo=Mem[IDVal+1].ll.link; // code list
		Mem[n].qqqq.b1=nf; // function number (less than 128 predefined functions)
		// only used by optimiser to precompute predefined inline functions
	}
	else
	{
		op=opPCall;
		Mem[n].qqqq.b1=IDLevel; // level
		Mem[n+1].hh.lo=nf; // function number
	}
	Mem[n].nh.op=op;
	// assign ty to the return type of the function while IDVal remains valid
	nt=Mem[IDVal].hh.hi;
	if (tr!=NIL) ty=Mem[nt]; // function return type,

	Mem[n+1].hh.hi=ActualPrmList(Mem[IDVal].hh.lo,nt);
	Mem[n].ll.link=NADA; // default value
	if (tr!=NIL)
	{
	   // for a function, scan attribute list if function returns pointer type
       Mem[n].ll.link=VarAttributesList(&nt);
	}
	// if the type is procedural, then check if the result
	// is a Call or an assignement
	ty=Mem[nt];
	while ((ty.th.bt==btFunc) || (ty.th.bt==btProc))
	{
	    if (TestDuo(':','=')) break; // if it is an assignement, then break
	    n=VarProcCall(&nt,n,ty);
	    ty=Mem[n];
	}
	if (tr!=NIL) *tr=nt;
	return n;
}

// same as FuncCall1, but returns the type in t
// instead of the type node

halfword_t FuncCall(memoryword_t*t)
{
	halfword_t nt;
	halfword_t e=FuncCall1(&nt);
	if (nt!=NADA) *t=Mem[nt]; else t->u=TVOID;
	return e;
}

// check for a  variable
// and parse a variable access
// assiogn variable type to t and variable node to n
// this function is used to parse actual parameter that correspond
// to a "var" formal parameter
// v=1 if call with a var parameter
// v=6 if called with a const parameter
void CheckAssignableVar(memoryword_t* t, halfword_t* n, halfword_t v)
{
	halfword_t x=CheckID();
	if (GetXID(x)!=sVAR) ReportError(VAREXPECTED); // variable expected
	// check that it is not a for index variable
	if (v==1) CheckAssignable(); // check assignable for variable parameters
	VarAccess(t,n,True);
}

// actual parameter list node:
//------------------------------
//+-------------------+
//| v  | bt |    +------------> next
//|-------------------|
//|   expr  |    +------------> type of the formal parameter
//+-------------------+
// v=0 : value parameter
// v=1 : var parameter
// v=2 : procedural parameter     ; Expr = function number
// v=3 : const in code parameter
// v=4 : expression reference (evaluated in stack)
// v=5 : return value of non simple type

// return actual parameter list
// fpl is the formal parameter list
// tr point to the return type, Nada for procedure
halfword_t ActualPrmList(halfword_t fpl, halfword_t tr)
{
	halfword_t alp;     // returned list
	byte_t v;      		// info about paraneter
	memoryword_t et;  	// expected type
	memoryword_t at;	// actual type
	halfword_t e;		// Expression Node;
	halfword_t h;		// list head/tail
	halfword_t x;		// identifier of a procedural parameter
	memoryword_t t;		// return type

	alp=NADA; // by default
	// add an actual parameter for the return value
	if (tr!=NADA) // function
	{
		t=Mem[tr];
		if (!RegisterValued(t))
		{	// additionnal actual parameter node
			alp=GetAvail(2); // this is first term of the list

			Mem[alp+1].hh.lo=NADA; 	// no expression
			Mem[alp+1].hh.hi=tr;   	// return type node

			Mem[alp].qqqq.b0=5;		 // variable node
			Mem[alp].nh.bt=t.th.bt; 	// basic type (useless)
			Mem[alp].ll.link=NADA;
		}
	}
	if (fpl!=NADA)
	{
		CheckChar('(');
		do
		{
			v=Mem[fpl].ll.info;
			// assign the expected type
			if (Mem[fpl+1].hh.lo==NADA)
			{  // if value is NADA, then it is a void parameter, i.e. a non typed var formal parameter
				et.i=TVOID;
			}
			else
			{  // else this node contains the expected type
				et=Mem[Mem[fpl+1].hh.lo]; // expected type
			}
			switch(v)
			{
			case 0:
			case 4:  // value parameter
				// value parameter even if non assignable
				Expression(&at,&e,NOIDENT);
				// check compatibility
			valueparam:
				v=0;
				AssCompTypes(et,at,&e);
				OptimizeExpression(&e);
				break;
			case 1: case 6: // var or const parameter
				if (ScanKW(kNIL))  goto nilparam; // nil accepted as actual parameter
				// and treated as a null pointer
				CheckAssignableVar(&at,&e,v);
				if (et.th.bt==btStr)
				{ // all string type are var compatibles
					if (at.th.bt!=btStr) ReportError(TYPEMISMATCH); // type mismatch
				}
				else if (et.th.bt!=btVoid) // untyped formal parameter
					// in this case, all variables are acceptede
					// else must be the same type
				{
					if (at.i!=et.i) ReportError(TYPEMISMATCH);
				}
				v=1;
				break;
			case 2: // procedural parameter
				if (ScanKW(kNIL)) goto nilparam;
				x=CheckID();
				switch(GetXID(x))
				{
				case sPROC: at.th.bt=btProc; break;
				case sFUNC: at.th.bt=btFunc; break; // build actual type
				case sVAR:
					VarAccess(&at,&e,False);
					goto valueparam;  // considered as a value pointer parameter
				default: ReportError(PROCTYPEEXPECTED); // procedural type expected;
				}
				at.th.link=IDVal;  // procedural type node is similar to first word of procedure node
				e=Mem[IDVal+1].hh.lo; // function number
				AssCompTypes(et,at,NIL);
				break;
			case 3:
				if (ScanKW(kNIL))
				{  // nil allowed as actual parameter
				nilparam:
					v=0;
					e=ConstNode(btPtr,0);
					et.th.bt=btPtr;  // force actual type to pointer type
									// for actual parameter node
				}
				else
				{
					Expression(&at,&e,NOIDENT);
					AssCompTypes(et,at,&e);
					OptimizeExpression(&e); // after additionnal cast nodes
					switch(Mem[e].nh.op)
					{
					case opConst: v=3; break;
					case opVar:   v=1; break; // const or variable string or set
					default:      v=4; // expression
					}
				}
			}
			// node construction
			h=GetAvail(2);
			Mem[h+1].hh.lo=e;      // expression
			// ??? Mem[h+1].hh.hi=fpl+1;  // Pointee is type of formal parameter
			Mem[h+1].hh.hi=Mem[fpl+1].hh.lo; // type of formal parameter

			Mem[h].qqqq.b0=v;      // set variable category
			Mem[h].nh.bt=et.th.bt; // actual basic type
			Mem[h].ll.link=alp;    // reverse order
			alp=h;
			fpl=Mem[fpl].ll.link; // next
			if (fpl!=NADA) CheckChar(',');
		}
		while(fpl!=NADA);
		CheckChar(')');
	}
    return alp;
}


// scan an expression that start with a variable identifier
//---------------------------------------------------------
//
// The result may be either a simple variable access node :
//
//  +-------------------------------+
//  | opVar |  bt   | size | xxxxx  |
//  |-------------------------------|
//  |  var id node  |   attr list   |
//  +-------------------------------+
// size is scalar size only significant for scalar types
//
// or a procedural variable call :
//
//  +--------------------------------+
//  | opPPCall |  l  |  attrib list  |
//  |--------------------------------|
//  |  var access    |     alp       |
//  +--------------------------------+
//
// var access may be either simple var access (opVar)
// or a procedural variable call.
// At the very leaf, there is the simple variable the expression starts with

// returns and initialise a variable node
halfword_t VarNode(memoryword_t t, halfword_t v, halfword_t la)
{
	halfword_t n=GetAvail(2);
	Mem[n].nh.op=opVar;
	Mem[n].nh.bt=t.th.bt;  // basic type
	Mem[n].qqqq.b2=t.th.s; // scalar size
	Mem[n+1].hh.hi=la; // attribute list;
	Mem[n+1].hh.lo=v;  // variable node that contains offset
	return n;
}

// Attribute list :
// ---------------
// Indexed variable or field component
//
//+------------------------+
//|      O    |      +---------->  next
//|------------------------|
//|    expr   |    shift   | expression to add to variable offset
//|------------------------|
//|        offset          |
//+------------------------+
//
// offset and shift are applied to expression go
// define the effectve address
// they are initialised to 0 and set by optimiser
//
// Domain access (pointee)
//
//+------------------------+
//|      1    |      +---------->  next
//+------------------------+
//
// File access
// 2 : file,  3 : text
//+------------------------+
//|     2/3    |      +---------->  next
//+------------------------+
//

// t=0 : info is expression for additionnal offset
//            (indexed variable)
//       shift is additionnal shift to offset, init to 0
//       and assigned by optimiser
// t=1 : indirection for pointer access no infos
// t=2 : file access
// t=4 : actual parameter list of a procedure variable
//

// append index attribute to list defined by head h and tail t
// i is the index expression
void  AppendAttribute3(halfword_t*h, halfword_t*t, halfword_t i)
{

	halfword_t n;

	if (*h==NADA)
    {
		n=GetAvail(3);
		*h=n;
  donode:
		Mem[n].ll.info=0;
		Mem[n].ll.link=NADA;
		Mem[n+1].hh.lo=i;
		Mem[n+1].hh.hi=0;  // shiftt
		Mem[n+2].i=0;      // offset
		*t=n;
    }
	else if (Mem[*t].ll.info==0)
	{ // an index node already exsts. append addition
		Mem[*t+1].hh.lo=OpNode2(opAdd,btInt,Mem[*t+1].hh.lo,i);
	}
	else // a node exist but of other type
	{
		n=GetAvail(3);
		Mem[*t].ll.link=n;  // link to tail
		goto donode;
	}
}


// indexed variable: scan index evaluation
// assign final type in t
// and append index node to tail t of list h
void IndexedVar(halfword_t* nt, halfword_t*h, halfword_t* t)
{
	memoryword_t tx,ti,tj;
	int m; // initial offset
	int s; // component size
	halfword_t e,n; // expression and node

	tx=Mem[*nt];
	do
	{
		switch(tx.th.bt)
		{
		case btArray:
			ti=Mem[tx.th.link];   // index type
			*nt=tx.th.link+1;  // component type node
			tx=Mem[*nt];
			s=TypeSizeOf(tx); // component size
			m=-s*TypeMin(ti); // Starting point
			break;
		case btStr:
			ti.i=TINTEGER;
			*nt=CharTypeNode;  // component type is char
			m=-1;  // string starts at location 1
			s=1;   // char is size 1
			tx=Mem[*nt];
			break;
		default: ReportError(UNEXPECTEDCHAR,'['); // unexpected '['
		}
		Expression(&tj,&e,NOIDENT);
		CheckCompatible(ti,tj);
		if (s>1) e=OpNode2(opMul,btInt,e,ConstNode(btInt,s));
		if (m!=0) e=OpNode2(opAdd,btInt,e,ConstNode(btInt,m));
		AppendAttribute3(h,t,e);
	}
	while (ScanChar(','));
	CheckChar(']');
}


// scan field list l to look for identifier x
// returns Nada if not found
// assign previous node to y
halfword_t ScanFields(halfword_t x, halfword_t * y)
{
	halfword_t l;
	l=*y;
	while (l!=NADA)
	{
		*y=l;
		if (Mem[l].ll.info==x) break;
		l=Mem[l].ll.link;
	}

	return l;
}

// field component
void FieldComponent(halfword_t* nt, halfword_t*h, halfword_t*t)
{
	halfword_t x; // field identifier
	halfword_t l;
	memoryword_t ty;

	ty=Mem[*nt];
	if (ty.th.bt!=btRec) ReportError(UNEXPECTEDCHAR,'.');
	x=CheckID();
	l=Mem[ty.th.link].hh.hi; // field list
	l=ScanFields(x,&l);
	if (l==NADA) ReportError(UNKNOWNID,x); // unknown field
	*nt=l+1; // field type node
	AppendAttribute3(h,t,ConstNode(btInt,Mem[l+2].i));
}





// domain access for a pointer type or a file type
// append a node int the attribute list defined by head h and tail t
// assigns to nt a type node of the domain type
void DomainAccess(halfword_t*nt, halfword_t*h, halfword_t*t, BasicType_t bt)
{
	halfword_t n;
	if (bt==btText) *nt=CharTypeNode;
	else
	{
		*nt=Mem[*nt].ll.link;
		if (bt==btPtr) *nt=Mem[*nt].ll.link; // another indirection for pointers
	}
	//AppendAttribute(h,t,v);
	// append a single attribute with value v
	// to the list
	// h : head of list, t: tail of list
	n=GetAvail(1);
	Mem[n].ll.info=(bt==btPtr)?1:2;// domain acces of a pointer (v=1) or a file (v=2) type type
	Mem[n].ll.link=NADA;
	if (*h==NADA) *h=n;
	else Mem[*t].ll.link=n;
	*t=n;
}

// assigns the node type in nt
// returns the attribute list
halfword_t VarAttributesList(halfword_t*nt)
{
	halfword_t h=NADA; // head of list
	halfword_t t=NADA; // tail of list
	for(;;)
	{
		if (ScanChar('['))
		{
			IndexedVar(nt,&h,&t);
			continue;
		}
		if ( (!TestDuo('.','.')) && (ScanChar('.')))
		{
			FieldComponent(nt,&h,&t);
			continue;
		}
		if (ScanChar('^'))
		{
			DomainAccess(nt,&h,&t,Mem[*nt].th.bt);
			continue;
		}
		break;
	}
	return h;
}

// procedural variable call
// output nt is result type node
// v is the variable access to the procedural variable
// t is the procedural type
halfword_t VarProcCall(halfword_t *nt, halfword_t v, memoryword_t t)
{
	halfword_t n=GetAvail(2);
	Mem[n].nh.op=opPPCall;
	Mem[n].ll.link=NADA;     // default attribute list
	// set level depending on what is the variable access
	if (Mem[v].nh.op==opVar) Mem[n].qqqq.b1=Mem[Mem[v+1].hh.lo].qqqq.b1;
	else Mem[n].qqqq.b1=Mem[v].qqqq.b1;

	// set actual parameter list
	Mem[n+1].hh.hi=ActualPrmList(Mem[t.th.link].hh.lo,Mem[t.th.link].hh.hi);
	Mem[n+1].hh.lo=v;

	// for function, then scan attribute list
	*nt=Mem[t.th.link].hh.hi; // result type node
	if (*nt!=NADA)
	{
		Mem[n].ll.link=VarAttributesList(nt);
	}
	return n;
}



// parse an expression that starts with a variable
// tn is a index to the node that contains the result type
// assign to n a general variable access
// if f=true, the analysis continues if the variable is
// a function
void VarAccess1(halfword_t*nt, halfword_t*n, boolean_t f)
{
	halfword_t la; // attribute list
	halfword_t v;  // var id node
	memoryword_t t;
	halfword_t n1;

	v=IDVal;  // memorise var id node while it is valid
	*nt=v+1;  // variable type node

	// scan attribute list
	// it has to be scanned except if variable is a functionnal type
	// to accept construction such that f^, where f is a function variable
	// with no parameter
	la=NADA; // default value
	if (Mem[*nt].th.bt!=btFunc) la=VarAttributesList(nt);
	// generate variable expression node
	n1=VarNode(Mem[*nt],v,la);

	// if the type is procedural, then check if the result
	// is a Call or an assignement
	t=Mem[*nt];
	if (f)
	{
		while ((t.th.bt==btFunc) || (t.th.bt==btProc))
		{
			if (TestDuo(':','=')) break; // if it is an assignement, then break
			n1=VarProcCall(nt,n1,t);
			t=Mem[*nt];
		}
	}
	*n=n1; // returns node
}


// same as VarAccess1, but returns type instead of
// node of the type
void VarAccess(memoryword_t*t, halfword_t*e, boolean_t f)
{
	halfword_t nt;
	VarAccess1(&nt,e,f);
	if (nt!=NADA) *t=Mem[nt]; else t->u=TVOID;
}


void OptimizedExpression(memoryword_t* ty, halfword_t*e, halfword_t id)
{
	Expression(ty,e,id);
	OptimizeExpression(e);
}

// Set Constructor
//+-----------------------------------+
//| opSCons | btSet |   size          |
//|-----------------------------------|
//|   constant part | descriptor list |
//+-----------------------------------+
// op is opConst when there is no descriptor list
// either empty set or all descriptor are constant
// and has been included in the constant part
// by the oiptimiser
//
// Descriptor list :
//+------------------------------+
//|  xxxxxxxxxx   |   next       |
//|------------------------------|
//|  lower bound  | upper bound  |
//+------------------------------+

void SetConstructor(memoryword_t*t, halfword_t*z)
{

	halfword_t x;  // Component type
	halfword_t s;  // set size
	halfword_t l;  // descriptor list
	halfword_t n;  // next in descriptor list
	memoryword_t t1,t2; // bounds types
	halfword_t e; // expression

	*z=GetAvail(2);
	Mem[*z].nh.bt=btSet;
	if (ScanChar(']'))
	{ // empty set
		Mem[*z].nh.op=opSCons; // empty set is constant
		s=0; // zero size
		x=NADA; // no component type
		l=NADA; // no descriptor list
	}
	else
	{
		Mem[*z].nh.op=opSCons;
		OptimizedExpression(&t1,&e,NOIDENT);
		CheckOrdinalType(t1);
		l=GetAvail(2);
		Mem[l].ll.link=NADA;
		for(;;)
		{
			Mem[l+1].hh.lo=e; // lower bound
			if (ScanDuo('.','.'))
			{
				OptimizedExpression(&t2,&e,NOIDENT); // upper bound
				CheckCompatible(t1,t2);
			}
			Mem[l+1].hh.hi=e; // lower bound again or upper bound
			if (!ScanChar(',')) break;
			OptimizedExpression(&t2,&e,NOIDENT); // new lower bound
			CheckCompatible(t1,t2);
			n=GetAvail(2); // new node
			Mem[n].ll.link=l; // link to previous
			l=n;
		}
		CheckChar(']');
		x=GetAvail(1);
		Mem[x]=t1; // component type
		s=SetSize(t1);
	}
	Mem[*z+1].hh.lo=NADA; // no constant part. This is assigned by optimisation
	Mem[*z+1].hh.hi=l;    // descriptor list
	Mem[*z].hh.hi=s;
	// type value
	t->th.bt=btSet;
	t->th.s=s;
	t->th.link=x;
}


// Address node
//+---------------------+
//| op | bt |     v     |
//+---------------------+
// op = opAddr
// bt = btPtr (variable) or btProc (static procedure or function)
// v  = variable access node or function number
halfword_t AddrFunc(BasicType_t bt, memoryword_t*t)
{
	halfword_t e=OpNode1(opAddr,bt,Mem[IDVal+1].hh.lo);
	   // returns procedural type
	t->th.bt=bt;
	t->th.s=4;
	t->th.link=IDVal; // first word of function node is similar
	    // to procedural type intos
	return e;
}

// @ operator for variable
// returns node and assigns type
// if it is a procedural variable f then @f is the
// value of the pointer
halfword_t AddrVar(memoryword_t*t)
{
	halfword_t la;
	halfword_t n;
	*t=Mem[IDVal+1];
	switch (t->th.bt)
	{
	case btProc:
	case btFunc:  // in this case, returns value
		// create a domain access node to access to value
		la=GetAvail(1);
		Mem[la].hh.lo=1;
		Mem[la].hh.hi=NADA;
		n=VarNode(*t,IDVal,la);
		break;
	default:
		VarAccess1(&t->hh.hi,&n,True);
		t->th.bt=btPtr;
		t->th.s=4;
		t->hh.hi=NADA; // untyped pointer
	}
	// the node contains basic type in both cases. It is only
	// used to distinguish with procedure address computation
	return OpNode1(opAddr,btPtr,n);;
}

halfword_t SizeOfExpr(memoryword_t*ty)
{
	halfword_t e;
	halfword_t x;
	memoryword_t mem;
	halfword_t nt;

	CheckChar('(');
	if (ScanKW(kSTRING)) nt=StringTypeNode;
	else
	{
		x=CheckID();
		switch (GetXID(x))
		{
		case sVAR:
			MarkMem(&mem);
			VarAccess1(&nt,&e,True);
			ReleaseMem(mem);
			if (Mem[nt].th.bt==btVoid) ReportError(VAREXPECTED);
			break;
		case sFUNC:
			MarkMem(&mem);
			FuncCall1(&nt);
			ReleaseMem(mem);
			if (Mem[nt].th.bt==btVoid) ReportError(VAREXPECTED);
			break;
		case sTYPE:
			nt=IDVal;
			break;
		default:
			ReportError(UNEXPECTEDID,x);
		}
	}

	e=ConstNode(btUInt,TypeSizeOf(Mem[nt]));
	ty->i=TINTEGER;
	CheckChar(')');
	return e;
}

// ordinal function ord(e), pred(e), succ(e)
halfword_t OrdF(memoryword_t*t)
{
	halfword_t e;
	Op_t op;
	CheckChar('(');
	switch (IDVal)
	{
	case 0: op=opNop; break; // ord : do nothing but returns integer type
	case 1: op=opPred; break;
	default: op=opSucc;
	}
	OptimizedExpression(t,&e,NOIDENT);
	CheckOrdinalType(*t);
	if (op==opNop) t->i=TINTEGER;
	else e=OpNode1(op,t->th.bt,e);
	CheckChar(')');
	return e;
}

// predefined numerical functoins for both int and real
halfword_t NumF(memoryword_t*t)
{
	halfword_t e;
	Op_t op;
    CheckChar('(');
    switch(IDVal)
    {
    case 0: op=opAbs; break;
    default: op=opSqr;
    }
    OptimizedExpression(t,&e,NOIDENT);
    CheckNumType(*t);
    CheckChar(')');
    return OpNode1(op,t->th.bt,e);
}

// symbols eof or eoln
//+----------------------------+
//| op | btBool |  File expr   | op=opEof/opEoln
//+----------------------------+
halfword_t eofNode(memoryword_t*t)
{

	memoryword_t t1;
	Op_t op;
	halfword_t e;
	halfword_t n;  // returned node
	CheckChar('(');
	switch(IDVal)
	{
	case 0:
		op=opEof;
		break;
	case 1:
		op=opEoln;
	}
	OptimizedExpression(&t1,&e,NOIDENT);
	switch (t1.th.bt)
	{
	case btFile:
		if (op==opEoln) ReportError(INVALIDTYPE);
	case btText:
		break;;
	default: ReportError(FILETYPEEXPECTED); // File/text expected
	}
	n=OpNode1(op,btBool,e);
	t->i=TBOOLEAN;
	CheckChar(')');
	return n;
}


static void Factor(memoryword_t*t, halfword_t*e, halfword_t id)
{
	if (id==NOIDENT)
	{
		if (ScanNumber(t,e)) return;  	// Factor --> Number
		if (ScanString(t,e)) return;  	// Factor --> String
		if (ScanNil(t,e)) return;     	// Factor --> nil
		if (ScanChar('('))				// Factor --> (Expr)
		{
			CheckStack();
			Expression(t,e,NOIDENT);
			CheckChar(')');
			return;
		}
		if (ScanKW(kNOT))			// Factor --> not Factor
		{
			CheckStack();
			Factor(t,e,NOIDENT);
			// check compatible type
			switch(t->th.bt)
			{
			case btBool: case btInt: case btUInt:
			  	break;
			default: ReportError(OPDONOTAPPLY);
			}
			*e=OpNode1(opNot,t->th.bt,*e);
			 return;
		}
		if (ScanChar('['))  // set constructor
		{
			SetConstructor(t,e);
			return;
		}
		if (ScanChar('@'))  // addresse operator @ variable, or @ function/procedure ID
		{
			halfword_t x=CheckID();
			switch (GetXID(x))
			{
			case sFUNC:
				*e=AddrFunc(btFunc,t);
				return;
			case sPROC:
				*e=AddrFunc(btProc,t);
				return;
			case sVAR:
				*e=AddrVar(t);
				return;
			default:
				ReportError(UNEXPECTEDID,x);
			}
		}
		id=CheckID();
	}
	IDEnum_t ti = GetXID(id);
	switch(ti)
	{
	case sCONST:
		*t=Mem[IDVal];
		if (t->th.bt==btReal)
		{
			*e=RealConstNode(Mem[IDVal+1].u, Mem[IDVal+2].u);
		}
		else
		{
			*e=ConstNode(t->th.bt,Mem[IDVal+1].u);
		}
		break;
	case sVAR:  // variable access
		VarAccess(t,e,True);
		break;
	case sFUNC: // function call
		*e=FuncCall(t);
		break;
	case sORDF: // Ordinal predef function prec, succ
		*e=OrdF(t);
		break;
	case sNUMF: // numeric functions for both integer and real abs, sqr
		*e=NumF(t);
		break;
	case sCHR:
	    CheckChar('(');
	    Expression(t,e,NOIDENT);
	    CheckOrdinalType(*t);
	    t->i=TCHAR;
	    CheckChar(')');
	    break;
	case sEOF:
		*e=eofNode(t);
		break;
	case sSIZEOF:
		*e=SizeOfExpr(t);
		break;
	default:
		ReportError(UNEXPECTEDID,id);
	}
}


static void Term(memoryword_t*t, halfword_t*e, halfword_t id)
{
	Op_t op;
	memoryword_t t2; // type of second operand
	halfword_t e2;   // second operand node

	Factor(t,e,id);
	for(;;)
	{
		op=opNop;
		if (ScanChar('*')) op=opMul;
		else if (ScanChar('/')) op = opDiv;
		else if (ScanKW(kDIV))  op = opIDiv;
		else if (ScanKW(kMOD))  op = opMod;
		else if (ScanKW(kAND))  op = opAnd;
		else if (ScanKW(kSHR))  op = opShr;
		else if (ScanKW(kSHL)) op = opShl;
		if (op==opNop) break;
		Factor(&t2,&e2,NOIDENT);
		CheckTypes(*t,t2,op,t,e,&e2);
		*e=OpNode2(op,t->th.bt,*e,e2);
	}
}

static void SimpleExpression(memoryword_t*t, halfword_t*e, halfword_t id)
{

	Op_t op;
	enum {NONE, PLUS, MINUS} sign;
	memoryword_t t2;
	halfword_t e2;

	sign=NONE;
	if (id==NOIDENT)
	{
		if (ScanChar('+')) sign=PLUS;
		else if (ScanChar('-')) sign=MINUS;
	}
	Term(t,e,id);
	if (sign!=NONE) CheckNumType(*t);  // sign only allowed for numerical types
	if (sign==MINUS)
	{
		*e=OpNode1(opNeg,t->th.bt,*e);
	}
	for(;;)
	{
		op=opNop;
		if (ScanChar('+')) op=opAdd;
		else if (ScanChar('-')) op=opSub;
		else if (ScanKW(kOR)) op=opOr;
		else if (ScanKW(kXOR)) op = opXor;
		if (op==opNop) break;
		Term(&t2,&e2,NOIDENT);
		CheckTypes(*t,t2,op,t,e,&e2);
	    *e=OpNode2(op,t->th.bt,*e,e2);
	}
}

//================================================
// Expression analysis
//--------------------
// parse an expression
// returns type in t, syntactic tree in e
// input id : parsed identifier if any, else NOIDENT
//---------------------------------------------
void Expression(memoryword_t*t, halfword_t*e, halfword_t id)
{
	Op_t 		op;		// operator
	memoryword_t t2;	// second type
	halfword_t 	n2;		// nodes of second expression

	SimpleExpression(t,e,id);
	op=opNop;
	if (ScanDuo('<','=')) op=opLe;
	else if (ScanDuo('>','=')) op=opGe;
	else if (ScanDuo('<','>')) op=opNe;
	else if (ScanChar('=')) op=opEq;
	else if (ScanChar('>')) op=opGt;
	else if (ScanChar('<')) op=opLt;
	else if (ScanKW(kIN)) op=opIn;
	if (op!=opNop)
	{
	    SimpleExpression(&t2,&n2,NOIDENT);
	    CheckTypes(*t,t2,op,t,e,&n2);
	    *e=OpNode2(op,t->th.bt,*e,n2); // build the node with type and operands
	    Mem[*e].qqqq.b2=t2.th.s; //TypeSize(t2);  // required for set comparison
	}
}


// parse an expression and check it is a constant
void ConstExpr(memoryword_t*t, halfword_t* e, halfword_t id)
{
	OptimizedExpression(t,e,id);
	if (Mem[*e].nh.op!=opConst) ReportError(NONCONSTANTEXPR);
}

// check if a basic type is integer
void CheckIntegerType(BasicType_t bt)
{
	switch (bt)
	{
	case btInt: case btUInt: return;
	}
	ReportError(INTTYPEEXECTED);
}

// parse an expression and check that it is
// a constant integer type, returns value
int ConstIntExpr()
{
	memoryword_t t,m;
	int i;
	halfword_t e;
	MarkMem(&m);
	ConstExpr(&t,&e,NOIDENT);
	if (!IsIntegerT(t)) ReportError(INTEGERCONSTEXPEC);
	i=Mem[e+1].i; // get constant value
	ReleaseMem(m);
	return i;
}


// parse a real constant and assign values
// in lo,hi
void ConstRealExpr(number_t*n)
{
	memoryword_t m,t;
	halfword_t e;
	MarkMem(&m);
	ConstExpr(&t,&e,NOIDENT);
	CheckNumType(t);
	ForceRealType(&t,&e);
	OptimizeExpression(&e);
	n->uu.lo=Mem[e+1].u;
	n->uu.hi=Mem[e+2].u;
	ReleaseMem(m);
}


// constant ordinal expression
// check ordinal type and returns ordinal value
// write type in t if it is not NIL
int ConstOrdExpr(memoryword_t*t,halfword_t id)
{
	memoryword_t m;
	memoryword_t ty;
	halfword_t e;
	int i;

	MarkMem(&m);
	ConstExpr(&ty,&e,id);
	CheckOrdinalType(ty);
	if (t!=NIL) *t=ty;
	i=Mem[e+1].i;
	ReleaseMem(m);
	return i;
}

void CheckBoolExpr(halfword_t*e)
{
	memoryword_t t;
	OptimizedExpression(&t,e,NOIDENT);
	if (t.th.bt!=btBool) ReportError(BOOLEANEXPREXP);
}

