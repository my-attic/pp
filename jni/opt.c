#include "pp.h"

// Optimiser
// this optimiser reduces the evaluation tree to an ewuivalent one

// this returns true if op an unary operator
static boolean_t Unary(Op_t op)
{
	return (op>=opNot) && (op<opAddr);// opAddr parameter is special
}


// this returns true if op is a binary operator
static boolean_t Binary(Op_t op)
{
	return (op>=opAdd) && (op<=opIn);
}

// compute the square of a double given by low and high unsigned words
void RealSqr(word_t*lo, word_t*hi)
{
    number_t x;
    x.uu.lo=*lo;
    x.uu.hi=*hi;
    x.d*=x.d;
    *lo=x.uu.lo;
    *hi=x.uu.hi;
}

// unmary evaluation
// return true if evaluation performed
// Replace the constant in Mem[e] by the result of operation op to the constant
//---------------------------------------
boolean_t UnaryEval(Op_t op, BasicType_t bt, halfword_t e)
{
	switch (op)
	{
	case opNeg:
		switch(bt)
		{
		case btInt: case btUInt:
			Mem[e+1].i=-Mem[e+1].i;
			Mem[e].nh.bt=btInt; // force signed
			break;
		case btReal:
			Mem[e+2].u^=0x80000000; // change sign bit
			break;
		default:
			return False;
		}
		break;
    case opSqr:
        switch(bt)
        {
        case btInt:
        case btUInt:
            Mem[e+1].u*=Mem[e+1].u;
            return True;
        case btReal:
            RealSqr(&Mem[e+1].u,&Mem[e+2].u);
            return True;
        }
        return False;
    case opAbs:
        switch (bt)
        {
            case btInt:
                if (Mem[e+1].i<0) Mem[e+1].i=-Mem[e+1].i;
            case btUInt: // For unsigned notihing to do
                return True;
            case btReal: // clear sign bit
                Mem[e+2].u&=0x7fffffff;
                return True;
        }
        return False;
    case opNot:
        switch (bt)
        {
        case btInt: case btUInt:
            Mem[e+1].u=~Mem[e+1].u;
            return True;
        case btBool:
            Mem[e+1].u^=1;
            return True;
        }
        return False;
    case opPred:
      Mem[e+1].i--;
      return True;
    case opSucc:
      Mem[e+1].i++;
      return True;
	default:
		return False;
	}
	return True;
}

boolean_t EvalBinaryReal(Op_t op, halfword_t a, halfword_t b)
{
    number_t na, nb;
    na.uu.lo=Mem[a+1].u;
    na.uu.hi=Mem[a+2].u;
    nb.uu.lo=Mem[b+1].u;
    nb.uu.hi=Mem[b+2].u;
    switch (op)
    {
    case opAdd:
        na.d+=nb.d;
    lr:
        Mem[a+1].u=na.uu.lo;
        Mem[a+2].u=na.uu.hi;
        FreeNode(b,3);
        return True;
    case opSub:
        na.d-=nb.d;
        goto lr;
    case opMul:
        na.d*=nb.d;
        goto lr;
    case opDiv:
        if (nb.d==0.0) ReportError(DIVISIONBYZERO);
        na.d/=nb.d;
        goto lr;
    default: return False;
    }
}

// replace the constant string of node a by the result on operation a and b
//
boolean_t BinaryEvalStr(Op_t op, halfword_t a, halfword_t b)
{
	halfword_t sa, sb; // constant strings
	sa=Mem[a+1].i; // string a
	sb=Mem[b+1].i; // string b

	switch (op)
	{
	case opAdd:
		Mem[a+1].i=ConcatStr(sa,sb);
		return True;
	case opLe:
	case opLt:
	case opEq:
	case opNe:
		break;
	}
	return False;
}
// replace the node a by the operation a op b
// and free the node b
// returns true if the computation is performed, and false elsewhere
//-------------------------------------------
boolean_t BinaryEval(Op_t op, BasicType_t bt, halfword_t a, halfword_t b)
{
    switch(bt)
    {
    case btInt:
    case btUInt:
        switch(op)
        {
        case opAdd:
            Mem[a+1].i+=Mem[b+1].i;
        li:
            Mem[a].nh.bt=bt;
            FreeNode(b,2);
            return True;
        case opSub:
            Mem[a+1].i-=Mem[b+1].i;
            goto li;
        case opIDiv:
            if (Mem[b+1].i==0) errdiv:ReportError(DIVISIONBYZERO);
            if (bt==btInt) Mem[a+1].i/=Mem[b+1].i;
            else  Mem[a+1].u/=Mem[b+1].u;
            goto li;
        case opMod:
            if (Mem[b+1].i==0) goto errdiv;
            if (bt==btInt) Mem[a+1].i%=Mem[b+1].i;
            else  Mem[a+1].u%=Mem[b+1].u;
            goto li;
        case opMul:
            Mem[a+1].u*=Mem[b+1].u;
            goto li;
        case opShr:
            if (bt==btInt) Mem[a+1].i>>=Mem[b+1].i;
            else Mem[a+1].u>>=Mem[b+1].i;
            goto li;
        case opShl:
            Mem[a+1].u<<=Mem[b+1].i;
            goto li;
        case opAnd:
            Mem[a+1].u&=Mem[b+1].u;
            goto li;
        case opOr:
            Mem[a+1].u|=Mem[b+1].u;
            goto li;
        case opXor:
            Mem[a+1].u^=Mem[b+1].u;
            goto li;
        default:
            return False;
        }
        return True;
    case btReal:
        return EvalBinaryReal(op,a,b);
    case btStr:
    	return BinaryEvalStr(op,a,b);
    default:
        return False;
    }
}

halfword_t RealNodeFromInteger(int i)
{
    halfword_t e;
    number_t x;
    x.d=i;
    e=RealConstNode(x.uu.lo, x.uu.hi);
    return e;
}


// optimize expression
void OptimizeExpression(halfword_t*e)
{
	Op_t op;
	BasicType_t bt;
	halfword_t y;
	halfword_t a, b; // operands of a binary operator

	op=Mem[*e].nh.op;
	bt=Mem[*e].nh.bt;

	if (op==opConst) return;

	if (op==opVar)
	{
		// TODO
		return;
	}

	if (Unary(op))
	{
		CheckStack();
		OptimizeExpression(&Mem[*e].ll.link); // recursive optimize operand
		halfword_t a=Mem[*e].ll.link; // parameter of the expression
		if (Mem[a].nh.op==opConst)
		{
		    if (op==opCast)
		    {   // cast may require more room that the parameter
		        if (bt==btReal)
		        {
		            y=*e;
		            *e=RealNodeFromInteger(Mem[a+1].i);
		            FreeNode(y,1);
		            FreeNode(a,2);;
		        }
		        else if (bt==btStr)
		        {	// cast char to string just require to change the basic type
		        	// as string number below 256 are simply single char strings
		        	Mem[a].nh.bt=btStr;
		        	FreeNode(*e,1); // free the cast node
		        	*e=a;  // replace const string node by transformed char param node
		        }
                return;
		    }
			if (UnaryEval(op,bt,a))
			{
				y=*e; // memorize the operator node
				*e=a;  // replace operator node by the precomputed constant
				FreeNode(y,1); // then free the operator node
			}
		}

		return;
	}


	if (Binary(op))
	{

		CheckStack();
		OptimizeExpression(&Mem[*e+1].hh.lo); // recursively optimize both parameters
		OptimizeExpression(&Mem[*e+1].hh.hi);
		a=Mem[*e+1].hh.lo;  // get both operands
		b=Mem[*e+1].hh.hi;
		if (Mem[a].nh.op==opConst)
		{  // first operand is a constant
			if (Mem[b].nh.op==opConst)
			{ 	// both are constant, thus precompute
				if (BinaryEval(op,bt,a,b))
				{
				    y=*e; // memorize to free it after
				    // replace the operator node by the precomuted constant
				    *e=a;
				    FreeNode(y,2);
				}
				return;
			}
		}
		else
		{
			if (Mem[b].nh.op==opConst)
			{	// first operand is not a constant but the second is

				switch(op)
				{	// for commutative operators, force the constant on the first operand
				case opAdd:
				case opMul:
				case opXor:
				case opAnd:
				case opOr:
					if (bt!=btStr)
					{
						Mem[*e+1].hh.lo=b;
						Mem[*e+1].hh.hi=a;
					}
					break;
				case opSub: // replace  a - k by (-k) + a
					// TO DO
					break;
				}
			}
		}
		// TODO simplification of associative operators
		// k1 op (k2 op expr) = (k1 op k2) op expr
		// TODO distributive operator
		// k1 * (k2 +- expr) = (k1*k2) +- (k1 * expr)

	}

	/// simplification of expressions
	/// duplicate opposites
	///--------------------
	/// neg ( neg expr) == expr
	/// not ( not expr) == expr
	/// absolute value
	///---------------
	/// abs(- expr) == abs(expr)
	switch(op)
	{
	case opMul:
		// absorbing element 0 * x == 0
		// neutral element 1 * x == x
		// negate instead of multiplinb by -1 :  -1 * x == - x
		// []*E == []
		// 0.0 * x == 0.0
		// 1.0 * x == x
		// -1.0 * x == - x
		break;
	case opAdd:
		// 0 + x = x
		// [] + x == x
		break;
	case opSub:
		// O - x == neg x
		// x - 0 == x
		// x - [] == x
		// [] - x == []
		break;
	case opAnd:
	    // 0 and x = O
	    // -1 and x = x
	    // false and x = false
	    // true and x = x
		break;
	case opOr:
		// true or x = true
		// false of x = x
		// -1 or x = -1
		// 0 or x = x
		break;
	case opXor:
		// true xor x = not x
		// false xor x = x
		// -1 xor x = not x
		// 0 xor x = x
		break;
	case opIDiv:
		// check division by zero
		// x div 1 == x
		// x div -1 == -x
		break;
	case opDiv:
		// check division by zero
		// x div 1.0 == x
		// x div -1.0 == -x
		// x div k == (1/k) * x
		break;
	}

}

void OptimizeStatements(halfword_t*n)
{
	// TODO
}
