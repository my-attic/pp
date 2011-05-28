#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include "rtl.h"

// the run-time library
//-----------------------
void emitChar(char c);       // defined in the loader
void emitString(char* msg);
char getChar();
void clrscr();


// mode of files
typedef enum
{
	GEN_UNDEF,  // generation mode with undefined data
	GEN_DEF,	// generation mode with data defined in buffer
	INSP_UNDEF,	// inspection mode with undefined data in the buffer
	INSP_DEF,	// inspection mode with data defined in buffer
	INSP_BATCH,	// bufferised inspection mode
	INSP_EOF	// inspection mode and end of file reached
} fileMode_t;


// data structure for buffered input
#define MAX_INPUT 80
struct
{
	int Last;  	// number of char in the input buffer
	int First;	// index of the first character
	char Data[MAX_INPUT]; // the array of the caracters
	// -----[-------------------------------[---------------
	//       ^ first char to be read         ^ Last = first empty entry
} con;

// rewrite the output for generation
// 108
void RewriteOutput(fileDesc_t*fd)
{
	fd->file=NULL;
	fd->mode=GEN_UNDEF;
	fd->size=1;
	clrscr();
}

// reset the input for inspection
// 112
void ResetInput(fileDesc_t*fd)
{
	fd->file=NULL;
	fd->mode=INSP_UNDEF;
	fd->size=1;
	con.Last=0;
	con.First=0;
}

// put
// 116
// returns error code. 0 if no error
int Put(fileDesc_t* fd)
{
	fd->mode=GEN_UNDEF;
	if (fd->file==NULL)
	{  // write on console the character c
		emitChar(fd->c);
		return 0;
	}
	if (fwrite((void*)&(fd->c),fd->size,1,fd->file)!=1)
	{ // if no item has been written
		return RTE_PUT;
	}
	return 0;
}

// putchar
int PutChar(fileDesc_t* fd, char c)
{
	fd->c=c;
	return Put(fd);
}

// emit a string "pc" on the stream "fd"
static void PutString(fileDesc_t*fd, char*pc)
{
	while (*pc!=0) PutChar(fd,*pc++);
}

// +++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// Input procedures

// wait a char on the console
static char GetCharConsole()
{
	return getChar();
}


// minimal line editor
void GetLine()
{
	int exit_flag;
	char c;
	con.First=0;
	con.Last=0;
	exit_flag=0;
	do
	{
		c=getChar();
		switch(c)
		{
		case 10: // return
			exit_flag=1;
			break;
		case 8:
		//case 127: // backspace
			if (con.Last>0)
			{
				con.Last--;
				emitChar(c);
			}
			break;
		default:
			if ((c>=' ')&&(con.Last<MAX_INPUT))
			{
				con.Data[con.Last++]=c;
				emitChar(c);
			}
		}
	}
	while (exit_flag==0);
	emitChar(10); // return
	con.Data[con.Last++]=10;
}

// peek a char in the stream fd
int Peek(fileDesc_t*fd)
{
	if (fd->file==NULL)
	{	// console
		if (fd->mode==INSP_BATCH)
		{
			if (con.First>=con.Last) GetLine();
			fd->c=con.Data[con.First++];
			if (con.First==con.Last) fd->mode=INSP_DEF;
		}
		else
		{
			fd->c=GetCharConsole();
			fd->mode=INSP_DEF;
		}
	}
	else
	{	// read on a file
		size_t s=fread(&fd->c,fd->size,1,fd->file);

		if (s==1) // s is the number of items read !! not the byte size
		{
			fd->mode=INSP_DEF;
			return 0;
		}
		if (ferror(fd->file)) return RTE_PEEK;
		// if there is no error, then it is end of file
		fd->mode=INSP_EOF;
		fd->c=0;
	}
	return 0;

}

// returns True if end of file is reached
int Eof(fileDesc_t*fd)
{
	switch (fd->mode)
	{
	case GEN_DEF:	// eof is true in generation mode
	case GEN_UNDEF:
	case INSP_EOF:
		return 1;
	//case INSP_DEF:
		//return 0;
	case INSP_UNDEF:
		Peek(fd);
		return fd->mode==INSP_EOF;
	default: return 0;
	}
}

int Get(fileDesc_t *fd)
{
	switch(fd->mode)
	{
	case INSP_UNDEF:
	case INSP_BATCH:
		return Peek(fd);
	case INSP_DEF:
		fd->mode=INSP_UNDEF;
		return 0;
	default:
		// Runtime error "not open in inspection mode"
		return RTE_GET;
	}

}

// update the buffer variable of a file
// procedure to be called while a file acces
// is performed v:=f^
void FileAccess(fileDesc_t*fd)
{
	if (fd->mode==INSP_UNDEF) Peek(fd);
	// TODO manage error returned by peek
}

// read a char on a text.
// this procedure is the core for other read procedures on text file
static void ReadCharText(fileDesc_t*fd)
{
	if (Eof(fd)) fd->c=0;
	else
	{
		FileAccess(fd);
		Get(fd);
	}
}

// retunrs True if end of line is reached on a text
int Eoln(fileDesc_t*fd)
{
	  if (fd->mode==INSP_UNDEF)
	  {
		  fd->mode=INSP_BATCH;
		  Peek(fd);
	  }
//	  FileAccess(fd);
	  if ( (fd->file!=NULL) && Eof(fd)) return 1;
	  return fd->c==10;
}

// this procedure is Called at the end
// of a readln statement
// it only occurs in insp_batch mode
// it flush the buffer and switch to insp_undef mode

void FlushLine(fileDesc_t *fd)
//-----------------------------
{// TODO : manage the error returned by "Get"
	  while (!Eoln(fd)) Get(fd);
	  fd->mode=INSP_UNDEF;
}


// read a char in a text stream
//------------------------------
void ReadChar(fileDesc_t* fd, char*c)
{ // TODO : manage the error returned by "Get"
  while (Eoln(fd)) Get(fd);
  //ReadCharText(fd);
  *c=fd->c;
  Get(fd);
}

// read a string on the input stream
void ReadString(fileDesc_t* f, char*s)
{
	char c;
	int i;
	i=0;
	if (!Eoln(f))
	{
		do
		{
			s[i++]=f->c;
			ReadCharText(f);
		}
		while (!Eoln(f));
	}
	s[i]=0;
}


// returns -1 it c is not a digit character
// else returns integer value
static int Digit(char c)
{
	if ((c<'0')||(c>'9'))  return -1;
	return c-'0';
}

// read an integer in the input stream
void ReadInt(fileDesc_t* f, int* j)
{
	int negate;
	int x;
    int d;

    while (Eoln(f)) Get(f);
    while (f->c==' ') ReadCharText(f);
    negate=0;
    switch(f->c)
    {
    case '-':
    	negate=1;
    case '+':
    	ReadCharText(f);
    	break;
    }

    x=Digit(f->c);
    if (x<0)
    {
    	*j=0;
    	return; // TODO : manage runtime error
    }
    do
    {
    	ReadCharText(f);
		d=Digit(f->c);
		if (d>=0) x=x*10+d;
    }
    while (d>=0);
	if (negate) *j=-x; else *j=x;
}


// read a real on the input stream
void ReadReal(fileDesc_t*f, double*r)
{
	while (Eoln(f)) Get(f); // force batch mode with non empty string
	sscanf(con.Data,"%lf",r);
}
/*
procedure ReadReal;//(var f:file_desc;var r:real);
var
   d      : integer; // current digit value
   x,a,e  : real;
   expo   : integer; // exponent
   dige   : integer;
   negate : boolean; // negative real
   expneg : boolean; // negative exponent
begin
  while eoln(f) do get(f); // force batch mode with non empty string
  negate:=false;
  expneg:=false;
  x:=0.0;
  expo:=0;
  dige:=0;
  while f.c=' ' do ReadCharFile(f);
  case f.c of
    '-' : begin negate:=true; ReadCharFile(f) end;
    '+' : ReadCharFile(f);
  end;
  d:=Digit(f.c);
  if d<0 then RunTimeError(10);
  repeat
    ReadCharFile(f);
    x:=x*10+d;
    d:=Digit(f.c);
  until d<0;
  if f.c='.' then
  begin
    ReadCharFile(f);
    d:=Digit(f.c);
    if d<0 then RunTimeError(11);
    repeat
      ReadCharFile(f);
      dige:=dige-1;
      x:=x*10+d;
      d:=Digit(f.c);
    until d<0;
  end;
  case f.c of
    'e','E':
    begin
      ReadCharFile(f);
      case f.c of
        '+': ReadCharFile(f);
        '-': begin expneg:=true; ReadCharFile(f); end;
      end;
      d:=Digit(f.c);
      if d<0 then RunTimeError(12);
      repeat
        ReadCharFile(f);
        expo:=expo*10+d;
        d:=Digit(f.c);
      until d<0;
    end;
  end;
  if expneg then expo:=-expo;
  expo:=expo+dige;
  expneg:=expo<0;
  expo:=abs(expo);
  a:=10;
  e:=1;
  if expo<>0 then
  begin // compute e = 10**expo
    while expo<>1 do
    begin // skip last squaring to avoid subnormal underflow
      if odd(expo) then e:=e*a;
      a:=sqr(a);
      expo:=expo shr 1;
    end;
    if expneg then x:=(x/e)/a
    else x:=(x*e)*a;
  end;
  if negate then r:=-x else r:=x;
end;

*/

// assign to buffer the full file path
static void fullPath(char*buffer,char*name)
{
    strcpy(buffer,"/data/data/pp.compiler/");
    strcat(buffer,name);
}


// initialise a file for inspection
// 160
int Reset(fileDesc_t*f, uint16_t nBytes, char*name)
{
	char path[128];

	fullPath(path,name),
	f->file=fopen(path,"r");
	if (f->file==NULL) return RTE_RESET;
	f->mode=INSP_UNDEF;
	f->size=nBytes;
	return 0;
}


// open a file in generation mode
// 164
int Rewrite(fileDesc_t*f, uint16_t nBytes, char*name)
{
	char path[128];
	fullPath(path,name),
	f->file=fopen(path,"w");
	if (f->file==NULL) return RTE_REWRITE;
	f->mode=GEN_UNDEF;
	f->size=nBytes;
	return 0;
}

// close a file
// 168
void Close(fileDesc_t*f)
{
	if (f->file!=NULL) fclose(f->file);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

// 1. memory allocation
void* get_memory(int size)
{
	return malloc(size);
}

// 2. memory dispose
void free_memory(void*p)
{
	free(p);
}

// 3. string writing
void write_string(char*s, int width, fileDesc_t*fd)
{
	char buffer[256];
	char*pc=buffer;
	snprintf(buffer,256,"%*s",width,s);
	PutString(fd,buffer);
}

// 4. newline
int new_line(fileDesc_t*fd)
{
	return PutChar(fd,'\n');
}

// 5. write integer
void write_integer(int x, int width,fileDesc_t*fd)
{
	char buffer[256];
	if (width<0) width=0;
	sprintf(buffer,"%*ld",width,x);
	PutString(fd, buffer);
}



// 20
void write_float(double x, int width, fileDesc_t*fd)
{
	char buffer[256];
	if (width<0) width=0;
	sprintf(buffer,"%*e",width,x);
	PutString(fd,buffer);
}

// 24
void write_boolean(int x, int width, fileDesc_t*fd)
{
	char buffer[256];
	if (width<0) width=0;
	sprintf(buffer,"%*s",width,x?"true":"false");
	PutString(fd,buffer);
}

// 28
void write_char(int x, int width,fileDesc_t* fd)
{
	char buffer[256];
	if (width<0) width=0;
	sprintf(buffer,"%*c",width,(char)x);
	PutString(fd,buffer);
}

// 32
void write_fixed(double x, int width, int prec, fileDesc_t*fd)
{
	char buffer[256];
	if (prec<0) prec=0;
	if (width<0) width=0;
	sprintf(buffer,"%*.*lf",width,prec,x);
	PutString(fd,buffer);
}


// 84 write unsigned
void write_unsigned(unsigned int x, int width, fileDesc_t*fd)
{
	char buffer[256];
	if (width<0) width=0;
	sprintf(buffer,"%*lu",width,x);
	PutString(fd,buffer);
}


// reals
//--------



int countLeadingZeros(uint32_t a)
{
	asm volatile ("clz r0,r0");
}
/*
static int countLeadingZeros(uint32_t a)
{
    int count=0;
    if (a==0) return 32;
    if (a<0x10000)
    {
        count+=16;
        a<<=16;
    }
    if (a<0x1000000)
    {
        count+=8;
        a<<=8;
    }
    if (a<0x10000000)
    {
        count+=4;
        a<<=4;
    }
    if (a<0x40000000)
    {
        count+=2;
        a<<=2;
    }
    if (a<0x80000000)
    {
        count+=1;
    }
    return count;
}
*/
/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point types.
*----------------------------------------------------------------------------*/

// 36
double intToReal(int a)
{
    number_t z;
    if (a==0) return 0.0;
    int sign=0;
    if (a<0)
    {
        sign=0x80000000;
        a=-a;
    }
    int count = countLeadingZeros( a ) - 11;
    if ( count>=0 )
    {
        z.r.hi = a<<count;
        z.r.lo= 0;
    }
    else
    {
        z.r.hi=a>>-count;
        z.r.lo=a<<(32+count);
    }
    z.r.hi+= sign + ((0x412-count) << 20);
    return z.d;
}

//40
double uintToReal(uint32_t a)
{
    number_t z;
    if (a==0) return 0;
    int count = countLeadingZeros( a ) - 11;
    if ( count>=0 )
    {
        z.r.hi = a<<count;
        z.r.lo= 0;
    }
    else
    {
        z.r.hi=a>>-count;
        z.r.lo=a<<(32+count);
    }
    z.r.hi+= ((0x412-count) << 20);
    return z.d;
}


/*============================================================================

Float 64 routines frome the softfloat package written by John R. Hauser
*/

typedef char flag;



/*----------------------------------------------------------------------------
| Software IEC/IEEE floating-point rounding mode.
*----------------------------------------------------------------------------*/

typedef enum
{
    float_round_nearest_even = 0,
    float_round_to_zero      = 1
} rounding_mode_t;

/*----------------------------------------------------------------------------
| Shifts the 64-bit value formed by concatenating `a0' and `a1' right by the
| number of bits given in `count'.  If any nonzero bits are shifted off, they
| are ``jammed'' into the least significant bit of the result by setting the
| least significant bit to 1.  The value of `count' can be arbitrarily large;
| in particular, if `count' is greater than 64, the result will be either 0
| or 1, depending on whether the concatenation of `a0' and `a1' is zero or
| nonzero.  The result is broken into two 32-bit pieces which are stored at
| the locations pointed to by `z0Ptr' and `z1Ptr'.
*----------------------------------------------------------------------------*/

// the same on place
void shift64RightJammingOnPlace(int count, uint32_t *z0, uint32_t *z1 )
{

    int negCount = ( - count ) & 31; // 32-coun limited to 5 bits

    if ( count == 0 ) return;
    if ( count < 32 )
    {
        *z1 = ( *z0<<negCount ) | ( *z1>>count ) | ( ( *z1<<negCount ) != 0 );
        *z0>>=count;
        return;
    }
    if ( count == 32 )
    {
        *z1 = *z0 | ( *z1 != 0 );
    }
    else if ( count < 64 )
    {
            *z1 = ( *z0>>( count & 31 ) ) | ( ( ( *z0<<negCount ) | *z1 ) != 0 );
    }
    else
    {
            *z1 = ( ( *z0 | *z1 ) != 0 );
    }
    *z0 = 0;
}

/*----------------------------------------------------------------------------*\
| Adds the 64-bit value formed by concatenating `a0' and `a1' to the 64-bit    !
| value formed by concatenating `b0' and `b1'.  Addition is modulo 2^64, so    !
| any carry out is lost.  The result is broken into two 32-bit pieces which    !
| are stored at the locations pointed to by `z0Ptr' and `z1Ptr'.               !
\*----------------------------------------------------------------------------*/

void
 add64(
     uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1, uint32_t *z0Ptr, uint32_t *z1Ptr )
{
    uint32_t z1;

    z1 = a1 + b1;
    *z1Ptr = z1;
    *z0Ptr = a0 + b0 + ( z1 < a1 );

}



/*----------------------------------------------------------------------------
| Subtracts the 64-bit value formed by concatenating `b0' and `b1' from the
| 64-bit value formed by concatenating `a0' and `a1'.  Subtraction is modulo
| 2^64, so any borrow out (carry out) is lost.  The result is broken into two
| 32-bit pieces which are stored at the locations pointed to by `z0Ptr' and
| `z1Ptr'.
*----------------------------------------------------------------------------*/

void
 sub64(
     uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1, uint32_t *z0Ptr, uint32_t *z1Ptr )
{

    *z1Ptr = a1 - b1;
    *z0Ptr = a0 - b0 - ( a1 < b1 );

}

/*----------------------------------------------------------------------------
| Multiplies `a' by `b' to obtain a 64-bit product.  The product is broken
| into two 32-bit pieces which are stored at the locations pointed to by
| `z0Ptr' and `z1Ptr'.
*----------------------------------------------------------------------------*/
void mul32To64(uint32_t a, uint32_t b, uint32_t* rhi, uint32_t*rlo)
{
	asm volatile
	("stmfd r13!,{r2,r3}\n"
	 "umull r2,r3,r0,r1\n"
	 "ldmfd r13!,{r0,r1}\n"
	 "str r3,[r0]\n"
	 "str r2,[r1]");
}
/*
void mul32To64A(uint32_t a, uint32_t b, uint32_t c, uint32_t* rhi, uint32_t*rlo)
{
	asm volatile
	("stmfd r13!,{r3}\n"
	 "mov r3,#0\n"
	 "umlal r3,r2,r0,r1\n"
	 "ldmfd r13!,{r0,r1}\n"
	 "str r2,[r0]\n"
	 "str r3,[r1]");
}
*/
/*
void mul32To64( uint32_t a, uint32_t b, uint32_t *z0Ptr, uint32_t *z1Ptr )
{
    uint16_t aHigh, aLow, bHigh, bLow;
    uint32_t z0, zMiddleA, zMiddleB, z1;

    aLow = a;
    aHigh = a>>16;
    bLow = b;
    bHigh = b>>16;
    z1 = ( (uint32_t) aLow ) * bLow;
    zMiddleA = ( (uint32_t) aLow ) * bHigh;
    zMiddleB = ( (uint32_t) aHigh ) * bLow;
    z0 = ( (uint32_t) aHigh ) * bHigh;
    zMiddleA += zMiddleB;
    z0 += ( ( (uint32_t) ( zMiddleA < zMiddleB ) )<<16 ) + ( zMiddleA>>16 );
    zMiddleA <<= 16;
    z1 += zMiddleA;
    z0 += ( z1 < zMiddleA );
    *z1Ptr = z1;
    *z0Ptr = z0;

}
*/
//  a * b + c --> z0-z1
void mul32To64AndAccumulate(uint32_t a, uint32_t b, int32_t c, uint32_t *z0Ptr, uint32_t *z1Ptr )
{
    mul32To64(a,b,z0Ptr,z1Ptr);
    *z1Ptr+=c;
    *z0Ptr+=(*z1Ptr<c);
}

/*----------------------------------------------------------------------------
| Returns an approximation to the 32-bit integer quotient obtained by dividing
| `b' into the 64-bit value formed by concatenating `a0' and `a1'.  The
| divisor `b' must be at least 2^31.  If q is the exact quotient truncated
| toward zero, the approximation returned lies between q and q + 2 inclusive.
| If the exact quotient q is larger than 32 bits, the maximum positive 32-bit
| unsigned integer is returned.
*----------------------------------------------------------------------------*/

static uint32_t estimateDiv64To32( uint32_t a0, uint32_t a1, uint32_t b )
{
    uint32_t b0, b1;
    uint32_t rem0, rem1, term0, term1;
    uint32_t z;

    if ( b <= a0 ) return 0xFFFFFFFF;
    b0 = b>>16;
    z = ( b0<<16 <= a0 ) ? 0xFFFF0000 : ( a0 / b0 )<<16;
    mul32To64( b, z, &term0, &term1 );
    sub64( a0, a1, term0, term1, &rem0, &rem1 );
    while ( ( (int32_t) rem0 ) < 0 )
    {
        z -= 0x10000;
        b1 = b<<16;
        add64( rem0, rem1, b0, b1, &rem0, &rem1 );
    }
    rem0 = ( rem0<<16 ) | ( rem1>>16 );
    z |= ( b0<<16 <= rem0 ) ? 0xFFFF : rem0 / b0;
    return z;

}


/*----------------------------------------------------------------------------
| Returns an approximation to the square root of the 32-bit significand given
| by `a'.  Considered as an integer, `a' must be at least 2^31.  If bit 0 of
| `aExp' (the least significant bit) is 1, the integer returned approximates
| 2^31*sqrt(`a'/2^31), where `a' is considered an integer.  If bit 0 of `aExp'
| is 0, the integer returned approximates 2^31*sqrt(`a'/2^30).  In either
| case, the approximation returned lies strictly within +/-2 of the exact
| value.
*----------------------------------------------------------------------------*/

static uint32_t estimateSqrt32( int16_t aExp, uint32_t a )
{
    static const int32_t sqrtOddAdjustments[] = {
        0x0004, 0x0022, 0x005D, 0x00B1, 0x011D, 0x019F, 0x0236, 0x02E0,
        0x039C, 0x0468, 0x0545, 0x0631, 0x072B, 0x0832, 0x0946, 0x0A67
    };
    static const int32_t sqrtEvenAdjustments[] = {
        0x0A2D, 0x08AF, 0x075A, 0x0629, 0x051A, 0x0429, 0x0356, 0x029E,
        0x0200, 0x0179, 0x0109, 0x00AF, 0x0068, 0x0034, 0x0012, 0x0002
    };
    //int8 index;
    uint32_t z;

    //index = ( a>>27 ) & 15;
    if ( aExp & 1 ) {
        z = 0x4000 + ( a>>17 ) - sqrtOddAdjustments[ ( a>>27 ) & 15 ];
        z = ( ( a / z )<<14 ) + ( z<<15 );
        a >>= 1;
    }
    else {
        z = 0x8000 + ( a>>17 ) - sqrtEvenAdjustments[ ( a>>27 ) & 15 ];
        z = a / z + z;
        z = ( 0x20000 <= z ) ? 0xFFFF8000 : ( z<<15 );
        if ( z <= a ) return (uint32_t) ( ( (int32_t) a )>>1 );
    }
    return ( ( estimateDiv64To32( a, 0, z ) )>>1 ) + ( z>>1 );

}

/*----------------------------------------------------------------------------
| Shifts the 96-bit value formed by concatenating `a0', `a1', and `a2' right
| by 32 _plus_ the number of bits given in `count'.  The shifted result is
| at most 64 nonzero bits; these are broken into two 32-bit pieces which are
| stored at the locations pointed to by `z0Ptr' and `z1Ptr'.  The bits shifted
| off form a third 32-bit result as follows:  The _last_ bit shifted off is
| the most-significant bit of the extra result, and the other 31 bits of the
| extra result are all zero if and only if _all_but_the_last_ bits shifted off
| were all zero.  This extra result is stored in the location pointed to by
| `z2Ptr'.  The value of `count' can be arbitrarily large.
|     (This routine makes more sense if `a0', `a1', and `a2' are considered
| to form a fixed-point value with binary point between `a1' and `a2'.  This
| fixed-point value is shifted right by the number of bits given in `count',
| and the integer part of the result is returned at the locations pointed to
| by `z0Ptr' and `z1Ptr'.  The fractional part of the result may be slightly
| corrupted as described above, and is returned at the location pointed to by
| `z2Ptr'.)
*----------------------------------------------------------------------------*/

void
 shift64ExtraRightJamming(
     uint32_t a0,
     uint32_t a1,
     uint32_t a2,
     int count,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr
 )
{
    uint32_t z0, z1, z2;
    int negCount = ( - count ) & 31;

    if ( count == 0 ) {
        z2 = a2;
        z1 = a1;
        z0 = a0;
    }
    else {
        if ( count < 32 ) {
            z2 = a1<<negCount;
            z1 = ( a0<<negCount ) | ( a1>>count );
            z0 = a0>>count;
        }
        else {
            if ( count == 32 ) {
                z2 = a1;
                z1 = a0;
            }
            else {
                a2 |= a1;
                if ( count < 64 ) {
                    z2 = a0<<negCount;
                    z1 = a0>>( count & 31 );
                }
                else {
                    z2 = ( count == 64 ) ? a0 : ( a0 != 0 );
                    z1 = 0;
                }
            }
            z0 = 0;
        }
        z2 |= ( a2 != 0 );
    }
    *z2Ptr = z2;
    *z1Ptr = z1;
    *z0Ptr = z0;

}

/*----------------------------------------------------------------------------
| Shifts the 64-bit value formed by concatenating `a0' and `a1' left by the
| number of bits given in `count'.  Any bits shifted off are lost.  The value
| of `count' must be less than 32.  The result is broken into two 32-bit
| pieces which are stored at the locations pointed to by `z0Ptr' and `z1Ptr'.
*----------------------------------------------------------------------------*/

void
 shortShift64Left(
     uint32_t a0, uint32_t a1, int count, uint32_t *z0Ptr, uint32_t *z1Ptr )
{

    *z1Ptr = a1<<count;
    *z0Ptr =
        ( count == 0 ) ? a0 : ( a0<<count ) | ( a1>>( ( - count ) & 31 ) );

}


/*----------------------------------------------------------------------------
| Adds the 96-bit value formed by concatenating `a0', `a1', and `a2' to the
| 96-bit value formed by concatenating `b0', `b1', and `b2'.  Addition is
| modulo 2^96, so any carry out is lost.  The result is broken into three
| 32-bit pieces which are stored at the locations pointed to by `z0Ptr',
| `z1Ptr', and `z2Ptr'.
*----------------------------------------------------------------------------*/

void incr96_64(
     uint32_t b1,
     uint32_t b2,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr
 )
{
    int carry;
    *z2Ptr+=b2;
    carry=(*z2Ptr<b2);
    *z1Ptr+=carry;
    carry=(*z1Ptr<carry);
    *z1Ptr+=b1;
    carry|=(*z1Ptr<b1);
    *z0Ptr+=carry;
}


/*----------------------------------------------------------------------------
| Subtracts the 96-bit value formed by concatenating `b0', `b1', and `b2' from
| the 96-bit value formed by concatenating `a0', `a1', and `a2'.  Subtraction
| is modulo 2^96, so any borrow out (carry out) is lost.  The result is broken
| into three 32-bit pieces which are stored at the locations pointed to by
| `z0Ptr', `z1Ptr', and `z2Ptr'.
*----------------------------------------------------------------------------*/

void
 sub96(
     uint32_t a0,
     uint32_t a1,
     uint32_t a2,
     uint32_t b0,
     uint32_t b1,
     uint32_t b2,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr
 )
{
    uint32_t z0, z1, z2;
    int borrow0, borrow1;

    z2 = a2 - b2;
    borrow1 = ( a2 < b2 );
    z1 = a1 - b1;
    borrow0 = ( a1 < b1 );
    z0 = a0 - b0;
    z0 -= ( z1 < borrow1 );
    z1 -= borrow1;
    z0 -= borrow0;
    *z2Ptr = z2;
    *z1Ptr = z1;
    *z0Ptr = z0;

}

/*----------------------------------------------------------------------------
| Multiplies the 64-bit value formed by concatenating `a0' and `a1' by `b'
| to obtain a 96-bit product.  The product is broken into three 32-bit pieces
| which are stored at the locations pointed to by `z0Ptr', `z1Ptr', and
| `z2Ptr'.
*----------------------------------------------------------------------------*/
void mul64By32To96
(
     uint32_t a0,
     uint32_t a1,
     uint32_t b,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr
 )
{
    //uint32_t z0, z1, z2, more1;
    mul32To64( a1, b, z1Ptr, z2Ptr );
    /*
    mul32To64( a0, b, &z0, &more1 );
    add64( z0, more1, 0, z1, &z0, &z1 );
    */
    mul32To64AndAccumulate(a0,b,*z1Ptr,z0Ptr,z1Ptr);
    //*z2Ptr = z2;
    //*z1Ptr = z1;
    //*z0Ptr = z0;

}
/*
void
 mul64By32To96(
     uint32_t a0,
     uint32_t a1,
     uint32_t b,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr
 )
{
    uint32_t z0, z1, z2, more1;
    mul32To64( a1, b, &z1, &z2 );
    mul32To64( a0, b, &z0, &more1 );
    add64( z0, more1, 0, z1, &z0, &z1 );
    *z2Ptr = z2;
    *z1Ptr = z1;
    *z0Ptr = z0;

}
*/
/*----------------------------------------------------------------------------
| Multiplies the 64-bit value formed by concatenating `a0' and `a1' to the
| 64-bit value formed by concatenating `b0' and `b1' to obtain a 128-bit
| product.  The product is broken into four 32-bit pieces which are stored at
| the locations pointed to by `z0Ptr', `z1Ptr', `z2Ptr', and `z3Ptr'.
*----------------------------------------------------------------------------*/
/*
void
 mul64To128(
     uint32_t a0,
     uint32_t a1,
     uint32_t b0,
     uint32_t b1,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr,
     uint32_t *z3Ptr
 )
{
    uint32_t z0, z1, z2, z3;
    uint32_t more1, more2;

    mul32To64( a1, b1, &z2, &z3 );
    mul32To64( a1, b0, &z1, &more2 );
    add64( z1, more2, 0, z2, &z1, &z2 );
    mul32To64( a0, b0, &z0, &more1 );
    add64( z0, more1, 0, z1, &z0, &z1 );
    mul32To64( a0, b1, &more1, &more2 );
    add64( more1, more2, 0, z2, &more1, &z2 );
    add64( z0, z1, 0, more1, &z0, &z1 );
    *z3Ptr = z3;
    *z2Ptr = z2;
    *z1Ptr = z1;
    *z0Ptr = z0;

}
*/
void
 mul64To128(
     uint32_t a0,
     uint32_t a1,
     uint32_t b0,
     uint32_t b1,
     uint32_t *z0Ptr,
     uint32_t *z1Ptr,
     uint32_t *z2Ptr,
     uint32_t *z3Ptr
 )
{
    //uint32_t z0, z1, z2, z3;
    uint32_t more1; //, more2;
//printf(".");
    mul32To64( a1, b1, z2Ptr, z3Ptr );
    /*
    mul32To64( a1, b0, &z1, &more2 );
    add64( z1, more2, 0, z2, &z1, &z2 );
    */
    mul32To64AndAccumulate(a1,b0,*z2Ptr,&more1,z2Ptr);
    /*
    mul32To64( a0, b0, &z0, &more1 );
    add64( z0, more1, 0, z1, &z0, &z1 );
    mul32To64( a0, b1, &more1, &more2 );
    add64( more1, more2, 0, z2, &more1, &z2 );
    add64( z0, z1, 0, more1, &z0, &z1 );
    */
    mul32To64AndAccumulate(a0,b1,*z2Ptr,z1Ptr,z2Ptr);
    mul32To64AndAccumulate(a0,b0,*z1Ptr,z0Ptr,z1Ptr);
    *z1Ptr+=more1;
    *z0Ptr+=(*z1Ptr<more1);
    //*z3Ptr = z3;
    //*z2Ptr = z2;
    //*z1Ptr = z1;
    //*z0Ptr = z0;

}


/*----------------------------------------------------------------------------
| Returns 1 if the 64-bit value formed by concatenating `a0' and `a1' is
| equal to the 64-bit value formed by concatenating `b0' and `b1'.  Otherwise,
| returns 0.
*----------------------------------------------------------------------------*/

inline flag eq64( uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1 )
{

    return ( a0 == b0 ) && ( a1 == b1 );

}

/*----------------------------------------------------------------------------
| Returns 1 if the 64-bit value formed by concatenating `a0' and `a1' is less
| than or equal to the 64-bit value formed by concatenating `b0' and `b1'.
| Otherwise, returns 0.
*----------------------------------------------------------------------------*/

inline flag le64( uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1 )
{

    return ( a0 < b0 ) || ( ( a0 == b0 ) && ( a1 <= b1 ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the 64-bit value formed by concatenating `a0' and `a1' is less
| than the 64-bit value formed by concatenating `b0' and `b1'.  Otherwise,
| returns 0.
*----------------------------------------------------------------------------*/

inline flag lt64( uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1 )
{

    return ( a0 < b0 ) || ( ( a0 == b0 ) && ( a1 < b1 ) );

}


/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a NaN;
| otherwise returns 0.
*----------------------------------------------------------------------------*/

flag float64_is_nan( real_t a )
{

    return
           ( 0xFFE00000 <= (uint32_t) ( a.hi<<1 ) )
        && ( a.lo || ( a.hi & 0x000FFFFF ) );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is a signaling
| NaN; otherwise returns 0.
*----------------------------------------------------------------------------*/

flag float64_is_signaling_nan( real_t a )
{

    return
           ( ( ( a.hi>>19 ) & 0xFFF ) == 0xFFE )
        && ( a.lo || ( a.hi & 0x0007FFFF ) );

}


/*----------------------------------------------------------------------------
| Takes two double-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static real_t propagateFloat64NaN( real_t a, real_t b )
{
    flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = float64_is_nan( a );
    aIsSignalingNaN = float64_is_signaling_nan( a );
    bIsNaN = float64_is_nan( b );
    bIsSignalingNaN = float64_is_signaling_nan( b );
    a.hi |= 0x00080000;
    b.hi |= 0x00080000;
    //if ( aIsSignalingNaN | bIsSignalingNaN ) float_raise( float_flag_invalid );
    if ( aIsSignalingNaN )
    {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b : a;
    }
    else if ( aIsNaN )
    {
        if ( bIsSignalingNaN | ! bIsNaN ) return a;
 returnLargerSignificand:
        if ( lt64( a.hi<<1, a.lo, b.hi<<1, b.lo ) ) return b;
        if ( lt64( b.hi<<1, b.lo, a.hi<<1, a.lo ) ) return a;
        return ( a.hi < b.hi ) ? a : b;
    }
    else
    {
        return b;
    }

}

/*----------------------------------------------------------------------------
| Returns the least-significant 32 fraction bits of the double-precision
| floating-point value `a'.
*----------------------------------------------------------------------------*/

inline uint32_t extractFloat64Frac1( real_t a )
{

    return a.lo;

}

/*----------------------------------------------------------------------------
| Returns the most-significant 20 fraction bits of the double-precision
| floating-point value `a'.
*----------------------------------------------------------------------------*/

inline uint32_t extractFloat64Frac0( real_t a )
{

    return a.hi & 0x000FFFFF;

}

/*----------------------------------------------------------------------------
| Returns the exponent bits of the double-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

inline int16_t extractFloat64Exp( real_t a )
{

    return ( a.hi>>20 ) & 0x7FF;

}

/*----------------------------------------------------------------------------
| Returns the sign bit of the double-precision floating-point value `a'.
*----------------------------------------------------------------------------*/

inline flag extractFloat64Sign( real_t a )
{

    return a.hi>>31;

}

/*----------------------------------------------------------------------------
| Normalizes the subnormal double-precision floating-point value represented
| by the denormalized significand formed by the concatenation of `aSig0' and
| `aSig1'.  The normalized exponent is stored at the location pointed to by
| `zExpPtr'.  The most significant 21 bits of the normalized significand are
| stored at the location pointed to by `zSig0Ptr', and the least significant
| 32 bits of the normalized significand are stored at the location pointed to
| by `zSig1Ptr'.
*----------------------------------------------------------------------------*/

static void normalizeFloat64Subnormal(
     uint32_t aSig0,
     uint32_t aSig1,
     int16_t *zExpPtr,
     uint32_t *zSig0Ptr,
     uint32_t *zSig1Ptr
 )
{
    int shiftCount;

    if ( aSig0 == 0 ) {
        shiftCount = countLeadingZeros( aSig1 ) - 11;
        if ( shiftCount < 0 ) {
            *zSig0Ptr = aSig1>>( - shiftCount );
            *zSig1Ptr = aSig1<<( shiftCount & 31 );
        }
        else {
            *zSig0Ptr = aSig1<<shiftCount;
            *zSig1Ptr = 0;
        }
        *zExpPtr = - shiftCount - 31;
    }
    else {
        shiftCount = countLeadingZeros( aSig0 ) - 11;
        shortShift64Left( aSig0, aSig1, shiftCount, zSig0Ptr, zSig1Ptr );
        *zExpPtr = 1 - shiftCount;
    }

}



/*----------------------------------------------------------------------------
| Returns the result of converting the double-precision floating-point value
| `a' to the 32-bit two's complement integer format.  The conversion is
| performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic---which means in particular that the conversion is rounded
| according to the current rounding mode.  If `a' is a NaN, the largest
| positive integer is returned.  Otherwise, if the conversion overflows, the
| largest integer with the same sign as `a' is returned.
*----------------------------------------------------------------------------*/

static int32_t real_to_int32( real_t a , rounding_mode_t roundingMode)
{
    flag aSign;
    int16_t aExp, shiftCount;
    uint32_t aSig0, aSig1, absZ, aSigExtra;
    int32_t z;

    aSig1 = a.lo; //extractFloat64Frac1( a );
    aSig0 = extractFloat64Frac0( a );
    aExp = extractFloat64Exp( a );
    aSign = extractFloat64Sign( a );
    shiftCount = aExp - 0x413;
    if ( 0 <= shiftCount )
    {
        if ( 0x41E < aExp )
        {
            if ( ( aExp == 0x7FF ) && ( aSig0 | aSig1 ) ) aSign = 0;
            goto invalid;
        }
        shortShift64Left(aSig0 | 0x00100000, aSig1, shiftCount, &absZ, &aSigExtra );
        if ( 0x80000000 < absZ ) goto invalid;
    }
    else {
        aSig1 = ( aSig1 != 0 );
        if ( aExp < 0x3FE ) {
            aSigExtra = aExp | aSig0 | aSig1;
            absZ = 0;
        }
        else {
            aSig0 |= 0x00100000;
            aSigExtra = ( aSig0<<( shiftCount & 31 ) ) | aSig1;
            absZ = aSig0>>( - shiftCount );
        }
    }
    if ( roundingMode == float_round_nearest_even ) {
        if ( (int32_t) aSigExtra < 0 ) {
            ++absZ;
            if ( (uint32_t) ( aSigExtra<<1 ) == 0 ) absZ &= ~1;
        }
        z = aSign ? - absZ : absZ;
    }
    else {
        aSigExtra = ( aSigExtra != 0 );
        if ( aSign ) {
            z = - (   absZ );
//                    + ( ( roundingMode == float_round_down ) & aSigExtra ) );
        }
        else {
            z = absZ; // + ( ( roundingMode == float_round_up ) & aSigExtra );
        }
    }
    if ( ( aSign ^ ( z < 0 ) ) && z )
    {
 invalid:
        //float_raise( float_flag_invalid );
        return aSign ? (int32_t) 0x80000000 : 0x7FFFFFFF;
    }
    //if ( aSigExtra ) float_exception_flags |= float_flag_inexact;
    return z;

}

//64
int32_t float64_round( real_t a )
{
    return real_to_int32(a,float_round_nearest_even);
}

//68
int32_t float64_trunc( real_t a )
{
    return real_to_int32(a,float_round_to_zero);

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is equal to
| the corresponding value `b', and 0 otherwise.  The comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

int float64_eq( real_t a, real_t b )
{

    if (    (    ( extractFloat64Exp( a ) == 0x7FF )
              && ( extractFloat64Frac0( a ) | extractFloat64Frac1( a ) ) )
         || (    ( extractFloat64Exp( b ) == 0x7FF )
              && ( extractFloat64Frac0( b ) | extractFloat64Frac1( b ) ) )
       ) {
        if ( float64_is_signaling_nan( a ) || float64_is_signaling_nan( b ) ) {
            //float_raise( float_flag_invalid );
        }
        return 0;
    }
    return
           ( a.lo == b.lo )
        && (    ( a.hi == b.hi )
             || (    ( a.lo == 0 )
                  && ( (uint32_t) ( ( a.hi | b.hi )<<1 ) == 0 ) )
           );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is less than
| or equal to the corresponding value `b', and 0 otherwise.  The comparison
| is performed according to the IEC/IEEE Standard for Binary Floating-Point
| Arithmetic.
*----------------------------------------------------------------------------*/

int float64_le( real_t a, real_t b )
{
    flag aSign, bSign;

    if (    (    ( extractFloat64Exp( a ) == 0x7FF )
              && ( extractFloat64Frac0( a ) | extractFloat64Frac1( a ) ) )
         || (    ( extractFloat64Exp( b ) == 0x7FF )
              && ( extractFloat64Frac0( b ) | extractFloat64Frac1( b ) ) )
       ) {
      //  float_raise( float_flag_invalid );
        return 0;
    }
    aSign = extractFloat64Sign( a );
    bSign = extractFloat64Sign( b );
    if ( aSign != bSign ) {
        return
               aSign
            || (    ( ( (uint32_t) ( ( a.hi | b.hi )<<1 ) ) | a.lo | b.lo )
                 == 0 );
    }
    return
          aSign ? le64( b.hi, b.lo, a.hi, a.lo )
        : le64( a.hi, a.lo, b.hi, b.lo );

}

/*----------------------------------------------------------------------------
| Returns 1 if the double-precision floating-point value `a' is less than
| the corresponding value `b', and 0 otherwise.  The comparison is performed
| according to the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

int float64_lt( real_t a, real_t b )
{
    flag aSign, bSign;

    if (    (    ( extractFloat64Exp( a ) == 0x7FF )
              && ( extractFloat64Frac0( a ) | extractFloat64Frac1( a ) ) )
         || (    ( extractFloat64Exp( b ) == 0x7FF )
              && ( extractFloat64Frac0( b ) | extractFloat64Frac1( b ) ) )
       ) {
        //float_raise( float_flag_invalid );
        return 0;
    }
    aSign = extractFloat64Sign( a );
    bSign = extractFloat64Sign( b );
    if ( aSign != bSign ) {
        return
               aSign
            && (    ( ( (uint32_t) ( ( a.hi | b.hi )<<1 ) ) | a.lo | b.lo )
                 != 0 );
    }
    return
          aSign ? lt64( b.hi, b.lo, a.hi, a.lo )
        : lt64( a.hi, a.lo, b.hi, b.lo );

}


/*----------------------------------------------------------------------------
| The pattern for a default generated double-precision NaN.  The `high' and
| `lo' values hold the most- and least-significant bits, respectively.
*----------------------------------------------------------------------------*/
double default_nan()
{
    number_t z;
    z.r.hi=0xFFF80000;
    z.r.lo=0;
    return z.d;
}

// returns +/- infinity
double infinity(int sig)
{
    return ((number_t)(real_t){0,0x7ff00000+(sig<<31)}).d;
/*
    number_t z;
    z.r.hi=0x7ff00000|((uint32_t)sig<<31);
    z.r.lo=0;
    return z.d;
*/
}

// returns +/- 0
double real_zero(int sig)
{
    return ((number_t)(real_t){0,sig<<31}).d;
}

/*----------------------------------------------------------------------------
| Takes two double-precision floating-point values `a' and `b', one of which
| is a NaN, and returns the appropriate NaN result.  If either `a' or `b' is a
| signaling NaN, the invalid exception is raised.
*----------------------------------------------------------------------------*/

static double propagateNaN( number_t a, number_t b )
{
    flag aIsNaN, aIsSignalingNaN, bIsNaN, bIsSignalingNaN;

    aIsNaN = float64_is_nan( a.r );
    aIsSignalingNaN = float64_is_signaling_nan( a.r );
    bIsNaN = float64_is_nan( b.r );
    bIsSignalingNaN = float64_is_signaling_nan( b.r );
    a.r.hi |= 0x00080000;
    b.r.hi |= 0x00080000;
    //if ( aIsSignalingNaN | bIsSignalingNaN ) float_raise( float_flag_invalid );
    if ( aIsSignalingNaN )
    {
        if ( bIsSignalingNaN ) goto returnLargerSignificand;
        return bIsNaN ? b.d : a.d;
    }
    else if ( aIsNaN )
    {
        if ( bIsSignalingNaN | ! bIsNaN ) return a.d;
 returnLargerSignificand:
        if ( lt64( a.r.hi<<1, a.r.lo, b.r.hi<<1, b.r.lo ) ) return b.d;
        if ( lt64( b.r.hi<<1, b.r.lo, a.r.hi<<1, a.r.lo ) ) return a.d;
        return ( a.r.hi < b.r.hi ) ? a.d : b.d;
    }
    else
    {
        return b.d;
    }

}

/*----------------------------------------------------------------------------
| Packs the sign `zSign', the exponent `zExp', and the significand formed by
| the concatenation of `zSig0' and `zSig1' into a double-precision floating-
| point value, returning the result.  After being shifted into the proper
| positions, the three fields `zSign', `zExp', and `zSig0' are simply added
| together to form the most significant 32 bits of the result.  This means
| that any integer portion of `zSig0' will be added into the exponent.  Since
| a properly normalized significand will have an integer portion equal to 1,
| the `zExp' input should be 1 less than the desired result exponent whenever
| `zSig0' and `zSig1' concatenated form a complete, normalized significand.
*----------------------------------------------------------------------------*/

double packReal( flag zSign, int16_t zExp, uint32_t zSig0, uint32_t zSig1 )
{
    number_t z;

    z.r.lo = zSig1;
    z.r.hi = ( ( (uint32_t) zSign )<<31 ) + ( ( (uint32_t) zExp )<<20 ) + zSig0;
    return z.d;

}

// returns minus real x
inline double real_minus(number_t x)
{
    x.r.hi^=0x80000000;
    return x.d;
}

// returns x/2 provided x is not NAN nor infinity
static double real_half(number_t x)
{
	int16_t m = extractFloat64Exp(x.r);
	if (m==0)
	{  // x is zero or subnormal
		x.r.lo=(x.r.lo>>1)+(x.r.hi<<31)+(x.r.hi&0x80000000);
		x.r.hi>>=1;
	}
	else if (m==1)
	{  // x becomes subnormal after division by two
		x.r.lo=(x.r.lo>>1)+(x.r.hi<<31);
		x.r.hi=(x.r.hi>>1)+0x00080000+(x.r.hi&0x80000000);
	}
	else x.r.hi-=0x00100000;
    return x.d;
}


// fast return 2*x if x is normal
inline double real_double(number_t x)
{
    x.r.hi+=0x00100000;
    return x.d;
}


/*----------------------------------------------------------------------------
| Takes an abstract floating-point value having sign `zSign', exponent `zExp',
| and extended significand formed by the concatenation of `zSig0', `zSig1',
| and `zSig2', and returns the proper double-precision floating-point value
| corresponding to the abstract input.  Ordinarily, the abstract value is
| simply rounded and packed into the double-precision format, with the inexact
| exception raised if the abstract input cannot be represented exactly.
| However, if the abstract value is too large, the overflow and inexact
| exceptions are raised and an infinity or maximal finite value is returned.
| If the abstract value is too small, the input value is rounded to a
| subnormal number, and the underflow and inexact exceptions are raised if the
| abstract input cannot be represented exactly as a subnormal double-precision
| floating-point number.
|     The input significand must be normalized or smaller.  If the input
| significand is not normalized, `zExp' must be 0; in that case, the result
| returned is a subnormal number, and it must not require rounding.  In the
| usual case that the input significand is normalized, `zExp' must be 1 less
| than the ``true'' floating-point exponent.  The handling of underflow and
| overflow follows the IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static double roundAndPack( flag zSign, int16_t zExp, uint32_t zSig0, uint32_t zSig1, uint32_t zSig2 )
{

    flag increment; //, isTiny;


    increment = ( (int32_t) zSig2 < 0 );
    if ( 0x7FD <= (uint32_t) zExp ) {
        if (    ( 0x7FD < zExp )
             || (    ( zExp == 0x7FD )
                  && eq64( 0x001FFFFF, 0xFFFFFFFF, zSig0, zSig1 )
                  && increment
                )
           ) {
            //float_raise( float_flag_overflow | float_flag_inexact );
            return packReal( zSign, 0x7FF, 0, 0 );
        }
        if ( zExp < 0 ) {
           // isTiny =
                  /* ( float_detect_tininess == float_tininess_before_rounding )
                ||*/ //( zExp < -1 )
             //   || ! increment
              //  || lt64( zSig0, zSig1, 0x001FFFFF, 0xFFFFFFFF );
            shift64ExtraRightJamming(
                zSig0, zSig1, zSig2, - zExp, &zSig0, &zSig1, &zSig2 );
            zExp = 0;
            //if ( isTiny && zSig2 ) float_raise( float_flag_underflow );

            increment = ( (int32_t) zSig2 < 0 );
        }
    }
    //if ( zSig2 ) float_exception_flags |= float_flag_inexact;
    if ( increment )
    {
        add64( zSig0, zSig1, 0, 1, &zSig0, &zSig1 );
        zSig1 &= ~ ( ( zSig2 + zSig2 == 0 )/* & 1roundNearestEven*/ );
    }
    else {
        if ( ( zSig0 | zSig1 ) == 0 ) zExp = 0;
    }
    return packReal( zSign, zExp, zSig0, zSig1 );

}


/*----------------------------------------------------------------------------
| Takes an abstract floating-point value having sign `zSign', exponent `zExp',
| and significand formed by the concatenation of `zSig0' and `zSig1', and
| returns the proper double-precision floating-point value corresponding
| to the abstract input.  This routine is just like `roundAndPackFloat64'
| except that the input significand has fewer bits and does not have to be
| normalized.  In all cases, `zExp' must be 1 less than the ``true'' floating-
| point exponent.
*----------------------------------------------------------------------------*/

static double normalizeRoundAndPack( flag zSign, int16_t zExp, uint32_t zSig0, uint32_t zSig1 )
{
    int shiftCount;
    uint32_t zSig2;

    if ( zSig0 == 0 ) {
        zSig0 = zSig1;
        zSig1 = 0;
        zExp -= 32;
    }
    shiftCount = countLeadingZeros( zSig0 ) - 11;
    if ( 0 <= shiftCount ) {
        zSig2 = 0;
        shortShift64Left( zSig0, zSig1, shiftCount, &zSig0, &zSig1 );
    }
    else {
        shift64ExtraRightJamming(
            zSig0, zSig1, 0, - shiftCount, &zSig0, &zSig1, &zSig2 );
    }
    zExp -= shiftCount;
    return roundAndPack( zSign, zExp, zSig0, zSig1, zSig2 );

}



// addition
//////////////

/*----------------------------------------------------------------------------
| Returns the result of adding the absolute values of the double-precision
| floating-point values `a' and `b'.  If `zSign' is 1, the sum is negated
| before being returned.  `zSign' is ignored if the result is a NaN.
| The addition is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static double addRealSigs( number_t a, number_t b, flag zSign )
{
    int16_t aExp, bExp, zExp;
    uint32_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1, zSig2;
    int16_t expDiff;

    aSig1 = extractFloat64Frac1( a.r );
    aSig0 = extractFloat64Frac0( a.r );
    aExp = extractFloat64Exp( a.r );
    bSig1 = extractFloat64Frac1( b.r );
    bSig0 = extractFloat64Frac0( b.r );
    bExp = extractFloat64Exp( b.r );
    expDiff = aExp - bExp;
    if ( 0 < expDiff ) {
        if ( aExp == 0x7FF ) {
            if ( aSig0 | aSig1 ) return propagateNaN( a, b );
            return a.d;
        }
        if ( bExp == 0 ) {
            --expDiff;
        }
        else {
            bSig0 |= 0x00100000;
        }
        shift64ExtraRightJamming(
            bSig0, bSig1, 0, expDiff, &bSig0, &bSig1, &zSig2 );
        zExp = aExp;
    }
    else if ( expDiff < 0 ) {
        if ( bExp == 0x7FF ) {
            if ( bSig0 | bSig1 ) return propagateNaN( a, b );
            return packReal( zSign, 0x7FF, 0, 0 );
        }
        if ( aExp == 0 ) {
            ++expDiff;
        }
        else {
            aSig0 |= 0x00100000;
        }
        shift64ExtraRightJamming(
            aSig0, aSig1, 0, - expDiff, &aSig0, &aSig1, &zSig2 );
        zExp = bExp;
    }
    else {
        if ( aExp == 0x7FF ) {
            if ( aSig0 | aSig1 | bSig0 | bSig1 ) {
                return propagateNaN( a, b );
            }
            return a.d;
        }
        add64( aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1 );
        if ( aExp == 0 ) return packReal( zSign, 0, zSig0, zSig1 );
        zSig2 = 0;
        zSig0 |= 0x00200000;
        zExp = aExp;
        goto shiftRight1;
    }
    aSig0 |= 0x00100000;
    add64( aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1 );
    --zExp;
    if ( zSig0 < 0x00200000 ) goto roundAndPack;
    ++zExp;
 shiftRight1:
    shift64ExtraRightJamming( zSig0, zSig1, zSig2, 1, &zSig0, &zSig1, &zSig2 );
 roundAndPack:
    return roundAndPack( zSign, zExp, zSig0, zSig1, zSig2 );

}

/*----------------------------------------------------------------------------
| Returns the result of subtracting the absolute values of the double-
| precision floating-point values `a' and `b'.  If `zSign' is 1, the
| difference is negated before being returned.  `zSign' is ignored if the
| result is a NaN.  The subtraction is performed according to the IEC/IEEE
| Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/

static double subRealSigs( number_t a, number_t b, flag zSign )
{
    int16_t aExp, bExp, zExp;
    uint32_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1;
    int16_t expDiff;

    aSig1 = extractFloat64Frac1( a.r );
    aSig0 = extractFloat64Frac0( a.r );
    aExp = extractFloat64Exp( a.r );
    bSig1 = extractFloat64Frac1( b.r );
    bSig0 = extractFloat64Frac0( b.r );
    bExp = extractFloat64Exp( b.r );
    expDiff = aExp - bExp;
    shortShift64Left( aSig0, aSig1, 10, &aSig0, &aSig1 );
    shortShift64Left( bSig0, bSig1, 10, &bSig0, &bSig1 );
    if ( 0 < expDiff ) goto aExpBigger;
    if ( expDiff < 0 ) goto bExpBigger;
    if ( aExp == 0x7FF )
    {
        if ( aSig0 | aSig1 | bSig0 | bSig1 )
        {
            return propagateNaN( a, b );
        }
        //float_raise( float_flag_invalid );
        return default_nan();
    }
    if ( aExp == 0 ) {
        aExp = 1;
        bExp = 1;
    }
    if ( bSig0 < aSig0 ) goto aBigger;
    if ( aSig0 < bSig0 ) goto bBigger;
    if ( bSig1 < aSig1 ) goto aBigger;
    if ( aSig1 < bSig1 ) goto bBigger;
    return 0.0; //packReal( /*float_rounding_mode == float_round_down*/ 0, 0, 0, 0 );
 bExpBigger:
    if ( bExp == 0x7FF )
    {
        if ( bSig0 | bSig1 ) return propagateNaN( a, b );
        return packReal( zSign ^ 1, 0x7FF, 0, 0 );
    }
    if ( aExp == 0 )
    {
        ++expDiff;
    }
    else
    {
        aSig0 |= 0x40000000;
    }
    shift64RightJammingOnPlace(- expDiff, &aSig0, &aSig1 );
    bSig0 |= 0x40000000;
 bBigger:
    sub64( bSig0, bSig1, aSig0, aSig1, &zSig0, &zSig1 );
    zExp = bExp;
    zSign ^= 1;
    goto normalizeRoundAndPack;
 aExpBigger:
    if ( aExp == 0x7FF )
    {
        if ( aSig0 | aSig1 ) return propagateNaN( a, b );
        return a.d;
    }
    if ( bExp == 0 ) {
        --expDiff;
    }
    else {
        bSig0 |= 0x40000000;
    }
    shift64RightJammingOnPlace(expDiff, &bSig0, &bSig1 );
    aSig0 |= 0x40000000;
 aBigger:
    sub64( aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1 );
    zExp = aExp;
 normalizeRoundAndPack:
    --zExp;
    return normalizeRoundAndPack( zSign, zExp - 10, zSig0, zSig1 );

}



/*----------------------------------------------------------------------------
| Returns the result of adding the double-precision floating-point values `a'
| and `b'.  The operation is performed according to the IEC/IEEE Standard for
| Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/
// 44
double real_add( number_t a, number_t b )
{
    flag aSign; //, bSign;

    aSign = extractFloat64Sign( a.r );
 //   bSign = extractFloat64Sign( b.r );
    if ( aSign == /*bSign*/ extractFloat64Sign( b.r ) )
    {
        return addRealSigs( a, b, aSign );
    }
    else
    {
        return subRealSigs( a, b, aSign );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of subtracting the double-precision floating-point values
| `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/
// 48
double real_sub( number_t a, number_t b )
{
    flag aSign; //, bSign;

    aSign = extractFloat64Sign( a.r );
//    bSign = extractFloat64Sign( b.r );
    if ( aSign == extractFloat64Sign( b.r )/*bSign*/ ) {
        return subRealSigs( a, b, aSign );
    }
    else {
        return addRealSigs( a, b, aSign );
    }

}

/*----------------------------------------------------------------------------
| Returns the result of multiplying the double-precision floating-point values
| `a' and `b'.  The operation is performed according to the IEC/IEEE Standard
| for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/
// 52
double real_mul( number_t a, number_t b )
{
    flag aSign, bSign, zSign;
    int16_t aExp, bExp, zExp;
    uint32_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1, zSig2, zSig3;


    aSig1 = extractFloat64Frac1( a.r );
    aSig0 = extractFloat64Frac0( a.r );
    aExp = extractFloat64Exp( a.r );
    aSign = extractFloat64Sign( a.r );
    bSig1 = extractFloat64Frac1( b.r );
    bSig0 = extractFloat64Frac0( b.r );
    bExp = extractFloat64Exp( b.r);
    bSign = extractFloat64Sign( b.r );
    zSign = aSign ^ bSign;
    if ( aExp == 0x7FF ) {
        if (    ( aSig0 | aSig1 )
             || ( ( bExp == 0x7FF ) && ( bSig0 | bSig1 ) ) ) {
            return propagateNaN( a, b );
        }
        if ( ( bExp | bSig0 | bSig1 ) == 0 ) goto invalid;
        return packReal( zSign, 0x7FF, 0, 0 );
    }
    if ( bExp == 0x7FF ) {
        if ( bSig0 | bSig1 ) return propagateNaN( a, b );
        if ( ( aExp | aSig0 | aSig1 ) == 0 ) {
 invalid:
            //float_raise( float_flag_invalid );
            return default_nan();
        }
        return packReal( zSign, 0x7FF, 0, 0 );
    }
    if ( aExp == 0 ) {
        if ( ( aSig0 | aSig1 ) == 0 ) return packReal( zSign, 0, 0, 0 );
        normalizeFloat64Subnormal( aSig0, aSig1, &aExp, &aSig0, &aSig1 );
    }
    if ( bExp == 0 ) {
        if ( ( bSig0 | bSig1 ) == 0 ) return packReal( zSign, 0, 0, 0 );
        normalizeFloat64Subnormal( bSig0, bSig1, &bExp, &bSig0, &bSig1 );
    }
    zExp = aExp + bExp - 0x400;
    aSig0 |= 0x00100000;
    shortShift64Left( bSig0, bSig1, 12, &bSig0, &bSig1 );
    mul64To128( aSig0, aSig1, bSig0, bSig1, &zSig0, &zSig1, &zSig2, &zSig3 );
    add64( zSig0, zSig1, aSig0, aSig1, &zSig0, &zSig1 );
    zSig2 |= ( zSig3 != 0 );
    if ( 0x00200000 <= zSig0 ) {
        shift64ExtraRightJamming(
            zSig0, zSig1, zSig2, 1, &zSig0, &zSig1, &zSig2 );
        ++zExp;
    }
    return roundAndPack( zSign, zExp, zSig0, zSig1, zSig2 );

}

/*----------------------------------------------------------------------------
| Returns the result of dividing the double-precision floating-point value `a'
| by the corresponding value `b'.  The operation is performed according to the
| IEC/IEEE Standard for Binary Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/
// 56
double real_div( number_t a, number_t b )
{
    flag aSign, bSign, zSign;
    int16_t aExp, bExp, zExp;
    uint32_t aSig0, aSig1, bSig0, bSig1, zSig0, zSig1, zSig2;
    uint32_t rem0, rem1, rem2, rem3, term0, term1, term2, term3;

    aSig1 = extractFloat64Frac1( a.r );
    aSig0 = extractFloat64Frac0( a.r );
    aExp = extractFloat64Exp( a.r );
    aSign = extractFloat64Sign( a.r );
    bSig1 = extractFloat64Frac1( b.r );
    bSig0 = extractFloat64Frac0( b.r );
    bExp = extractFloat64Exp( b.r );
    bSign = extractFloat64Sign( b.r );
    zSign = aSign ^ bSign;
    if ( aExp == 0x7FF ) {
        if ( aSig0 | aSig1 ) return propagateNaN( a, b );
        if ( bExp == 0x7FF ) {
            if ( bSig0 | bSig1 ) return propagateNaN( a, b );
            goto invalid;
        }
        return packReal( zSign, 0x7FF, 0, 0 );
    }
    if ( bExp == 0x7FF )
    {
        if ( bSig0 | bSig1 ) return propagateNaN( a, b );
        return packReal( zSign, 0, 0, 0 );
    }
    if ( bExp == 0 ) {
        if ( ( bSig0 | bSig1 ) == 0 ) {
            if ( ( aExp | aSig0 | aSig1 ) == 0 ) {
 invalid:
                //float_raise( float_flag_invalid );
                return default_nan();
            }
            //float_raise( float_flag_divbyzero );
            return packReal( zSign, 0x7FF, 0, 0 );
        }
        normalizeFloat64Subnormal( bSig0, bSig1, &bExp, &bSig0, &bSig1 );
    }
    if ( aExp == 0 ) {
        if ( ( aSig0 | aSig1 ) == 0 ) return packReal( zSign, 0, 0, 0 );
        normalizeFloat64Subnormal( aSig0, aSig1, &aExp, &aSig0, &aSig1 );
    }
    zExp = aExp - bExp + 0x3FD;
    shortShift64Left( aSig0 | 0x00100000, aSig1, 11, &aSig0, &aSig1 );
    shortShift64Left( bSig0 | 0x00100000, bSig1, 11, &bSig0, &bSig1 );
    if ( le64( bSig0, bSig1, aSig0, aSig1 ) )
    {
        aSig1=(aSig1>>1)+(aSig0<<31);
        aSig0>>=1;

        ++zExp;
    }
    zSig0 = estimateDiv64To32( aSig0, aSig1, bSig0 );
    mul64By32To96( bSig0, bSig1, zSig0, &term0, &term1, &term2 );
    sub96( aSig0, aSig1, 0, term0, term1, term2, &rem0, &rem1, &rem2 );
    while ( (int32_t) rem0 < 0 )
    {
        --zSig0;
        //add96( rem0, rem1, rem2, 0, bSig0, bSig1, &rem0, &rem1, &rem2 );
        incr96_64(bSig0, bSig1, &rem0, &rem1, &rem2 );
    }
    zSig1 = estimateDiv64To32( rem1, rem2, bSig0 );
    if ( ( zSig1 & 0x3FF ) <= 4 ) {
        mul64By32To96( bSig0, bSig1, zSig1, &term1, &term2, &term3 );
        sub96( rem1, rem2, 0, term1, term2, term3, &rem1, &rem2, &rem3 );
        while ( (int32_t) rem1 < 0 ) {
            --zSig1;
            //add96( rem1, rem2, rem3, 0, bSig0, bSig1, &rem1, &rem2, &rem3 );
            incr96_64(bSig0, bSig1, &rem1, &rem2, &rem3 );
        }
        zSig1 |= ( ( rem1 | rem2 | rem3 ) != 0 );
    }
    shift64ExtraRightJamming( zSig0, zSig1, 0, 11, &zSig0, &zSig1, &zSig2 );
    return roundAndPack( zSign, zExp, zSig0, zSig1, zSig2 );

}

/*----------------------------------------------------------------------------
| Returns the square root of the double-precision floating-point value `a'.
| The operation is performed according to the IEC/IEEE Standard for Binary
| Floating-Point Arithmetic.
*----------------------------------------------------------------------------*/
// 60
double real_sqrt( number_t a )
{
    flag aSign;
    int16_t aExp, zExp;
    uint32_t aSig0, aSig1, zSig0, zSig1, zSig2, doubleZSig0;
    uint32_t rem0, rem1, rem2, rem3, term0, term1, term2, term3;


    aSig1 = extractFloat64Frac1( a.r );
    aSig0 = extractFloat64Frac0( a.r );
    aExp = extractFloat64Exp( a.r );
    aSign = extractFloat64Sign( a.r );
    if ( aExp == 0x7FF ) {
        if ( aSig0 | aSig1 ) return propagateNaN( a, a );
        if ( ! aSign ) return a.d;
        goto invalid;
    }
    if ( aSign ) {
        if ( ( aExp | aSig0 | aSig1 ) == 0 ) return a.d;
 invalid:
        //float_raise( float_flag_invalid );
        return default_nan();
    }
    if ( aExp == 0 ) {
        if ( ( aSig0 | aSig1 ) == 0 ) return packReal( 0, 0, 0, 0 );
        normalizeFloat64Subnormal( aSig0, aSig1, &aExp, &aSig0, &aSig1 );
    }
    zExp = ( ( aExp - 0x3FF )>>1 ) + 0x3FE;
    aSig0 |= 0x00100000;
    shortShift64Left( aSig0, aSig1, 11, &term0, &term1 );
    zSig0 = ( estimateSqrt32( aExp, term0 )>>1 ) + 1;
    if ( zSig0 == 0 ) zSig0 = 0x7FFFFFFF;
    doubleZSig0 = zSig0 + zSig0;
    shortShift64Left( aSig0, aSig1, 9 - ( aExp & 1 ), &aSig0, &aSig1 );
    mul32To64( zSig0, zSig0, &term0, &term1 );
    sub64( aSig0, aSig1, term0, term1, &rem0, &rem1 );
    while ( (int32_t) rem0 < 0 ) {
        --zSig0;
        doubleZSig0 -= 2;
        add64( rem0, rem1, 0, doubleZSig0 | 1, &rem0, &rem1 );
    }
    zSig1 = estimateDiv64To32( rem1, 0, doubleZSig0 );
    if ( ( zSig1 & 0x1FF ) <= 5 ) {
        if ( zSig1 == 0 ) zSig1 = 1;
        mul32To64( doubleZSig0, zSig1, &term1, &term2 );
        sub64( rem1, 0, term1, term2, &rem1, &rem2 );
        mul32To64( zSig1, zSig1, &term2, &term3 );
        sub96( rem1, rem2, 0, 0, term2, term3, &rem1, &rem2, &rem3 );
        while ( (int32_t) rem1 < 0 ) {
            --zSig1;
            shortShift64Left( 0, zSig1, 1, &term2, &term3 );
            term3 |= 1;
            term2 |= doubleZSig0;
            //add96( rem1, rem2, rem3, 0, term2, term3, &rem1, &rem2, &rem3 );
            incr96_64(term2, term3, &rem1, &rem2, &rem3 );
        }
        zSig1 |= ( ( rem1 | rem2 | rem3 ) != 0 );
    }
    shift64ExtraRightJamming( zSig0, zSig1, 0, 10, &zSig0, &zSig1, &zSig2 );
    return roundAndPack( 0, zExp, zSig0, zSig1, zSig2 );

}


// transcendentals functions

// ----- Exp -----
//returns e raised to the power xx // e:= 2.71828182845904523
double real_exp(number_t xx)
{
#define     twom1000        9.33263618503218878990e-302
#define     o_threshold     7.09782712893383973096e+02
#define     u_threshold     -7.45133219101941108420e+02
#define     invln2          1.44269504088896338700e+00
#define     P1              1.66666666666666019037e-01
#define     P2              -2.77777777770155933842e-03
#define     P3              6.61375632143793436117e-05
#define     P4              -1.65339022054652515390e-06
#define     P5              4.13813679705723846039e-08
#define     ln2HI_0         6.93147180369123816490e-01  // 0x3fe62e42, 0xfee00000
#define     ln2HI_1         -ln2HI_0
#define     ln2LO_0         1.90821492927058770002e-10  // 0x3dea39ef, 0x35793c76
#define     ln2LO_1         -ln2LO_0

number_t //z,
         hi,
         lo,
         c,
         t,
         x;
int k, xsb;
uint32_t hx;
const static number_t ln2hi[2]={ln2HI_0, ln2HI_1};
const static number_t ln2lo[2]={ln2LO_0,ln2LO_1};
/*
  ln2hi[0].d=ln2HI_0; // $3fe62e42, $fee00000
  ln2hi[1].d=ln2HI_1;
  ln2lo[0].d=ln2LO_0; // $3dea39ef, $35793c76
  ln2lo[1].d=ln2LO_1;
*/
  x.r=xx.r;
  hx=x.r.hi;
  xsb=hx >> 31; // get sign of x
  hx=hx & 0x7FFFFFFF; // high word of absolute value of x
   // fiter out non finite arguments
  if (hx>=0x40862E42)    // when  |x|>709.78...
  {
        if (hx>=0x7FF00000)
        {
            if (((hx & 0xFFFFF) | x.r.lo)!=0) return default_nan(); // nan
            if (xsb!=0) return real_zero(0); // exp(+-inf)=+inf/0
            return infinity(0);
        }
        if (float64_lt( ((number_t)o_threshold).r,x.r)) //(x.d>o_threshold)
        {
            //printf("threshold1");
           return default_nan();
        }
        if (float64_lt(x.r,((number_t)u_threshold).r)) //(x.d<u_threshold)
        {
           return real_zero(0);
        }
  }
  if (hx>0x3FD62E42)    // if |x| > 0.5 ln 2
  {
        if (hx<0x3FF0A2B2)   // and |x| <1.5 ln 2
        {
            hi.d=real_sub((number_t)x.r,ln2hi[xsb]);
            lo=ln2lo[xsb];
            k=1-xsb-xsb;  // +1 / -1
        }
        else
        {
            k=float64_round( ((number_t)real_mul((number_t)invln2,x)).r);
            t.d=intToReal(k);
            hi.d=real_sub(x,(number_t)real_mul(t,(number_t)ln2HI_0));
            lo.d=real_mul(t,(number_t)ln2LO_0);
        }
        x.d=real_sub(hi,lo);
  }
  else if (hx<0x3E300000) // when |x|<2**-28
  {
        x.d=real_add(x,(number_t)1.0);
        return x.d;
  }
  else
  {
      k=0;
  }
  t.d=real_mul(x,x);
  //t.d=x.d*x.d;

  c.d=real_sub(x,
             (number_t)real_mul(t,
                      (number_t)real_add( (number_t)P1,
                               (number_t)real_mul(t,
                                       (number_t) real_add( (number_t)P2,
                                                (number_t) real_mul(t,
                                                         (number_t) real_add( (number_t)P3,
                                                                   (number_t)real_mul(t,
                                                                            (number_t)real_add( (number_t)P4,
                                                                                     (number_t)real_mul(t, (number_t)P5))))))))));

    //c.d=x.d-t.d*(P1+t.d*(P2+t.d*(P3+t.d*(P4+t.d*P5))));

  if (k==0)
  {

        return real_sub((number_t)1.0,
                        (number_t)real_sub( (number_t)real_div( (number_t)real_mul(x,c),
                                                               (number_t)real_sub(c, (number_t)2.0)),
                                           x));
  }
  else
  {
      //x.d=1.0-((lo.d-(x.d*c.d)/(2.0-c.d))-hi.d);

      // x.r:=1.0-((lo-(x.r*c)/(2.0-c))-hi);

     x.d=real_sub((number_t)1.0,
                (number_t)real_sub( (number_t)real_sub(lo,
                                                       (number_t)real_div(  (number_t)real_mul(x,c),
                                                                            (number_t)real_sub( (number_t)2.0,c) )),
                                   hi));

  }
  if (k>-1021)  x.r.hi+=k << 20;
  else
  {
      x.r.hi+=((k+1000) << 20);

      //x.d*=twom1000;
      x.d=real_mul(x,(number_t)twom1000);

  }
  return x.d;
}


// ----- Ln -----
//returns the natural log of xx (log base e)
double real_ln(number_t xx)
{

#define     ln2_hi     6.93147180369123816490e-01
#define     ln2_lo     1.90821492927058770002e-10
#define     two54      1.80143985094819840000e+16  //0x43500000, 0x00000000
#define     Lg1        6.666666666666735130e-01
#define     Lg2        3.999999999940941908e-01
#define     Lg3        2.857142874366239149e-01
#define     Lg4        2.222219843214978396e-01
#define     Lg5        1.818357216161805012e-01
#define     Lg6        1.531383769920937332e-01
#define     Lg7        1.479819860511658591e-01

    number_t hfsq,f,z,R,w,dk,s;
    number_t x;
    int k,hx,i;

    k=0;
    x=xx;
    hx=x.r.hi;
    if (hx<0x00100000)  // x < 2**-1022
    {
          if (((hx & 0x7FFFFFFF) | x.r.lo)==0)
          {
               return infinity(1); //packReal(1,0x7ff,0,0); //-1/0;  // ln(+-0)=-inf;
          }
          if (hx<0) return default_nan();
          k-=54; // subnormal, scale up to x
          x.d=real_mul(x,(number_t)two54);
          //x.r.hi+=0x03500000; // *2^54
          hx=x.r.hi;
    }
    if (hx>=0x7FF00000)
    { // ln(nan)=nan and ln(+inf)=+inf
      //x.r:=x.r+x.r;
      return x.d;
    }
    k+=(hx >> 20)-1023; // exponent
    hx&=0xFFFFF;
    i=(hx+0x95F64) & 0x100000;
    x.r.hi=hx | (i ^ 0x3FF00000);
    k+=(i >> 20);
    f.d=real_sub(x,(number_t)1.0);
    if ((0xFFFFF & (2+hx))<3)  // |f| < 2**-20
    {
          if (float64_eq(f.r,((number_t)0.0).r))
          {
                if (k==0) return 0.0;
                dk.d=intToReal(k);
                return real_add((number_t)real_mul(dk,(number_t)ln2_hi),(number_t)real_mul(dk,(number_t)ln2_lo));
          };
          x.d=real_mul((number_t)real_mul(f,f), (number_t)real_add((number_t)0.5, (number_t)real_mul(f,(number_t)(-1/3.0)) ) );  //  sqr(f)*(0.5+f*(-1/3.0));
          if (k==0)
          {
              return (real_sub(f,R));
          }
          dk.d=intToReal(k);
          return real_sub((number_t)real_mul(dk,(number_t)ln2_hi),(number_t)real_sub((number_t)real_mul((number_t)real_sub(R,dk),(number_t)ln2_lo),f) );  // dk*ln2_hi-((R-dk*ln2_lo)-f);
    }
    s.d=real_div(f,(number_t)real_add((number_t)2.0,f));
    z.d=real_mul(s,s);
    w.d=real_mul(z,z);
    dk.d=intToReal(k);
    x.d=real_add((number_t)real_mul(w,(number_t)real_add ((number_t)Lg2, (number_t) real_mul(w,(number_t)real_add((number_t)Lg4, (number_t)real_mul( w,(number_t)Lg6 ) ) ) )),
               (number_t)real_mul(z,
                      (number_t)real_add((number_t)Lg1,
                                      (number_t)real_mul( w, (number_t)real_add ((number_t)Lg3  ,(number_t) real_mul( w, (number_t)real_add((number_t)Lg5, (number_t)real_mul( w,(number_t)Lg7 ) ) )  ) ) )));
         // w*(Lg2+w*(Lg4+w*Lg6))+ z*(Lg1+w*(Lg3+w*(Lg5+w*Lg7)));
     if ( ((hx-0x6147A) | (0x6B851-hx))>0 )
    {
        hfsq.d=real_half((number_t)real_mul(f,f)); //real_mul((number_t)0.5,(number_t) real_mul(f,f) );  // TODO optimize multiplication by 0.5
        s.d=real_mul(s, (number_t)real_add(hfsq,R) );
        if (k==0)  x.d=real_sub(f, (number_t)real_sub(hfsq,s));
        else x.d = real_sub((number_t) real_mul( dk,(number_t)ln2_hi) ,
                            (number_t) real_sub((number_t) real_sub( hfsq , (number_t)real_add(s , (number_t)real_mul(dk,(number_t)ln2_lo)) ),f) );  // x.d:=dk*ln2_hi-((hfsq-(s+dk*ln2_lo))-f);
    }
    else
    {
          s.d=real_mul(s, (number_t)real_sub(f,x));
          if (k==0)  x.d=real_sub(f,s);
          else x.d= real_sub( (number_t)real_mul( dk,(number_t)ln2_hi  ) ,
                             (number_t) real_sub( (number_t)real_sub(s ,  (number_t)real_mul(dk,(number_t)ln2_lo)  ) , f ) );
    }
    return x.d;
}// ln


// ----- Arctan -----
//Returns radian angle whose tangent is xx.
double real_atan(number_t xx)
{

#define huge   1.0e300
#define one  1.0
#define at0  3.33333333333329318027e-01
#define at1 -1.99999999998764832476e-01
#define at2  1.42857142725034663711e-01
#define at3 -1.11111104054623557880e-01
#define at4  9.09088713343650656196e-02
#define at5 -7.69187620504482999495e-02
#define at6  6.66107313738753120669e-02
#define at7 -5.83357013379057348645e-02
#define at8  4.97687799461593236017e-02
#define at9 -3.65315727442169155270e-02
#define at10 1.62858201153657823623e-02

    number_t x;
    number_t  w,s1,s2,z;
    int ix,hx,id;
    uint32_t lx;
    const static real_t atanhiP[4]=
    {
        {0x0561bb4f, 0x3fddac67}, // 4.63647609000806093515e-01,
        {0x54442d18, 0x3fe921fb}, // 7.85398163397448278999e-01,
        {0xd281f69b, 0x3fef730b}, // 9.82793723247329054082e-01,
        {0x54442d18, 0x3ff921fb}  // 1.57079632679489655800e+00
        };

    const static real_t atanloP[4]=
    {
       {0x222f65e2, 0x3c7a2b7f}, // 2.26987774529616870924e-17,
       {0x33145c07, 0x3c81a626}, // 3.06161699786838301793e-17,
       {0x7af0cbbd, 0x3c700788}, // 1.39033110312309984516e-17,
       {0x33145c07, 0x3c91a626}  // 6.12323399573676603587e-17
    };

    x=xx;
    hx=x.r.hi;
    ix=hx & 0x7FFFFFFF;
    if (ix>=0x44100000)
    {
        if ((ix>0x7FF00000)||((ix=0x7FF00000)&&(x.r.lo!=0)))
        {
            return real_add(x,x);  // TODO improve doubling
        }
        if (hx>0) return real_add((number_t)atanhiP[3],(number_t)atanloP[3]);
        return real_mul((number_t)-1.0,(number_t)real_add((number_t)atanhiP[3],(number_t)atanloP[3]));
    }
    if (ix<0x3FDC0000)
    {

       if (ix<0x3E200000)
       {
           if (float64_lt(((number_t)one).r,((number_t)real_add(((number_t)huge),x)).r)) return x.d;// huge+x.r>one;
       }
       id=-1;
    }
    else
    {
       x.r.hi &= 0x7FFFFFFF;
       if (ix<0x3FF30000)
       {
            if (ix<0x3FE60000)
            {
                id=0;
                x.d=real_div((number_t)real_sub((number_t)/*real_mul((number_t)2.0,x)*/real_double(x),
                		                          (number_t)one),
                		             (number_t)real_add((number_t)2.0,x));
            }
            else
            {
                id=1;
                x.d=real_div((number_t)real_sub(x,(number_t)one),(number_t)real_add(x,(number_t)one));
            }
       }
       else
       {
            if (ix<0x40038000)
            {
                id=2;
                x.d=real_div((number_t)real_sub(x,(number_t)1.5),(number_t)real_add((number_t)one,(number_t)real_mul((number_t)1.5,x)));
            }
            else
            {
                id=3;
                x.d=real_div((number_t)-1.0,x);
            }
       }
    }
    z.d=real_mul(x,x);
    w.d=real_mul(z,z);
    s1.d= real_mul(z,
                 (number_t) real_add((number_t)at0,
                        (number_t)   real_mul(w,
                                (number_t)    real_add ((number_t)at2 ,
                                             (number_t) real_mul(w,
                                                      (number_t) real_add((number_t)at4 ,
                                                               (number_t) real_mul( w,
                                                                        (number_t) real_add((number_t)at6,
                                                                                  (number_t)real_mul( w,
                                                                                          (number_t) real_add((number_t)at8,
                                                                                                   (number_t) real_mul(w,(number_t)at10)))))))))));
    s2.d=real_mul(w,
                  (number_t)real_add((number_t)at1,
                           (number_t)real_mul(w,
                                   (number_t) real_add((number_t)at3,
                                             (number_t)real_mul(w,
                                                     (number_t) real_add((number_t)at5,
                                                              (number_t) real_mul(w,
                                                                        (number_t)real_add((number_t)at7,
                                                                                 (number_t)real_mul(w,(number_t)at9)))))))));
    if (id<0)
        x.d=real_sub(x,(number_t)real_mul(x, (number_t)real_add(s1,s2)));
    else
    {
        x.d=real_sub((number_t)atanhiP[id],
                     (number_t)real_sub((number_t)real_sub((number_t)real_mul(x,(number_t)real_add(s1,s2)),(number_t)atanloP[id]),x));
        if (hx<0) return real_minus(x); //real_negate(&x); //.d=real_mul((number_t)-1.0,x);
    }
    return x.d;
}


//***** circular functions sine, cosine
//These functions are limited to abs(angle) <1.073741824e9 (2**30), because of a
//rapid loss of accuracy beyond this limit (just like my Excel 97) when reducing the angle to the 2 first octants
//You will get a NaN value if x is greater than this value.

double k_sin(number_t x)
{

#define  sincof0  1.58962301576546568060E-10
#define  sincof1 -2.50507477628578072866E-8
#define  sincof2  2.75573136213857245213E-6
#define  sincof3 -1.98412698295895385996E-4
#define  sincof4  8.33333333332211858878E-3
#define  sincof5 -1.66666666666666307295E-1

    number_t z;

    z.d=real_mul(x,x);
    return real_add(x,
               (number_t)real_mul(z,
                  (number_t) real_mul(x,
                      (number_t)real_add((number_t)sincof5,
                          (number_t)real_mul(z,
                              (number_t)real_add((number_t)sincof4,
                                   (number_t)real_mul(z,
                                      (number_t)real_add((number_t)sincof3,
                                          (number_t)real_mul(z,
                                               (number_t) real_add((number_t)sincof2,
                                                   (number_t)real_mul(z,
                                                       (number_t) real_add((number_t)sincof1,
                                                            (number_t)real_mul(z,(number_t)sincof0)))))))))))));
}


double k_cos(number_t x)
{

#define   coscof0 -1.13585365213876817300E-11
#define   coscof1  2.08757008419747316778E-9
#define   coscof2 -2.75573141792967388112E-7
#define   coscof3  2.48015872888517045348E-5
#define   coscof4 -1.38888888888730564116E-3
#define   coscof5 4.16666666666665929218E-2

number_t z;

  z.d=real_mul(x,x);
  return real_sub((number_t)1.0, (number_t)real_sub((number_t)/*real_mul(z,(number_t)0.5)*/real_half(z), // TODO : improve half
                            (number_t)real_mul(z,
                                  (number_t) real_mul(z,
                                       (number_t)real_add((number_t)coscof5,
                                          (number_t) real_mul(z,
                                               (number_t)real_add((number_t)coscof4,
                                                   (number_t)real_mul(z,
                                                       (number_t)real_add((number_t)coscof3,
                                                            (number_t)real_mul(z,
                                                                 (number_t)real_add((number_t)coscof2,
                                                                     (number_t)real_mul(z,
                                                                          (number_t)real_add((number_t)coscof1,
                                                                                (number_t)real_mul(z,(number_t)coscof0))))))))))))));
//  1.0-(z*0.5-z*(z*(coscof5+z*(coscof4+z*(coscof3+z*(coscof2+z*(coscof1+z*coscof0)))))));
}


#define  pio4  7.85398163397448309616E-1 // pi/4
#define  pio2  1.57079632679489661923    // pi/2


// replace x by remainder modulo pi/2 in ]-pi/4,pi/4[
// and returns the quotient mod 4
int Reduce(number_t* x)
{


#define pp1 1.570796251296997     // 20 bits of pi/2
#define pp2 7.549789415861596e-8  // 20 following bits
#define pp3 5.390302858158119e-15 // 52 following bits

    int q;

    q=float64_round( ((number_t)real_div(*x,(number_t)pio2)).r );   // integer part of the quotient
    //  x:=((x-q*pp1)-q*pp2)-q*pp3; // remainder

    x->d=real_sub( (number_t)real_sub( (number_t)real_sub(*x, (number_t)real_mul((number_t)intToReal(q),(number_t)pp1)) ,
                                                          (number_t)  real_mul((number_t)intToReal(q),(number_t)pp2) )  ,
               (number_t) real_mul((number_t)intToReal(q),(number_t)pp3) ); // remainder
    if (float64_lt( ((number_t)pio4).r,x->r))//(x>pio4) then
    { // keep in the range ]-pi/4,pi/4[
        x->d=real_sub(*x,(number_t)pio2);
        q++;
    }

    return q & 3; // only 2 bits
}

// clear sign of x
inline void real_abs(real_t*x)
{
    x->hi&=0x7fffffff;
}

// ----- Sin ----- }
//return the sine of x, with x in radians
double real_sin(number_t x)
{
    number_t c=x;
    real_abs(&c.r);
    if ( float64_lt(c.r ,((number_t)pio4).r ) ) return k_sin(x); //abs(x)<pio4 then sin:=k_sin(x)
    c=x;
    // take care of special cases and overflow (x > 2^30)
    if ((c.r.hi & 0x7fffffff)> 0x41cfffff) return default_nan();
    switch(Reduce(&x))
    {
        case 0:
            return k_sin(x);
        case 1:
            return k_cos(x);
        case 2:
            return real_minus((number_t)k_sin(x));
        case 3:
            return real_minus((number_t)k_cos(x));
    }
}


// -----Cos ----- }
//return the cosine of x, with x in radians
double real_cos(number_t x)
{

    number_t c;
    c=x;
    real_abs(&c.r);
    if ( float64_lt(c.r,((number_t)pio4).r) ) return k_cos(x);
    // take care of special cases and overflow
    // if abs(x)>2^30 then returns nan
    c=x;
    if ((c.r.hi & 0x7fffffff)>0x41cfffff ) return default_nan();
    switch (Reduce(&x))
    {
        case 0:
            return k_cos(x);
        case 1:
            return real_minus((number_t)k_sin(x));
        case 2:
            return real_minus((number_t)k_cos(x));
        case 3:
            return k_sin(x);
    }

} // cos

void* MemAlloc(int s)
{
	return malloc(s);
}

void FreeMem(void*p)
{
	free(p);
}

//---------------------
//  Set routines
//---------------------

// set singleton
// include a single element in a set 0..bound
void SetSingle(long unsigned* s, long unsigned e, long unsigned bound)
{
	long unsigned x=e>>5;
	if (x<bound) s[x]|=1<<(e&31);
	/*
	 *
	mov r3,r1,asr #5;
	and r1,r1,#31
	cmps r2,r3
	movhi r12,#1
	ldrhi r2,[r0,r3,lsl #2]
	orrhi r2,r2,r12,lsl r1
	strhi r2,[r0,r3,lsl #2]
	mov pc,lr
	 *
	 */
}

// include intervall [lo,hi] in set 0..x
// bound is the word size
void SetIntervall(long unsigned *s, long unsigned lo, long unsigned hi, long unsigned bound)
{
	long unsigned lo_mask, hi_mask;  // masks for bounds words
	long unsigned x=bound<<5;
	if (lo>hi) return;
	if (lo>x) return;
	if (hi>x) hi=x-1;
	lo_mask=0xffffffff<<(lo & 31);
	lo>>=5;
	hi_mask=~(0xfffffffe<<(hi&31));
	hi>>=5;
	if (lo>hi) return;
	if (lo==hi)
	{
		s[lo]|=(hi_mask & lo_mask);
		return;
	}
	s[lo]|=lo_mask;
	s[hi]|=hi_mask;
	for (lo++;lo<hi;lo++) s[lo]|=0xffffffff;
}

// test if set a is subset of set b, s being the word size of both sets
int IsSubset(long unsigned*a, long unsigned*b, int s)
{
	int i;
	for (i=0;i<s;i++)
	{
		if ((a[i]&~b[i])!=0) return 0;
	}
	return 1;
}


// test if two sets are equal
int SetEq(long unsigned*a, long unsigned*b, int s)
{
	int i;
	for (i=0;i<s;i++)
	{
		if (a[i]!=b[i]) return 0;
	}
	return 1; 
}

// test if e belongs to the set s = [0..max[
// b is word size
int IsInSet(long unsigned *s, long unsigned e, long unsigned b)
{
	long unsigned x=e>>5;
	if (x>=b) return 0;
	if (((s[x]>>(e&31))&1)!=0) return 1;
	return 0;
}


// set addition r=a+b union
void SetAdd(long unsigned *r, long unsigned *a, long unsigned*b, int s)
{
	int i;
	for (i=0;i<s;i++) r[i]=a[i]|b[i];
}

// set multiplication r=a*b intersection 
void SetMul(long unsigned *r, long unsigned *a, long unsigned*b, int s)
{
	int i;
	for (i=0;i<s;i++) r[i]=a[i]&b[i];
}

// set substraction r=a-b 
void SetSub(long unsigned *r, long unsigned *a, long unsigned*b, int s)
{
	int i;
	for (i=0;i<s;i++) r[i]=a[i]&~b[i];
}

// set xor symetrical difference r=a^b 
void SetXor(long unsigned *r, long unsigned *a, long unsigned*b, int s)
{
	int i;
	for (i=0;i<s;i++) r[i]=a[i]^b[i];
}

// clear a memory zone
void ClearMem(long unsigned*r, int s)
{
	int i;
	for (i=0;i<s;i++) r[i]=0;
}

