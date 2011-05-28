// pp.h header
//////////////////

#ifndef PP_H
#define PP_H


// dimension of the system
#define STACKSIZE	8192
#define POOL_SIZE 	32000  // size of the character pool
#define NB_FILES  	16     // number of files opened at the same time
#define INPUTSTACKSIZE 	8

#define MEMSIZE   	32000  // size of the main memory
#define HSIZE 		3000   // number of entry in hash table
#define HPRIME  	2399   // a prime number of about 80% HSize
#define STRMAX      5999   // Max number of multichar strings
#define CODESIZE    50000  // number of 32 bits words in code ~ 200 ko
#define LINKSIZE	5000  // number of entries  of the link table
#define MAXGLOBALS	1000000 // max byte size of globals
// max byte size of locals
#define MAXLOCALS	5000

// maximum value for a set -- MUST be multiple of 4 for word alignement reasons
#define MAXSET 256

// memory word Pascal and TeX style
//---------------------------------
typedef enum {False=0, True=1} boolean_t;

typedef	long unsigned word_t;
typedef	unsigned char byte_t;
typedef long int integer_t;
typedef void* pointer_t;
typedef short unsigned halfword_t;


// global variables declaration
//-----------------------------
// These variables are all defined in the main file "compile.c"

// The Pool
//----------
// the Pool is divided in two parts,
// the lowest part from 0 to LoPoolMax-1 contains the input characters
// the hisest part from HiPoolMin to POOLSIZE-1 contains the strings
// description of the identifiers.
extern char Pool[POOL_SIZE];
extern int LoPoolMax, HiPoolMin;

//
// Errors
/////////////////////////////////////////////////
#define ALLOWED_STACK    1024

#define INTERNAL		255

#define UNEXPECTEDID	1
#define UNKNOWNID		2
#define EXPECTEDCHAR	3
#define EXPECTEDKW		4
#define CANTOPEN		5
#define REGULAR_TEXT	6
#define UNEXPECTEDCHAR	7
#define DUPLICATEID		8
#define UNEXPECTEDKW	9
#define UNDEFFWDTYPE	10
#define UNDECLAREDLABEL	11
#define UNDEFINEDID		12
#define UNDECLAREDLBL	13
#define LBLALREADYDEF	14
#define UNKNOWNMNEMONIC	15
#define INVALIDOPCODE	16


extern const char* error_msg[];  // contains the error messages

#define STACK_OVERFLOW   	REGULAR_TEXT,error_msg[0]
#define TOOMUCHFILES     	REGULAR_TEXT,error_msg[1]
#define READERROR        	REGULAR_TEXT,error_msg[2]
#define PROCTYPEEXPECTED	REGULAR_TEXT,error_msg[3]
#define OUTOFMEMORY      	REGULAR_TEXT,error_msg[4]
#define UNEXPECTEDEOF    	REGULAR_TEXT,error_msg[5]
#define IDENTEXPECTED    	REGULAR_TEXT,error_msg[6]
#define CANTCREATEEXE    	REGULAR_TEXT,error_msg[7]
#define NONNUMERICALTYPE 	REGULAR_TEXT,error_msg[8]
#define TYPEMISMATCH     	REGULAR_TEXT,error_msg[9]
#define ORDINALTEXP 		REGULAR_TEXT,error_msg[10]
#define UNEXPECTEDEOL		REGULAR_TEXT,error_msg[11]
#define OVERFLOW			REGULAR_TEXT,error_msg[12]
#define LABELTOOLONG		REGULAR_TEXT,error_msg[13]
#define OPDONOTAPPLY		REGULAR_TEXT,error_msg[14]
#define NONWRITEABLE		REGULAR_TEXT,error_msg[15]
#define INTTYPEEXECTED		REGULAR_TEXT,error_msg[16]
#define CODETOOLARGE		REGULAR_TEXT,error_msg[17]
#define NONCONSTANTEXPR		REGULAR_TEXT, error_msg[18]
#define INTEGERCONSTEXPEC	REGULAR_TEXT, error_msg[19]
#define INVALIDORDER		REGULAR_TEXT, error_msg[20]
#define OUTOFBOUND			REGULAR_TEXT, error_msg[21]
#define TOOMUCHLOCALS		REGULAR_TEXT, error_msg[22]
#define TOOMUCHGLOBALS		REGULAR_TEXT, error_msg[23]
#define TYPEIDEXPECTED		REGULAR_TEXT, error_msg[24]
#define STRUCTURETOOLARGE	REGULAR_TEXT, error_msg[25]
#define VAREXPECTED			REGULAR_TEXT, error_msg[26]
#define PROCVALEXPECTED     REGULAR_TEXT,error_msg[27]
#define INVALIDSTM   	  	REGULAR_TEXT,error_msg[28]
#define ILLEGALASSIGN     	REGULAR_TEXT,error_msg[29]
#define BOOLEANEXPREXP		REGULAR_TEXT,error_msg[30]
#define UNEXPECTEDTOKEN		REGULAR_TEXT,error_msg[31]
#define DUPLICATECASE		REGULAR_TEXT,error_msg[32]
#define INVALIDINDEX		REGULAR_TEXT,error_msg[33]
#define TOOMUCHENUM			REGULAR_TEXT,error_msg[34]
#define CONSTOUTOFBOUND		REGULAR_TEXT,error_msg[35]
#define RECTYPEEXPECTED		REGULAR_TEXT,error_msg[36]
#define TOOMUCHPARAMS		REGULAR_TEXT,error_msg[37]
#define PTRVAREXPECTED		REGULAR_TEXT,error_msg[38]
#define NOSUCHVARIANT		REGULAR_TEXT,error_msg[39]
#define UNKNOWNDIR			REGULAR_TEXT,error_msg[40]
#define INVALIDTYPE			REGULAR_TEXT,error_msg[41]
#define NONREADABLETYPE		REGULAR_TEXT,error_msg[42]
#define FILETYPEEXPECTED	REGULAR_TEXT,error_msg[43]
#define TOOMUCHDEFINES     	REGULAR_TEXT,error_msg[44]
#define TOOMUCHCOND			REGULAR_TEXT,error_msg[45]
#define MISPLACEDELSE     	REGULAR_TEXT,error_msg[46]
#define MISPLACEDENDIF     	REGULAR_TEXT,error_msg[47]
#define BEYONGLIMIT			REGULAR_TEXT,error_msg[48]
#define DIVISIONBYZERO      REGULAR_TEXT,error_msg[49]
#define LABELEXPECTED		REGULAR_TEXT,error_msg[50]


#define NOTYETIMPLEM     	REGULAR_TEXT,error_msg[51]



// report an error
// accordint to the value of the error number, a second parameter
// precises the error, such as identifier value, key word or char expected expected
int ReportError(int no,...);
void CheckStack();  // call to check stack on a recursive call
extern char exe_name[64];

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  The input stack
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

extern int nb_lines;
// The input stack is used to manage file included
// on a directive {$i <filename>) the current state is memorised
// and recover at the end of parsing the included file

typedef struct
{
	int RefNum;    // reference of the input stream return by the aopenin() function
	int NamePtr;   // index of the file name int the Pool
	int StartPtr;  // start of the line in the Pool = LoPoolMax to restaure
	int SavePtr;   // value of TkPtr to restaure while poping
//	int LineNum;   // current line number
	int AbsPos;    // Absolute position in the input stream

} StackEntry_t;

extern StackEntry_t inputStack[INPUTSTACKSIZE];
extern int iStackPtr;

void PushInput();

void PopInput();

int aopenin(const char*name);
int agetchar(int refnum, char*pc);
void aclose(int refnum);
int aeof(int refnum);


// Preprocessor
//--------------
extern int NestedIf; // current index in CondStack
typedef enum
// InputState defines the analysis status
{ ScanAlways, // always parse (normal text)
    ScanIf,     // parse in [$if xxx] section
    ScanElse,   // parse in [$else] section
    SkipElse,   // skip in [$else] section
    SkipIf,     // skip in [$if xxx] section
    SkipAlways  // always skip (nested conditionnal with skip status)
} InputState_t;
#define NBCOND	8
extern InputState_t CondStack[NBCOND]; // memorise return InputState
#define NBDEFINES 50
typedef struct { halfword_t s,d; } DefineEntry_t;
extern DefineEntry_t DefinesTable[NBDEFINES];
extern int DefinesPtr; // max index in defines table



// lexical analysis
//-----------------
extern InputState_t InputState;

extern int IdPtr; // Index dans le pool du début d'un identificateur
extern int LenID; // longueur de l'identificateur courant
extern int TkPtr; // pointeur du prochain token dans le pool
extern int eofAllowed; // equals 1 if the end of fileis allowed while scanning
      // end of file is not allowed while parsing a bloc of level >=1
      // this flag is set if a declaration of level 0 is fully parsed

// put the value of TkPtr to the begining of the next token
// this function skip comments and interprets the directives
void NextToken();


typedef struct
{
	 unsigned char s; // packed data for IDEnum_t
	 unsigned char l; // level of definition
	 halfword_t val;  // value

}Symbol_t;



typedef union
	{
		word_t u;
		integer_t i;
		pointer_t p;
		struct { halfword_t lo,hi ;} hh;
		struct { short signed int lo,hi;} ii;
	    //             +----------------------+
	    // list node   |   info    |   link   |
	    //             +----------------------+
		struct { halfword_t info,link;} ll;
	    //             +----------------------+
	    // type header | bt | size |   link   |
	    //             +----------------------+
	    struct {byte_t bt; byte_t s; halfword_t link;} th;
	    //             +----------------------+
	    // node header | op | bt   |  link    |
	    //             +----------------------+
	    struct {byte_t op; byte_t bt; halfword_t link; } nh;

	    struct { byte_t b0,b1,b2,b3;} qqqq;
	    Symbol_t sy;
	} memoryword_t;

//record to store reals, represented as double IEEE754
typedef union
{
		double d;
		struct {word_t lo,hi;} uu;
} realrec_t;

#define NADA 0
#define NIL (void*)0

// Keyword stuff
//-------------
typedef enum
    // two letters
{   kDO,kIF,kIN,kOF,kOR,kTO,
    // three letters
    kAND,kASM,kDIV,kEND,kFOR,kMOD,kNIL,kNOT,kSET,kSHL,kSHR,
    kVAR,kXOR,
    // four letters
    kCASE,kELSE,kFILE,kGOTO,kTHEN,kTYPE,kWITH,
    // five letters
    kARRAY,kBEGIN,kCONST,kLABEL,kUNTIL,kWHILE,
    // 6
    kDOWNTO,kPACKED,kRECORD,
    kREPEAT,kSTRING,
    kPROGRAM,
    kFUNCTION,
    kPROCEDURE,

    kFORWARD,kINLINE, // these are not keyword, but directive
    kZZZZZZ
} KWEnum_t;


#define  NBKEYWORDS  43 // number of keyword
extern char KWPool[];  // needed in compile.c for error reporting
extern int KWStart[NBKEYWORDS]; // idem

int ScanKW(KWEnum_t kw);
//int TestKW(KWEnum_t kw);
void CheckKW(KWEnum_t kw);
void ScanProgName();
int ScanChar(char c);
void CheckChar(char c);


// basic types of expressions
typedef  enum
{
    btVoid,  // no type
    btBool,  // boolean
    btChar,  // char
    btInt,   // signed integer
    btUInt,  // unsigned integer
    btReal,  // real
    btPtr,   // pointer
    btStr,   // string

    btSet,   // set
    btRec,   // record
    btArray, // array
    btFile,  // file
    btText,  // text
    btFunc,  // function
    btProc,  // procedure
    btConf,  // conformant array paraneter

    btEnum,  // starting point of user defined enumerate
    btMax=255
} BasicType_t;

typedef enum
{   // operator for syntactic nodes

	// terminal operators
    opConst,  // constant
    opVar,    // variable

    // unary expression
    opNot,
    opNeg,   // negate iateger
    opCast,  // cast integer to real or char to string
    opPred,
    opSucc,
    opAbs,   // absolute value
    opSqr,
    opEof,
    opEoln,
    opAddr,  // adress


    // binary expression
    opAdd,
    opMul,
    opSub,
    opDiv,   // real division
    opIDiv,  // integer division
    opMod,
    opShl,
    opShr,
    opAnd,opOr,opXor,
    opEq,opNe,opLt,opLe,opGt,opGe, // order dependant
    opIn,

    opSCons, // set constructor


    // nodes for statement
    opNop,   // no operation
    opLabel,
    opWrite,     // write on a file
    opWriteText, // on a text
    opNewLine,
    opReadText,
    opReadln,
    opRead, // read on a file
    opGoto,
    opAssign, // assignement
    opPCall,  // procedure call
    opPPCall, // prucedural variable call
    opFAssign,
    opIf,
    opCase,
    opWhile,opRepeat,opFor,
    opWith,
    opNew,opDispose,
    opFile,opOpen,

    opInline,  // inline definition
    opAsm      // assembler item
} Op_t;


// identifiers category
typedef enum
{ // Identifier type
    sUNDEF, // undefined
    sCHR,   // che predefined function
    sEOF,
    sNEW,   // new and dispose
    sPUT,
    sREAD,
    sWRITE,
    sSIZEOF,
    sCONST,
    sTYPE,
    sVAR,
    sPROC,
    sFUNC,
    sLABEL,
    sORDF,  // ordinal function pred,succ,ord
    sNUMF,  // numeric function abs,sqr
    sFILEP, // put, get, close
    sOPEN,  // reset, rewrite
    sPARAM  // program parameter
} IDEnum_t;


extern halfword_t ArmVersion;

 // Strings
 //-----------

#define MAXCHAR 256
extern halfword_t StrStart[STRMAX];
void InitStrings();
int StringLen(halfword_t s);
halfword_t ConcatStr(halfword_t sa, halfword_t sb);


// The hash table
//-----------------

// Type for hash table entry
//  +--------------------------+
//  |   link      |   text     |
//  +--------------------------+
//  | s    |  l   |   value    |
//  +--------------------------+
typedef struct
{
 	halfword_t	link;
 	halfword_t	text;
 	Symbol_t	symb;
} HEntry_t;

// The Hash Table
extern HEntry_t HTable[HSIZE];

void InitHTable();

#define NOIDENT HSIZE
halfword_t ScanID();

extern halfword_t IDVal;      // current symbol value
extern unsigned char IDLevel; // current symbol level

IDEnum_t GetXID(halfword_t x);


halfword_t NewSymbol(char*name,IDEnum_t s,halfword_t val);
halfword_t NewType(char*name, word_t);
void NewConst(char*name, word_t t, word_t v);
extern BasicType_t EnumCount;


// union for numbers
//------------------
typedef union
{
	long unsigned u;
	double d;
	struct{long unsigned lo,hi;} uu;
} number_t;

// declarations for parsing
//-------------------------

extern int sLevel;
extern halfword_t LabelList;
// identifiant de la fonction en cours de définition
// est utilisé en niveau >1 pour tester qu'un nouvel identificateur est légitime
// le nom de la fonction courante est un identificateur de niveau n-1
extern halfword_t CurFID;
void InitParse();
void VarAccess(memoryword_t*t, halfword_t*e, boolean_t f);
void AssCompTypes(memoryword_t t1, memoryword_t t2, halfword_t*e);
halfword_t AddrFunc(BasicType_t bt, memoryword_t*t);


void do_compile();

extern int GlobalsIndex;  // index to globals. At the end, contains the globals size
extern int LocalsIndex;   // idem for locals
extern int PrmIndex;      // Parameters size of currently code generate functin
extern int TmpVarIndex;   // used for temporary variables in "for" and "with" statements


// Memory data
//------------
extern halfword_t LoMemMax, HiMemMin;
extern memoryword_t Mem[MEMSIZE];
halfword_t GetAvail(halfword_t n);
void Room(halfword_t n);
void InitMem();
void MarkMem(memoryword_t *);
void ReleaseMem(memoryword_t);


// these variables are set in the setup process and are checked while parsing
// program parameters
extern halfword_t InputVar;  // input predefined symbol identifier
extern halfword_t OutputVar; // idem for output


// Expressions
//-----------
extern memoryword_t StringType;
extern halfword_t StringTypeNode; // node that contains the string type
extern halfword_t CharTypeNode;   // node that contains the char type


void OptimizedExpression(memoryword_t* ty, halfword_t*e, halfword_t id);
void OptimizeExpression(halfword_t*e);
void Expression(memoryword_t*ty, halfword_t*e, halfword_t id);
void ConstEx(memoryword_t*t, halfword_t* e, halfword_t id);
void CheckIntegerType(BasicType_t bt);
void CheckBoolExpr(halfword_t*e);


// constant that define predefined types
//////////////////////////////////////////
#define TBOOLEAN 	((word_t)btBool+(1<<8)+  (NADA<<16))
#define TUINTEGER	((int)btUInt+(4 <<8) + (NADA << 16))
#define TINTEGER	((int)btInt+(4<<8)+(NADA<<16))
#define TREAL		((word_t)btReal+(8<<8)+(NADA<<16))
#define TPOINTER	((word_t)btPtr + (4<<8)+(NADA <<16))
#define TCHAR		(btChar+(1<<8)+(NADA<<16))
#define TVOID		((word_t)btVoid)
#define TTEXT		((word_t)btText)

// the size of a string type is t.u >> 16
// it is 256 by default, but it may be redefined by a directive $maxstring
// thus string type is accessible by a variable StringType, the value of this variable
// may change.
#define TSTRING		((word_t)btStr + (256<<16) + (1<<8))
// byte aligned


// optimization
void OptimizeExpression(halfword_t*e);
void OptimizeStatements(halfword_t*n);

// code generation

#define BOOTSTRAP	3  // function number of the bootstrap
#define PMAIN 		6  // function number of the main section of the program
#define HALT		9  // function called to exit the program with an error code in 0
#define SIGNEDDIV	12  // signed division routine
#define UNSIGNEDDIV 15  // unsigned division routine
#define MEMMOVE		18  // memory move
#define STRCOPY		21  // string copy
#define STRCAT		24	// string concatenation
#define STRCOMPARE  27	// string comparison
#define PUSHDATA	30	// push byte aligned data
#define POPDATA		33  // pop word aligned data
#define LENGTH		36  // lenght of a string
// Next one is 39 !

#define LAST_RTL	36 // last predefined function in the run time library included in exe

void StartupCode();
void InitCode();
void OpenFunction(halfword_t n);
void CloseFunction(memoryword_t tr);
void StmCodeAndLink(halfword_t);
// link
void do_link();
extern word_t* pExe;
extern int ExePtr;

// systraps
#define GETMEM			0
#define FREEMEM 		4
#define WRITESTRING 	8
#define NEWLINE			12
#define WRITEINTEGER	16
#define WRITEFLOAT		20
#define WRITEBOOLEAN	24
#define WRITECHAR		28
#define WRITEFIXED		32
#define INTTOREAL		36
#define UINTTOREAL		40
#define REALADD			44
#define REALSUB			48
#define REALMUL			52
#define REALDIV			56
#define REALSQRT		60
#define REALROUND		64
#define REALTRUNC		68
#define REALEQ			72
#define REALLE			76
#define REALLT			80
#define WRITEUNSIGNED	84
#define REALLN			88
#define REALEXP			92
#define REALATAN		96
#define	REALSIN			100
#define REALCOS			104
#define REWRITEOUTPUT	108
#define RESETINPUT		112
#define PUT				116
#define CLRSCR			120
#define READSTRING		124
#define READREAL		128
#define READINTEGER 	132
#define ENDOFFILE		136
#define GET				140
#define FILEACCESS		144
#define READCHAR		148
#define ENDOFLINE		152
#define FLUSHLINE		156
#define RESET			160
#define REWRITE			164
#define CLOSE			168
#define WHEREX          172
#define WHEREY          176
#define SCREENWIDTH  	180
#define SCREENHEIGHT	184
#define GOTOXY			188

#define SETSINGLE		192
#define SETINTERVALL	196
#define ISSUBSET		200
#define SETEQ			204
#define ISINSET			208
#define SETADD			212
#define SETMUL			216
#define SETSUB			220
#define SETXOR			224
#define CLEARMEM		228



void NewInline1(char*name, halfword_t t, halfword_t tr, short signed int nf, int nb, ...);
void NewFunc1(char*name, halfword_t t, halfword_t tr, halfword_t nf);

void asmEpilog();
void asmProlog(halfword_t nf);
int IsIDChar(char c);
char LowCase(char c);
halfword_t FuncCall(memoryword_t*t);
int CheckFitOp2(word_t k);


#endif
