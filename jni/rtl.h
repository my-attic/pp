#ifndef RTL
#define RTL
#include <inttypes.h>

// runtime error in the runtime library
// system function that are supposed to have an error returns an int
// the returned value is 0 if no error has occured
// and else, it returns an error number
// the call of such routine is
//      MOV lr,pc			; save return adress
//		LDR R15,[R9,#n]		; system call
//      CMPS R0,0           ; test return value
//      BNE Halt			; if R0 is not zero, then halt
//

#define RTE_REWRITE	4	// "rewrite can't open file"
#define RTE_PEEK	5	// peek error
#define RTE_GET		6	// get error
#define RTE_RESET	8  	// "reset can't open file"
#define RTE_PUT 	10 	// "put error"




typedef struct
{
    uint32_t lo, hi;
} real_t;

typedef union
{	double d;
	real_t r;
}number_t;

// structure for Pascal files
typedef struct
{
	FILE* file;    // 0
	uint8_t mode;  // 4
	uint8_t spare; // 5
	uint16_t size; // 6
	char c;			// 8
} fileDesc_t;


// 0
void* get_memory(int size);
// 4
void free_memory(void*p);
// 8
void write_string(char*s, int width, fileDesc_t*fd);
// 12
int new_line(fileDesc_t* fd);
// 16
void write_integer(int x, int width,fileDesc_t*fd);
// 20
void write_float(double x, int width, fileDesc_t*fd);
// 24
void write_boolean(int x, int width, fileDesc_t*fd);
// 28
void write_char(int x, int width,fileDesc_t* fd);
// 32
void write_fixed(double x, int width, int prec, fileDesc_t*fd);
// 36
double intToReal(int a);
// 40
double uintToReal(uint32_t a);
// 44
double real_add( number_t a, number_t b );
// 48
double real_sub( number_t a, number_t b );
// 52
double real_mul( number_t a, number_t b );
// 56
double real_div(number_t a, number_t b);
// 60
double real_sqrt( number_t a );
// 64
int32_t float64_round( real_t a );
// 68
int32_t float64_trunc(real_t a);
//72
int float64_eq( real_t a, real_t b );
//76
int float64_le( real_t a, real_t b );
//80
int float64_lt( real_t a, real_t b );
// 84
void write_unsigned(unsigned int x, int width, fileDesc_t*fd);
//#define REALLN			88
double real_ln(number_t);
//#define REALEXP			92
double real_exp(number_t);
//#define REALATAN		96
double real_atan(number_t);
//#define	REALSIN			100
double real_sin(number_t);
//#define REALCOS			104
double real_cos(number_t);
// 108
void RewriteOutput(fileDesc_t*fd);
// 112
void ResetInput(fileDesc_t*fd);
// 116 -- 28
int Put(fileDesc_t* fd);
// 120 -- 29
// clrscr
// 124 -- 31
//char GetCharConsole();
void ReadString(fileDesc_t* f, char*s);
// 128 -- 32
void ReadReal(fileDesc_t*,double*);
// 132 -- 33
void ReadInt(fileDesc_t* f, int* j);
// 136 -- 34
int Eof(fileDesc_t*fd);
// 140
int Get(fileDesc_t *fd);
// 144
void FileAccess(fileDesc_t*fd);
// 148
void ReadChar(fileDesc_t* fd, char*c);
//void ReadCharText(fileDesc_t*fd);
// 152
int Eoln(fileDesc_t*fd);
// 156
void FlushLine(fileDesc_t *fd);
// 160
int Reset(fileDesc_t*f, uint16_t nBytes, char*name);
// 164
int Rewrite(fileDesc_t*f, uint16_t nBytes, char*name);
// 168
void Close(fileDesc_t*f);

// 192 -- 48
void SetSingle(long unsigned* s, long unsigned e, long unsigned bound);
// 196 -- 49
void SetIntervall(long unsigned *s, long unsigned lo, long unsigned hi, long unsigned bound);
// 200 - 50
int IsSubset(long unsigned*a, long unsigned*b, int s);
// 204 - 51
int SetEq(long unsigned*a, long unsigned*b, int s);
// 208 - 52
int IsInSet(long unsigned *s, long unsigned e, long unsigned bound);
// 212 - 53 set addition r=a+b union
void SetAdd(long unsigned *r, long unsigned *a, long unsigned*b, int s);
// 216 - 54 set multiplication r=a*b intersection 
void SetMul(long unsigned *r, long unsigned *a, long unsigned*b, int s);
// 220 - 55 set substraction r=a-b 
void SetSub(long unsigned *r, long unsigned *a, long unsigned*b, int s);
// 224 - 56 set xor symetrical difference r=a^b 
void SetXor(long unsigned *r, long unsigned *a, long unsigned*b, int s);
// 228 - 57
void ClearMem(long unsigned*r, int s);



#endif
