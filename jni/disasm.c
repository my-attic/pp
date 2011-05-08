#include <stdio.h>
#include <jni.h>


char buffer[80];
int Index;
int dotarget;
int target;

// append char c in buffer
char AppendChar(char c)
{
	buffer[Index]=c;
	if (c!=0) buffer[++Index]=0;
	return c;
}

void Comma()
{
	AppendChar(',');
}

void Space()
{
	AppendChar(' ');
}
void AppendString(char*s)
{
	while (AppendChar(*s++));
}

// apend condition code
void Condition(long unsigned x)
{
	switch(x>>28)
	{
	case 0: AppendString("EQ"); break;
	case 1: AppendString("NE"); break;
	case 2: AppendString("CS"); break;
	case 3: AppendString("CC"); break;
	case 4: AppendString("MI"); break;
	case 5: AppendString("PL"); break;
	case 6: AppendString("VS"); break;
	case 7: AppendString("VC"); break;
	case 8: AppendString("HI"); break;
	case 9: AppendString("LS"); break;
	case 10: AppendString("GE"); break;
	case 11: AppendString("LT"); break;
	case 12: AppendString("GT"); break;
	case 13: AppendString("LE"); break;
	}
}

void AppendReg(int n)
{
	int k=n&15;
	switch(k)
	{
	case 14:
		AppendString("LR");
		break;
	case 15:
		AppendString("PC");
		break;
	default:
		Index+=sprintf(buffer+Index,"R%d",k);
	}
}

void AppendNum(int n)
{
	Index+=sprintf(buffer+Index,"%ld",n);
}

void AppendHexa(int n)
{
	Index+=sprintf(buffer+Index,"%lx",n);
}

char*ShiftStr[4]={"LSL", "LSR", "ASR", "ROR" };

void AppendShift(long unsigned x)
{
	int k;
    int sn; // shift number

//  AppendChar(',');
    if ((x >> 4)&1)
    { // shift register
    	Comma();
    	AppendString(ShiftStr[(x >> 5) & 3]);
    	Space();
    	AppendReg(x >> 8);
    }
    else
    { // shift amount
    	k=(x >> 7) & 31;
    	sn=((x >> 5) & 3);
    	if ((k==0) && (sn==3))
    	{// ror#0 means rrx
    		AppendChar(',');
    		AppendString("RRX");
    	}
    	else //if k<>0 then
    	{
    		if ((sn!=0) && (k=0) ) k=32;
    		if ((sn!=0) || (k!=0) )
    		{ // write shift only if it is not lsl #0
    			Comma();
    			AppendString(ShiftStr[(x >> 5) & 3]);
    			AppendChar('#');
    			AppendNum(k);
    		}
    	}
    }
}


void SetCondition(long unsigned x)
{
  if ((x >> 20)&1)  AppendChar('S');
}


void AppendPSR(long unsigned x)
{
	if ((x >> 22)&1) AppendString("SPSR");
	else AppendString("CPSR");
}

void  AppendRegList(long unsigned x)
{
	int i;
	int l; // last register set
	int s; // length of last consecutive "1" sequence
	long unsigned maskreg;

	AppendChar('{');
	maskreg=x & 65535;
	l=-2;
	s=0;
	for (i=0;i<=16;i++)
	{
		if (maskreg&1)
		{
			if (s==0 )
			{
				if (l>=0 ) Comma();
				AppendReg(i);
			}
			s++;
			l=i;
		}
		else
		{
			if (l==i-1)
			{
				if (s==2) Comma();
				else if (s>2)  AppendChar('-');
				if (s>=2 ) AppendReg(l);
			}
			s=0;
		}
		maskreg>>=1;
	}
	AppendChar('}');
}


void PCDest(long unsigned a,long unsigned x, int d)
{
	if (((x >> 16) & 15)==15)
	{ // compute destination
		dotarget=1;
		target=a+d+8;
	}
}

long unsigned RotateRight(long unsigned x, int k)
{
	return (x>>k) + (x<<(32-k));
}


void Bkpt(long unsigned x)
{
	AppendString("BKPT" );
	Space();
    AppendNum(((x >> 4) & 0x0000fff0)+(x & 0xf));
}

void  BXBLX(long unsigned x)
{
	// BX - BLX

	if ((x >> 5)&1) AppendString("BLX");
	else AppendString("BX");
	Condition(x);
	Space();
	AppendReg(x);
}

void BBL(long unsigned a, long unsigned x)
{   // branch
	int y;
	AppendChar('B');
	if ((x >> 24)&1) AppendChar('L');
	Condition(x);
	Space();
	y=x&0x00ffffff;
	if ((y>>16)>127) y|=0xff000000;
	AppendNum(y);
	dotarget=1;
	target=(a+(y<<2))+8;
}

void BLX1(long unsigned a, long unsigned x)
{
	int y;
	// BLX(1) branch and exchange to thumb
	AppendString("BLX");
	y=((x & 0x00ffffff) << 1) + ((x >>24) & 1);
	if ((y>>16)>127) y|=0xff000000;
	Space();
	AppendNum(y);
	dotarget=1;
	target=a+(y << 1)+8;
}


/*
begin
  pu:=DataPointer;
  dotarget:=false;
  // write address
  if @s<>nil then
  begin
    x:=Swap32(pu^[a shr 2]);
    i:=1;
    if wa then
    begin
      HexaChars4(@s,a);
      i:=6;
    end;
    if wd then
    begin
      HexaChars8(@s[i],x);
      i:=i+9;
    end;
 */

char* ArmDataProcStr[16]=
{  "AND","EOR","SUB","RSB","ADD","ADC","SBC","RCS",
   "TST","TEQ","CMP","CMN","ORR","MOV","BIC","MVN"
};

char* ArmInstr(long unsigned a, long unsigned x)
{
	int y,k;

	Index=0;
	buffer[0]=0;
	dotarget=0;
    if ((x & 0x0fffffd0)==0x012fff10)  BXBLX(x);
    else if ((x & 0x0e000000)==0x0a000000)  BBL(a,x);
    else if ((x & 0xfe000000)==0xea000000)  BLX1(a,x);
    // psr transfert
    else if ((x & 0x0fbf0fff)==0x010f0000)
    {  // psr to reg
      AppendString("MRS");
      Condition(x);
      Space();
      AppendReg(x >> 12);
      Comma();
      AppendPSR(x);
    }
    else if ((x & 0xfff000f0)==0xe1200070)  Bkpt(x);
    else if ((x & 0x0fbffff0)==0x0129f000)
    {// reg to psr
    	AppendString("MSR");
    	Condition(x);
    	Space();
    	AppendPSR(x);
    	Comma();
    	AppendReg(x);
    }
    else if (((x & 0x0FbFfFf0)==0x0129f000) ||
    	       ((x & 0x0dbff000)==0x0128f000))
    {
    	AppendString("MSR");
    	Condition(x);
    	Space();
    	if ((x >> 22) & 1) AppendString("SPSR_flg");
    	else AppendString("CPSR_flg");
    	Comma();
    	if ((x >> 25)&1)
    	{
    		AppendChar('#');
    		AppendNum(RotateRight(x & 255, ((x >> 8) & 15) << 1));

    	//        AppendNum((x and 255) shl ((x shr 8) and 31))
    	}
    	else AppendReg(x);
    }
    // count leading zeros
    else if ((x & 0x0fff0ff0)==0x016f0f10)
    {
    	AppendString("CLZ");
    	Condition(x);
    	Space();
    	AppendReg(x >> 12);
    	Comma();
    	AppendReg(x);
    }
    // single data swap -- to be checked before halfword and signed data transferts
    else  if ((x & 0x0fb00ff0)==0x01000090 )
    {
    	AppendString("SWP");
    	Condition(x);
    	if ((x >> 22)&1) AppendChar('B');
    	Space();
    	AppendReg(x >> 12);
    	Comma();
    	AppendReg(x);
    	AppendString(",[");
    	AppendReg(x >> 16);
    	AppendChar(']');
    }
    // multiply ( and accumulate )
    else if ((x & 0x0fc000f0)==0x00000090)
    {
    	if ((x >> 21)&1)
    	{ // accumulate
    		AppendString("MLA");
    	}
    	else
    	{ // multiply only
    		AppendString("MUL");
    	}
    	Condition(x);
    	SetCondition(x);
    	Space();
    	AppendReg( x >> 16);
    	Comma();
    	AppendReg(x);
    	Comma();
    	AppendReg(x >> 8);
    	if ((x >> 21)&1)
    	{
    		Comma();
    		AppendReg(x >> 12);
    	}
    }
    // multiply long
    else if ((x & 0xf8000f0)==0x00800090)
    {
    	if ((x >> 22)&1)  AppendChar('S'); else AppendChar('U');
    	if ((x >> 21)&1)  AppendString("MLA"); else AppendString("MUL");
    	Condition(x);
    	SetCondition(x);
    	Space();
    	AppendReg(x >> 12);
    	Comma();
    	AppendReg(x >> 16);
    	Comma();
    	AppendReg(x);
    	Comma();
    	AppendReg(x >> 8);
    }
    // halfword and signed data transfert
    else if ( ((x & 0x0e400090)==0x00400090) ||
         ((x & 0x0e400f90)==0x00000090)  )
//         and
//       ( (x and $60)<>0 )

    {
    	AppendString((x >> 20)&1?"LDR":"STR");
    	Condition;
		switch (x & 0x60)
		{
		case 0x20: AppendChar('H'); break;
		case 0x40: AppendString("SB"); break;
		case 0x60: AppendString("SH"); break;
		}
		Space();
		AppendReg(x >>12);
		AppendString(",[");
		AppendReg(x >> 16);
		if ((x >> 24)&1)
		{ // preindexed
			if ((x >> 22)&1)
			{ // immediate offset
				y=(x & 15)+((x >> 4)& 0xf0);
				if (((x >> 23)&1)==0)  y=-y;
				if (y!=0)
				{
					AppendString(",#");
					AppendNum(y);
				}
				PCDest(a,x,y);
			}
			else
			{ // register offset
				Comma();
				if (!((x >> 23)&1)) AppendChar('-');
				AppendReg(x);
			}
			AppendChar(']');
			if ((x >> 21)&1)  AppendChar('!');
		}
		else
		{ // postindexed
			AppendString("],");
			if ((x >> 22)&1)
			{ // immediate offset
				y=(x & 15)+((x >> 4)& 0xf0);
				if (!((x >> 23)&1))  y=-y;
				if (y!=0)
				{
					AppendChar('#');
					// AppendString(comsharp);
					AppendNum(y);
				}
			}
			else
			{ // register offset
				if (!( (x >> 23)&1))  AppendChar('-');
				AppendReg(x);
			}
		}
    }
	else if ((x & 0x0c000000)==0)
    { // data processing
		k=(x >> 21) & 15;
		AppendString(ArmDataProcStr[k]);
		Condition(x);
		SetCondition(x);
		Space();
		switch (k)
		{
		case 13:
		case 15: // mov,mvn
			AppendReg(x >> 12);
			break;
		case 8:
		case 9:
		case 10:
		case 11: // cmp,cmn,teq,tst
			AppendReg(x >> 16);
			break;
		default:
			AppendReg(x >> 12);
			Comma();
			AppendReg(x >> 16);
		}
		// operand 2
		if ((x >> 25)&1)
		{ // immediate value
			y=RotateRight(x & 255, (x >> 7) & 0x1e);
			AppendString(",#");
			AppendNum(y);
			if (((x>>16)&15)==15)
			{
			    if (k==4) PCDest(a,x,y);
			    else if (k==2) PCDest(a,x,-y);
			}
		}
		else
		{ // register
			Comma();
			AppendReg(x);
			AppendShift(x);
		}
    }
    // single data transfert
	else if ((x & 0x0c000000)==0x04000000 )
	{
    	AppendString((x >> 20)&1?"LDR":"STR");
		Condition(x);
		if ((x >> 22)&1 ) AppendChar('B');
		if ( ((x >> 21)&1) && (! ((x >> 24)&1)))  AppendChar('T');
		Space();
		AppendReg(x >> 12);
		AppendString(",[");
		AppendReg(x >> 16);
		if ((x >> 24)&1 )
		{  // preindexed
			if ((x >> 25)&1 )
			{ // register offset
				Comma();
				if (! ((x >> 23)&1 ) ) AppendChar('-');
				AppendReg(x);
				AppendShift(x);
			}
			else // immediate offset
			{
				y=x & ((1 << 12)-1); // offset
				if (!((x >> 23)&1)) y=-y;
				if (y!=0 )
				{
					AppendString(",#");
					AppendNum(y);
				}
				PCDest(a,x,y);
			}
			AppendChar(']');
			if ((x >> 21)&1 ) AppendChar('!');
		}
		else
		{  // postindexed
			AppendString("],");
			if (! ((x >> 25)&1) )
			{ // immediate offset
				k=x & ((1 << 12)-1); // offset
				if (! ((x >> 23) &1)) k=-k;
				AppendChar('#');
				AppendNum(k);
			}
			else
			{ // register ofset
				if (!( (x >> 23)&1))  AppendChar('-');
				AppendReg(x);
				AppendShift;
			}
		}
    }
    // bloc data transfert
	else if ((x & 0x0e000000) == 0x08000000 )
    {
		if ((x >> 20)&1)  AppendString("LDM"); else AppendString("STM");
		Condition(x);
		if (((x >> 16) & 15)==13 )
		{ // stack
			if (((x >> 24)&1) ^ ((x >> 20)&1) ) AppendChar('F'); else AppendChar('E');
			if (((x >> 23)&1) ^ ((x >> 20)&1) ) AppendChar('A'); else AppendChar('D');
		}
		else
		{ // others
			if ((x >> 23)&1)  AppendChar('I'); else AppendChar('D');
			if ((x >> 24)&1)  AppendChar('B'); else AppendChar('A');
		}
		Space();
		AppendReg(x >> 16);
		if ((x >> 21)&1 ) AppendChar('!');
		Comma();
		AppendRegList(x);
		if ((x >> 22)&1 ) AppendChar('^');
    }
    // software interrupt
	else if (((x >> 24) & 15)==15 )
    {
		AppendString("SWI");
		Condition(x);
		// append comment field
		Space();
		AppendNum(x&0x00ffffff);
    }
    // coprocessor data operation
	else if ((x & 0x0f000010) == 0x0e000000 )
    {
      AppendString("CDP");
      Condition(x);
      Space();
      AppendChar('p');
      AppendNum((x >> 8) & 15);
      Comma();
      AppendNum((x >> 20) & 15);
      AppendString(",cr");
      AppendNum((x >> 12) & 15);
      AppendString(",cr");
      AppendNum((x >> 16) & 15);
      AppendString(",cr");
      AppendNum(x & 15);
      if (((x>>5)&7)!=0)
      {
        Comma();
        AppendNum((x >> 5) & 7);
      }

    }
    // coprocessor data transfert
	else if ((x & 0x0e000000)==0x0c000000 )
    {
		if ((x >> 20)&1 ) AppendString("LDC"); else AppendString("STC");
		Condition(x);
		if ((x >> 22)&1 ) AppendChar('L');
		Space();
		AppendChar('p');
		AppendNum((x >> 8) & 15);
		AppendString(",cr");
		AppendNum((x >> 12) & 15);
		AppendString(",[");
		AppendReg(x >> 16);
		if ((x >> 24) &1)
		{// preindexed
			y=x & 255;
			if (! ((x >> 23) &1)) y=-y;
			if (y!=0)
			{
				AppendString(",#");
				AppendNum(y);
			}
			AppendChar(']');
			if ((x >> 21)&1) AppendChar('!');
		}
		else
		{ // postindexed
			AppendString("],#");
			y=x & 255;
			if (! ((x >> 23) &1)) y=-y;
			AppendNum(y);
		}
		PCDest(a,x,y);
    }
    // coprocessor register transfert
	else if ((x & 0x0f000010) == 0x0e000010 )
    {
    	if ((x >> 20)&1 ) AppendString("MRC"); else AppendString("MCR");
    	Condition(x);
    	Space();
    	AppendChar('p');
    	AppendNum((x >> 8) & 15);
    	Comma();
    	AppendNum((x >> 21) & 7);
    	Comma();
    	AppendReg((x >> 12));
    	AppendString(",cr");
    	AppendNum((x >> 16) & 15);
    	AppendString(",cr");
    	AppendNum(x & 15);
    	if (((x>>5)&7)!=0)
    	{
            Comma();
            AppendNum((x >> 5) & 7);
    	}
    }
    else
    {
    	AppendString("undefined !");
    }






    if (dotarget)
    {
    	do AppendChar(' '); while (Index<25);
    	AppendChar(';');
    	AppendHexa(target);
    }

    return buffer;

}

/*

    // undefined instruction
    AppendString('undefined');
    i:=i+1;
    HexaChars8(@s[i],x);
  end;
10:
 ArmDisasm:=4;
end;


 */


// main function of the arm disassembler

jstring Java_pp_compiler_Inspect_DisasmArm(JNIEnv* env, jobject obj, jint a, jint x)
{
	char msg[80];
	sprintf(msg, "%06x %08x %s\n", a, x, ArmInstr(a,x));
	return (*env)->NewStringUTF(env, msg);
}
