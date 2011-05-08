#include <jni.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

//#include <sys/types.h> -- is in "rtl.h"

#include <unistd.h>
#include "rtl.h"

// java method identifier of method that are used
jmethodID emitcharID;  // emit a character on the console
jmethodID getcharID;   // wait for an input character on the console
jmethodID clrscrID,    // clear console screen
            wherexID,   // x position method
            whereyID,	// y position
            screenWidthID, // screen width
            screenHeightID,
            gotoxyID; 

jobject Obj;
JNIEnv* Env;

// pour connaître la taille du fichier, utiliser fstat
//  int fstat(int filedes, struct stat *buf)
/* struct stat {
     dev_t     st_dev;     // ID of device containing file
     ino_t     st_ino;     // inode number
     mode_t    st_mode;    // protection
     nlink_t   st_nlink;   // number of hard links
     uid_t     st_uid;     // user ID of owner
     gid_t     st_gid;     // group ID of owner
     dev_t     st_rdev;    // device ID (if special file)
     off_t     st_size;    // total size, in bytes
     blksize_t st_blksize; // blocksize for filesystem I/O
     blkcnt_t  st_blocks;  // number of blocks allocated
     time_t    st_atime;   // time of last access
     time_t    st_mtime;   // time of last modification
     time_t    st_ctime;   // time of last status change
 };
*/

int fileSize(int fd)
{
	struct stat infos;
	int e;
	e=fstat(fd,&infos);
	if(e!=0) return 0;
	return infos.st_size;
}


char getChar()
{
	return (*Env)->CallCharMethod(Env, Obj, getcharID);
}

void emitChar(char c)
{
    (*Env)->CallVoidMethod(Env, Obj, emitcharID, (jchar) c);
}

void clrscr()
{
    (*Env)->CallVoidMethod(Env, Obj, clrscrID);
}


int wherex()
{
    (*Env)->CallIntMethod(Env, Obj, wherexID);
}

int wherey()
{
    (*Env)->CallIntMethod(Env, Obj, whereyID);
}

int screenwidth()
{
    (*Env)->CallIntMethod(Env, Obj, screenWidthID);
}

int screenheight()
{
    (*Env)->CallIntMethod(Env, Obj, screenHeightID);
}

void gotoxy(int x, int y)
{
    (*Env)->CallVoidMethod(Env, Obj, gotoxyID, (jint)x, (jint)y);
}

void emitString(char* msg)
{
	while (*msg!=0)
	{
		emitChar(*msg++);
	}
}




// exécute le programme f qui est une fonction
//  int f()
// mode=0 : ARM
// mode=1 : THUMB
int Execute(void*f,int mode, void** rtl)
{
	if (mode!=0)
	{   // in thumb mode, force an odd addres
		// because f call is performed by instruction BX Rn
		// where Rn contains f address
		f=(void*)((long)f|1);
	}
	return ((int(*)(void**))f)(rtl);
}


void emitHexa(long unsigned x)
{
	char buffer[32];
	sprintf(buffer,"%08x\n",x);
	emitString(buffer);
}

// the array of run time library system calls
void* rtl[58];

// load and execute the file given by name jname
//------------------------------------------------
jint Java_pp_compiler_Exec_execute(JNIEnv* env, jobject obj, jstring jname)
{

	const char*path;
	long unsigned* prog_mem;
	int e,f;
	int fs;

  	// save parameters
	Env=env;
	Obj=obj;

	// compute the identifier of the Java Method used by the compiler
	jclass cls = (*env)->GetObjectClass(env, obj);
  	emitcharID = (*env)->GetMethodID(env, cls, "emitChar", "(C)V");
  	getcharID =  (*env)->GetMethodID(env, cls, "getChar", "()C");
  	clrscrID =  (*env)->GetMethodID(env, cls, "clrscr", "()V");
  	wherexID =  (*env)->GetMethodID(env, cls, "WhereX", "()I");
  	whereyID =  (*env)->GetMethodID(env, cls, "WhereY", "()I");
  	screenWidthID =  (*env)->GetMethodID(env, cls, "ScreenWidth", "()I");
  	screenHeightID =  (*env)->GetMethodID(env, cls, "ScreenHeight", "()I");
  	gotoxyID = (*env)->GetMethodID(env, cls, "GotoXY", "(II)V");

    // check if one ov the method has not been created
  	if ((emitcharID==NULL)||
  		(getcharID==NULL)||
  		(clrscrID==NULL)||
  		(wherexID==NULL)||
  		(whereyID==NULL)||
  		(screenWidthID==NULL)||
  		(screenHeightID==NULL)|
  		(gotoxyID== NULL) ) return -1;  // Humm, method not found

	// get the path of the file to compile
    path=(*env)->GetStringUTFChars(env,jname,NULL);
	if (path==NULL) return -2;       // out of memory

    int fd=open(path,O_CREAT|O_RDWR,S_IRWXU);
    if (fd==-1) return -3;    // unable to open the file

    fs=fileSize(fd);
    // map le fichier en mémoire
    prog_mem=mmap(0, // start
    	       fs,  // length
    	       PROT_EXEC|PROT_READ|PROT_WRITE, // Prot flag
    	       MAP_PRIVATE|MAP_LOCKED,
    	       fd,
    	       0);
    close(fd);
    if (prog_mem==(void*)-1)
    {
    	return -4; // unable to map the file in memory
    }


    // run time librairy assignement
    rtl[0]=get_memory;
    rtl[1]=free_memory;
    rtl[2]=write_string;
    rtl[3]=new_line;
    rtl[4]=write_integer;
    rtl[5]=write_float;
    rtl[6]=write_boolean;
    rtl[7]=write_char;
    rtl[8]=write_fixed;
    rtl[9]=intToReal;
    rtl[10]=uintToReal;
    rtl[11]=real_add;
    rtl[12]=real_sub;
    rtl[13]=real_mul;
    rtl[14]=real_div;
    rtl[15]=real_sqrt;
    rtl[16]=float64_round;
    rtl[17]=float64_trunc;
    rtl[18]=float64_eq;
    rtl[19]=float64_le;
    rtl[20]=float64_lt;
    rtl[21]=write_unsigned;
    rtl[22]=real_ln;
    rtl[23]=real_exp;
    rtl[24]=real_atan;
    rtl[25]=real_sin;
    rtl[26]=real_cos;
    rtl[27]=RewriteOutput;
    rtl[28]=ResetInput;
    rtl[29]=Put;
    rtl[30]=clrscr;
    rtl[31]=ReadString;//GetCharConsole;
    rtl[32]=ReadReal;
    rtl[33]=ReadInt;
    rtl[34]=Eof;
    rtl[35]=Get;
    rtl[36]=FileAccess;
    rtl[37]=ReadChar;
    rtl[38]=Eoln;
    rtl[39]=FlushLine;
    rtl[40]=Reset;
    rtl[41]=Rewrite;
    rtl[42]=Close;
    rtl[43]=wherex;
    rtl[44]=wherey;
    rtl[45]=screenwidth;
    rtl[46]=screenheight;
    rtl[47]=gotoxy;
    
    rtl[48]=SetSingle;
    rtl[49]=SetIntervall;
    rtl[50]=IsSubset;
    rtl[51]=SetEq;
    rtl[52]=IsInSet;
    rtl[53]=SetAdd;
    rtl[54]=SetMul;
    rtl[55]=SetSub;
    rtl[56]=SetXor;
    rtl[57]=ClearMem;



    // execute the program in mode arm and passing the rtl as parameter
    e=Execute(prog_mem,0,rtl);

    // unmap the memory page
    f=munmap(prog_mem,fs);
    if (f!=0) return -5;

    return e;
}

jint Java_pp_compiler_Exec_errno(JNIEnv* env, jobject obj)
{
	return errno;
}

