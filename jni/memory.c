// memory management for pp
//-------------------------

#include "pp.h"

// Dynamic memory
//---------------

// due to the stack structure of the compiler, no complex dynamic memory managment is needed
// a node is allocated by incrementing the index of available memory in the array Mem
// freing nodes link them in a linked list
// before allocating a new memory room, the list is checked for a free node of the same size
// as the required size.


// variable definition
halfword_t LoMemMax, HiMemMin;
memoryword_t Mem[MEMSIZE];
halfword_t Avail;


//+--------------------------------------------------------------+
//|                 |                    |                       |
//|                 |                    |                       |
//+--------------------------------------------------------------+
//          LoMemMax ^                    ^ HiMemMin

// Memory initialisation
void InitMem()
{
   LoMemMax=1; // nodes must not be zero because zero represents nada = empty list
   HiMemMin=MEMSIZE; //-1;
   //Mem[HiMemMin].u=HiMemMin; // create the first activation bloc with no header
   Avail=NADA;
}


// check if there is room in memory for n memorywords
// else, report an out of memory error
void Room(halfword_t n)
{
	if (HiMemMin<=LoMemMax+n) ReportError(OUTOFMEMORY);
}


// return a memory node of size n
halfword_t GetAvail(halfword_t n)
{
	halfword_t x;
	Room(n);
	/*
	while (Avail!=NADA)
	{   // first, search in the list of freed nodes of size n
		x=Avail;
		Avail=Mem[Avail].ll.link;
		if (Mem[x].ll.info==n) return x;
	}
	// if not found, then allocate a new one

	*/
	x=LoMemMax;
	LoMemMax+=n;

	return x;
}

/*

// return a node of size 2
function GetNode2:UInt16;
label 99;
var x : UInt16;
begin
  // first, search in free list
  while Avail<>Nada do
  begin
    x:=Avail;
    Avail:=pMem^[Avail].ll.link;
    if pMem^[x].ll.info=2 then
    begin
      GetNode2:=x;
      goto 99;
    end;
  end;
  // if not found, then allocate new
  GetNode2:=LoMemMax;
  LoMemMax:=LoMemMax+2;
  if HiMemMin<LoMemMax then ReportError(6,101);
99:
end;

// return a node of size 3
function GetNode3:UInt16;
label 99;
var x : UInt16;
begin
  // first, search in free list
  while Avail<>Nada do
  begin
    x:=Avail;
    Avail:=pMem^[Avail].ll.link;
    if pMem^[x].ll.info=3 then
    begin
      GetNode3:=x;
      goto 99;
    end;
  end;
  // if not found, then allocate new
  GetNode3:=LoMemMax;
  LoMemMax:=LoMemMax+3;
  if HiMemMin<LoMemMax then ReportError(6,101);
99:
end;

// return a node of size n
function GetNodeNn(n:UInt16):UInt16;
label 99;
var x : UInt16;
begin
  // first, search in free list
  while Avail<>Nada do
  begin
    x:=Avail;
    Avail:=pMem^[Avail].ll.link;
    if pMem^[x].ll.info=n then
    begin
      GetNodeNn:=x;
      goto 99;
    end;
  end;
  // if not found, then allocate new
  GetNodeNn:=LoMemMax;
  LoMemMax:=LoMemMax+n;
  if HiMemMin<LoMemMax then ReportError(6,101);
99:
end;

*/

// append node x in free list
// the size of the node is set in info field of the list
void FreeNode(halfword_t x, halfword_t s)
{
//emitString("[Free Node]");
	// check if node was latest allocated
	if (x+s==LoMemMax) LoMemMax=x;
	else
	{
		Mem[x].ll.info=s;
		Mem[x].ll.link=Avail;
		Avail=x;
	}
}

/*
// free a constant node
// the size depends on the basic type
procedure FreeConst_(x:UInt16);
begin
  case pMem^[x].nh.bt of
    btReal: FreeNoden(x,3);
    else    FreeNoden(x,2);
  end;
end;
*/

// mark memory
//------------
void MarkMem(memoryword_t*m)
{
	m->hh.lo=LoMemMax;
	m->hh.hi=Avail;
	Avail=NADA;
}

// release all memory location allocated
// from latest mark call
void ReleaseMem(memoryword_t m)
{
	LoMemMax=m.hh.lo;
	Avail=m.hh.hi;
}

