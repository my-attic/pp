#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <setjmp.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "pp.h"

//#define DEBUG

// compile.c
// this is the main file of pp
// it contains:
// all the system dependant functions are localised in this file
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// global variables definition
//////////////////////////////

jmethodID emitcharID;  // index de la méthode d'affichage d'un caractère sur la console
jobject Obj;           // objet java passé à la fonction native
JNIEnv* Env;           // environnement java passé à la fonction native

char exe_name[64];     // exec file name, initialised by the ScanProgName() procedure
int nb_lines;          // total number of lines of the source code
                       // initialised when parse starts, in do_compile()
                       //incremented when a new line is loaded in NextToken()

char message[1024];    // Buffer to various temporaly messages
                       // used in particuler for messages to transmit to the console
                       // for errors and at the end of the compilation process


clock_t start_time;

halfword_t ArmVersion; // this variable defines the instruction set
  // 400 (default) version 4
  // 500 ARMv5
  // 501 ARMv5STExP (dsp extension but ldrp, mcrr, mrrc pld and strd
  // 502 ARMv5STE (ehanced dsp instructions)

char Pool[POOL_SIZE];
int LoPoolMax, HiPoolMin;


// procedure to communicate with the console
//------------------------------------------
void emitChar(char c)
{
    (*Env)->CallVoidMethod(Env, Obj, emitcharID, (jchar) c);
}



void emitString(char* msg)
{
	while (*msg)
	{
		emitChar(*msg++);
	}
}

// for DEBUG only
void emitInt(int x)
{
	char buffer[32];
	sprintf(buffer,"%ld\n",x);
	emitString(buffer);
}

void emitHexa(long unsigned x)
{
	char buffer[16];
	sprintf(buffer,"%lx\n",x);
	emitString(buffer);
}


void emitReal(double x)
{
	char buffer[32];
	sprintf(buffer,"%lf\n",x);
	emitString(buffer);
}
// fin DEBUG ONLY


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Input streaming
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// the table of files, initialized to NULL by initialize
// the structure memorizes the last read char to check end of file
struct {FILE*f; int c; }Files[NB_FILES];

// return a reference of the input stream with a given name
// returns -1 in case of error
int aopenin(const char*name)
{
	char full_path[1024];  // max path length is not clear, sometime 256, sometime 1024
	int i;
	for (i=0;i<NB_FILES;i++)
	{
		if (Files[i].f==NULL) break;
	}
	if (i==NB_FILES) ReportError(TOOMUCHFILES);
	// compute the full name from the path given by the initial file to compile
	// and the current name in the Pool
	sprintf(full_path,"%s/%s",Pool,name);
	Files[i].f=fopen(full_path,"r");
	Files[i].c=0;
	if (Files[i].f==NULL) ReportError(CANTOPEN,name);
	return i;
}

// read a char in the input stream given by number refnum
// the read char is assigned to c
// returns O except -1 on eof -- on error, report the error
int agetchar(int refnum, char*pc)
{
	int c;
	FILE *f=Files[refnum].f;
	if (Files[refnum].c!=EOF) c=fgetc(f); else c=EOF;
	Files[refnum].c=c;  // memorize last read char
	if (c==EOF)
	{
		if (ferror(f))
		{   // it EOF is due to an error, then report it
			ReportError(READERROR);
		}
		*pc=0;
        return -1;
	}
	else *pc=(char)c;
	return 0;
}

void aclose(int refnum)
{
	fclose(Files[refnum].f);  // close the file
	Files[refnum].f=NULL;     // declare the entry as available
}

int aeof(int refnum)
{
	return Files[refnum].c==EOF;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// replace for example "/data/data/pp.compiler/name.pas" by
// "/data/data/pp.compiler\0name.pas" and returns address of "name.pas
char* split(const char*path)
{
	int i,j;
	char c;
	i=j=0;
	do
	{
		c=path[i];
		Pool[i++]=c;
		if (c=='/') j=i;
	}
	while (c!=0);
	Pool[j-1]=0;
	LoPoolMax=i;
	// the first identifier is the source name
	IdPtr=j;
	LenID=i-j-1;
	return Pool+j;
}

// prompt
//-------------
static void prompt(char*name)
{
	emitString("Pépé le compiler v 1.03\nFreeware (c)2010-2011\nby Philippe Guillot\n");
	sprintf(message,"Compiling \"%s\"...\n",name);
	emitString(message);
}

/*
int compilefile(FILE*f)
{
	int c;
	do
	{
		c=getc(f);
		if (c!=EOF) emitChar((char)c);
	}
	while(c!=EOF);
	return 0;
}
*/
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//  Errors
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// environnement pour la gestion d'erreur
jmp_buf jmp_env;
long unsigned initial_stack_value;

// fonction d'erreur
// affiche un message d'erreur et fait un goto non local
// vers la boucle d'appel définie par l'environnement "env"
////////////////////////////////////////////////////////////

// the error messages
const char* error_msg[]=
{
	"stack overflow",
	"too much files",
	"Read error",
	"procedural type expected",
	"out of memory",
	"unexpected end of file",
	"identifier expected",
	"unable to create exe file",
	"numerical expression expected",
	"type mismatch",
	"ordinal type expected",
	"unexpected end of line",
	"overflow",
	"label too long",
	"operator does not apply",
	"non writeable type",
	"integer expression expected",
	"code too large",
	"non constant expression",
	"integer constant expected",
	"invalid ordering",
	"out of bound",
	"too much locals",
	"too much globals",
	"type identifier expected",
	"structure too large",
	"variable expected",
	"procedural value expected",
	"invalid statement",
	"illegal assignement",
	"boolean expression expected",
	"unexpected token",
	"duplicate case",
	"invalid index variable",
	"too much enumerates",
	"constant out of bound",
	"record expected",
	"too much parameters",
	"pointer variable expected",
	"no such variant",
	"unknown directive",
	"invalid type",
	"non readable type",
	"file type expected",
	"too much defines",
	"too much conditionnals",
	"misplaced $else",
	"misplaced $endif",
	"displacement beyond limit",
    "division by zero",
    "label expected",


	"not yet implemented"
};
/*


"set type expected",

"string type expected",
"program parameter expected",

"illegal subrange",
"too much initializers",


"illegal parameter",


"out of scope",

"duplicate resource",

"integer expression expected",

// assembler error
"unexpected token",



"code too large",
"unable to create PRC",
"stack overflow",


*/

// return adress of identifier number x
static char* getStringPtr(halfword_t x)
{
	x=HTable[x].text;
	if (x<256)
	{   // single char string
		Pool[HiPoolMin]=x;
		Pool[HiPoolMin+1]='\0';
		return Pool+HiPoolMin;
	}
	// multi char string
	Pool[StrStart[x-256]]='\0'; // terminal zero
	return Pool+StrStart[x-255];
}
/*
function GetStringPtr(x:UInt16):StringPtr;
begin
  x:=pHTable^[x].text;
  if x>=256 then
  begin // regular string
    pPool^[pStrStart^[x-1]]:=chr(0); //  zero terminal
    GetStringPtr:=@pPool^[pStrStart^[x]];
  end
  else
  begin // monocharacter string
    CharString[1]:=chr(x); // assign global buffer for this use
    GetStringPtr:=@CharString;
  end;
end;
*/

int ReportError(int no,...)
{
	va_list  v1;
	va_start (v1,no);
	char Buffer[32];
	KWEnum_t kw;
	int i,j;
	char*msg;

	switch (no)
	{
	case UNEXPECTEDID:
		i=va_arg(v1,int);
		sprintf(message,"'%s' unexpected", getStringPtr(i));
		break;
	case UNKNOWNID:
		i=va_arg(v1,int);
		sprintf(message,"'%s' unknown", getStringPtr(i));
		break;
	case EXPECTEDCHAR:
		i=va_arg(v1,int);
		if (i<256)
		{  // signe char notification
		   sprintf(message,"'%c' expected",(char)i);
		}
		else
		{   // dual char notification
			sprintf(message,"'%c%d' expected", (char)(i&0xff),(char)(i>>8));
		}
		break;
	case EXPECTEDKW:
		kw=va_arg(v1,KWEnum_t);

		i=KWStart[kw];
		j=KWStart[kw+1]-i;
		memcpy(Buffer,KWPool+i,j);
		Buffer[j]=0;
		sprintf(message,"\"%s\" expected", Buffer);
		break;
	case CANTOPEN:
		msg=va_arg(v1,char*);
		sprintf(message,"Could not open file \"%s\"",msg);
		break;
	case REGULAR_TEXT:
		msg=va_arg(v1,char*);
		sprintf(message,"%s",msg);
		break;
	case UNEXPECTEDCHAR:
		i=va_arg(v1,int);
		sprintf(message,"unexpected '%c'",(char)i);
		break;
	case DUPLICATEID:
		i=va_arg(v1,int);
		sprintf(message,"duplicate '%s'",getStringPtr(i));
		break;
	case UNEXPECTEDKW:
		kw=va_arg(v1,KWEnum_t);
		i=KWStart[kw];
		j=KWStart[kw+1]-i;
		memcpy(Buffer,KWPool+i,j);
		Buffer[j]=0;
		sprintf(message,"unexpected \"%s\"", Buffer);
		break;
	case UNDEFFWDTYPE:
		i=va_arg(v1,int);
		sprintf(message,"undefined '%s'", getStringPtr(i));
		break;
	case UNDECLAREDLABEL:
		i=va_arg(v1,int);
		sprintf(message,"undeclared '%s'", getStringPtr(i));
		break;
	case UNDEFINEDID:
		i=va_arg(v1,int);
		sprintf(message,"undefined '%s'",getStringPtr(i));
		break;
	case UNDECLAREDLBL:
		i=va_arg(v1,int);
		sprintf(message,"undeclared '%s'",getStringPtr(i));
		break;
	case LBLALREADYDEF:
		i=va_arg(v1,int);
		sprintf(message,"duplicate definition of '%s'",getStringPtr(i));
		break;
	case UNKNOWNMNEMONIC:
		Pool[IdPtr+LenID]=0;
		sprintf(message,"unknown mnemonic '%s'",Pool+IdPtr);
		break;
	case INVALIDOPCODE:
		Pool[IdPtr+LenID]=0;
		sprintf(message,"invalid opcode '%s'",Pool+IdPtr);
		break;
	case INTERNAL:
		msg=va_arg(v1,char*);
		sprintf(message,"Internal Error '%s'",msg);
		break;
	}
    longjmp(jmp_env,no);
    return no;
}

// assembly function to get the value of the stack pointer
// this is used to check stack consumtion when recursive call occurs
// doc in http://www.ethernut.de/en/documents/arm-inline-asm.html
static long unsigned get_stack_value()
{
	asm("mov r0,r13");
}

void CheckStack()
{
	if (initial_stack_value-get_stack_value()>ALLOWED_STACK)
	{
		ReportError(STACK_OVERFLOW);
	}
}

halfword_t CharTypeNode;
halfword_t InputVar;  // input predefined symbol identifier
halfword_t OutputVar; // idem for output


// Initialize internal variables of the compiler
void initialize()
{
	int i;

	ArmVersion=400;  // default ARM version
	// variables for predefined types
	halfword_t IntegerTypeNode;
	halfword_t BooleanTypeNode;
	halfword_t RealTypeNode;
	halfword_t PointerTypeNode;

	// initialization of variables
	for (i=0;i<NB_FILES;i++) Files[i].f=NULL;
    iStackPtr=0;  // declare that no input file is open

    HiPoolMin=POOL_SIZE;
    LoPoolMax=0;

    InitMem();   // memory initialization
    InitStrings();
    InitHTable();
    InitCode();
    InitParse();

	// read the time
	start_time=clock();
	// initialize initial stack value
	initial_stack_value=get_stack_value();

	// Predefined identifier initialisation
	NewSymbol("write",sWRITE,0);
	NewSymbol("writeln",sWRITE,1);
	NewSymbol("sizeof",sSIZEOF,0);

	NewConst("true",TBOOLEAN,1);
	NewConst("false",TBOOLEAN,0);
	NewConst("maxint",TINTEGER,0x7fffffff);

	// predefined types
	IntegerTypeNode=NewType("integer",TINTEGER);
	BooleanTypeNode=NewType("boolean",TBOOLEAN);
	RealTypeNode=NewType("real",TREAL);
	CharTypeNode=NewType("char",TCHAR);
	PointerTypeNode=NewType("pointer",TPOINTER);
	NewType("text",TTEXT);

	NewSymbol("ord", sORDF,0);
	NewSymbol("pred", sORDF,1);
	NewSymbol("succ", sORDF,2);
	NewSymbol("abs",sNUMF,0);
	NewSymbol("sqr",sNUMF,1);
	NewSymbol("chr",sCHR,0);
	NewSymbol("new",sNEW,0);
	NewSymbol("dispose",sNEW,1);

	// predefined inlines functions
	NewInline1("odd",IntegerTypeNode,BooleanTypeNode,-1,1,0xe2100001);
	NewInline1("sqrt",RealTypeNode,RealTypeNode,-2,2,0xE1A0E00F, 	//  MOV lr,pc
											0xE599F000+REALSQRT);
	NewInline1("round",RealTypeNode,IntegerTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALROUND);
	NewInline1("trunc",RealTypeNode,IntegerTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALTRUNC);

	NewInline1("ln",RealTypeNode,RealTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALLN);
	NewInline1("exp",RealTypeNode,RealTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALEXP);
	NewInline1("sin",RealTypeNode,RealTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALSIN);
	NewInline1("cos",RealTypeNode,RealTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALCOS);
	NewInline1("arctan",RealTypeNode,RealTypeNode,-4,2,0xE1A0E00F,0xE599F000+REALATAN);

	NewFunc1("halt",IntegerTypeNode,NADA,HALT);

	NewInlineProc("clrscr",-10,2,0xE1A0E00F,0xE599F000+CLRSCR);

	InputVar=NewVar("input", TTEXT, -36);
	OutputVar=NewVar("output", TTEXT, -24);

	NewSymbol("rewrite",sOPEN,1);
	NewSymbol("reset",sOPEN,0);

	NewSymbol("close",sFILEP,CLOSE);
	NewSymbol("get",sFILEP,GET);
	NewSymbol("put",sFILEP,PUT);

// crt functions
    NewInline1("wherex",NADA,IntegerTypeNode,-4,2,0xE1A0E00F,0xE599F000+WHEREX);
    NewInline1("wherey",NADA,IntegerTypeNode,-4,2,0xE1A0E00F,0xE599F000+WHEREY);
    NewInline1("screenwidth",NADA,IntegerTypeNode,-4,2,0xE1A0E00F,0xE599F000+SCREENWIDTH);
    NewInline1("screenheight",NADA,IntegerTypeNode,-4,2,0xE1A0E00F,0xE599F000+SCREENHEIGHT);

    NewInline2("gotoxy",IntegerTypeNode, IntegerTypeNode, -4,2,0xE1A0E00F,0xE599F000+GOTOXY);

    NewSymbol("read",sREAD,0);
    NewSymbol("readln",sREAD,1);

    NewSymbol("eof",sEOF,0);
    NewSymbol("eoln",sEOF,1);

    NewFunc1("length",StringTypeNode,IntegerTypeNode,LENGTH);
}

int exe_size;

void build_exec()
{
#ifdef DEBUG
    int i;
    emitString("Code:\n");
    for (i=0;i<ExePtr;i++)
    {
        emitHexa(pExe[i]);
    }
#endif

//	sprintf(message,"%s/%s",Pool,exe_name);  // create full filename including the path
    // always write the executable file in internal data storage as external is not
    // an executable zone
	sprintf(message,"/data/data/pp.compiler/%s",exe_name);  // create full filename including the path
    FILE* f=fopen(message,"r");     // check if the file exists
    if (f!=NULL) remove(message);   // if yes, then remove it
    int fd=open(message,O_CREAT|O_RDWR,S_IRWXU);  // create an executable file

    if (fd==-1) ReportError(CANTCREATEEXE);

    //int s=write(fd,CodeSample,exe_size);
    exe_size=ExePtr << 2;
    int s=write(fd,pExe,exe_size); //write data
    close(fd);

    if (s!=exe_size) ReportError(CANTCREATEEXE);

}


void terminate()
{
	int i;
	// close the files in the stack
	for (i=0;i<iStackPtr;i++)
		aclose(inputStack[i].RefNum);
}
// génération d'un message du genre
// 1352 lines compiled (2.3 sec)
// brol.exe (256 bytes) created
void post_message()
{
	clock_t time=clock()-start_time; // lecture de l'heure en clocks
	long time_sec,time_hun;          // secondes, centièmes de seconde

	time_sec=time/CLOCKS_PER_SEC;    // nombre de secondes
	time_hun=(time%CLOCKS_PER_SEC)/(CLOCKS_PER_SEC/100);  // nombre de centièmes
	// production du message
	sprintf(message,"%d lines compiled (%d.%d sec.)\n\"%s\" (%d bytes) created.",
			nb_lines,time_sec,time_hun,exe_name,exe_size);
}



jint Java_pp_compiler_Compile_compiler(JNIEnv* env, jobject obj, jstring jname)
{

	 int e;
	 const char*path;
	 int i;
	 char*name; // index of the file name

     initialize();

	 // error capture
	 e=0;
     if ((e=setjmp(jmp_env))!=0)
     {
    	 terminate();
         if (iStackPtr>0)
    	 {
				#ifdef DEBUG
					 return 12345;  // for debug to stay in console
				#else
					 return e;
				#endif
    		  // lauch editor on the error if e>0
    		  // stay on the console on fatal error <0
    	 }
    	 return -1;
     }

	 // Parameters are used in other functons such as emitChar
     // so save them in globals
	 Env=env;
	 Obj=obj;

	 // compute the identifier of the Java Method used by the compiler
	 jclass cls = (*env)->GetObjectClass(env, obj);
   	 emitcharID = (*env)->GetMethodID(env, cls, "emitChar", "(C)V");
     if (emitcharID == NULL) ReportError(INTERNAL,"method not found");  // Humm, method not found

	 // get the path of the file to compile
     path=(*env)->GetStringUTFChars(env,jname,NULL);
	 if (path==NULL) ReportError(OUTOFMEMORY);       // out of memory

	 // copy the path in the Pool and split it
	 // split the path into the path and the name
	 // a terminal zero is included in place of the last '/'
	 // assign LoPoolMax to the end of the path
	 name=split(path);
     (*env)->ReleaseStringUTFChars(env,jname,path);

     prompt(name);

     do_compile();

     do_link();

     build_exec();

     terminate();

     post_message();

	 return 0;

}

// function used by the java console to retrieve a message from the compiler
// this is used to print an error message
//---------------------------------------------------------------------------
jstring Java_pp_compiler_Compile_message(JNIEnv* env, jobject obj)
{
	return (*env)->NewStringUTF(env, message);
}

// returns the absolute position where the error has occured in the input file
jint Java_pp_compiler_Compile_errPos(JNIEnv*env, jobject obj)
{
	// if the position is out of bound, either <0 or > the size of the file, then
	// the set selection method  of the editable class in Android crashes.
	// if the end of file occurs while scanning an objet, the end of file character is taken into account
	// and this lead to LoPoolMax=TkPtr+1 If this occurs on the first line, the returned pos is -1
	// if the end of file occurs in a comment, as, the eof char is parsed, one has TkPtr = LoPoolMax
	// to avoid crash in any of these case, the position is forced tobe >=0
	int pos=inputStack[iStackPtr-1].AbsPos+TkPtr-LoPoolMax;
	if (pos<0) pos=0;
	return pos;
}

// returns the current source file name in order to launch the editor
// the file name is recovered from the input stack
jstring Java_pp_compiler_Compile_currFileName(JNIEnv* env, jobject obj)
{
	char full_path[1024];
	// path is memorised at the beginning of the Pool
	sprintf(full_path,"%s/%s",Pool,Pool+inputStack[iStackPtr-1].NamePtr);
// TODO voir si on peut récupérer le nom complet fait par build_exe
	return (*env)->NewStringUTF(env, full_path);
}

// returns the current exec file name in order to run it after compilation
jstring Java_pp_compiler_Compile_currExeName(JNIEnv* env, jobject obj)
{
	char full_path[1024];
	// path is memorised at the beginning of the Pool
//	sprintf(full_path,"%s/%s",Pool,exe_name);
	sprintf(full_path,"/data/data/pp.compiler/%s",exe_name);  // create full filename including the path
	return (*env)->NewStringUTF(env, full_path);
}
