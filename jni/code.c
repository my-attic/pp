#include <stdarg.h>
#include <stdio.h>  // for debug only
#include "pp.h"

// code generation module
//-----------------------

// TODO : implement secure strings operation, that move no more byte than
// the size of the destination

// Globals system layout
//----------------------
// -4  - PC
// -8  - SP
// -12 - FP
// -16
// -20
// -24 output (3 words)
// -28
// -32
// -36 input (3 words
// -40 xxxx

// number of globals needed by the system (dword aligned)
#define SYSVARS 40
#define OUTPUT_VAR -24
#define INPUT_VAR -36
// variables for code generation

// the code is generated in the array "Code"
// the lower part of this array, indexed by "CodePtr" contains the code
// of functions under generation
// the higher part of this array contains link informations
// it is indexed by "Linkptr"
union
{
	word_t	u;
	int		i;
	struct {halfword_t lo,hi;} hh;
} Code[CODESIZE];

// Index of code in the low side of the Code array, initialised to 0
int CodePtr;
// index in the upper zone of the Code array, initialised to CODESIZE
int LinkPtr;

//+------------------------------------------------------+
//| Code ...    |                      | Link infos ...  |
//|             |                      |                 |
//+------------------------------------------------------+
//               ^ CodePtr              ^ LinkPtr         ^ CODESIZE
// CodePtr is the first free room in the code section (post incrementation access)
// LinkPtr is the least buzy foom in the link section (pre decrementation access)

// Structure of the link infos
// Function header 3 words

//     +----------------------
//     |  NbSub   | SubFuncs  |
//     +----------------------+
//     |     Size             |
//     +----------------------+
// f-> |      aCode           |   - aCode is the index of the StartCode in the Code section
//     +----------------------+   - Size is the size of the code
//                                  initialised by "CloseFunction"
//                                - SubFuncs is the index to the sub functions section
//                                  initialised by "OpenFunction"
//                                - NbSubs is the number of subroutines to link in this code
//                                  Incremented by "Subroutine()"
//
// Subfunctions section
//     |     ...              |
//     +----------------------+
//     | numfun 1 |  offset 1 | <-- CODESIZE - SubFuncs
//     +----------------------+
//  -1 | numfun 2 |  offset 2 |  - numfun is the number of the function to link at offset
//     +----------------------+  - offset is the offset from the starting code ptr to link function
//  -2 |      ...             |    number nofon
//
// NOTES:
//-------
// the index of the function headre is SIZECODE-f, with f is an halfword_t
//
// The function header is allocated by the "NewFunction" function while the function is first declared
// as it may be a forward declaration, the subfunctions section may not be consecutive to the header
//
// The subfunctions records are allocated by the "Subroutine" function while generating the code
// of the current function

int FuncNum;      // absolute index of the current function number in Link infos. Set by openfunc
halfword_t ScratchRegs;  // bitmap of the registers scratched by this function
int StartCode;           // starting position of the code for the current function

// initialisation of code variables
void InitCode()
{
	CodePtr=0;
	LinkPtr=CODESIZE;
}

// check for size of n words in the Code array
void CodeRoom(int n)
{
	if (CodePtr+n>LinkPtr) ReportError(CODETOOLARGE);
}

// check that the code of the current function does not exceed 64k words
void CheckCode()
{
	if (CodePtr-StartCode>65520) ReportError(CODETOOLARGE); // take into account possible header
}

// Allocation of a new function number
halfword_t NewFunction()
{
	int f;
	CodeRoom(3);
	LinkPtr-=3;
	f=CODESIZE-LinkPtr;
	if (f>=65536) ReportError(OUTOFMEMORY); // no room in link section
	return (halfword_t)f;
}


// functions to append words of code in the code section
//------------------------------------------------------
// append a single word in the code
static void Append(word_t u)
{
	CodeRoom(1);
	Code[CodePtr++].u=u;
	CheckCode();
}

// append n words in the code
static void AppendN(int n, ...)
{
	int i;
	va_list vl;

	CodeRoom(n);
	va_start(vl,n);
	for (i=0;i<n;i++)
	{
		Code[CodePtr++].u=va_arg(vl,word_t);
	}
	CheckCode();
}


////////////////////////////////////////////////////////////////////////////
//
//      Assembler
//
///////////////////////////////////////////////////////////////////////////


// generate call to system function number n
static void SystemCall(word_t n)
{
	AppendN(2,
			0xE1A0E00F,   	//  MOV lr,pc
			0xE599F000+n);	//  LDR R15,[R9,#n]
    ScratchRegs|=0x4000;
}

// MOV immediate value "x" in a register "rd"
// this  procedure is simple for beginning
// There exists possible improvements
//  - repeated pattern with ADD Rd,Rd,shl 16
//  - save one instruction with add and sub
void MovImm(int rd, word_t x)
{
	word_t 		y=x,z=~x,m=x;
	halfword_t 	k=0,r=0;
	boolean_t 	n=False;
	// search the longest sequence of constant bits
	do
	{
		if (z<m) {r=k; n=True; m=z; }
		if (y<m) {r=k; n=False; m=y; }
	    y=(y << 2)+(y >> 30);
	    z=(z << 2)+(z >> 30);
	    k++;
	}
	while (x!=y);
	y=(rd << 12)+((word_t)n << 22)+(r << 8);
	if (m<=255)
	{   // teminal case
	    y+=0xe3a00000; // MOV or MVN
	    Append(y+m);
	}
	else
	{
		x=m & 255;
	    m>>=8;
	    k=(r+r-8) & 31;
	    if (n) m=~m;
	    MovImm(rd,(m >> k)+(m << (32-k)));
	    y+=0xe3800000+(rd << 16); // ORR or BIC
	    Append(y+x);
	}
	// RegInfos[rd].Info:=RegConst;
	// RegInfos[rd].value:=x;
	ScratchRegs|=(1 << rd);
}

// push registers
// regs = register bitmap
void PushRegs(halfword_t regs)
{
  Append(0xe92d0000+regs); //  stmfd sp!,{regs}
}

// pop regs
// this is mainly used to pop  actual parameters in a procedure call
// a simple optimisation consists for the moment
// to avoid to pop registers that have been just pushed
void PopRegs(halfword_t regs)
{
	word_t cc;
	halfword_t pushedRegs;
	cc=Code[CodePtr-1].u; // if previoius instruction is push the same regs, then just erase it
	if ((cc & 0xffff0000) == 0xe92d0000)
	{ 	// previous instruction was pushregs
		pushedRegs=(halfword_t)cc; // previsously pushed registers
		if ((pushedRegs & regs)==pushedRegs)
		{	// all those that were pushed are poped now
			CodePtr--;  		// remove the push
			regs^=pushedRegs; 	// pop the others only
		}
	}
	if (regs) Append(0xe8bd0000+regs); // ldmfd sp!,{regs}
}

// mov   rd,rn
//-------------
void MovRdRn(int rd, int rn)
{
	if (rd!=rn) Append(0xe1a00000+(rd << 12)+rn);
}

// compute immediate operand
// returns 12bit value in Imm (rotate and byte)
// returns missing info
word_t ImmConst(word_t k, halfword_t*imm)
{
	halfword_t rr;
	rr=0;
	 while ((k & 3)==0)
	 {
		 rr++;
		 k>>=2;
	 }
	 *imm=(k & 255)+(((-rr) & 15) << 8);
	 return (k >> 8) << ((2*rr)+8);
}



// immediate addition rd <- rn + x
//--------------------------------
void AddImm(int rd, int rn, int x)
{
	halfword_t imm;
	word_t c;
	if (x==0) MovRdRn(rd,rn);
	else
	{
		if (x<0)
		{
			x=-x;
			c=0xe2400000; // sub
		}
		else c=0xe2800000; // add
		while (x!=0)
		{
			x=ImmConst(x,&imm);
			Append(c+(rn<<16)+(rd<<12)+imm);
			rn=rd;
		}
	}
}

// returns rotate right in 0..15 if k fits as a
// immediate operand of the instruction
// else, returns -1
int FitImm(word_t*k)
{
	int r,i;
    word_t m,x;
	if (*k<256) return 0;
	i=0;
	m=0xffffffff;
	x=*k;
	do
	{
		if (*k<m) { r=i; m=*k; }
		i++;
		*k=(*k << 2) + (*k >> 30);
	}
	while (*k!=x);
	if (m>=256) r=-1; else *k=m;
	return r;
}

// mov nr,nr lsl shift
void lslReg(byte_t nr, halfword_t shift)
{
  Append(0xe1a00000+(nr << 16)+nr+(shift << 7));
/*  if RegInfos[nr].info=RegConst then
    RegInfos[nr].value:=RegInfos[nr].value shl shift
*/
}

// extended cmp rd,#k instruction
void CmpX(int rd, int k)
{
	word_t x=k;
	int r;

	r=FitImm(&x);
	if (r>=0)  // cmp
		Append(0xE3500000+(rd <<  12)+(r << 8)+x);
	else
	{
		x=-k;
		r=FitImm(&x);
		if (r>=0)  // cmn
			Append(0xe3700000+(rd << 12)+(r << 8)+x);
		else
		{
			MovImm(12,k); // mov r12,#k
			Append(0xe150000c); // cmp rd,r12
		}
	}
}

// immediate conmparison
// rd : destinatuion register
// k  : constant to compare
// c  : code condition
// this in only to test equality or difference
void CmpImm(int rd, word_t k, word_t c)
{
	word_t x,y,m,cc;
    boolean_t n;  // negate ?
    int r,i;
	x=k;
	y=-k;
	// search for simple immediate
	if (x<256)
	{
	 	Append(c+0x03500000+(rd << 12)+x); // cmp
		return;
	}
	if (y<256)
	{
	 	Append(c+0x03700000+(rd << 12)+y); // cmn
		return;
	}
	// searche best rotated
	n=False;
	i=0;
	m=0xffffffff;
	do
	{
		if (y<=m)
		{
			r=i;
			n=True;
			m=y;
		}
		if (x<=m)
		{
			r=i;
			n=False;
			m=x;
		}
		i++;
		x=(x << 2)+(x >> 30);
		y=(y << 2)+(y >> 30);
	}
	while (x!=k);
	if (m<256)
	{
		Append(c+0x03500000+(rd << 12)+(n << 21)+(r << 8)+m);
		return;
	}
	// complicated case
	// subcc r12,r0,k1
	// subcc  r12,r12,k2....
	// addcc r12,r0,k1
	// addcc r12,r12,k2..
	c+=0x02400000;               // sub
	if (n) c^=0x00c00000; // switch to add
	do
	{
		cc=c+(m & 255)+(r << 8);
		m>>=8;
		r=(r-4) & 15;
		if (m=0) cc+=0x00100000; // set condition code on last
		Append(cc+(rd << 16)+0xc000);
		rd=12; // next source register is R12
	}
	while (m!=0);
}



//=================================================


////////////////////////////////////////////////////////////////////////////
// an effective address is constituted of a base
  // register and
  // either an immediate offset
  // either a register offset
typedef struct
{
	signed char baseReg;		// base register
	signed char regOffset;		// offset register
	byte_t sz;			// scalar size
	byte_t shift;		// shift on offset register
	BasicType_t bt;		// base type
	int offset;			// immediate offset
} EA_t;

// Load effective address in register rd
//--------------------------------------
void LEA(byte_t rd, EA_t*ea)
{
  //if rd<0 then rd:=GetReg;
	if (ea->regOffset<0)
	{
		AddImm(rd,ea->baseReg,ea->offset);
	}
	else
	{ // register offset add rd,rb,rx,shift
		Append(0xe0800000+(ea->baseReg << 16)+(rd << 12)+(ea->shift << 7)+ea->regOffset);
	}
}

// generates a LDR/STR instruction
// ea offset is supposed to fit
void DataTransfert(byte_t nr, EA_t * ea, boolean_t load)
{
	int c;
	boolean_t sig;
	halfword_t offset;
	sig=(ea->bt==btInt)?1:0;
	if (ea->sz==4) sig=False; 	// force unsigned for words
	if (!load) sig=False; 		// store always unsigned

	if ((ea->sz==2)|| sig)
	{	// halfword or signed data transfert
		c=0xe1800090+(sig<<6); // up bit set by default
		if (ea->sz==2) c+=0x20; // half word bit
		if (ea->regOffset<0)
		{ // immediate offset
			offset=ea->offset;
			c+=0x00400000;
			if (ea->offset<0)
			{
				c-=0x00800000;  // suppress up bit
				offset=-offset;
			}
			c+=(offset & 15)+((offset & 0xf0) << 4);
		}
		else
		{ // register offset
			c+=ea->regOffset;
		}
	}
	else
	{ // single data transfert
		c=0xe5800000;
		if (ea->sz==1) c+=0x00400000; //unsigned byte
		if (ea->regOffset<0)
		{ // immediate offset
			offset=ea->offset;
			if (ea->offset<0)
			{
				c^=0x00800000; // clear unsigned bit
				offset=-offset;
			}
			c+=offset;
		}
		else
		{ // register offset
			c+=0x02000000 + ea->regOffset + (ea->shift << 7);
		}
	}
	c|=(nr << 12)          // destination register
    		 | (ea->baseReg << 16)  // base register
    		 | (load << 20);  // load bit
	Append(c);
//  RegInfos[nr].Info:=RegUndef;
	ScratchRegs|=(1 << nr);
}

// normalise effective address
// for signed byte and halfword load, offset
// is limited to +-255
// else (unsigned byte and words) it is limited
// to +-4096
// if offset is over this limit, then get
// a register with the constant offset
// r12 register with the offset and update
// ea data
// this procedure normalise ea data according to
// to the bound
// for halfword or signed transfert with shift
// include shift to offset register
void NormaliseEA(EA_t* ea, boolean_t load)
{
	int off;
	boolean_t sig;
	int bound;
	sig=ea->bt==btInt;
	if (ea->sz==4) sig=False;  	// sign not relevant for word
	if (!load) sig=False; 		// no sign propagation for store
	if (ea->regOffset<0)
	{
		off=ea->offset;
		if (off<0) off=-off;
		if (sig || (ea->sz==2)) bound=255; // signed or halfword
		else bound=4095;
		if (off>bound)
		{
			ea->regOffset=12;
			MovImm(12,ea->offset); //ConstReg(ea->offset);
			ea->offset=0;
		}
    }
	else if ((sig || (ea->sz==2)) && (ea->shift!=0) )
    { 	// include shift to offset register
		lslReg(ea->regOffset,ea->shift);
    }
}

// Load data in register
void LDR(byte_t rd, EA_t*ea)
{
	NormaliseEA(ea,True);
	DataTransfert(rd,ea,True);
}

// Store register in memory
void STR(byte_t rd, EA_t * ea) // should not modify ea
{
  //LockRegister(rd);
  NormaliseEA(ea,False);
  DataTransfert(rd,ea,False);
//  UnlockRegister(rd);
}

// generate single word transfert instruction
//     LDR/STR rd,[rb,#k]
// if k does not fit then
//     MOV R12,#k
//     LDR/STR rd,[rb,r12]
// l=true : LDR;  l=false : STR
boolean_t LDSTRwi(int rd, int rb, int k, boolean_t l)
{
	int c=0xe5800000+(rd << 12)+(rb << 16)+(l << 20);
	if (k<0) { c^= (1 << 23); k=-k; };
	if (k<4096) c+=k;
	else
	{
		MovImm(12,k);
	    c+=(1 << 25)+12; // register offset
	}
    Append(c);
}


void REx(int rd, halfword_t e);

////////////////////////////////////////////////////////////////////////////
// procedure that  generate code for
// computing offset of the variable, given an attribute list l
void AttrList(halfword_t l, EA_t*ea)
{
	BasicType_t bt=ea->bt;
	byte_t sz=ea->sz;
	byte_t nr;

	while (l!=NADA)
	{
		switch(Mem[l].ll.info)
		{
		case 0: // index expression
			if (ea->baseReg<10) PushRegs(1 << ea->baseReg);
			REx(0,Mem[l+1].hh.lo);
			AddImm(0,0,ea->offset);
//emitString("AttrList offset="); emitInt(ea->offset);
			if (ea->baseReg<10)
			{
				PopRegs(2); // pop in r1  MOTOS pourquoi r1 ?
				ea->baseReg=1;
			}
			ea->regOffset=0;
			ea->offset=0;
			break;
		case  1: // pointer domain
			if (ea->baseReg<10) nr=ea->baseReg; else nr=0;
			ea->bt=btPtr;
			ea->sz=4;
			LDR(nr,ea); // LDR r1,ea
			ea->baseReg=nr;
			ea->regOffset=-1;
			ea->offset=0;
			ea->shift=0;
			break;

		case 2:
			//!!!!!!!!!!!!!!!!!!!
		case 3: // text component
			ea->bt=btPtr;
			ea->sz=4;
			LEA(0,ea);   // compute effective address in r0
			PushRegs(1); // push file address
			SystemCall(FILEACCESS); // call file access procedure
			PopRegs(1); // restaure file address
			ea->baseReg=0; // initialise new effective address of text component
			ea->regOffset=-1;
			ea->offset=8; // char buffer offset
			ea->bt=btChar;
			ea->sz=1;
			ea->shift=0;
			break;
		default:
			ReportError(INTERNAL,"AttrList"); // bad case
		}
		l=Mem[l].ll.link;
	}
}



// recall of meaning of variable category
// 0 = value
// 1 = reference (var parameter)
// 2 = procedural parameter
// 3 = const in code (str, set)
// 4 = index variable of a for loop
// 5 = temporary variable of a with statement
//     variable is  address. offset of the pointer
//     post offset is additionnal offset on dereferenced value
// 6 = non assignable reference
//--------------------------------------------------------------
// initialise effective address of variable v
void EAVar(EA_t*ea, halfword_t v)
{
	byte_t lv=Mem[Mem[v+1].hh.lo].qqqq.b1; // variable level
	byte_t ct=Mem[Mem[v+1].hh.lo].qqqq.b0; // category
	byte_t sz=Mem[v].qqqq.b2; // scalar size of whole variable access
	byte_t nr; // register number
	BasicType_t bt=Mem[v].nh.bt; // basic type

	// set base register
	if ((lv<=1)&&(ct!=5)) ea->baseReg=10;  // global variable or program parameter
	else if (lv==sLevel) ea->baseReg=11; // pure local variable -- r11 for temporary var of with stm
	else // non local scope
	{
		LDSTRwi(3,11,PrmIndex+12-(lv << 2),True); // LDR R3,[R11,#off] MOTOS !! pourquoi R3 ?
        ea->baseReg=3;
	}
	ea->regOffset=-1; // default is no register offset
	ea->offset=Mem[Mem[v+1].hh.lo+2].i; // variable offset
	ea->shift=0;
	switch(ct)
	{
		// 0 is value nothing else to do
	case 1: case 3: case 5: case 6: // indirection (1,6) with postoffset (5)
		ea->bt=btPtr; // pointer type while loading address
		ea->sz=4;
		if (ea->regOffset<0) nr=2;  // MOTOS !! why  R2 ?
		else nr=ea->regOffset;
		LDR(nr,ea);
		ea->baseReg=nr;
		ea->regOffset=-1;  // no offset
		if (ct!=5) ea->offset=0;
		else ea->offset=Mem[Mem[v+1].hh.lo+3].i; // postoffset
		ea->shift=0;
	}
	// indirect access
	AttrList(Mem[v+1].hh.hi,ea);
	// set variable size and basic type
	ea->bt=bt;
	ea->sz=sz;
}

// initialise ea with a sytem var of given offset
void SystemVar(EA_t*ea, int offset)
{
	ea->baseReg=10;
	ea->regOffset=-1;
	ea->offset=offset;
}


// code for actual parameter list
// returns the stack to free after procedure call
// alp : the list of actual parameters
// lv  : Level of the called function to push frame
// f=true if evaluate return value in a location pushed on top of stack
// returns stack to free after procedure call
// if @popreg is nil, then pop registers r0-r3, else, dont pop
// and assign what should be poped for procedural variable call
// after variable evaluation
// pop is assigned to the quantity which remains in stack
// This is used for string valued function and ignoring the result

// assign to ea the effective address of a pointer
// return function
// la is the attribute list
// assumes v node is a opPCall
void EAFunc(EA_t*ea, halfword_t la)
{
	if (la==NADA) ReportError(INTERNAL,"EAFunc 1");
	switch(Mem[la].ll.info)
	{
	case 0: // index of a string
		MovRdRn(0,13);
	    ea->bt=btStr;
	    ea->regOffset=-1;
	    ea->offset=0;
	    ea->baseReg=0;
	    ea->sz=4;
	    ea->shift=0;
	    AttrList(la,ea);
	    AddImm(13,13,StringType.th.link);
	    break;
	case 1: // pointer access
		ea->bt=btPtr;
	    ea->regOffset=-1;
	    ea->offset=0;
	    ea->baseReg=0;
	    ea->sz=4;
	    ea->shift=0;
	    AttrList(Mem[la].ll.link,ea);
	    break;
	default:
		ReportError(INTERNAL,"EAFunc 2");
	}
}

void CodeConst(halfword_t e,int nr);
void EAVarAccess(EA_t*ea, halfword_t v);
void PushEx(halfword_t e, BasicType_t bt, byte_t algn, int s);
static void Subroutine(halfword_t f, word_t code);

void StackEx(halfword_t e, BasicType_t bt, int s);


// load the address of procedure nf in register r0
void LoadProcAddr(halfword_t nf)
{
	word_t f=CODESIZE-(word_t)nf; // index of function in Code array

	Code[f].u|=0x10000000;		// indicates to add a pre header
	// call to offset -2
	Subroutine(nf,0xebfffffe);
}
/*

// load the address of procedure nf in register r0
procedure LoadProcAddr(nf:UInt16);
begin
  // indicates to add a pre header
  pLinkTable^[nf].longinfo :=pLinkTable^[nf].LongInfo or $10000000;
  // call to offset -2
  Call(nf,$ebfffffe);
end;



*/


// Procedure call
// input :
//   n = Node (opCall,opInline or opPPCall)
//   dest = destination
//     0 : ignored (function like procedure)
//     1 : 1 register  (REx, BEx)
//     2 : 2 retisters (RealEx)
//     3 : stack (PushE)
//     4 : @ on stack (StackEx)
// returns attribute list
halfword_t ProcCall(halfword_t n, halfword_t dest)
{


	int xs;
	halfword_t popr;

	EA_t ea1;
	CheckStack();

	// expand node
	byte_t 		lv 	= Mem[n].qqqq.b1;
	halfword_t 	la	= Mem[n].hh.hi;
	Op_t 		op	= Mem[n].nh.op;
	halfword_t 	x	= Mem[n+1].hh.lo;  // function number or codes list for inlines
	halfword_t 	alp	= Mem[n+1].hh.hi;

	if (la!=NADA)
		if (Mem[la].ll.info==0)// indexed attribute
			dest=3; // if the return value is a string then force the destination to be the stack

	if (dest==0) la=NADA; // clear attribute list if it is a procedure call
    // in this case, this field contains the link to the following statements
	// Push actual parameter lsit
	{ // ex Actual Parameter List
		  halfword_t 	next;
		  halfword_t	e; 		// expression
		  int			PrmS; 	// size of pushed parameters
		  int			s;
		  int 			PrmP; 	// position of const parameter
		  int			RemS; 	// size that remains in stack
		  EA_t			ea;
		  int			i; 		// level counter
		  int			l; 		// list
		  memoryword_t	t; 		// formal parameter type
		  int			as;    	// additionnal stack
		  BasicType_t	bt;

		  PrmS=0;
		  // compute additionnal staxk room for non simple expressions
		  l=alp;
		  as=0;
		  RemS=0;
		  while (l!=NADA)
		  {
				switch(Mem[l].qqqq.b0)
				{
				case 4: // expression computed in stack for
					// const parameter
					t=Mem[Mem[l+1].hh.hi];
					as+=(TypeSize(t)+3) & 0xfffffffc;
					break;
				case 5: // return value : make room only if
					 // no expression and not called by StackEx
					if ((Mem[l+1].hh.lo==NADA) && (dest!=4))
					{
						t=Mem[Mem[l+1].hh.hi];
						RemS=(TypeSize(t)+3) & 0xfffffffc;
						as+=RemS;
					}
				}
				l=Mem[l].ll.link;
		  }
		  // alloc stack for those parameters
		  if (as>0) AddImm(13,13,-as); // sub sp,sp,#as
		  // lexical scope frame parameters
		  if (sLevel>1)
		  {
				if (lv==sLevel)
				{// push current frame pointer
					i=lv-2;
				}
				else i=lv-1;
				if (i>0)
				{
					// load RO with address of scope prm
					AddImm(0,11,PrmIndex+8);
					do
					{	// data transfert of all scope parameters
						AppendN(2,0xe530c004,  // LDR R12,[R0,-4]!
							0xe52dc004);  // STR R12,[SP,-4]!
						i--;
						PrmS+=4;
					}
					while (i!=0);
				}
				if (lv==sLevel)
				{
					PushRegs(1 << 11);
					PrmS+=4;
				}
		  }
		  // scan actual parameter list
		  next=alp;
		  PrmP=0;
		  while (alp!=NADA)
		  {
			  next=Mem[alp].ll.link; // next paramter
			  e=Mem[alp+1].hh.lo;    // expression
			  switch(Mem[alp].qqqq.b0)   // category
			  {
			  case 0: // value parameter
				  t=Mem[Mem[alp+1].hh.hi];
				  bt=Mem[alp].nh.bt; // basic type of the actual parameter
				  // size of parameter
				  // it the actual parameter is nil instead of
				  // a reference, the size is not the type size
				  if (bt==btPtr)  s=4;
				  else  s=(TypeSize(t)+3) & 0xfffffffc;
				  // need size basic type and alignement
				  PushEx(e,bt,t.th.s & 3,s);
				  PrmS+=s;
				  break;
			  case 1: case 6:  // address (var)
		      label14:
					EAVar(&ea,e);
					LEA(0,&ea);
					PushRegs(1);
					PrmS+=4;
					break;
			  case 2:  // procedural parameter
					LoadProcAddr(e); // load address in r0
					PushRegs(1);     // push it
					PrmS+=4;    // it is a pointer
					break;
			  case 3:  // code constant (string, set)
				    // append constant in list
					CodeConst(e,0);
					PushRegs(1);
					PrmS+=4;
					break;
			  case 4:  // nonscalar expression evaluated in stack
				    AddImm(0,13,PrmS+PrmP); // add r0,sp,#x
				    PushRegs(1);            // stfmd sp!, {r0}
				    PrmS+=4;
				    t=Mem[Mem[alp+1].hh.hi];
				    s=(TypeSize(t)+3) & 0xfffffffc;
				    PrmP+=s;
				    StackEx(e,t.th.bt,s); // evaluate in deep stack
				    break;
			  case 5:  // return non scalar value
					if (e!=NADA) goto label14; // as a var parameter
					// else, push address
					if (dest==4)
					{
						LDSTRwi(0,13,PrmS+as,True);
					}
					else
					{
						AddImm(0,13,PrmS+PrmP); // add r0,sp,#x
					}
					PushRegs(1);            // stfmd sp!, {r0}
					PrmS+=4;
			  }

			  alp=next;
		  }
		  // pop up to 4 registers
		  s=PrmS;
		  if (s>16) s=16; // Max 4 parameters to pop
		  PrmS-=s;
		  popr=(1 << (s >> 2))-1;
		  // compute quantity that remains in stack
		  xs=PrmS+as;
		  if (dest!=0) xs-=RemS;

	}
	switch (op)
	{
	case opPCall:
		if (popr>0) PopRegs(popr);
		Subroutine(x,0xeb000000);
		break;
	case opPPCall:
		EAVarAccess(&ea1,x);
		LDR(12,&ea1);
		if (popr>0) PopRegs(popr);
		if (ArmVersion>=500)
		{
			Append(0xe12fff3c); // blx r12
		}
		else
		{
			Append(0xe1a0e00f); // mov lr,pc
			Append(0xe12fff1c); // bx r12
		}
		break;
	case opInline:
		if (popr>0) PopRegs(popr);;
		while (x!=NADA)
		{
			Append(Mem[x+1].i);
			x=Mem[x].ll.link;
		}
		break;
	default:
		{
			//char buffer[32];
			//sprintf(buffer,"alp (%d)",op);
			ReportError(INTERNAL,"ProcCall");
		}
	}
	AddImm(13,13,xs); // free stack
	return la;
}


// generate code to get effective address of any
// variable access
void EAVarAccess(EA_t*ea, halfword_t v)
{
	switch(Mem[v].nh.op)
	{
	case opVar:
		EAVar(ea,v);
		break;
	case opPCall:
	case opPPCall:
	case opInline:
		EAFunc(ea,ProcCall(v,1));
		break;
	}
}

// Get a register which is neither base nor index
// register of a give effective address
halfword_t GetRegEA(EA_t* ea)
{
	halfword_t r=0;
	while ((r==ea->baseReg)|| (r==ea->regOffset)) r++;
	return r;
}


// pop data to effective address
// ea : effective address
// s  : size to pop
void PopEA(EA_t*ea, BasicType_t bt, byte_t algn, int s)
{

	halfword_t r;
	halfword_t regs;

	s=(s+3)&0xfffffffc; // word aligned
	if (s<=4)
	{
		r=GetRegEA(ea);
		PopRegs(1<<r);
		STR(r,ea);
		return;
	}
	if ((algn==0)&&(s<=16))
	{
		LEA(12,ea); // move address in R12
		regs=(1 << ((s+3) >> 2))-1; // registers bitmap
		PopRegs(regs); // pop in R0-Rx
		Append(0xe88c0000+regs); // Stmia R12,{R0-Rx}
		return;
	}
	if (bt==btStr)
	{
		LEA(0,ea);
		Append(0xe1a0100d); 	// mov r1,sp
		Subroutine(STRCOPY,0xeb000000);
		AddImm(13,13,s); 		// free stack
		return;
	}
	// pop is always word aligned (used for set types only)
	LEA(0,ea);  // destination in r0
    MovImm(1,((s+3)& 0xffffffc)); // load word aligned size in r1
    Subroutine(POPDATA,0xeb000000);
}


///////////////////////////////////////////////////////////////////////////////////////////////

halfword_t ConstList;
// Constlist is the list of typed constant to append
// to the code after code generation
// +---------------------+
// | Str Num | Next ---------->
// |---------------------|
// |  index in code      |
// +---------------------+
// The instruction load register with address
// of the constant
// near   :  add rx,pc,#offset
// far    :  bl #offset  and
// const starts with  mov rx,pc
//                    mov pc,lr



// include a call to function f at current position
// initialise code with
// usage : for unconditionnal branch to procedure : Code=0xeb000000;
//         for code to return address header : Code=0xebfffffd; (offset initialised with -2)
static void Subroutine(halfword_t f, word_t code)
{
	CodeRoom(1); // link table overflow
	LinkPtr--;
	Code[FuncNum+2].hh.lo++; // increment number of sub functions
	Code[LinkPtr].hh.lo=f;   // assign function number
	Code[LinkPtr].hh.hi=(halfword_t)(CodePtr-StartCode); // current offset
	Append(code);
	ScratchRegs|=0x4000;  // declare lr as sratched
}

// returns True if a global goto occurs in the local functions definitions
// It so, the stack must be restaured from the stack pointer
// and a mov fp,sp must be aded in the prolog and mov sp, fp in the epilog
static boolean_t GlobalGotoOccurs()
{
	halfword_t l=LabelList;
	while (l!=NADA)
	{
		if (Mem[l+3].i!=0) return True;
		l=Mem[l].ll.link;
	}
	return False;
}

boolean_t ggo;  // global variable set by Openfunction that indicates if global goto
  // occured in local functions

// Open a new function
// to be called before generating code for a function
// n is the function number which is supposed to be generated by NewFunction();
void OpenFunction(halfword_t n)
{
	FuncNum=CODESIZE-(word_t)n;  // true index of the function in the Code array
	int SubFuncOffs;  // offset of the subfunctions zone
	ScratchRegs=0;
	ConstList=NADA;   // clear constants
	ggo=GlobalGotoOccurs();
	// reserve room for code header
	switch (sLevel)
	{
	case 0: // system level, no header
		break;
/*
	case 1:
		Append(0); // make room for push lr if needed
		break;
*/
	default:  // reserve room for header
		// data are written by CloseFunc ones values are known
		AppendN(6,
				0,  // stdmd sp!,{r0..r3} if any
				0,  // stdmd sp!,{lr,pc}
				0, 	// mov fp,sp  // frame pointer
				0,  // sub sp,#locals size lo
				0,  // sub sp,#locals size hi
				0); // stmfd sp!.{r4..r8} if any
	}
	StartCode=CodePtr; // global StartCode initialised
	// initialize link infos
	Code[FuncNum].u=StartCode;  // starting position of the code
	if ((sLevel==1) && ggo)
		LDSTRwi(11,10,-12,False); // sdr fp,[r10,-12] : save frame to global section
	Code[FuncNum+2].hh.lo=0;    // initialize number of subfunctions
	SubFuncOffs=CODESIZE-LinkPtr+1;  // initialize subfunctions zone
	if (SubFuncOffs>65536) ReportError(OUTOFMEMORY); // offset exceed 64k words
	Code[FuncNum+2].hh.hi=(halfword_t)SubFuncOffs;
}
// procedure prolog for assembler
void asmProlog(halfword_t nf)
{
	int SubFuncOffs;  // offset of the subfunctions zone
	FuncNum=CODESIZE-(word_t)nf;  // true index of the function in the Code array
	StartCode=CodePtr;
	// initialize link infos
	Code[FuncNum].u=StartCode;  // starting position of the code
	Code[FuncNum+2].hh.lo=0;    // initialize number of subfunctions
	SubFuncOffs=CODESIZE-LinkPtr+1;  // initialize subfunctions zone
	if (SubFuncOffs>65536) ReportError(OUTOFMEMORY); // offset exceed 64k words
	Code[FuncNum+2].hh.hi=(halfword_t)SubFuncOffs;

}
/*
procedure Prolog(nf:Int16);
begin
  StartCode:=CodePtr;
  StartLink:=LinkPtr;
  LinkPtr:=LinkPtr+1;
  FuncRoom;
  pLinkTable^[nf].ShortInfo:=StartLink;
  // initialise to 0 subfunctions
  pLinkTable^[StartLink].ShortInfo:=0; // No subfunctions
end;
*/


void BLink(int c, int l);

static void LabelsLink()
{
	int x,n,d;
	while (LabelList!=NADA)
	{
		// precompute label destination
		d=Mem[LabelList+2].i;
		// link local labels
		BLink(d,Mem[LabelList+1].i);
		// link globals labels
		d-=StartCode; // displacement from start code
		x=Mem[LabelList+3].i; // list of non local go to this label

		while (x!=0)
		{
			n=Code[x].i; // next goto
			Code[x].i=0xea000000+d;
			x=n;
		}
		LabelList=Mem[LabelList].ll.link;
	}
}




// check if the constants appended in code CPtr scratch the Link Regiter
// It has to effect to modify ScratchRegs
static void CheckConsts(int CPtr)
{
	halfword_t l=ConstList;
	int dest,offs,imm;
	halfword_t s;
	while (l!=NADA)
	{
		dest=Mem[l+1].i;
		offs=CPtr-dest-2;
		// check near or far
		imm=offs;
		while ((imm & 3)==0) imm>>=2;
		if (imm>255)
		{  //far
			ScratchRegs|=0x400; // one suffices
			return;
		}
		s=Mem[l].ll.info;  // get const info
		if (s<256) CPtr++; // if it is a single char, increment code ptr
		else
		{
			int len=StrStart[s-1]-StrStart[s]; // length in bytes
			CPtr+=(len+4)>>2; // increment by len in words, including zero
		}
		l=Mem[l].ll.link;
	}
}

// append a string in code
// s is the string number
void AppendStr(halfword_t s)
{
	halfword_t len, // byte length
			x,
			y;
	int l;			// word length
	char* pc;
	if (s<256)
	{	// append monochar string in code
		Append((word_t)s);
		return;
	}
	x=StrStart[s-255];
	len=StrStart[s-256]-x;
	l=(len+4)>>2;  // word length including terminal zero
	CodeRoom(l);
	pc=(char*)(Code+CodePtr);
	for (y=0;y<len;y++) *pc++=Pool[x++];
	do { *pc++=0; y++; } while ((y&3)!=0);
	CodePtr+=l;
}

// append constant strings and set to the code
// and link addresses

static void AppendConsts()
{
	int offs, dest;
	halfword_t s; 	// string number
	halfword_t ror; // rotate info
	int imm;
	int nr;      	// destination register
	while (ConstList!=NADA)
	{
		dest=Mem[ConstList+1].i; // destination to link
		offs=CodePtr-dest-2;
		s=Mem[ConstList].ll.info; // string;
		nr=Code[dest].u;
		// check near of far
		ror=15;
		imm=offs;
		while (imm&3==0) { imm>>=2; ror--; }
		if (imm<256)
		{
			Code[dest].u=0xe28f0000+imm+(ror << 8)+(nr << 12);
		}
		else
		{
		     // set branch
		     Code[dest].u=0xEB000000+offs;
		     AppendN(2,0xe1a0000f+(nr << 12), // mov rx,pc
			    				   0xe1a0f00e); // mov pc,lr

		}
		// transfert data
		AppendStr(s);
		ConstList=Mem[ConstList].ll.link;
	}
}


static halfword_t parity(halfword_t x)
{
	x^=x>>8;
	x^=x>>4;
	x^=x>>2;
	return x&1;
}

int sPos; // position of the globals size information in the bootstrap code
// initialised in StartupCode

// close function definition
// to be called when function definition is over
// tr is the return type

void CloseFunction(memoryword_t tr)
{
	int i;

	int ps=0; // prolog size
	// write prolog now that data are known
	switch (sLevel)
	{
	case 0:
		break;
/*
	case 1: // main section of the Pascal program
		MovImm(0,0); // return code = 0
		if (ScratchRegs&0x4000==0)
		{
			// check if constants scratches the link register
			CheckConsts(CodePtr+1);
		}
		// epilog of the main
		if (ScratchRegs&0x4000)
		{	// if lr is scratched, push it in prolog
			Code[--StartCode].u=0xE92D4000; //  STMFD R13!,{lr};
			Append(0xe8bd8000);      // and return is ldmfd sp!,{pc}
			ps=1; 					 // prolog of size 1
		}
		else
		{	// if lr is not scratched, return is just mov pc,lr
			Append(0xE1A0F00E); 	 //  MOV pc,lr
		}
		// assign global size
		Code[sPos].u=(SYSVARS+GlobalsIndex+15)&0xfffffff0;
		break;
*/
	default:
		// load registers with return value
		switch(tr.th.bt)  // depending on return type
		{
		case btStr:
		case btVoid:
			if (sLevel==1) MovImm(0,0);
			break; // do nothing
		case btReal:
			AppendN(2,0xe51b0008,  // LDR RO,[FP,-8]
					  0xe51b1004); // LDR R1,[FP,-4])
			break;
		default: // ordinal type
			switch(tr.th.s) // depending in size
			{
			case 1: // one byte
				if (tr.th.bt==btInt) Append(0xe15b00d1); // signed LDSB RO,[FP,-1]
				else Append(0xe55b0001);  // unsigned LDRB R0,[FP,-1]
				break;
			case 2: // halfword
				if (tr.th.bt!=btInt) Append(0xe15b00b2); // unsigned LDRH RO,[FP,-1]
				else Append(0xe15b00f2);  // signed LDRSH R0,[FP,-2]
				break;
			default: // word
	            Append(0xe51b0004); // LDR R0,[FP,-4]
			}
		}
		ps=StartCode; // initialise to compute prolog size
		// write prolog
		ScratchRegs&=0x1f0;  // only registers r4-r8
		// force even number of registers
		if (parity(ScratchRegs)==1) ScratchRegs|=0x1000; // include r12 in registers to get
		   // an even number of registers
		LocalsIndex=(LocalsIndex+7)&0xfffffff8; // force multiple of 8
		PrmIndex=(PrmIndex+7)&0xfffffff8;
		// stack pointer must be multiple of 8 (dword aligned) for real operations
		if (ScratchRegs!=0)
		{
			Code[--StartCode].u=0xe92d0000+ScratchRegs;  // STMFD SP!,{registers}
			Append(0xe8bd0000+ScratchRegs);  // LDMFD SP!,{registers} restaure variables registers
		}
		// sub locals
		int ls=LocalsIndex;
		halfword_t pp;
		while (ls>0)
		{
			ls=ImmConst(ls,&pp);
			Code[--StartCode].u=0xe24dd000+pp;
		}
		if ( ((LocalsIndex|PrmIndex)!=0) || ggo )
			Code[--StartCode].u=0xe1a0b00d; // mov fp,sp
		// TODO : check if lr is scrached to push it only if necessary
		// and return by mov pc,lr
		Code[--StartCode].u=0xe92d4800; //*/0xe92d4800; // stfmd sp!,{lr,fp}
        pp=PrmIndex;
        if (pp>16) pp=16;
        if (pp>0)
        {
        	Code[--StartCode].u=0xe92d0000+(1 << (pp >> 2))-1; // STMFD SP!,{R0-R3} ; push parameters
        }
        // Write epilog
        if ( ((LocalsIndex|PrmIndex)!=0) || ggo )
        	Append(0xe1a0d00b);  // mov sp,fp  restaure stack
        if (pp>0)
        {
        	AppendN(3,
        			0xe8bd4800,		// ldmfd  sp!,{lr,fp}
        			0xe28dd000+pp,  // add sp,#pp  free parameters
        	        0xe12fff1e); // bx lr for intermode call
        }
        else
        {
        	Append(0xe8bd8800); // ldmfd sp!,{fp,pc}
        	/*AppendN(2,
        			0xe8bd4800,//0xe8bd4800,   	// ldmfd sp!,(fp,lr)
        	        0xe12fff1e);  	// bx lr*/
        }
        ps-=StartCode; // compute header size
		// assign global size
		if (sLevel==1) Code[sPos].u=(SYSVARS+GlobalsIndex+15)&0xfffffff0;

	}

	AppendConsts();     // Append constants

	LabelsLink();   // link of labels

	// compute size
	Code[FuncNum+1].hh.lo=(halfword_t)(CodePtr-StartCode);
	// set StartCode
	Code[FuncNum].u=StartCode;
	// adjust relative offset of subfunctions
	int nbsubs = (int)Code[FuncNum+2].hh.lo;  // number of subfunctions
	int subs = CODESIZE-(int)Code[FuncNum+2].hh.hi;    // index of subfunctions
	for (i=0;i<nbsubs;i++) Code[subs-i].hh.hi+=(halfword_t)ps;
}

// procedure Epilog for assembler
void asmEpilog()
{
	// compute size
	Code[FuncNum+1].hh.lo=(halfword_t)(CodePtr-StartCode);
	// set StartCode
	Code[FuncNum].u=StartCode;
}
/*
Procedure Epilog(nf:Int16);
var i : Int16;
begin
  // compute size
  pLinkTable^[StartLink].longinfo:=CodePtr-StartCode;
  // set start code
  pLinkTable^[nf].longinfo:=pLinkTable^[nf].longinfo+StartCode;
  // set sub functions to relative offset ( startcode only known now )
  for i:=1 to pLinkTable^[StartLink].shortinfo do
    pLinkTable^[StartLink+i].longinfo:=pLinkTable^[StartLink+i].longinfo-StartCode;

end;
*/

// generates startup code for the very main function
void StartupCode()
{

	sLevel=0;
	LabelList=NADA;
	LinkPtr=CODESIZE-LAST_RTL;  // allocate for all predefined functions
	OpenFunction(BOOTSTRAP);    // MAIN defined as 3
	AppendN(17,
			0xE92D5ff0, //  STMFD R13!,{r4-R12,R14}; doit empiler un nombre pair de registres
			0xe1a09000, //  mov r9,r0         	; assign rtl to r9
			0xe59f0054, //  ldr r0,[pc,#84]    	; load globals size      0xE25F0014           SUBS R0,R15,#20
			0xE1A0E00F, //  MOV lr,pc			; if yes, alloc it
			0xE599F000, //  LDR R15,[R9]
			0xE290a000+SYSVARS, //  adds r10,r0,#SYSVARS  	; assign r10 to globals memory, and check null pointer
						// and make room for system variables
			0x03e00001, //  mnveq r0,#1			; in this case return code =-1
			0x0a00000e, //  beq fin
			0xe28f0020,  // add r0,PC,#32  ; save in r0 the address for exit at position (***)
			             // do not use str pc,[r0,#-4] as the value of pc depends on arm versions
			0xe50a0004,  //  str r0,[r10,#-4]     ; save return adress for exit
			0xe50ad008,  //  str sp,[r10,#-8]    ; save stack for exit
			0xe24a0000-OUTPUT_VAR,  //  sub r0,r0,24 ; output
			0xe1a0e00f,  // mov lr,pc
			0xe599f06c,  // ldr pc,[r9,#108] ; rewrite output
			0xe24a0000-INPUT_VAR,  //  sub r0,r0,36 ; input
			0xe1a0e00f,  // mov lr,pc
			0xe599f070);  // ldr pc,[r9,#112] ; reset input
	Subroutine(PMAIN,0xEB000000);  // call to the UserMain
	AppendN(8,
			0xE92D0001, //  STMFD R13!,{r0}		; save return value (****)
			0xE24a0000+SYSVARS, //  sub r0,r10,#8		; global pointer to free
			0xe1A0E00F, //  MOV lr, pc		    ; free mem
			0xe599F004, //  LDR R15,[R9,#4]
			0xE8BD0001, //  LDMFD R13!,{r0}     ; restaure return code
			0xE8BD5ff0, //  LDMFD R13!,{r4-r12,R14}	; restaure saved registers
			0xE12FFF1E, //  BX R14              ; inter mode return
			0); //  dcd 0  ; room for global memory assignement
	sPos=CodePtr-1; // memorize position of the globals size

	CloseFunction((memoryword_t)TVOID);  // main returns integer, but no return variable has been created
	// run time error
	OpenFunction(HALT);
	AppendN(2,
		0xe51ad008,	// ldr sp,[r10,#-8]	; restaure stack
		0xe51af004	// ldr pc,[r10,#-4]	; goto return position
			);
	CloseFunction((memoryword_t)TVOID);
	// signed division
	OpenFunction(SIGNEDDIV);
	AppendN(6,
		0xE2103102, // ANDS R3,R0,#-2147483648 ; get sign of a
		0x42600000, // RSBMI R0,R0,#0
		0xE033C041, // EORS R12,R3,R1,ASR#32
		0x22611000, // RSBCS R1,R1,#0
		0xE1B02000, // MOVS R2,R0,R0
		0x03A00001  // MOVEQ R0,R0,#1
		);
	Subroutine(HALT,0x0a000000);// BEQ Halt
	AppendN(14,
		0xE15200A1, // CMPS R0,R2,R1,LSR#1
		0x91A02082, // MOVLS R2,R0,R2,LSL#1
		0x3AFFFFFC, // BCC -4    ; 03B0
		0xE1510002, // CMPS R0,R1,R2
		0xE0A33003, // ADC R3,R3,R3
		0x20411002, // SUB CS R1,R1,R2
		0xE1320000, // TEQS R0,R2,R0
		0x11A020A2, // MOVNE R2,R0,R2,LSR#1
		0x1AFFFFF9, // BNE -7    ; 03BC
		0xE1A00003, // MOV R0,R0,R3
		0xE1B0C08C, // MOVS R12,R0,R12,LSL#1
		0x22600000, // RSBCS R0,R0,#0
		0x42611000, // RSBMI R1,R1,#0
		0xE1A0F00E  // MOV LR,PC ; return
			);
	CloseFunction((memoryword_t)TVOID);
	// unsigned division
	OpenFunction(UNSIGNEDDIV);
	AppendN(3,
		0xE3A03000, // MOV R3,R0,#0
		0xE1B02000, // MOVS R2,R0,R0
		0x03A00001 // MOVEQ R0,R0,#1
		);
	Subroutine(HALT,0x0a000000); // beq Halt
	AppendN(11,
		0xE15200A1, // CMPS R0,R2,R1,LSR#1
		0x91A02082, // MOVLS R2,R0,R2,LSL#1
		0x3AFFFFFC, // BCC -4    ; 03F8
		0xE1510002, // CMPS R0,R1,R2
		0xE0A33003, // ADC R3,R3,R3
		0x20411002, // SUB CS R1,R1,R2
		0xE1320000, // TEQS R0,R2,R0
		0x11A020A2, // MOVNE R2,R0,R2,LSR#1
		0x1AFFFFF9, // BNE -7    ; 0404
		0xE1A00003, // MOV R0,R0,R3
		0xE1A0F00E // MOV R15,R0,R14
			);
	CloseFunction((memoryword_t)TVOID);

	OpenFunction(MEMMOVE);  // memory move, byte format, without overlaping and non zero size
	AppendN(5,
			0xe4d13001, // ldrb r3,[r1],1  ; load byte [r1] in r3 and then increment r1
			0xe4c03001, // strb r3,[r0],1
			0xe2522001, // subs r2,r2,#1
			0x1afffffb, // bne -5
			0xe1a0f00e  // mov lr,pc
			);
	CloseFunction((memoryword_t)TVOID);

	OpenFunction(STRCOPY);  // string copy with terminal zero
	AppendN(5,
			0xe4d13001, // ldrb r3,[r1],1  ; load byte [r1] in r3 and then increment r1
			0xe4c03001, // strb r3,[r0],1
			0xe3530000, // cmps r3,#0
			0x1afffffb, // bne -5
			0xe1a0f00e  // mov lr,pc
			);
	CloseFunction((memoryword_t)TVOID);

	OpenFunction(STRCAT);  // string concatenation
	AppendN(9,
			0xe4d02001, // ldrb r2,[r0],1
			0xe3520000, // cmps r2,#0
			0x1afffffc, // bne  -4
			0xe2400001, // sub r0,r0,#1
			0xe4d12001, // ldrb r2,[r1],#1
			0xe4c02001, // strb r2,[r0],#1
			0xe3520000, // cmps r2,#0
			0x1afffffb, // bne -5
			0xe1a0f00e  // mov lr,pc
			);
	CloseFunction((memoryword_t)TVOID);

	OpenFunction(STRCOMPARE); // string comparison
	AppendN(8,
			0xe4d02001,		// ldrb r2,[r0],#1
			0xe4d13001,		// ldrb r3,[r1],#1
			0xe1520003,		// cmps r2,r3
			0x11a0f00e,		// movne pc,lr
			0xe1720003,		// cmns r2,r3
			0x1afffff9,		// bne -7
			0xe1520003,		// cmps r2,r3
			0xe1a0f00e		// mov pc,lr
			);
	CloseFunction((memoryword_t)TVOID);

	OpenFunction(PUSHDATA);  // push byte aligned data
	AppendN(9,
			0xe2812003,		// add r2,r1,#3
			0xe3c22003,		// bic r2,r2,#3
			0xe04dd002,		// sub sp,sp,r2
			0xe1a0100d,		// mov r0,sp
			0xe4d03001,		// ldrb r3,[r0],#1
			0xe4c13001,		// strb r3,[r1],#1
			0xe2522001,		// subs r2,r2,#1
			0x1afffffb,		// bne -5
			0xe1a0f00e);	// mov pc,lr
	CloseFunction((memoryword_t)TVOID);

	OpenFunction(POPDATA);   // pop word aligned data
	AppendN(5,
			0xe49dc004,		// ldr r12,[sp],#4
			0xe480c004,		// str r12,[r0],#4
			0xe2511004,		// subs r1,r1,#4
			0x1afffffb,	 	// bne -5
			0xe1a0f00e);	// mov pc,lr
	CloseFunction((memoryword_t)TVOID);


}

////////////////////////////////////////////////////////////////
//
//     Expressions
//
/////////////////////////////////////////////////////////////////
void RealEx(int rn, halfword_t e);

// code for rd = a +/- b
// s is sign true=-, false = +
void AddInt(int rd, halfword_t e, boolean_t s)
{
	word_t c;
	halfword_t a=Mem[e+1].hh.lo;
	halfword_t b=Mem[e+1].hh.hi;

	if (Mem[a].nh.op==opConst)
	{
		REx(rd,b);
		if (s)
		{	// rsb rd, rd, #0
			Append(0xe2600000+(rd<<16)+(rd<<12));
			// TODO : if constant fit in instruction, just do rsb rd,rd,k
		}
		AddImm(rd,rd,Mem[a+1].i);
	}
	else
	{
		if (s) c=0xe040000c; else c=0xe080000c; // add, rd, rd, r12
		REx(rd,b);
		PushRegs(1<<rd);
		REx(rd,a);
		PopRegs(1<<12);
		Append(c+(rd<<12)+(rd<<16));
	}
}

void NotOp(int rd,halfword_t e)
{
    REx(rd,Mem[e].ll.link);
    switch(Mem[e].nh.bt)
    {
    case btBool:
    	Append(0xe2600001+(rd<<16)+(rd<<12)); // RSB rd,rd,#1
    	break;
    default:
        Append(0xe1e00000+(rd << 12)+rd); // MVN rd,rd ; TODO fix when boolean !!!
    }
}

// string comparison
// generate code that set code condition to compare a and b
void StrCompareCC(halfword_t a, halfword_t b)
{	// this must be improved to check if push is
	// necessary
	PushEx(b,btStr,3,StringType.th.link);
	PushEx(a,btStr,3,StringType.th.link);
	Append(0xe1a0000d); // mov r0,sp
	AddImm(1,13,StringType.th.link); // add r1,sp,s
	Subroutine(STRCOMPARE,0xeb000000);
	AddImm(13,13,StringType.th.link << 1); // add sp,sp    ; free stack
}


// Binary operator on sets a and b with result in register r0
void ProcCompSet(halfword_t a, halfword_t b, int s, halfword_t nf)
{
	PushEx(b,btSet,0,s);
	PushEx(a,btSet,0,s);
	AddImm(1,13,s); // @b
	MovRdRn(0,13); // @a
	MovImm(2,s >> 2); // word size
	SystemCall(nf);
	AddImm(13,13,s<<1); // free stack
}

// set comparision
// call the corresponding routine
// result is in R0
void SetCompare(halfword_t e)
{
	halfword_t a,b;
	int s;
	a=Mem[e+1].hh.lo;
	b=Mem[e+1].hh.hi;
	s=Mem[e].qqqq.b2;
	switch(Mem[e].nh.op)
	{
	case opEq:
		ProcCompSet(a,b,s,SETEQ);
		break;
	case opNe:
		ProcCompSet(a,b,s,SETEQ);
		Append(0xe2200001); // eor r0,r0,1
		break;
	case opLe:
		ProcCompSet(a,b,s,ISSUBSET);
		break;
	case opGe:
		ProcCompSet(b,a,s,ISSUBSET);
		break;
	default: ReportError(INTERNAL, "SetCompare");
	}
}


// assign the result of the comparison of a and b in register rn
void CompareEx(int rn, halfword_t e)
{
	halfword_t a=Mem[e+1].hh.lo;
	halfword_t b=Mem[e+1].hh.hi;
	int nf;
	halfword_t t;
	boolean_t negate=False;
	boolean_t exchange=False;
	switch(Mem[a].nh.bt)
	{
	case btReal:
		switch(Mem[e].nh.op)
		{
		case opEq:
			nf=REALEQ;
			break;
		case opNe:
			nf=REALEQ;
			negate=True;
			break;
		case opGt:
			exchange=True;
		case opLt:
			nf=REALLT;
			break;
		case opGe:
			exchange=True;
		case opLe:
			nf=REALLE;
			break;
		}
		if (exchange) {t=a; a=b; b=t; }
		RealEx(0,b);
		PushRegs(3);
		RealEx(0,a);
		PopRegs(0xC);
		SystemCall(nf);
		if (negate) Append(0xe2600001+(rn<<12)); // rsb rn,r0,#1
		else MovRdRn(rn,0);
		break;
	case btStr: // string comparison
		StrCompareCC(Mem[e+1].hh.lo,Mem[e+1].hh.hi);
		word_t cc;
		switch(Mem[e].nh.op)
		{
		case opEq: cc=0x00000000; break;
		case opNe: cc=0x10000000; break;
		case opGt: cc=0x80000000; break;
		case opLt: cc=0x30000000; break;
		case opGe: cc=0x20000000; break;
		case opLe: cc=0x90000000; break;
		}
		AppendN(2,
				0xe3a00000+(rn << 12),		// mov    rd,#0
		        0x03a00001+(rn << 12)+cc);	// movxx  rd,#1
		break;
	case btSet: // set comparison
		SetCompare(e);
		MovRdRn(rn,0);
		break;
	case btInt:  // scalar comparison
		switch(Mem[e].nh.op)
		{
		case opEq:
			nf=0x00000000;  // eq
			break;
		case opNe:
			nf=0x10000000;  // ne
			break;
		case opGt:
			nf=0xc0000000; // signed gt
			break;
		case opLt:
			nf=0xb0000000; // signed lt
			break;
		case opGe:
			nf=0xa0000000; // signed ge
			break;
		case opLe:
			nf=0xd0000000; // signed le
			break;
		}
		goto scalarComp;
	default: // unsigned comparison
		switch(Mem[e].nh.op)
		{
		case opEq:
			nf=0x00000000; // eq
			break;
		case opNe:
			nf=0x10000000; // ne
			break;
		case opGt:
			nf=0x80000000; // unsigned hi
			break;
		case opLt:
			nf=0x30000000; // unsigned lo=cc
			break;
		case opGe:
			nf=0x20000000; // unsigned hs
			break;
		case opLe:
			nf=0x90000000; // unsigned ls
			break;
		}
	scalarComp:
		REx(rn,Mem[e+1].hh.hi);
		PushRegs(1<<rn);
		REx(rn,Mem[e+1].hh.lo);
	    PopRegs(1 << 12);
	    Append(0xe150000c+(rn << 12));    // cmp    rd,R12
	    Append(0xe3a00000+(rn << 12));    // mov    rd,#0
	    Append(0x02800001+(rn << 12)+nf); // addne  rd,#1
	}
}

// logical expression
void LogicalEx(int rd, int c, halfword_t e)
{
	REx(rd,Mem[e+1].hh.hi);
    PushRegs(1 << rd);
    REx(rd,Mem[e+1].hh.lo);
    PopRegs(1 << 12);
    Append(c+(rd << 12)+(rd << 16));
}

// generate code for "a shl/r b" expression
// left : true for left and false for right
void Shift(int rd, halfword_t e, boolean_t left)
{
	int c,k;
	halfword_t a=Mem[e+1].hh.lo; // a : first operand
	halfword_t b=Mem[e+1].hh.hi; // b : second operand
	boolean_t sign; //true if a is signed (sign propagation for right shift
	sign=Mem[a].nh.bt==btInt; // is first operand signed ?
	// compute data according to direction and signed operation
	if (left) c=0x00;   // lsl
	else if (sign) c=0x40; // asr
	else c=0x20; // lsr
	// check if shift is constant
	if (Mem[b].nh.op==opConst)
	{
		k=Mem[b+1].i;   // get constant shift
		REx(rd,a);  // compute first operand in rd
		c+=((k & 31) << 7); // get 5 bits of constant
	}
	else
	{	// expression shift
		c+=0xc10; // register shift R12
		REx(rd,b);
		PushRegs(1 << rd);  // push second operand
		REx(rd,a);  // compute first operand in rd
		PopRegs(1 << 12); // pop first operand in R12
	}
	Append(0xe1a00000+rd+(rd << 12)+c); // Mov rd,rd shift r12
}

// call the euclidian division routine and returns the result
// which is in nr : 0 quotient, 1 : remainder
//-----------------------------------------------------------
void EuclidianDiv(int rd, int nr, halfword_t e)
{
    int fc;  // function number
	if (Mem[e].nh.bt==btInt)fc=SIGNEDDIV; else fc=UNSIGNEDDIV;
	// division by 2^n == mov rd, rd, lsr n
	REx(0,Mem[e+1].hh.hi);  // a in r0
	PushRegs(1);            // push it
	REx(1,Mem[e+1].hh.lo);  // b in r1
	PopRegs(1);
	Subroutine(fc,0xeb000000);
	MovRdRn(rd,nr);
}

// integer multiplication
//-----------------------
void MulInt(int rd, halfword_t e)
{
	halfword_t a=Mem[e+1].hh.lo;  // parameters
	halfword_t b=Mem[e+1].hh.hi;
	// test if first operand is a constant
	if (Mem[a].nh.op==opConst)
	{

		int k=Mem[a+1].i; // get this constant
		// this constant is assumed to be <> 0 and <> 1
		// these cases are checked in optimiser

    // if it is 2^n, replace by shift: mov rd,rd,lsl n
    // if it is  2^n+1, replace by add rd,rd,rd,lsl n
    // if it is 2^n-1, replace by rsb rd,rd,rd,lsl n
    // if it is 2^n-2^k, with n>k replace by
    //    rsb rd,rd,rd,lsl n-k  ; mul by 2^{n-k}-1
    //    mov rd,rd,lsl k    ; mul by 2^k
    // if it is 2^n+2^k, with n>k replace by
    //    add rd,rd,rd,lsl n-k  ; mul by 2^{n-k}+1
    //    mov rd,rd,lsl k    ; mul by 2^k
		{// general case
			REx(rd,b);  // get b in rd
			MovImm(12,k);  // get a in r12
			Append(0xe000009c+(rd << 8)+(rd << 16)); // mul rd,rd,r12
		}
	}
	else
	{ // general case expr * expr
		REx(0,b);
		PushRegs(1); // empiler b
		REx(rd,a);             // calculer a dans rd
		PopRegs(1 << 12);      // dépiler b dans r12
		Append(0xe000009c+(rd << 8)+(rd << 16)); // mul rd,rd,r12
	}
}

void RFunc(int rd, halfword_t e)
{
	EA_t ea;
	halfword_t la=ProcCall(e,1);
	if (la==NADA)
	{	// no attribute list
		MovRdRn(rd,0);
	}
    else
    {
    	EAFunc(&ea,la); // get ea
    	LDR(rd,&ea);    // load value
    }
}


void Address(int rd, halfword_t e)
{
	EA_t ea;
	switch(Mem[e].nh.bt)
	{
	case btPtr: // variable address
        EAVar(&ea,Mem[e].hh.hi);
        LEA(rd,&ea);
        break;
	case btFunc:
	case btProc: // improvement possible : check if it is the current function
				// and implement a add r0,pc,#offset
        LoadProcAddr(Mem[e].hh.hi);
        MovRdRn(rd,0);
        break;
	default: ReportError(INTERNAL,"Address");
	}
}

void EofProcReg(int rd, halfword_t e, word_t nf)
{
	EA_t ea;
	EAVar(&ea,Mem[e].hh.hi);
	LEA(0,&ea);
	SystemCall(nf);
	MovRdRn(rd,0);
}

// Procedure that generates the test x in e
void SetInProc(halfword_t n)
{
	  halfword_t x=Mem[n+1].hh.lo;
	  halfword_t e=Mem[n+1].hh.hi;
	  int s=Mem[n].qqqq.b2;
	  int xs; // bytes to pop
	  EA_t ea;
	  xs=0;
	  // get address of set in r0
	  switch(Mem[e].nh.op)
	  {
	  case opConst:
          CodeConst(e,0);
          break;
	  case opVar:
		  EAVarAccess(&ea,e);
		  LEA(0,&ea);
		  break;
	  default:
		  PushEx(e,btSet,0,s); // compute in stack
		  MovRdRn(0,13);  // mov r0,sp; address is sp
		  xs=s; // set must be then poped
	  }
	  PushRegs(1);   // push it
	  REx(1,x);      // Get Expression in r1
	  PopRegs(1);    // Pop @set
	  MovImm(2,s>>2);  // set word size to r2
	  SystemCall(ISINSET); // Call routine
	  AddImm(13,13,xs); // pop stack
}


// generate the code that compute an expression in a register
//-----------------------------------------------------------
void REx(int rd, halfword_t e)
{
	EA_t ea;
	switch(Mem[e].nh.op)
	{
	case opConst:
		MovImm(rd,Mem[e+1].i);
		break;
	case opVar:
		EAVar(&ea,e);
		LDR(rd,&ea);
		break;
	case opNot:
		NotOp(rd,e);
	    break;
	case opNeg:
	    REx(rd,Mem[e].ll.link);
	    Append(0xe2600000+(rd << 16)+(rd << 12)); // RSB rd,rd,#0
	    break;
	case opPred: // SUB Rd,Rd,#1
		REx(rd,Mem[e].ll.link);
		Append(0xe2400001+(rd << 16)+(rd << 12));
		break;
	case opSucc: // ADD Rd,Rd,#1
		REx(rd,Mem[e].ll.link);
		Append(0xe2800001+(rd << 16)+(rd << 12));
		break;
	case opAbs:
		REx(rd,Mem[e].ll.link);
		Append(0xE3300000+(rd << 16));  // TEQ Rd,#0
		Append(0x42600000+(rd << 16)+(rd << 12)); // RSBMI Rd,Rd,#0
		break;
	case opSqr:
		REx(rd,Mem[e].ll.link);
		Append(0xe0000090+(rd << 16)+(rd << 8)+rd);// MUL Rd,Rd,Rd
		break;
	case opAddr:
		Address(rd,e);
		break;
	case opAdd:
		AddInt(rd,e,False);
	    break;
	case opMul:
		MulInt(rd,e);
		break;
	case opSub:
		AddInt(rd,e,True);
		break;
	case opIDiv:
		EuclidianDiv(rd,0,e);  // quotient is in r0
		break;
	case opMod:
		EuclidianDiv(rd,1,e);  // remainder is in r1
		break;
	case opShl:
	    Shift(rd,e,True);
	    break;
	case opShr:
	    Shift(rd,e,False);
	    break;
	case opAnd:
		LogicalEx(rd,0xe000000c,e);
		break;
	case opOr:
		LogicalEx(rd,0xe180000c,e);
		break;
	case opXor:
		LogicalEx(rd,0xe020000c,e);
		break;
	case opEq:
	case opNe:
	case opGt:
	case opLt:
	case opGe:
	case opLe:
		CompareEx(rd,e);
		break;
	case opIn:
		SetInProc(e);
		MovRdRn(rd,0);
		break;
	case opPCall:
	case opPPCall:
	case opInline:
		RFunc(rd,e);
		break;
	case opEof:
		EofProcReg(rd,e,ENDOFFILE);
		break;
	case opEoln:
		EofProcReg(rd,e,ENDOFLINE);
		break;
	default:
		ReportError(INTERNAL,"Rex");
	}
}


// binary operations on reals
void  RealBinOp(int rn, halfword_t e, int nf)
{
	RealEx(0,Mem[e+1].hh.hi);
	PushRegs(3);
	RealEx(0,Mem[e+1].hh.lo);
	PopRegs(0xc);
	SystemCall(nf);
	MovRdRn(rn,0);
	MovRdRn(rn+1,1);
}

void  RealFunc(int n1, halfword_t e)
{
    EA_t ea;
	halfword_t la=ProcCall(e,2);
	if (la==NADA)
	{	// No attribute list
		MovRdRn(n1,0);
		MovRdRn(n1+1,1);
	}
	else
	{
		EAFunc(&ea,la);
		LEA(12,&ea);
	    Append(0xe89c0000+(3 << n1)); // lddmia r12,{r0,r1}
	}
}

// evaluate a real expression in registers rn (lo) and rn+1 (hi)
// the result takes place in two consecutive registers thus n1 is even
//---------------------------------------------------------------------
void RealEx(int rn, halfword_t e)
{
	EA_t ea;
	CheckStack();

	switch(Mem[e].nh.op)
	{
	case opConst:
		MovImm(rn,Mem[e+1].u);
		MovImm(rn+1,Mem[e+2].u);
		break;
	case opVar:
		EAVar(&ea,e);
		LEA(12,&ea);
		Append(0xe89c0000+(3 << rn)); // sdmia r12,{n1,n2}
		break;
	case opNeg:
		RealEx(rn,Mem[e].hh.hi);
		// insert instruction that change sign bit of result
		Append(0xe2200102+((rn+1) << 16)+((rn+1) << 12)); // eor n2,n2,$80000000
		break;
	case opCast: // cast integer to real
		e=Mem[e].hh.hi; // integer expression
		REx(0,e);  // evaluate it in r0
		if (Mem[e].nh.bt==btInt)SystemCall(INTTOREAL); else SystemCall(UINTTOREAL);
		MovRdRn(rn,0);
		MovRdRn(rn+1,1);
		break;
	case opAbs:
		RealEx(rn,Mem[e].hh.hi);
		// clear sign bit
		Append(0xe3c00102+((rn+1) << 16)+((rn+1) << 12)); // bic n2,$80000000
		break;
	case opSqr:
		RealEx(0,Mem[e].hh.hi);	 // evaluate parameter in r0-r1
		MovRdRn(2,0);  			// duplicate in r2-r3
		MovRdRn(3,1);
		SystemCall(REALMUL);
		MovRdRn(rn,0);
		MovRdRn(rn+1,1);
		break;
	case opAdd:
		RealBinOp(rn,e,REALADD);
		break;
	case opMul:
		RealBinOp(rn,e,REALMUL);
		break;
	case opSub:
		RealBinOp(rn,e,REALSUB);
		break;
	case opDiv:
		RealBinOp(rn,e,REALDIV);
		break;
	case opPCall: case opPPCall: case opInline :
		RealFunc(rn,e);
		break;
	default:
		ReportError(INTERNAL, "RealEx");
	}

}

////////////////////////////
//
// Conditions and branches
//
////////////////////////////

// attach link info n at the end of link list ll and
// replace n by ll
// Both are linked position in the code array. All
// these position are branch instruction to the same
// destination. This procedure finally appends those
// two lists
void Attach(int*n, int ll)
{
	int x,y;
	if (ll!=0)
	{
		x=ll;
		y=Code[x].u&0x00ffffff;
		while (y!=0)
		{
			x=y;
			y=Code[x].u & 0x00ffffff;
		}
		Code[x].u=(Code[x].u & 0xff000000) + ((y+*n) & 0x00ffffff);
		*n=ll;
	}
}

// set all branches linked in l to destination c
void BLink(int c, int l)
{
	word_t t;
	while(l!=0)
	{
		t=Code[l].u;
		Code[l].u=((t&0xff000000) + ((c-l-2)&0x00ffffff) ); // keep instruction code and set offset
		l=t&0x00ffffff;
	}
}


// branch expression on condition
// cc  : condition true or false
// e   : expression to evaluate
// lnk : link chain of branch
// Generates the code to evaluate the expression
// and
void BEx(boolean_t cc, halfword_t e, int*lnk);

void Branch(int c, int *lnk)
{
	Append(c+0x0a000000+*lnk); // bxx and link
	*lnk=CodePtr-1;
}


// branch on the result of r0
void RegBranch(boolean_t cc, int *lnk)
{
	Append(0xe3500000); // cmp r0,#0
	Branch(cc<<28, lnk);
}

// scalar comparison
// c =  code condition
void ScalarComp(int c, boolean_t cc, halfword_t e, int* lnk)
{
	c ^= (cc << 28);
	REx(0,Mem[e+1].hh.hi);
	PushRegs(1);
	REx(0,Mem[e+1].hh.lo);
	PopRegs(1 << 12);
	Append(0xe150000c);       // cmp r0,r12
	Branch(c, lnk);
}

// real comparison
void RealComp(boolean_t cc, halfword_t e, int *lnk)
{
	word_t nf;  // function number
	switch(Mem[e].nh.op)
	{
	case opNe:
		cc^=True;
	case opEq:
		nf=REALEQ;
		break;
	case opGe:
		cc^=True;
	case opLt:
		nf=REALLT;
		break;
	case opGt:
		cc^=True;
	case opLe:
		nf=REALLE;
	}
	RealEx(0,Mem[e+1].hh.hi);
	PushRegs(3);
	RealEx(0,Mem[e+1].hh.lo);
	PopRegs(0xc);
	SystemCall(nf),
	RegBranch(cc, lnk);
}

// string comparison
void StrComp(word_t c, word_t cc, halfword_t e,int*lnk)
{
	StrCompareCC(Mem[e+1].hh.lo,Mem[e+1].hh.hi);
	Branch(c ^ (cc << 28),lnk);
}

// set comparison
void SetComp(boolean_t cc, halfword_t e, int*lnk)
{
	SetCompare(e);
	RegBranch(cc,lnk);
}

// comparison
// uc : unsigned comparison code
// sc : signed comparison code
void Compare(int uc,int sc, boolean_t cc,halfword_t e, int *lnk)
{
	BasicType_t bt=Mem[Mem[e+1].hh.lo].nh.bt;
	switch(bt) // check type of one of the terms
	{
	case btBool:
	case btChar:
	case btUInt:
	case btPtr:
		ScalarComp(uc,cc,e, lnk);
		break;
	case btInt:
		ScalarComp(sc,cc,e, lnk);
		break;
	case btStr:
		StrComp(uc,cc,e,lnk);
		break;
	case btSet:
		SetComp(cc,e,lnk);
		break;
	case btReal:
		RealComp(cc,e, lnk);
		break;
	default:
      if (bt>=btEnum) ScalarComp(uc,cc,e, lnk);
      else ReportError(INTERNAL,"Compare");
	}
}

// a : true for and operator
void LogicalBranch(boolean_t aa, halfword_t e, boolean_t cc, int* lnk)
{
	int l;
	if (cc ^ aa)
	{
		BEx(cc,Mem[e+1].hh.lo,lnk);
		BEx(cc,Mem[e+1].hh.hi,lnk);
	}
	else
	{
		l=0;
		BEx(cc^True,Mem[e+1].hh.lo,&l);
		BEx(cc,Mem[e+1].hh.hi,lnk);
		BLink(CodePtr,l);
	}
}

void BRFunc(halfword_t e, boolean_t cc, int *lnk)
{
	halfword_t la;
	EA_t ea;

	la=ProcCall(e,1);
	if (la==NADA) RegBranch(cc,lnk);
	else
	{
		EAFunc(&ea,la);
		LDR(0,&ea);
		RegBranch(cc, lnk);
	}
}

void EofProc(halfword_t e, boolean_t cc, int*lnk, word_t nf)
{
	EA_t ea;

	EAVar(&ea,Mem[e].hh.hi);
	LEA(0,&ea);
	SystemCall(nf);

	RegBranch(cc,lnk);
}


void XorBranch(halfword_t e, boolean_t cc, int*lnk)
{
	REx(0,Mem[e+1].hh.lo);
	PushRegs(1);
	REx(0,Mem[e+1].hh.hi);
	PopRegs(1 << 12);
	Append(0xe130000c); // teq r0,r12
	Append(0x0a000000+*lnk + (cc << 28)); // branch
	*lnk=CodePtr-1;
	// condition is xored by cc
}

// TODO : voir si BEx peut renvoyer la valeur lnk au lieu de l'affecter à lnk

void BEx(boolean_t cc, halfword_t e, int*lnk)
{

	CheckStack();
again:
	switch(Mem[e].nh.op)
	{
	case opVar:
	case opConst:
		REx(0,e);
		RegBranch(cc,lnk);
		break;
	case opNot:
		cc^=True;
		e=Mem[e].ll.link;
		goto again;
		break;
	case opAnd:
		LogicalBranch(True,e,cc,lnk);
		break;
	case opOr:
		LogicalBranch(False,e,cc,lnk);
		break;
	case opXor:
		XorBranch(e,cc,lnk);
		break;
	case opEq:
		Compare(0x10000000,0x10000000,cc,e,lnk); // branch on neq
		break;
	case opNe:
		Compare(0x00000000,0x00000000,cc,e,lnk); // branch on eq
		break;
	case opLe:
		Compare(0x80000000,0xc0000000,cc,e,lnk); // branch on hi,gt
		break;
	case opLt:
		Compare(0x20000000,0xa0000000,cc,e,lnk); // branch on hs,ge
		break;
	case opGe:
		Compare(0x30000000,0xb0000000,cc,e,lnk); // branch on lo,lt
		break;
	case opGt:
		Compare(0x90000000,0xd0000000,cc,e,lnk); // branch on ls,le
		break;
	case opIn:
		SetInProc(e);
		RegBranch(cc,lnk);
		break;
	case opPCall:
	case opPPCall:
	case opInline:
		BRFunc(e,cc,lnk);
		break;
	case opEof:
		EofProc(e,cc,lnk,ENDOFFILE);
		break;
	case opEoln:
		EofProc(e,cc,lnk,ENDOFLINE);
		break;
	default:
		ReportError(INTERNAL,"BEx");
	}
}

// the statement code function generates the code for the given list of nodes
// and return the linked list in the code array of branches to the end of the code statement
// this allows to optimize jumps.
int StatementCode(halfword_t n);

// generate statement code and link
// this means that all the destinations defined by the return list of Statement code are
// solved to the current position in the code.
void StmCodeAndLink(halfword_t x)
{
	int s=StatementCode(x);  // this instruction canges CodePtr !
	BLink(CodePtr,s);
}



////////////////////////////////////////////////////////////////
//
//     Statements
//
/////////////////////////////////////////////////////////////////


void NewLine(halfword_t f)
{
	EA_t ea;
	if (f==NADA)
		SystemVar(&ea,OUTPUT_VAR);
	else
		EAVar(&ea,f);
	LEA(0,&ea);
	SystemCall(NEWLINE);
}



// code to load a code constant address e in register nr
// append the constant in const list
void CodeConst(halfword_t e,int nr)
{
	  Mem[e].ll.info=Mem[e+1].i; // string number
	  Mem[e].ll.link=ConstList;
	  ConstList=e;
	  Mem[e+1].i=CodePtr;        // code position to link to
	  Append(nr);                // initialise with register number
}

////////////////////////////////////////////////////////////////////////////////////////////////

void WriteText(halfword_t n)
{
	BasicType_t bt;
	halfword_t f,e,w,p;
	boolean_t fixed,  	// flag if parameter is a fixed point real
		real,			// flag is parameter is a real
		stringexpr=False;		// flag if parameter is a string evaluated in stack
	int rp;   // precision register
	EA_t ea;
	int pp=0;  // size to pop, by default zero
	// used for string evaluated in stack

	bt=Mem[n].nh.bt;   // basic type
	f=Mem[n+1].hh.lo;  // file
	e=Mem[n+1].hh.hi;  // expression
	w=Mem[n+2].hh.lo;  // width
	p=Mem[n+2].hh.hi;  // precision


	real = (bt==btReal);
	fixed=((real)&&(p!=NADA));

	if (bt==btStr)
	{  // for string evaluated in stack, evaluate now
		switch (Mem[e].nh.op)
		{
		case opConst:
		case opVar:
			break;
		default:
			pp=StringType.th.link; // canonical size of string
			PushEx(e,btStr,3,pp);
			stringexpr=True;
		}
	}
	// evaluate the file variable
	// push file variable
	if (f==NADA) SystemVar(&ea,OUTPUT_VAR); else EAVar(&ea,f);
	LEA(0,&ea);
	if (fixed) PushRegs(3); else PushRegs(1);

	// push precision for fixed reals
	if (fixed)
	{
		REx(3,p);
		PushRegs(8);
		pp=8; // for fixed, stack contains file descriptor, and stack must be dword aligned
	}
	if (real) rp=2; else rp=1;  // width parameter is in R1 for single words value
	// push width parameter
	if (w==NADA) MovImm(rp,0); else REx(rp,w);
	PushRegs(1<<rp);
	switch (bt)
	{
	case btReal:
		RealEx(0,e);  // expression in r0 r1
		break;
	case btBool: case btChar: case btInt: case btUInt:
		REx(0,e);
		break;
	case btStr:
		switch(Mem[e].nh.op)
		{
		case opConst:
			CodeConst(e,0);
			break;
		case opVar:  // load address of s in r0
			EAVar(&ea,e);
			LEA(0,&ea);
	        break;
		}
		break;
	}
	// now pop other parameters
	if (fixed) PopRegs(0x0c);  // pop width and precision, but file remains in stack
	else if (real) PopRegs(0x0c); // pop width and file
	else PopRegs(6); // pop width and file
	//

	if (stringexpr)
	{
		MovRdRn(0,13);  // parameter is in stack
	}

	switch(bt)
	{
	case btInt:
		SystemCall(WRITEINTEGER);
		break;
	case btUInt:
		SystemCall(WRITEUNSIGNED);
		break;
	case btChar:
		SystemCall(WRITECHAR);
		break;
	case btBool:
		SystemCall(WRITEBOOLEAN);
		break;
	case btReal:
		if (fixed) SystemCall(WRITEFIXED); else SystemCall(WRITEFLOAT);
		break;
	case btStr:
		SystemCall(WRITESTRING);
		break;
//	default:
//		ReportError(INTERNAL, "WriteText 2");
	}
	AddImm(13,13,pp); //free stack
}


// File procedure close, get, put
// it just call the system call given by nf = function number
static void FileProc(halfword_t f, halfword_t nf)
{
	EA_t ea;

	EAVar(&ea,f);
	LEA(0,&ea);
	SystemCall(nf);
	if (nf!=CLOSE)
	{	// except for "close", branch on Halt on fatal error
		Append(0xe3500000); // cmps r0,#0
		Subroutine(HALT,0x1a000000); // bne Halt

	}
}

// rw = 0 for reset and 1 for rewrite
// n is the expression node of the name
void OpenProc(byte_t rw, halfword_t f, halfword_t n, int s)
{
	halfword_t pp; // to free from stack after call -- set by name evaluation
	halfword_t nf; // function number
	EA_t ea;

	if (n==NADA)
	{  	// open without a name means initialise console
		// just load file in R0 and call ResetInput or RewriteOutput, depending on rw
		EAVar(&ea,f);
		LEA(0,&ea);
		if (rw==0) nf=RESETINPUT; else nf=REWRITEOUTPUT;
		SystemCall(nf);
	}
	else
	{	// open with a given name
		pp=0;
		// first, push adress of name
		switch (Mem[n].nh.op)
		{
		case opConst: 	// constant
	        CodeConst(n,2); // load address of name in register R2
	        PushRegs(4);    // push it
	        break;
		case opVar:		// variable
	        EAVar(&ea,n);
	        LEA(2,&ea);
	        PushRegs(4);
			break;
		default:		// expression
			pp=StringType.th.link; // maxString
			PushEx(n,btStr,3,pp);
	        PushRegs(1 << 13);
		}
		MovImm(1,s);  // load R1 with the size of a record
		PushRegs(2);  // push it (may be optimized)
	    EAVar(&ea,f); // now load file variable in R0
	    LEA(0,&ea);
	    PopRegs(2+4);
	    if (rw==0) nf=RESET; else nf=REWRITE; // function number Reset / Rewrite
	    SystemCall(nf);  // call reset or rewrite
	    AddImm(13,13,pp);  // free stack
	    // fatal error branch
		Append(0xe3500000); // cmps r0,#0
		Subroutine(HALT,0x1a000000); // bne Halt
	}
}





//******************************************************
// Code for pushing an expression
//*****************************************************


// code for pushing string of size s
void PushStrCode(halfword_t e, int s)
{
	halfword_t la;
	EA_t ea;
	switch(Mem[e].nh.op)
	{
	case opConst:
		CodeConst(e,1); // source address in r1
	copytostack:
		AppendN(2,
				0xe24d0f00+(s >> 2), // SUB r0,sp,s ; destination address
			    0xe1a0d000);  // mov sp,r0
		Subroutine(STRCOPY,0xeb000000);
		break;
	case opVar:
		EAVar(&ea,e);
		LEA(1,&ea);
		goto copytostack;
	case opCast:  // cast char to sting
		REx(0,Mem[e].ll.link); // char in r0
		AppendN(2,
				0xe24dDf00+(s >> 2), // SUB sp,sp,s ; destination address
				0xe50d0000);  // STR r0,[sp]
		break;
	case opAdd: // concatenation
		CheckStack();
		PushEx(Mem[e+1].hh.lo,btStr,3,s); // push first
		PushEx(Mem[e+1].hh.hi,btStr,3,s); // push second
		AppendN(2,
				0xe28d0f00+(s >> 2), // add r0,sp,s
				0xe1a0100d); // mov r1,sp
		Subroutine(STRCAT,0xeb000000); // StrCat
		Append(0xe28ddf00+(s >> 2)); // add sp,sp,s; pop second parameter
        break;
	case opPCall:
	case opPPCall:
	case opInline:
		la=ProcCall(e,3);
		if (la!=NADA)
		{
			EAFunc(&ea,la);
			LEA(1,&ea);
			AddImm(0,13,-s);
			Append(0xe1a0d000);  // mov sp,r0
			Subroutine(STRCOPY,0xeb000000);
		}
	}
}

void PushVar(halfword_t e, byte_t algn, int s)
{
	EA_t ea;
	halfword_t Regs;
	// set ea in r12
	EAVarAccess(&ea,e);
	if ((s<=16) && (algn=0))
	{	 // multiple load in registers R0..R4
		LEA(12,&ea); // load address in r12
		Regs=( 1 << ( (s+3) >> 2) ) - 1  ;
		Append(0xe89c0000+Regs);  // ldmia 12,{r0,...}
		PushRegs(Regs);
		ScratchRegs|=Regs;
	}
	else // use a routine
	{
		LEA(0,&ea);  // load address in r0
		MovImm(1,(s+3) & 0xfffffffc); // load word aligned size in r1
		Subroutine(PUSHDATA,0xeb000000); // the routine is dynamically dependent on alignement
	}
}

// push a constant
void PushConstSet(halfword_t e, int s)
{
	// may be improved by multiple load/store if size is small
	CodeConst(e,0); // address in r0
	MovImm(1,(s+3) & 0xfffffffc); // load word aligned size in r1
	Subroutine(PUSHDATA,0xeb000000); // the routine is dynamically dependent on alignement
}

// push the result of a set operation
// compute a op b on the stack
void PushSetOp(halfword_t nf, halfword_t e,  int s)
{
	PushEx(Mem[e+1].hh.lo, btSet,0,s);  // Push a, this is room for destination
	PushEx(Mem[e+1].hh.hi, btSet,0,s);  // push b
	AddImm(0,13,s);			// r0 = destination
	MovRdRn(1,0);           // r1 = @a
	MovRdRn(2,13);          // r2 = @b
	MovImm(3,s >> 2);      // r3 = word zize
	SystemCall(nf);
	AddImm(13,13,s);		// pop second parameter
}


// procedure to assign a set to a destination
// rd is a register that contain destination address
// e is a set variable expression
// s is the destination size

void MovSetVar(halfword_t rd, halfword_t e, int s)
{
	int s1;
	EA_t ea;
	s1=Mem[e].qqqq.b2; // source size
	PushRegs(1 << rd);
	EAVarAccess(&ea,e);
	LEA(1,&ea);   // source in r1
	PopRegs(1);  // destination in r0
	MovImm(2,s);
	Subroutine(MEMMOVE,0xeb000000);
	if (s>s1) // clear remaining data
	{
		MovImm(1,(s-s1) >> 2);
		SystemCall(CLEARMEM);
	}
}


// push  variable set
// the problem occurs when the destination size s
// is larger that the source, for example whine assigning
// a set of 0..10 to a set of 0..100
// the remaining data must be cleared
void PushSetVar(halfword_t e, int s)
{
	AddImm(13,13,-s);
	MovRdRn(0,13);
	MovSetVar(0,e,s);
}



// Set constructor to destination given by register rd
void SetSCons(halfword_t rd, halfword_t e, int s)
{
	halfword_t ld; // descriptor list
	halfword_t cc; // constant part
	halfword_t lo,hi; // intervall bounds
	cc=Mem[e+1].hh.lo;
	PushRegs(1 << rd);
	if (cc==NADA)
	{ // no constant part then clear memory
		MovRdRn(0,rd);
	    MovImm(1,s>>2); // ClearMem is word aligned
	    SystemCall(CLEARMEM);
	}
	else
	{ // copy constant part
		MovRdRn(0,rd);
	    CodeConst(cc,1);
	    MovImm(2,s);
	    Subroutine(MEMMOVE,0xeb000000);
	}
	ld=Mem[e+1].hh.hi;
	while (ld!=NADA)
	{   // call single or intervall
		lo=Mem[ld+1].hh.lo; // lower bound
	    hi=Mem[ld+1].hh.hi; // upper bound
	    if (lo==hi)
	    { // singleton
	    	REx(1,lo);
	    	MovImm(2,s); // bound
	    	Append(0xe51d0000); // ldr r0,[sp]
	    	SystemCall(SETSINGLE);
	    }
	    else
	    {  // intervall
	    	REx(2,hi);
	    	PushRegs(4);
	    	REx(1,lo);
	    	PopRegs(4);
	    	Append(0xe51d0000); // ldr r0,[sp]
	    	MovImm(3,s); // bound
	    	SystemCall(SETINTERVALL);
	    }
	    ld=Mem[ld].ll.link;
	}
	PopRegs(1 << rd);
}


// push a set
void PushSet(halfword_t e, int s)
{

	switch(Mem[e].nh.op)
	{
	case opConst:
		PushConstSet(e,s);
	    break;
	case opVar:
		PushSetVar(e,s);
		break;
	case opSCons:
		AddImm(13,13,-s); // make room in stack
		MovRdRn(0,13);    // destination register
		SetSCons(0,e,s);
		break;
	case opAdd:
		PushSetOp(SETADD,e,s);
		break;
	case opSub:
		PushSetOp(SETSUB,e,s);
		break;
	case opMul:
		PushSetOp(SETMUL,e,s);
		break;
	case opXor:
		PushSetOp(SETXOR,e,s);
		break;
	default:  //opPCall,opPPCall,opInline: ReportError(6,1000);
		ReportError(INTERNAL,"Push set");
	}
}

// push expression 'e' of size 's'
//--------------------------------
void PushEx(halfword_t e, BasicType_t bt, byte_t algn, int s)
{

	// s must be word aligned
	s=(s+3)&0xfffffffc;
	switch(bt)
	{
	case btReal:
		RealEx(0,e);  // evaluate e in r0, r1
		PushRegs(3);  // push those registers
		break;
	case btSet:
		PushSet(e,s);
		break;
	case btStr:
		PushStrCode(e,s);
		break;
	case btArray:
	case btRec:  // only var array or record should be pushed, other is a bad case
		if (Mem[e].nh.op==opVar)
		{
			PushVar(e,algn,s);
		}
		else  ReportError(INTERNAL,"Push array/rec"); // bad case for the moment only variables
		break;

	default:
		REx(0,e);    // load expression in r0
		PushRegs(1); // and push it
	}
}

///////////////////////////////////////////////////////////
// Evaluate expression e of basic type bt and size s
// in a destination given by the address pushed in stack
// this is used to evaluate non scalar expression
// as a const parameter
// this is used only for set and strings
///////////////////////////////////////////////////////////


// evaluate string in stack
void StackExStr(halfword_t e, int s)
{
	halfword_t la;
	EA_t ea;
	switch(Mem[e].nh.op)
	{
	case opCast:
		REx(0,Mem[e].ll.link); // load char in r0
		AppendN(2,
				0xe51dc000,	// ldr r12,[sp]
				0xe50c0000);	// str r0,[r12]
		break;
	case opAdd: // for the moment (may be improved)
		PushEx(e,btStr,3,s);     // push result
		LDSTRwi(0,13,s,True);   // destination address in r0
		Append(0xe1a0100d);      // source address in r1
		Subroutine(STRCOPY,0xeb000000); // StrCpy
		AddImm(13,13,s);        // free stack
		break;
	case opPCall:
	case opPPCall:
	case opInline:
		la=ProcCall(e,4);;
		if (la!=NADA)
		{
			EAFunc(&ea,la);
		    Append(0xe51d0000); // ldr r0,[sp]
		    LEA(1,&ea);
		    Subroutine(STRCOPY,0xeb000000);
		}
		break;
	  default:
		  ReportError(INTERNAL,"StackExStr");
	  }
}

/*
procedure StackEx_; // (e:UInt16;bt:BasicTypeEnum:s:integer);

procedure FuncCall(function f(e:UInt16;ff:boolean):UInt16);
var
  la : UInt16;
  ea : EAType;
begin
  la:=f(e,true);
  if la<>Nada then
  begin
    EAFunc(ea,la);
    AppendCode($e51d0000); // ldr r0,[sp]
    LEA(1,ea);
    Subroutine(StrCpy,$eb000000);
  end;
end;
*/

// Compute the result of set operation a op b
// nf is the routine number to call
void StackExSetOp(halfword_t nf, halfword_t e, int s)
{
	StackEx(Mem[e+1].hh.lo,btSet,s); // evaluate first parameter in destination
    PushEx(Mem[e+1].hh.hi,btSet,0,s); // push second parameter
    LDSTRwi(0,13,s,True);   // destination address
    MovRdRn(1,0);             // @ a
    MovRdRn(2,13);            // @ b
    MovImm(3,s >> 2);        // word size
    SystemCall(nf);       // call routine
    AddImm(13,13,s);          // free stack
}


// evaluate a set in stack
void StackExSet(halfword_t e, int s)
{
	switch(Mem[e].nh.op)
	{
	case opConst:
		CodeConst(e,1);          // const address in r1
		LDSTRwi(0,13,0,True);    // dest address in r0
		MovImm(2,s);             // dest size in r2, as const are canonical sized
		Subroutine(MEMMOVE,0xeb000000); // MemMove
		break;
	case opVar:
		LDSTRwi(0,13,0,True); // get address in r0
		MovSetVar(0,e,s);     // simply move set variable
		break;
	case opSCons:
		LDSTRwi(0,13,0,True);  // ldr r0,[sp]
		SetSCons(0,e,s);
		break;
	case opAdd:
		StackExSetOp(SETADD,e,s);
		break;
	case opSub:
		StackExSetOp(SETSUB,e,s);
		break;
	case opMul:
		StackExSetOp(SETMUL,e,s);
		break;
	case opXor:
		StackExSetOp(SETXOR,e,s);
		break;
	//case opPCall: case opPPCall: case opInline:
	default:
		ReportError(INTERNAL,"StackExSet");
	}
}


// compute an expression in the stack
// this is used when a procedure with const parameter is called with operation with the operands
// for example procedure foo(const:tset)... and called with foo(a+b)
// a+b is evaluated in the stack, and the address is passed to the procedure
void StackEx(halfword_t e, BasicType_t bt, int s)
{
	if (e!=NADA)
	{	// as this function is invoqued also for return value that may be ignored
		switch(bt)
		{
		case btStr:
			StackExStr(e,s);
			break;
		case btSet:
			StackExSet(e,s);
			break;
		default:
			ReportError(INTERNAL,"StackEx");
		}
	}
}


//////////////////////////////
// Statements
//////////////////////////////

// generates code to move s sized data from variable access o to d
void MoveData(halfword_t d, halfword_t o, int s)
{
	EA_t ea;
	// This calls the routine MemMove that is bytetwize
	// memory transfert
	// it may be improved with word aligned data of short size
	// by calling ldmia/stmia instructions
	EAVarAccess(&ea,d);
	LEA(0,&ea);        // dest address in r0
	PushRegs(1);        // push it
	EAVarAccess(&ea,o);
	LEA(1,&ea);          // src address in r0
	PopRegs(1);         // pop dest address to r0
	MovImm(2,s);        // load size to move to r2
	Subroutine(MEMMOVE,0xeb000000); // MemMove
}


// Assignement
//------------
void Assign(halfword_t n)
{
	BasicType_t bt=Mem[n].nh.bt;	// basic type
	halfword_t v=Mem[n+1].hh.lo; 	// variable
	halfword_t e=Mem[n+1].hh.hi;	// expression to assign
	int s = Mem[n+2].i;      		// size to transfert
	int srcalgn = s>>30;			// source alignement
	int dstalgn = (s>>28)&3;		// destination alignement
	s &=0x0fffffff;					// extract size data
	Op_t op=Mem[e].nh.op;  // operator of expression
	EA_t dest;
	EA_t src;
	switch(bt)
	{
	case btStr:
		switch (op)
		{
		case opConst:
	        EAVarAccess(&dest,v);
	        LEA(0,&dest);     // destination address in r0
	        CodeConst(e,1);   // source in r1
	        Subroutine(STRCOPY,0xeb000000);
	        return;
		case opVar:
	        EAVarAccess(&dest,v);
	        LEA(0,&dest);
	        PushRegs(1);     // push destination address
	        EAVar(&src,e);
	        LEA(1,&src);
	        PopRegs(1);      // pop dest address in r0
	        Subroutine(STRCOPY,0xeb000000);
	        return;
		case opCast:
	        EAVarAccess(&dest,v);
	        LEA(0,&dest);
	        PushRegs(1);
	        REx(0,Mem[e].ll.link);
	        PopRegs(2);  // pop dest address in r1
	        AppendN(3,
					0xe4e10000, // STRB r0,[r1] ; byte data transfert because of byte alignement
					0xe3a00000, // MOV r0,0
					0xe5c10001); // STRB r0,[r1,1]
	        return;
		}
		break;
	case btArray:
	case btRec:
		if (s!=0) MoveData(v,e,s);
		return;
	}

	PushEx(e,bt,srcalgn,s);
	EAVarAccess(&dest,v);
	PopEA(&dest,bt,dstalgn,s);
}


//-------------
// returns
int IfStmCode(halfword_t n)
{
	// expand node
	halfword_t c=Mem[n+1].hh.lo; // condition
	halfword_t t=Mem[n+1].hh.hi; // statement if true
	halfword_t f=Mem[n+2].hh.lo; // statement if false;
	int ll=0; // link
	int lt; // link then
	BEx(False,c,&ll);  // condition
	lt=StatementCode(t);
	if (f==NADA) Attach(&ll,lt);
	else
	{
		Append(0xea000000);  // B xxx
		Attach(&lt,CodePtr-1);
		BLink(CodePtr,ll);  // branch condition here
		Attach(&lt,StatementCode(f));  // else statement
		ll=lt;
	}
	return ll;
}

// while statement
//----------------
void WhileStmCode(halfword_t e,int*ll)
{
	// expand node
	halfword_t c=Mem[e+1].hh.lo;
	halfword_t s=Mem[e+1].hh.hi;
	int lnk;
	int cp = CodePtr;
	Append(0xea000000+*ll); // b
	StmCodeAndLink(s);
	BLink(CodePtr,cp); // b and previous links go ther
	lnk=0;
	BEx(True, c, &lnk);
	BLink(cp+1,lnk);
	*ll=0;
}


// repeat statement
// c : condition
// s : statement
void RepeatStmCode(halfword_t e)
{
	int lnk, x;
	halfword_t c=Mem[e+1].hh.lo;
	halfword_t s=Mem[e+1].hh.hi;
	lnk=0;
	x=CodePtr;
	StmCodeAndLink(s);
	BEx(False,c,&lnk);
	BLink(x,lnk);
}

// assign min and max of a list of constant
// returns number of elements in the list
// used by the CaseStmCode
static halfword_t MinMax(int*min, int*max, halfword_t l)
{
	halfword_t l1;
    halfword_t nb;
    int x,y;
    nb=0;
    *max=0x80000000;  // el flaco
    *min=0x7fffffff;  // el gordo
    do
    {
		x=Mem[l+1].i;
		l1=Mem[l].ll.link;
		if (l1!=NADA)
		{
			nb+=2;
			y=Mem[l1+1].i;
			if (x<=y)
			{
				if (x<*min) *min=x;
				if (y>*max) *max=y;
			}
			else
			{
				if (y<*min) *min=y;
				if (x>*max) *max=x;
			}
			l=Mem[l1].ll.link;
		}
		else
		{
			nb++;
			if (x<*min) *min=x;
			if (x>*max) *max=x;
			l=l1;
		}
    }
    while (l!=NADA);
    return nb;
}


// case statement
//---------------
static int CaseStmCode(halfword_t n)
{
	halfword_t e=Mem[n+1].hh.lo;  //e : expression
	halfword_t l=Mem[n+1].hh.hi;  //l : constant list
	halfword_t el=Mem[n+2].hh.lo;  //el: else statement if any
	int c1,c2;
	int min,max;
	halfword_t nb,s;
	halfword_t i;

	c1=0; // ultimate link

	REx(0,e); // evaluate expression in R0
	nb=MinMax(&min,&max,l); // count min,max and length of the list
	// decide branch table or successive comparisons
	// if at least 4 cases and all values occurs in the intervall
	// then, it is a branch table
	if ((nb>=4) && (max-min==nb-1))
	{	// branch table
		if (min!=0) AddImm(0,0,-min);
		CmpX(0,nb);
		AppendN(2,0x308ff100, // ADDCC PC,PC,R0,LSL
				  0xea000000); // b else
		c2=CodePtr;  // c2 is start of table
		// initialise all brancges
		for (i=0; i<nb;i++)  Append(0xea000000); // b case i
		// code for each case
		do
		{
			s=Mem[l].ll.info;
			BLink(CodePtr,c2+Mem[l+1].i-min);
			l=Mem[l].ll.link;
			// loop for each constant with this statement
			while ((l!=NADA) && (Mem[l].ll.info==s))
			{ // link branch table to here
				BLink(CodePtr,c2+Mem[l+1].i-min);
				l=Mem[l].ll.link;
			}
			Attach(&c1,StatementCode(s)); // statetment that branch to the end
			if ((l!=NADA)||(el!=NADA))
			{ // no branch on the last case
				Append(0Xea000000); // b to end
				Attach(&c1,CodePtr-1);
			}
		}
		while (l!=NADA);
		// else statement
		if (el!=NADA)
		{
			BLink(CodePtr,c2-1); // link else to here
			Attach(&c1,StatementCode(el));
		}
		else Attach(&c1,c2-1); // other cases without else
		return c1;
	}
	// successive comparisons
	do
	{
		s=Mem[l].ll.info;     // current statement
		CmpImm(0,Mem[l+1].i,0xe0000000);    // cmp R0,#ki
		l=Mem[l].ll.link;
		while ((l!=NADA) && (Mem[l].ll.info==s))
		{
			CmpImm(0,Mem[l+1].i,0x10000000);  // cmpne R0,#ki
			l=Mem[l].ll.link;
		}
		c2=CodePtr;
		Append(0x1a000000);      // bne to next comparison
		Attach(&c1,StatementCode(s));
		if ((l!=NADA) || (el!=NADA))
		{ // there is another statement either case or else
			Append(0xea000000+c1); // b to end
			c1=CodePtr-1;
			BLink (CodePtr,c2);   // solve bne
		}
		else Attach(&c1,c2);
	}
	while(l!=NADA);
	if (el!=NADA) Attach(&c1,StatementCode(el));
	return c1;
}


// for statement code
//----------------------

static int ForStmCode(halfword_t n)
{
	byte_t d=Mem[n].qqqq.b1; // d is direction d=0 : to ; d=1 : downto
	halfword_t v=Mem[n+1].hh.lo; // v : variable node of index variable
	halfword_t i=Mem[n+1].hh.hi; // i : initial expression
	halfword_t b=Mem[n+2].hh.lo; // b : bound exprerssion
	halfword_t s=Mem[n+2].hh.hi; // s : statement
	int bof=Mem[n+3].i;

	int lnk=0, l1,c1;
	word_t kb;
	int r;			// rotation if immediate fit in single instructin
	halfword_t rb;   // base register of bound variable
	EA_t ea;
	boolean_t ng;

	EAVar(&ea,v);
	if (Mem[b].nh.op==opConst)
	{  // bound is a constant
		kb=Mem[b+1].i;
		r=FitImm(&kb);   // check if bound fits in cmp instr
		ng=False;
		if (r<0)
		{
			kb=-Mem[b+1].i;
			r=FitImm(&kb); // check if it fits in a cmn instr
			if (r<0) goto nonconst; // if bound does not fit in a single
			ng=True;
		}
		// instruction, then treat as non constant bound
		if (Mem[i].nh.op==opConst)
		{
			if ((d==0) && (Mem[i+1].i>Mem[b+1].i)) goto terminate; // i>b
			if ((d==1) && (Mem[i+1].i<Mem[b+1].i)) goto terminate;
			REx(0,i); // load inital value in R0
		}
		else
		{
			REx(0,i); // load and compare
			Append(0xe3500000+(r << 8)+kb+(ng << 21)); // cmp/cmn R0,kb
			lnk=CodePtr;
			if (d==0) Append(0xca000000); else Append(0xba000000);
	//      Append($ca000000+(d shl 28));   // bgt / blt -->
		}
		l1=CodePtr;       // label
		STR(0,&ea);         // STR R0,index var
		StmCodeAndLink(s); // statement
		EAVar(&ea,v);   // re-init effective addres as it may be normalised but register
		  // r12 scratched by the statement code
		LDR(0,&ea);         // LDR R0,Index
		Append(0xe3500000+(r << 8)+kb+(ng << 21)); // CMP/CMN R0,kb
		if (d==0)  Append(0x12800001); else Append(0x12400001); // ADDNE/SUBNE R0,#1
		goto terminate;
	}
	else
	{  // non constant final bound -- additionnal variable on stack
	  // allocate offset for bound variable
	nonconst:
	    rb=11;
		//if (sLevel==1)  rb=10; else rb=11; // base register of bound variable
		REx(1,b);  // Compute bound in R1
		LDSTRwi(1,rb,bof,False); // STR R1,[R11,bound]
		REx(0,i);  // load initial value in R0
		Append(0xe1500001); // cmp R0,R1
		lnk=CodePtr;
		if (d==0) Append(0xca000000); else Append(0xba000000);
	//  Append($ca000000+(d shl 28));// bgt / blt -->
		l1=CodePtr;
		STR(0,&ea);              // STR R0,[Index]
		StmCodeAndLink(s);      // statement
		EAVar(&ea,v);          // ea may be normalised but register scratched by statement
		LDR(0,&ea);              // LDR R0,Index
		LDSTRwi(1,rb,bof,True); // LDR R1,[bound]
		Append(0xe1500001); // cmp R0,R1
		if (d==0) Append(0x12800001); else Append(0x12400001); // addne/subne R0,#1
	terminate:
		Append(0x1a000000); // BNE -->
		BLink(l1,CodePtr-1);
	}
	return lnk;
}

// with Statement
//---------------
// assign variable v to  offset offs
void WithStmCode(halfword_t v, int offs)
{
	EA_t ea;
	int rb;
	EAVar(&ea,v);
	LEA(0,&ea);
	rb=11; // base register
	//if (sLevel<=1) rb=10; else rb=11; // base register
	LDSTRwi(0,rb,offs,False);
}

// label node
// just assign code pr value to label node
static void LblStmCode(halfword_t n)
{
	Mem[n+2].i=CodePtr;
}


// goto statement
//---------------
// lnk = local link of branch to here
// globals goto restaure the frame pointer at the level of the function or procedure
// where the label is defined
// CAUTION, it does not restaure the stack ! This may lead to stack overflow if the
// function call occurs after the execution of a goto
static void GotoStmCode(halfword_t e, int lnk)
{

	byte_t  l=Mem[e].qqqq.b1; //  destination level
	halfword_t n=Mem[e+1].hh.lo; // label node

	if (l==sLevel)
	{   // goto in the current block
		Attach((int*)&Mem[n+1].i,lnk);
		Append(Mem[n+1].i+0xea000000);
		Mem[n+1].i=CodePtr-1;
	}
	else
	{
		// solves branches to here
		BLink(CodePtr,lnk);
		// generate frame restauration
		if (l==1)
		{ // goto to global section
			// the frame pointer has been stored in global -12 by OpenFunc if a global goto occurs
			LDSTRwi(11,10,-12,True); // ldr fp,[r10,-12] : restaure stack to global section
		}
		else
		{
			// all the frame pointers are passed as parameter to the function.
			LDSTRwi(11,11,PrmIndex+12-(l << 2),True);  // LDR r11,[r11,#offs_r11]
		}
		// Link this code position to the global link in Label dec node
		Subroutine(Mem[n].ll.info,Mem[n+3].i);
		Mem[n+3].i=CodePtr-1;
	}
}


// code for a new statement
// alloc memory chunk of size s to pointer variable v
static void MemAlloc(halfword_t e)
{
	EA_t ea;
	halfword_t v= Mem[e+1].hh.lo; // to variable v
	int s = Mem[e+2].i; // size to allocate
	MovImm(0,s); // load size to r0
	SystemCall(GETMEM);
	PushRegs(1); // push result
	EAVar(&ea,v);
	PopEA(&ea,btPtr,0,4);
}

// code for a dispose statement
// free pointer variable v
void FreeMem(halfword_t e)
{
	EA_t ea;
	halfword_t v=Mem[e+1].hh.lo; // variable to free
	EAVar(&ea,v);
	LDR(0,&ea);
	SystemCall(FREEMEM);
}


void  ReadProc(halfword_t n)
{
	EA_t ea;
	halfword_t f;	// file variable
	halfword_t le; 	// expression list
	halfword_t lla; // position to append file access node

	f=Mem[n+1].hh.lo; // file variable
	le=Mem[n+1].hh.hi; // expression list
	lla=Mem[n+2].hh.lo;
	EAVarAccess(&ea,f);
	LEA(0,&ea);
	PushRegs(1); // push file variable
	// prepare file access
	Mem[lla].ll.link=n+2; // add file access3415 node
	// change value for file access node
	Mem[n+2].hh.lo=2;
	while (le!=NADA)
	{ // loop on all expression in the list
		Assign(le);
		LDSTRwi(0,13,0,True);  // ldr r0,[sp]
		SystemCall(GET); // get(f)
		le=Mem[le].ll.link; // next expression
	}
	AddImm(13,13,4); // free stack from file variable
}

// read type bt from text file f to variable v
void ReadText(BasicType_t bt, halfword_t f, halfword_t v)
{

	EA_t ea;
	word_t nf;  // function number
    halfword_t r;   // temporary register
    boolean_t Short;

	Short=False;
	switch (bt)
	{ // check size of destination if integer type
	case btInt:
	case btUInt:
		if (Mem[Mem[v+1].hh.lo].qqqq.b2<4)
	    {
	      Short=True; // assignement of integer to a smaller location
	      AddImm(13,13,-4); // make room in stack for integer
	      MovRdRn(1,13);
	    }

	}
	if (!Short)
	{
		EAVar(&ea,v);
		LEA(1,&ea);
	}
	PushRegs(2);
	if (f==NADA) // output
	{
		SystemVar(&ea,INPUT_VAR);
	}
	else  EAVar(&ea,f);
	LEA(0,&ea);
	PopRegs(2);
	switch (bt)
	{
	case btChar:
		nf=READCHAR;
		break;
	case btStr:
		nf=READSTRING;
		break;
	case btInt:
	case btUInt:
		nf=READINTEGER;
		break;
	case btReal:
		nf=READREAL;
		break;
	default:
		ReportError(INTERNAL, "ReadProc");
	}
	SystemCall(nf);
	if (Short)
	{
		EAVar(&ea,v);  // get destination address
		r=GetRegEA(&ea); // get register
		PopRegs(1 << r);
		STR(r,&ea);
	}
}


void ReadLine(halfword_t f)
{
	EA_t ea;
	if (f==NADA)
	{    // output
		SystemVar(&ea,INPUT_VAR);
	}
	else  EAVar(&ea,f);
	LEA(0,&ea);
	SystemCall(FLUSHLINE);
}

// write(f,v) is equivalent to f^:=v; put(f)
void WriteProc(halfword_t n)
{
	EA_t ea;
	halfword_t f; // file variable access
	halfword_t le;
	halfword_t lla; // position to append file access node

	f=Mem[n+1].hh.lo;   // file variable access
	le=Mem[n+1].hh.hi;   // expression list
	lla=Mem[n+2].hh.lo;
	EAVarAccess(&ea,f);
	LEA(0,&ea); // load file variable in r0
	PushRegs(1); // push it
	// change value of access node
	Mem[n+2].ll.info=2;  // info value of the file access node
  	Mem[lla].ll.link=n+2; // append file access node
  	while (le!=NADA)
  	{
  		Assign(le);           // f^:=exp
  		LDSTRwi(0,13,0,True); // ldr r0,[sp]
  		SystemCall(PUT); // call put(f)
  		le=Mem[le].ll.link; // next expression
  	}
  	AddImm(13,13,4);  // free stack from file address
}

static int ByteDisplacement(halfword_t x)
{
	return (Mem[x+1].i+StartCode-CodePtr-2) << 2; // displacement
}

// asm opcode
// generate code while code generation process
//---------------------------------------------
static void AsmOp(halfword_t n)
{
	halfword_t x;
	int d,c;
	byte_t cc;

	cc=Mem[n].qqqq.b1; // defines operation to perform
	c=Mem[n+1].i;      // asm code
	switch(cc)
	{
	case 0:
		Append(c);  // brut code
		break;
	case 1: // local link
		x=c;
		d=Mem[x+1].i+StartCode-CodePtr-2; // word displacement
		c&=0xff000000;
		c|=(d & 0x00ffffff);
		Append(c);
		break;
	case  2:  // external call, code contains function number
	    Subroutine(c & 0xffffff,c & 0xff000000);
	    break;
	case 3:  // pc relative LDR/STR
		x=((c >> 4) & 0xf000) + (c & 0xfff);
		d=ByteDisplacement(x);
		if ((d>0xfff)||(d<-0xfff))
		{
			ReportError(BEYONGLIMIT);// PC relative offset beyong limit
		}
		if (d>=0) c|= (1 << 23); // sign
		else d=-d;
		Append((c & 0xfff0f000)+0xf0000+d);
		break;

   case 4:    // pc relative signed data transfert
        x=(c & 0x1f)+((c >> 2) & 0x3e0)+((c >> 6) & 0x3c00)+((c >> 11) & 0xc000);
        d=ByteDisplacement(x);
        //if abs(d)>$ff then ReportError(6,46); // PC relative offset beyong limit
        if (d>=0)  c|= (1 << 23);  else d=-d; // sign
        if (d>0xff) ReportError(BEYONGLIMIT);
        Append((c & 0xf1f0f060)+0xf0090+(d & 0xf)+((d << 4) & 0xf00));
        break;
    case 5: // PC relative offset for move instruction
       // mov r0,@label is replaced by add/sub r0,pc,#offset_label
        x=(c & 0xfff) + ((c >> 4) & 0x000);
        d=ByteDisplacement(x);
        c&=0xfe10f000; // erase opcode and displacement infos
        if (d>=0) c|=(4 << 21);
        else
        {
            c|= (2 << 21); // add or sub
            d=-d;
        }
        d=CheckFitOp2(d);
        Append(c+d+0x000f0000); // to PC
        break;
	case 6:  // pc relative LDC/STC
		x=((c >> 8) & 0xf00) + (c & 0xff) + ((c>>12)&0xf000);
		d=ByteDisplacement(x);
		if ((d>0xff)||(d<-0xff))
		{
			ReportError(BEYONGLIMIT);// PC relative offset beyong limit
		}
		if (d>=0) c|= (1 << 23); // sign
		else d=-d;
		Append((c & 0xfff0ff00)+0x0d0f0000+d);
		break;
	default:
		ReportError(NOTYETIMPLEM);
	}
}


//--------------------------------
// generate code for a statement list
// returns link infos to branch to
//--------------------------------
int StatementCode(halfword_t n)
{
	int lnk=0;
	Op_t op;
	CheckStack();
	while (n!=NADA)
	{
		op=Mem[n].nh.op;
		if ((op!=opGoto)&&(op!=opWhile))
		{
			BLink(CodePtr,lnk);
			lnk=0;
		}
		switch(op)
		{
		case opGoto:
			GotoStmCode(n,lnk);
			lnk=0;
			break;
		case opWhile:
			WhileStmCode(n,&lnk);
			break;
		case opNop:
			break;
		case opAssign:
			Assign(n);
			break;
		case opInline:
		case opPCall:
		case opPPCall:
			ProcCall(n,0);
			break;
		case opIf:
			lnk=IfStmCode(n);
			break;
		case opCase:
			lnk=CaseStmCode(n);
			break;
		case opRepeat:
			RepeatStmCode(n);
			break;
		case opFor:
			lnk=ForStmCode(n);
			break;
		case opFAssign:
			Assign(n);  // TODO is FAssign really necessary ?
			break;
		case opLabel:
			LblStmCode(Mem[n+1].hh.lo);
			break;
		case opWith:
			WithStmCode(Mem[n+1].hh.lo,Mem[n+2].i);
			break;
		case opNew:
			MemAlloc(n);
			break;
		case opDispose:
			FreeMem(n);
			break;
		case opAsm:
			AsmOp(n);
			break;
		case opNewLine :
			NewLine(Mem[n+1].hh.lo);
			break;
		case opWriteText :
			WriteText(n);
			break;
		case opReadln:
			ReadLine(Mem[n+1].hh.lo);
			break;
		case opReadText:
			ReadText(Mem[n].nh.bt,Mem[n+1].hh.lo,Mem[n+1].hh.hi);
			break;
		case opFile:
			FileProc(Mem[n+1].hh.lo,Mem[n+1].hh.hi);
			break;

		case opOpen:
			OpenProc(Mem[n].qqqq.b1, Mem[n+1].hh.lo,Mem[n+1].hh.hi,Mem[n+2].i);
			break;
		case opRead:
			ReadProc(n);
			break;
		case opWrite:
			WriteProc(n); //pMem^[n+1].hh.lo,pMem^[n+1].hh.hi);
			break;
		default:
			ReportError(INTERNAL, "Statement Code");
		}
		n=Mem[n].ll.link; // next statement
	}
	return lnk;
}




/*





procedure SetCmp(subset:boolean;order:boolean;eq:boolean);
var
  x,y : UInt16;
begin
  with pMem^[e+1] do
    if order then begin x:=hh.lo; y:=hh.hi end
    else begin x:=hh.hi; y:=hh.lo end;
  SetCompare(y,x,pMem^[e].qqqq.b2,subset);
  if not eq then AppendCode($e2200001); // eor r0,r0,1
  MovRdRn(rd,0);
end;





// the frame sprocedure loks like this
// +-----------------------+
// | Frame pointer level 2 | <-- FP + PrmSize + 4
// |-----------------------|
// | Frame pointer level 3 |
// |-----------------------|
// :                       :
// |-----------------------|
// |   last parameter      |
// !-----------------------|
// :                       :
// |-----------------------|
// |   first parameter     | <-- FP + 8
// |-----------------------|
// |  previous frame ptr   |
// |-----------------------|
// |   return address      |  <-- current frame ptr
// |-----------------------|
// |     locals            |
// :                       :
// :                       :  <-- SP
// +-----------------------+




// assign to ea the effective address of a pointer
// return function
// la is the attribute list
// assumes v node is a opPCall
procedure EAFunc(var ea:eaType;la:UInt16);
label 666;
begin
  if la=Nada then goto 666;
  case pMem^[la].ll.info of
    0: // index of a string
    begin // the function call has been forced in stack
      MovRdRn_(0,13);
      ea.bt:=btStr;
      ea.RegOffset:=-1;
      ea.offset:=0;
      ea.BaseReg:=0;
      ea.sz:=4;
      ea.shift:=0;
      AttrList_(la,ea);
      AddImm_(13,13,StringType.th.link);
    end;
    1: // pointer access
    begin
      ea.bt:=btPtr;
      ea.RegOffset:=-1;
      ea.offset:=0;
      ea.BaseReg:=0;
      ea.sz:=4;
      ea.shift:=0;
      AttrList_(pMem^[la].ll.link,ea);
    end;
    else 666:ReportError(100,101);
  end;
end;




/ read type bt from text file f to variable v
procedure ReadText(bt:BasicTypeEnum;f,v:UInt16);
var ea : eaType;
    nf : UInt16;  // function number
    r  : Int16;   // temporary register
    short : boolean;
begin
  short:=false;
  case bt of // check size of destination if integer type
    btInt,btUInt: if pMem^[pMem^[v+1].hh.lo].qqqq.b2<4 then
    begin
      short:=true; // assignement of integer to a smaller location
      AddImm(13,13,-4); // make room in stack for integer
      MovRdRn(1,13);
    end;
  end;
  if not short then
  begin
    EAVar(ea,v);
    LEA_(1,ea);
  end;
  PushRegs(2);
  if f=Nada then // output
  begin
    SystemVar(ea,-56);
  end
  else  EAVar(ea,f);
  LEA_(0,ea);
  PopRegs(2);
  case bt of
    btChar: nf:=LinkSize-69;
    btStr:  nf:=LinkSize-70;
    btInt,btUInt: nf:=LinkSize-72;
    btReal: nf:=LinkSize-73;
    else ReportError(6,103);
  end;
  Call(nf,$eb000000);
  if short then
  begin
    EAVar(ea,v);  // get destination address
    r:=GetRegEA(ea); // get register
    PopRegs_(1 shl r);
    STR(r,ea);
  end;
end;









*/

// array of executable
word_t* pExe;
int ExePtr;

// copy code of function number "f"
// set the index in exe in replacement of field (Subs / SProlog)
void CopyCode(halfword_t nf)
{
	int i;
	word_t f=CODESIZE-(word_t)nf; // index of function in Code array
	word_t a=Code[f].u; 		// start index in Code
	int    s=Code[f+1].i;       // size;
	word_t ph=a&0xf0000000;  	// pre header indicator
	a&=0x0fffffff;				// start code
	Code[f].i=-1; 				// mark this function as copied
	if (ph)
	{	// copy pre-header if any -- the pre heaser load address in r0
		pExe[ExePtr++]=0xe1a0000f;  // mov r0,pc
		pExe[ExePtr++]=0xe1a0f00e;	// mov pc,lr
	}
	Code[f+1].u=ExePtr;  // set exe index index in place of size
	// now copy the function
	for (i=0;i<s;i++)
	{
		pExe[ExePtr++]=Code[a++].u;
	}
}

// set relative branch to function f at code
// position x in pExe
void Displacement(word_t x, halfword_t nf)
{
	word_t c=pExe[x]; 		// code in the exe file
	word_t d=c&0x00ffffff;	// extract offset
	word_t f=CODESIZE-nf;
	c&=0xff000000; 			// extract opcode
	d+=Code[f+1].u-x-2;
	d&=0x00ffffff;
	pExe[x]=c+d;			// write code in exe
}

// link
void Link(halfword_t f);

// link subfunctions of function f
void LinkSubs(halfword_t fn)
{
	word_t a;   // index of the function in exe
	word_t f=CODESIZE-(word_t)fn;  // index of function in Code
	word_t sf;
	halfword_t l;
	int nbf;
	int i;
	CheckStack();  // because it is a recursive call

	a=Code[f+1].u;  // index in the function -- the function is already copied
	nbf=(int)Code[f+2].hh.lo;     // number of sub functions
	sf=CODESIZE-(word_t)Code[f+2].hh.hi; // index of subfunction records

	for (i=0;i<nbf;i++)
	{
		halfword_t fi=Code[sf-i].hh.lo;  // number of the subfunction
		halfword_t ofs=Code[sf-i].hh.hi; // offset where to link

		Link(fi);                        // link the sub function
		Displacement(a+(word_t)ofs,fi);  // set the displacement to function fi at adresse a+offset
	}
}


// definition of link procedure
void Link(halfword_t f)
{
	// check if already linked
	if (Code[CODESIZE-(word_t)f].i>=0)
	{
		CopyCode(f);
		LinkSubs(f);
	}
}




// the main link procedure
//
void do_link()
{
	// Mem is no longer necessary now, it us reused to build the executable file
	pExe=(word_t*)Mem;
	ExePtr=0;
	Link(BOOTSTRAP); // link the Boot Strap
}



