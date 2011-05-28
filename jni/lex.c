#include <stdio.h>
#include <stdarg.h>
#include "pp.h"


// input stack

int iStackPtr;
StackEntry_t inputStack[INPUTSTACKSIZE];
int eofAllowed;

void PushInput()
{
	char*pname;
	if (iStackPtr>=INPUTSTACKSIZE-1) ReportError(TOOMUCHFILES);
	pname=Pool+IdPtr; // get string name
	pname[LenID]=0; // force terminal zero to get a regular string
	inputStack[iStackPtr].RefNum=aopenin(pname);
	inputStack[iStackPtr].NamePtr=IdPtr;
	inputStack[iStackPtr].StartPtr=LoPoolMax;
	inputStack[iStackPtr].SavePtr=TkPtr;
	inputStack[iStackPtr].AbsPos=0;
//	inputStack[iStackPtr].LineNum=0;
	TkPtr=LoPoolMax;
	iStackPtr++;
}

// called at the end of a file to pop to the previous input file
void PopInput()
{
	iStackPtr--;
	LoPoolMax=inputStack[iStackPtr].StartPtr;
	TkPtr=inputStack[iStackPtr].SavePtr;
	aclose(inputStack[iStackPtr].RefNum);
}
//-----------------------------------------------------------------

// Lexical analysis
//=========================


// Utilities for lexical analysis
//++++++++++++++++++++++++++
// Basic character routines
//++++++++++++++++++++++++++

char LowCase(char c)
{
	if ((c>='A') && (c<='Z')) return c+'a'-'A';
	return c;
}

char CurrLocaseChar()
{
    return LowCase(Pool[TkPtr]);
}

static int IsLetter(char c)
{
	return  ( ((c>='a') && (c<='z'))  ||  ((c>='A') && (c<='Z')) ) ;
}

int IsIDChar(char c)
{
	return  ( ((c>='a') && (c<='z'))  ||  ((c>='A') && (c<='Z')) ||
			   ((c>='0') && (c<='9')) || (c=='_')   );
}

static int IsFileChar(char c)
{
	return  ( ((c>='a') && (c<='z'))  ||  ((c>='A') && (c<='Z')) ||
			   ((c>='0') && (c<='9')) || (c=='_') ||(c=='.') || (c=='/')  );
}

static int IsDigit(char c)
{
	return   ((c>='0') && (c<='9'));
}

// returns the hexadecimal value of c, or -1 if it is not a hexa char
static int HexaDigit(char c)
{
	c=LowCase(c);
	if ((c>='0') && (c<='9')) return c-'0';
	if ((c>='a') && (c<='f')) return c-'a'+10;
	return -1; // if it is not a hexa char
}

InputState_t InputState;

int IdPtr;  // position of the start of an identifier
int LenID;  // length of the identifier
int TkPtr;  // current position to read in the input buffer (low part of Pool)


int NestedIf; // current index in CondStack
InputState_t CondStack[NBCOND]; // memorise return InputState
// the defines table contains the strings of the defined symbols and their status
// the field "d" says if the symbol is defined (d=1) or undefined (d=0)
DefineEntry_t DefinesTable[NBDEFINES];
int DefinesPtr; // max index in defines table

 //+++++++++++++++++++++++++++++++++++++++
// Directive
//+++++++++++++++++++++++++++++++++++++++

// returns next non blank char in line
static char SkipBlanks()
{
	char c=Pool[TkPtr];
	// skip blanks
	while (c==' ')
	{
		c=Pool[++TkPtr];
	}
	if (c<' ') ReportError(UNEXPECTEDEOL);
	return c;
}


// skip all char until end of directive
static void SkipToEndOfDir(halfword_t e)
{
	char c;

	c=Pool[TkPtr];
	while (c>=' ')
	{
		if (c==(e & 255))
		{  // check current char matches
			// check next if necessary
			if (e<256)  return; // not necessary
			c=Pool[++TkPtr];
			if (c==(e >> 8)) return;
		}
		else
		{
			c=Pool[++TkPtr];
		}
	}
}


// check if char match to e e=='}' or '*'
static void CheckDirChar(char e)
{
	SkipBlanks();
	if (e!=Pool[TkPtr]) ReportError(EXPECTEDCHAR,e);
	if (e=='}') return;
	if (Pool[++TkPtr]!=')') ReportError(EXPECTEDCHAR,')');
}



// scan for a directive identifier
static void ScanDirID(char e)
{
	char c;
	c=SkipBlanks();
	if (!IsLetter(c)) ReportError(UNEXPECTEDCHAR,c);
	IdPtr=TkPtr;
	do
	{
		c=Pool[++TkPtr];
	}
	while (IsIDChar(c));
	LenID=TkPtr-IdPtr;
	CheckDirChar(e);
}


// scan for a directive identifier
static void ScanFileName(char e)
{
	char c;
	c=SkipBlanks();
	if (!IsFileChar(c)) ReportError(UNEXPECTEDCHAR,c);
	IdPtr=TkPtr;
	do
	{
		c=Pool[++TkPtr];
	}
	while (IsFileChar(c));
	LenID=TkPtr-IdPtr;
	CheckDirChar(e);
}

static halfword_t NewString(); // return a new string with the current identifier
static boolean_t MatchString(halfword_t s); // check if string s matches with the current identifier


// look up in define table and returns index in table
// or -1 if not found
static int LookUpDefine()
{

	int i;
	for (i=0;i<DefinesPtr;i++)
	{
		if (MatchString(DefinesTable[i].s)) return i;
	}
	return -1;
}

// define (b=true) or undefine (b=false) the
// current identifier
static void  DoDefine(boolean_t d)
{
	int i;
	i=LookUpDefine();
	if (i<0)
	{	 // if not exists then create entry
		if (DefinesPtr>=NBDEFINES) ReportError(TOOMUCHDEFINES); //(TooMuchDefines);
		DefinesTable[DefinesPtr].s=NewString();
		i=DefinesPtr;
		DefinesPtr++;
	}
	// set defined bit
	DefinesTable[i].d=d;
}

// returns true iff current identifier is defined
static boolean_t IsDefined()
{
	int i;
	i=LookUpDefine();
	if (i<0) return False;  // undefined directive symbol
	return DefinesTable[i].d;  // may be a defined symbol, but set to undef
}

//-------------------------------
// data structure for directives
//-------------------------------
// number of directives
#define NBDIR 8
// pool of directive names
const char DirPool[]=
		"define"
		"undef"
		"ifdef"
		"ifndef"
		"else"
		"endif"
		"i"
		"echo"
		//"maxstring"
		;

// starting index of each directiveb in pool
const int  DirStart [NBDIR+1]={0,6,11,16,22,26,31,32,36}; //,41};

// scan directive name and return directive number
// returns -1 if unknown directive
int ScanDirective(char e)
{
	int i,j,k,l;
	char c;

	TkPtr++; // pass '$'
	IdPtr=TkPtr;
	// scan character of directive name
	c=Pool[TkPtr];
	while ((c>' ') && (c!=e))
	{
		Pool[TkPtr++]=LowCase(c);
		c=Pool[TkPtr];
	}
	LenID=TkPtr-IdPtr;
	// search the directive name
	for (i=0;i<NBDIR;i++)
	{
		k=DirStart[i];
		l=DirStart[i+1];
		if ((l-k)!=LenID) continue;
		for (j=0;j<LenID;j++)
		{
			if (Pool[IdPtr+j]!=DirPool[k+j]) break;
		}
		if (j==LenID) return i;
	}
	ReportError(UNKNOWNDIR);
}


// parse and interpret compilation directive
// e='}' or '*' for directives that ends with '*)'
void Directive(char c1)
{
	word_t n;
	char c;
	int x=ScanDirective(c1);
	if (((x<=1)||(x>=6)) && (InputState>=SkipElse))
	{
		SkipToEndOfDir(c1);
	}
	else switch(x)
	{
	case -1:
		ReportError(UNKNOWNDIR);
	case 0: case 1: // define, undef
		ScanDirID(c1);
		DoDefine(x==0);
		break;
	case 2: case 3: // ifdef ifndef
		ScanDirID(c1);
		if (NestedIf>=NBCOND) ReportError(TOOMUCHCOND); // too much conditionnals.
		CondStack[NestedIf++]=InputState;
		if (InputState>=SkipElse)  InputState=SkipAlways;
		else
		{
			if ((x==2)^IsDefined()) InputState=SkipIf;
			else InputState=ScanIf;
		}
		break;
	case 4: // else
		CheckDirChar(c1);
	    if (InputState==ScanIf) InputState=SkipElse;
	    else if (InputState==SkipIf) InputState=ScanElse;
	    else if (InputState!=SkipAlways) ReportError(MISPLACEDELSE); // misplaced else
	    break;
	case 5: // endif
		CheckDirChar(c1);
		if (NestedIf==0) ReportError(MISPLACEDENDIF); // misplaced endif
		NestedIf--;
		InputState=CondStack[NestedIf];
	    break;
	case 6: // i (include)
		ScanFileName(c1);
		CheckDirChar(c1);
		PushInput();
	    break;
	case 7: // echo
		c=SkipBlanks();
		while (c!=c1)
		{
			if (c<' ') ReportError(UNEXPECTEDEOL);
			emitChar(c);
			c=Pool[++TkPtr];
		}
		CheckDirChar(c1);
		break;
	default:
		ReportError(NOTYETIMPLEM);
	}
}





// Set TkPtr to the next token position
// interprets comments and directives
// allows nested comment of the same category
//    - brace style { }
//    - parent style (*   *)
//---------------------------------------------
void NextToken()
{
	char c, cc;
    enum {NOCOMMENT, BRACECOMMENT, PARENTCOMMENT} cs; // comment state
    int l; // level of nested comments

	cs=NOCOMMENT;
	for(;;)
	{
		while (TkPtr<LoPoolMax)
		{
			c=Pool[TkPtr];
			switch (cs)
			{
			case NOCOMMENT:
				switch(c)
				{
				case '{':
					if (Pool[TkPtr+1]=='$') { TkPtr++; Directive('}'); } // check directive
					else if (InputState<=ScanElse)
					{
						cs=BRACECOMMENT;
						l=1;
					}
					break;
				case '(':
					// check next char for (*
					if (Pool[TkPtr+1]!='*') if(InputState<=ScanElse) return;
					TkPtr++;
					if (Pool[TkPtr+1]=='$')
					{
						TkPtr++;
						Directive('*'); //,')');
					}
					if (InputState<=ScanElse)
					{
						cs=PARENTCOMMENT;
						l=1;
					}
					break;
				case '/': // C++ style comment
					if (InputState<=ScanElse)
					{
						if (Pool[TkPtr+1]!='/') return;  // regular character
						TkPtr=LoPoolMax;   // skip until the end of line
					}
					break;
				default:
					if (c>' ') if (InputState<=ScanElse) return; // non space type character
				}
				break;
			case BRACECOMMENT:
				if (c=='}')
				{
					if (--l==0) cs=NOCOMMENT;
				}
				else if (c=='{') l++;
				break;
			case PARENTCOMMENT:
				if (c=='*')
				{
					TkPtr++;
					if (Pool[TkPtr]==')')
					{
						if (--l==0) cs=NOCOMMENT;
					}
				}
				else if (c=='(')
				{
					TkPtr++;
					if (Pool[TkPtr]=='*') l++;
				}
				break;
			}
			TkPtr++;  // continue to next char
		}
		// a new line needs to be read
		if (aeof(inputStack[iStackPtr-1].RefNum)!=0) // check end of file
		{
			if (eofAllowed==False) ReportError(UNEXPECTEDEOF); // check if it is allowed
			if (iStackPtr==0)
			{   // if top level
				return;
			}
			else
			{   // if it is not top level, then pop the input
				PopInput(); // return to previous file and scan again
			}
		}
		LoPoolMax=inputStack[iStackPtr-1].StartPtr; // return to current start of line
		TkPtr=LoPoolMax;                            // set current token ptr to begining of line
		// get a new line
		char c;
		do
		{
			int eof=agetchar(inputStack[iStackPtr-1].RefNum,&c);
			if (HiPoolMin<=LoPoolMax) ReportError(OUTOFMEMORY); // check room
			Pool[LoPoolMax++]=c;  // save the char
			if (eof!=0) break;   // do not increment absolute position on eof to stay in range
			inputStack[iStackPtr-1].AbsPos++;  // increment absolute position
		}
		while( (c!='\n')  );  // until end of line
		nb_lines++; // increment number of lines
	}
}


// Key words

//+++++++++++++++++++++++++++++++++++++++++
// Data for Keyword recognition
//+++++++++++++++++++++++++++++++++++++++++



// keywords are sorted by length
// for a given length, they are sorted in lexicographic order
// for make faster the test if a given text of a given length
// is a keyword or not


// keyword pool
// the sequences of charaters in keywords by lenth and
// in lexicographic order for keyword of a given length
char KWPool[]=
  "do" "if" "in"  "of" "or" "to"
  "and" "asm" "div" "end" "for" "mod" "nil" "not" "set" "shl" "shr" "var" "xor"
  "case" "else" "file" "goto" "then" "type" "with"
  "array" "begin" "const" "label" "until" "while"
  "downto" "packed" "record" "repeat" "string"
  "program" "function" "procedure"
  "forward" "inline";


  // index of the start of the keyword text in the KWPool
   int KWStart[NBKEYWORDS] =
  { 0,2,4,6,8,10,
    12,15,18,21,24,27,30,33,36,39,42,
    45,48,51,55,59,63,67,71,75,79,
    84,89,94,99,104,109,
    115,121,127,133,139,
    146,
    154,
    163,
    170,
    176
  };

   KWEnum_t KWByLen[9]=
   {
     kDO,   // first keyword of length 2
     kAND,  // first keyword of length 3
     kCASE, // and so on
     kARRAY,
     kDOWNTO,
     kPROGRAM,
     kFUNCTION,
     kPROCEDURE,
     kFORWARD  // first directive
   };


// test if the current input mathces with keyword kw
// if yes, returns the position of the following char
// if not, returns the current TkPtr
// Does not change TkPtr
int TestKW(KWEnum_t kw)
{
	int i=KWStart[kw];
	int j=KWStart[kw+1];
	int k=TkPtr;
	while (i<j)
	{
		char c=Pool[k];
		if (LowCase(c)!=KWPool[i]) return TkPtr;
		i++;
		k++;
	}
	if (! IsIDChar(Pool[k])) return k;
	return TkPtr;
}

// returns True if the currend symbol (IdPtr / LenID) is a key word
// this routine is called to check if a given identifier is not a keyword
static int IsKW()
{
	int k;
	char c1,c2;
	if (LenID<2) return False;
	if (LenID>=10) return False;
	for(k=KWByLen[LenID-2];k<KWByLen[LenID-1];k++)
	{ // Scan all keyords of length LenID
		c1=LowCase(Pool[IdPtr]);
		c2=KWPool[KWStart[k]];
		if (c1<c2) return False;  // becauses they are sorted
		if (c1>c2) continue;      // following.
		if (TestKW(k)!=TkPtr) return True;  // if first letter match,
	}
	return False;
}

//returns true iff keyword kw in in current position
// if so, flush it from input by incrementing TkPtr
int ScanKW(KWEnum_t kw)
{
	int k=TestKW(kw);
	if (k!=TkPtr)
	{
		TkPtr=k;
		NextToken();
		return 1;
	}
	return 0;
}

// check if kyword kw in in input stream
// if not, report the error
void CheckKW(KWEnum_t kw)
{
	if (!ScanKW(kw)) ReportError(EXPECTEDKW,kw);
}

// returns true iff input stream has current character c
int ScanChar(char c)
{
	if (Pool[TkPtr]==c)
	{
		TkPtr++;
		NextToken();
		return True;
	}
	return False;
}

void CheckChar(char c)
{
	if (ScanChar(c)==0) ReportError(EXPECTEDCHAR,c);
}

// returns true iff input stream has two char c1 and c2
int ScanDuo(char c1, char c2)
{
	if ((Pool[TkPtr]==c1)&&(Pool[TkPtr+1]==c2))
	{
		TkPtr+=2;
		NextToken();
		return True;
	}
	return False;
}

int CheckDuo(char c1, char c2)
{
	if (ScanDuo(c1,c2)==0) ReportError(EXPECTEDCHAR,c1+(c2<<8));
}

// Same as scanduo but do not flush the token from tbe
// input stream
// returns true iff input stream has two char c1 and c2
int TestDuo(char c1, char c2)
{
	if ((Pool[TkPtr]==c1)&&(Pool[TkPtr+1]==c2))
	{
		return True;
	}
	return False;
}


// scan program name following the first keyword "program"
// check that it is a non empty identifier
// generate the name of the exe file
// this is the only case sensitive part of the parser
void ScanProgName()
{
	char c=Pool[TkPtr];
	int i;
	IdPtr=TkPtr;   // set starting position of program name
	if (IsLetter(c)==0) ReportError(IDENTEXPECTED);  // Check it starts with a letter
	TkPtr++;
	c=Pool[TkPtr];
	while (IsIDChar(c)!=0)  // read until another char type
	{
		c=Pool[++TkPtr];
	}
	LenID=TkPtr-IdPtr;     // set identifier length
	NextToken();
	// now initialize the exe name
	if (LenID>59) LenID=59;  // limit length to 64 bytes
	for (i=0;i<LenID;i++)
	{
		exe_name[i]=Pool[IdPtr+i]; // move name
	}
	// ajout de ".exe"
	exe_name[i++]='.';
	exe_name[i++]='e';
	exe_name[i++]='x';
	exe_name[i++]='e';
	exe_name[i]='\0';  // zero terminal
}


//--------------------------------
// Strings
//--------------------------------

// single character strings are simply denoted by the ascii value of the character
//
// the characters of the multi char strings are stored in the high part of the Pool
// a string is represented by an index in the StrStart array
// This array gives the index in the pool of the first character of the string
// For a string s>=, the value StrStart[s-MAXCHAR+1] gives the index of the first char in the pool
//  Pool ...xxxxxxxxxxxxxxx]xxxxxxxxxxxxxxxxxxxxxxxxxxx]xxxxxxxxxxxxxxx]
//    ...        StrStart[2]^                StrStart[1]^    StrStart[0]^
//                 s=257                        s=256       not assigned
// MAXCHAR = 256


halfword_t StrPtr;  // denotes the first free string to allocate
halfword_t StrStart[STRMAX];  // the array as explained above

// a string value is either the ascii value of a single character string
// or 255 + index in the StrStart array
// this array contains the start index of the chars in the pool

// returns a new string index with the current identifier givet by IDPtr / LenID
static halfword_t NewString()
{
	halfword_t result,i;
	if (LenID==1) return Pool[IdPtr];  // single char string
	result=StrPtr+MAXCHAR-1; // the result will be this value
	if (StrPtr>=STRMAX) ReportError(OUTOFMEMORY); // to much strings
	// Allocate room for characters
	HiPoolMin-=LenID;
	// check memory overflow
	if (HiPoolMin<LoPoolMax) ReportError(OUTOFMEMORY); // no room in pool
	StrStart[StrPtr]=HiPoolMin;
	StrPtr++;  // increment for the next time
	// copy characters
	for(i=0;i<LenID;i++) Pool[HiPoolMin+i]=Pool[IdPtr+i];
	return result;
}


// Initialize the string stuff
void InitStrings()
{
	  // string initialisations
	  StrPtr=1; // first free string number
	  StrStart[0]=POOL_SIZE; // end of first string number
}

// returns the length of string s
int StringLen(halfword_t s)
{
	if (s<MAXCHAR) return 1; // single character string
	return StrStart[s-MAXCHAR]-StrStart[s-MAXCHAR+1]; // length in bytes
}

// returns True if the string s mathches with the currend ID
static boolean_t MatchString(halfword_t s)
{
	halfword_t i,j;
	if (s<MAXCHAR) return (LenID==1)&&(Pool[IdPtr]==s);
	j=StrStart[s-MAXCHAR+1];  // a string number equal to MAXCHAR means a str ptr equal to 1
	// check length
	if (LenID!=StrStart[s-MAXCHAR]-j) return False;
	// check characters
	for (i=0;i<LenID;i++)
	{
		if (Pool[IdPtr+i]!=Pool[j+i]) return False;
	}
	return True;
}

// string concatenation
// return a new string which is the concatenation of string a and string b
halfword_t ConcatStr(halfword_t sa, halfword_t sb)
{
	int lb,la; // length of the strings
	int i;
	halfword_t result;
	if (sa==0) return sb;  // empty string is neutral element
	if (sb==0) return sa;
	lb=StringLen(sb); // length of sb
	la=StringLen(sa); // length of sa
	// check string number
	if (StrPtr>=STRMAX) ReportError(OUTOFMEMORY); // to much strings
	// allocate room for characters
	HiPoolMin-=la+lb;
	if (HiPoolMin<LoPoolMax) ReportError(OUTOFMEMORY); // no room in pool
	StrStart[StrPtr]=HiPoolMin;
	result=StrPtr+MAXCHAR-1;  // value of currently constructed string
	StrPtr++;  // increment string number
	// copy characters
	if (sa<MAXCHAR) Pool[HiPoolMin]=sa;
	else for (i=0;i<la;i++) Pool[HiPoolMin+i]=Pool[StrStart[sa-MAXCHAR+1]+i];
	if (sb<MAXCHAR) Pool[HiPoolMin+la]=sb;
	else for (i=0;i<lb;i++) Pool[HiPoolMin+la+i]=Pool[StrStart[sb-MAXCHAR+1]+i];
	// assign new string value to node a
	return result;
}

//--------------------------------
// the hash table
//--------------------------------


// a symbol value is represented by an entry in the hash table
// The value HSIZE means "no entry"

HEntry_t HTable[HSIZE];

halfword_t HUsed;  // initialised to HSIZE

void InitHTable()
{
	halfword_t i;
	// HTable initialisation
	HUsed=HSIZE;
	for (i=0;i<HSIZE;i++)
	{
	    HTable[i].link=0;
	    HTable[i].text=0;
	    HTable[i].symb.s=sUNDEF;
	    HTable[i].symb.val=NADA;
	}
}


// this function returns the entry in the hash table of the current identifier
// defined by IdPtr and LenID
halfword_t LookUp()
{
	halfword_t j,
	  p,  // the Hash Key
	  s;  // text of the symbol, represented by an entry in the string table
	// compute the hash Key
	p=0;
	for (j=0;j<LenID;j++)
	{
		p+=(p+Pool[IdPtr+j]);
		p%=HPRIME;  //
	}
	for(;;)
	{
		s=HTable[p].text;
		if (s==0)
		{   // if no symbol with this key has this text, create a new string
		found:
			HTable[p].text=NewString();
			HTable[p].symb.s=sUNDEF;
			HTable[p].symb.val=NADA;
			return p;
		}
		else
		{	// there exists a symbol with key p
			if (s<MAXCHAR)
			{	// This is a single character string
				if ((LenID==1)&&(Pool[IdPtr]==s)) return p; // length and character match
			}
			else
			{	// string with several characters
				if (MatchString(s)) return p;
			}
		}
		// here, an entry on p exists but does not match with the symbol
		if (HTable[p].link==0)
		{	// searche for a free entry in the table to link here
			do
			{
				HUsed--;
				if (HUsed==0) ReportError(OUTOFMEMORY);  // Hash Table Full

			}
			while(HTable[HUsed].text!=0);
			HTable[p].link=HUsed;
			p=HUsed;
			goto found;
		}
		// search on the next linked symbol
		p=HTable[p].link;
	}
}

// identifiers
///-----------
// return the identifier value
// given by index in hash table
// or HSIZE if the input stream is not an identifier
halfword_t ScanID()
{
	halfword_t k,r;
	char c;

	IdPtr=TkPtr;
	k=TkPtr;
	c=Pool[k];
	if (IsLetter(c))
	{
		do
		{	// scan the entry while allowed identifier char
			Pool[k]=LowCase(c);  // replace char by low case in Pool
			c=Pool[++k];         // next char
		}
		while (IsIDChar(c));
		LenID=k-TkPtr;  // set the length of the identifier
		if (IsKW()) return HSIZE;  // a key word is not an allowed identifier
		TkPtr=k;         // flush it from input stream
		r=LookUp();      // because NextToken() may change IdPtr and LenID
		NextToken();
		return r;
	}
	return HSIZE;
}


// Check that the current token is an identifier
halfword_t CheckID()
{
	halfword_t x=ScanID();
	if (x==HSIZE) ReportError(IDENTEXPECTED);
	return x;
}


// Push values of a symbol
// such as these values are restaured at the end
// of the current bloc
void PushSymbol(halfword_t x)
{
	word_t top;
	Room(2);
	top=Mem[HiMemMin].u;  // bottom of the bloc
	HiMemMin-=2;          // allocate
	Mem[HiMemMin+1].sy=HTable[x].symb;  // save symbol
	Mem[HiMemMin+2].hh.lo=x;  // save the value
	Mem[HiMemMin].u=top;      // save bottom on the top of the stack
}


// check if the identifier given by x is durrently allowed as a new identifier
static boolean_t NewIDAllowed(halfword_t x,IDEnum_t idt)
{
	if (HTable[x].symb.s==sUNDEF) return True; // the symbol is not yet defined
	if (sLevel>1)
	{ 	// Current function identifier is defined on the previous level
		if (x==CurFID) return False;
	}
	if (HTable[x].symb.l<sLevel) return True;  // identifier of a previouiuos bloc
	if (sLevel==1)
	{	// program parameters accepted as globak variable in main bloc
		if ((idt==sVAR) && (HTable[x].symb.s==sPARAM) ) return True;
	}
	return False;
}

// new identifier
// x is index in hash table of identifier
// check that this is a new identifier in the current bloc
// and push previous informations
void NewID(halfword_t x,IDEnum_t idt)
{
	if (! NewIDAllowed(x,idt)) ReportError(DUPLICATEID,x);
	if (sLevel>1) PushSymbol(x);
	HTable[x].symb.s=idt;
	HTable[x].symb.l=sLevel;
}



halfword_t IDVal;      // current symbol value set by GetXID
unsigned char IDLevel; // curent symbol level set by GetXID

// check for existing identifier
// returns id type of identifier
// x is valid index in Hash table
// Initialise global IdVal to current symbol value and IDLevel to current symbol level
IDEnum_t GetXID(halfword_t x)
{
	HEntry_t* h=HTable+x;
	IDEnum_t idt=h->symb.s;
	IDLevel=h->symb.l;
	IDVal=h->symb.val;
	if (idt==sUNDEF) ReportError(UNKNOWNID,x);
	return idt;
}


//Predefined symbol initialistion
// s = type of the symbol
// val = value of the symbol
// returns the symbol identifier
//---------------------------------------
halfword_t NewSymbol(char*name,IDEnum_t s,halfword_t val)
{
	char c;
	IdPtr=0;
	LenID=0;
	do
	{
		c=*name++;
		Pool[LenID++]=c;
	}
	while(c!=0);
	LenID--;  // flush the final zero
	halfword_t x=LookUp();
	HEntry_t* h=HTable+x;
	h->symb.s=(byte_t)s;
	h->symb.l=0;
	h->symb.val=val;
	return x;
}

// define a new constant of a giver name, type (t) and value (v)
// e.g. NewConst("true", TBOOLEAN, 1)
void NewConst(char*name, word_t t, word_t v)
{
	halfword_t e = GetAvail(2);
	Mem[e].u=t;
	Mem[e+1].u=v;
	NewSymbol(name,sCONST,e);
}


// define a new type with name and type t
// and returns the node
halfword_t NewType(char*name, word_t t)
{
	halfword_t e=GetAvail(1);
	Mem[e].u=t;
	NewSymbol(name,sTYPE,e);
	return e;
}

// TODO : factorize the nodes generation of the two following NewFunc1 and NewInline1

// defines a new function which calls the routnie number nf
// t is a node to the parameter type
// tr is a node to the return type
void NewFunc1(char*name, halfword_t t, halfword_t tr, halfword_t nf)
{
	halfword_t fpl = GetAvail(2);
	halfword_t e= GetAvail(2);
	// formal parameter list
	Mem[fpl].ll.info=t==StringTypeNode?3:0;		// value parameter except for string parameter (length)
	Mem[fpl].ll.link=NADA; 	// only one parameter
	Mem[fpl+1].hh.lo=t;		// type of the parameter

	Mem[e].hh.lo=fpl;   // formal parameter list
	Mem[e+1].hh.lo=nf;  // function number
	Mem[e].hh.hi=tr;    // return type
	Mem[e+1].hh.hi=NADA;

	NewSymbol(name,tr==NADA?sPROC:sFUNC,e); // procedure if return type is NADA, function elsewhere

}

// defines a new variable of type ty at offset o
// returns the identifier to this new var
// the only predefined variable in ISO Pascal are "input" and "output"
halfword_t NewVar(char*name,word_t ty, int o)
{
	halfword_t n=GetAvail(3);
	Mem[n].i=0x00000100; // level 1
	Mem[n+1].i=ty;
	Mem[n+2].i=o;
	return NewSymbol(name, sVAR, n);
}


// defines a new inline function
// name is the name :)
// t is the parameter type node
// tr is the return type node
// nf is a negative function number starting at -1
// nb is the size of the code
// and follows the codes
void NewInline1(char*name, halfword_t t, halfword_t tr, short signed int nf, int nb, ...)
{
	va_list vl;
	int i;
	halfword_t e=GetAvail(2);
	halfword_t fpl;

	if (t!=NADA)
	{
        fpl=GetAvail(2); // formal parameter list

        // formal parameter list
        Mem[fpl].ll.info=0;		// value parameter
        Mem[fpl].ll.link=NADA; 	// only one parameter
        Mem[fpl+1].hh.lo=t;		// type of the parameter
	}
	else fpl=NADA;

	Mem[e].hh.lo=fpl;
	Mem[e].hh.hi=tr;
	Mem[e+1].hh.lo=nf;
	// construct code list
	halfword_t h=NADA,tx,l;
	int c;
	va_start(vl,nb);
	for (i=0;i<nb;i++)
	{
		c=va_arg(vl,int);
		l=GetAvail(2);
		if (h==NADA) h=l; else Mem[tx].ll.link=l;
		Mem[l].ll.info=0;
		Mem[l].ll.link=NADA;
		Mem[l+1].u=c;
		tx=l;
	}
	va_end(vl);
	Mem[e+1].hh.hi=h;
	NewSymbol(name,sFUNC,e);
}

void NewInlineProc(char*name, short signed int nf, int nb, ...)
{
	va_list vl;
	int i;
	halfword_t e=GetAvail(2);
	Mem[e].hh.lo=NADA;   // formal parameter list
	Mem[e+1].hh.lo=nf;  // function number
	Mem[e].hh.hi=NADA;    // return type
	// construct code list
	halfword_t h=NADA,tx,l;
	int c;
	va_start(vl,nb);
	for (i=0;i<nb;i++)
	{
		c=va_arg(vl,int);
		l=GetAvail(2);
		if (h==NADA) h=l; else Mem[tx].ll.link=l;
		Mem[l].ll.info=0;
		Mem[l].ll.link=NADA;
		Mem[l+1].u=c;
		tx=l;
	}
	va_end(vl);
	Mem[e+1].hh.hi=h;
	NewSymbol(name,sPROC,e);
}

// build a new inline procedure with two parameters
void NewInline2(char*name, halfword_t tp1, halfword_t tp2, short signed int nf, int nb, ...)
{
	va_list vl;
	int i;
	halfword_t e=GetAvail(2);
	halfword_t fpl2;

    fpl2=GetAvail(2); // formal parameter list

    // formal parameter list
    Mem[fpl2].ll.info=0;		// value parameter
    Mem[fpl2].ll.link=NADA; 	// last parameter
    Mem[fpl2+1].hh.lo=tp2;		// type of the parameter 2
    
    halfword_t fpl1=GetAvail(2);
    // formal parameter list
    Mem[fpl1].ll.info=0;		// value parameter
    Mem[fpl1].ll.link=fpl2; 	// second parameter
    Mem[fpl1+1].hh.lo=tp1;		// type of the parameter 1
 
	Mem[e].hh.lo=fpl1;
	Mem[e].hh.hi=NADA; // return typetr;
	Mem[e+1].hh.lo=nf;
	// construct code list
	halfword_t h=NADA,tx,l;
	int c;
	va_start(vl,nb);
	for (i=0;i<nb;i++)
	{
		c=va_arg(vl,int);
		l=GetAvail(2);
		if (h==NADA) h=l; else Mem[tx].ll.link=l;
		Mem[l].ll.info=0;
		Mem[l].ll.link=NADA;
		Mem[l+1].u=c;
		tx=l;
	}
	va_end(vl);
	Mem[e+1].hh.hi=h;
	NewSymbol(name,sPROC,e);
}


// Get a string in the input stream
// returns string number
// this function is called after a test "if (Pool[TkPtr]=='\'') ... "
halfword_t GetString()
{
	halfword_t Len=-1; // length of the string
	halfword_t StrPos=++TkPtr; // skip the quote that caused the call to this function
	halfword_t s;
	int i;
	char c;
	for(;;)
	{
		c=Pool[TkPtr];
		if (c=='\'')
		{
			TkPtr++;
			if (Pool[TkPtr]!='\'') break;  // this is the last quote
			Len--;  // skip the current quote which is used to confirm the previous
		}
		else if ((c=='\0')||(c=='\n'))
		{
			ReportError(UNEXPECTEDEOL);
		}
		TkPtr++;
	}
	// compute length of the string
	Len+=TkPtr-StrPos;
	// now, allocate the string
	switch (Len)
	{
	case 0:
		s=0;  // empty string
		break;
	case 1:   // single char string
		s=Pool[StrPos];
		break;
	default:
		if (StrPtr>=STRMAX) ReportError(OUTOFMEMORY); // no more string
		s=StrPtr+(MAXCHAR-1);
		HiPoolMin-=Len; // allocate room for this string
		if (HiPoolMin<LoPoolMax) ReportError(OUTOFMEMORY);
		StrStart[StrPtr++]=HiPoolMin;
		// copy the string
		i=HiPoolMin;
		do
		{
			c=Pool[StrPos++];
			Pool[i++]=c;
			if (c=='\'') StrPos++; // skip second quote
			Len--;
		}
		while(Len!=0);
	}
	NextToken();
	return s;
}

/*
// juste pour le test
void emitStr(halfword_t s)
{
	int i,j;
	if (s<MAXCHAR)
	{
		emitChar(s);
		return;
	}
	j = StrStart[s-MAXCHAR];
    for (i=StrStart[s-MAXCHAR+1];i<j;i++)
    {
    	emitChar(Pool[i]);
    }
}
*/


//----------------------------------------------------------------------------------
// scan for an unsigned number. The sign is managed by the expression parser
// returns numeral basic type if a number is scanned (btReal, or btUInt)
// if so, it assigns it to the union number which is either a double, or an unsigned
// if the input do not contains any number, then returns btVoid

BasicType_t GetNumber(number_t*x)
{
	char c;
	int n;
	BasicType_t bt;
	c=Pool[TkPtr];
	if (c=='$')
	{	// hexadecimal digit
		c=Pool[++TkPtr];
		n=HexaDigit(c);
		if (n<0) ReportError(UNEXPECTEDCHAR,c);
		x->u=0;
		while (n>=0)
		{
			if (x->u>=0x10000000) ReportError(OVERFLOW);
			x->u=(x->u<<4)+n;
			n=HexaDigit(Pool[++TkPtr]);
		}
		NextToken();
		return btUInt;
	}
	if (IsDigit(c))
	{
		bt=btUInt;
		halfword_t Start=TkPtr;
		do
		{  // scan the integral part
			c=Pool[++TkPtr];
		}
		while (IsDigit(c));
		if (c=='.')
		{   // scan the fractionnal part
			c=Pool[TkPtr+1];
			if (c=='.') goto integral; // allow construction of interval such as 5..10
			TkPtr++;
			if (!IsDigit(c)) ReportError(UNEXPECTEDCHAR,c);
			bt=btReal;
			do
			{
				c=Pool[++TkPtr];
			}
			while (IsDigit(c));
		}
		if (LowCase(c)=='e')
		{   // scan the exponent
			bt=btReal;
			c=Pool[++TkPtr];
			if ( (c=='+') || (c=='-') ) c=Pool[++TkPtr];
			if (!IsDigit(c)) ReportError(UNEXPECTEDCHAR,c);
			do
			{
				c=Pool[++TkPtr];
			}
			while (IsDigit(c));
		}
		if (bt==btReal)
		{
			// TODO, avoid call to sscanf
			sscanf(Pool+Start,"%lf",&x->d);
		}
		else
		{
		integral:
		//  sscanf do not manage overflow, so do it by hand
			x->u=0;
			do
			{
				n=HexaDigit(Pool[Start++]);
				// max integer is 2 147 483 647 = 2^31-1
				// if the next digit lead to a value greater that MaxInt, then overflow
				if (((n>7) && (x->u >= 214748364)) || (x->u >= 214748365)) ReportError(OVERFLOW);
				x->u = (x->u  << 1) + (x->u << 3) + n;  // avoid multiplication
			}
			while(Start<TkPtr);
		}
		NextToken();
		return bt;
	}
	return btVoid;
}


// scan for a label
// a label is a sequence of 1 to 4 digits
// returns NOIDENT if no label scanned, else returns
// identifier value, i.e. entry in the hash table

halfword_t ScanLabel()
{
	char c=Pool[TkPtr];
	if (!IsDigit(c)) return NOIDENT;
	IdPtr=TkPtr;
	c=Pool[++TkPtr];
	while (IsDigit(c))
	{
		c=Pool[++TkPtr];
	}
	LenID=TkPtr-IdPtr;
	if (LenID>4) ReportError(LABELTOOLONG);
	halfword_t res=LookUp();
	NextToken();
	return res;
}


