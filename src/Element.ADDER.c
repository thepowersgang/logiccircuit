/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 *
 * Element.ADDER.c
 * - Arbitary length full adder
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

typedef struct
{
	 int	Bits;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	bits;
	
	if( NParams != 1 )	return NULL;

	bits = Params[0];

	tElement *ret = EleHelp_CreateElement(1+bits*2+1, 1+bits+1, sizeof(t_info));
	if(!ret)	return NULL;
	t_info	*info = ret->Info;
	
	info->Bits = bits;
	
	return ret;
}

static uint64_t _ReadValue(tElement *Ele, int First, int Size)
{
	assert(Size <= 64);
	uint64_t ret = 0;
	for( int i = 0; i < Size; i ++ ) {
		if( GetEleLink(Ele, First+i) )
			ret |= (1ULL << i);
	}
	return ret;
}

static void _WriteValue(tElement *Ele, int First, int Size, uint64_t Value)
{
	assert(Size <= 64);
	for( int i = 0; i < Size; i ++ ) {
		SetEleLink(Ele, First + i, !!(Value & (1ULL << i)));
	}
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	
	if( !GetEleLink(Ele, 0) )
		return ;

	if( this->Bits <= 64 )
	{
		uint64_t	ret, a, b;
		 int	c, cout;
		
		a = _ReadValue(Ele, 1, this->Bits);
		b = _ReadValue(Ele, 1+this->Bits, this->Bits);
		c = _ReadValue(Ele, 1+this->Bits*2, 1);
		ret = a + b + c;
		if( this->Bits < 64 )
			ret &= (1ULL << this->Bits)-1;
		cout = (ret < a);
		
		//printf("%lli + %lli + %i = %lli + %i<<%i\n",
		//	a, b, c, ret, cout, this->Bits);
		
		_WriteValue(Ele, 1, this->Bits, ret);
		_WriteValue(Ele, 1+this->Bits, 1, cout);
	}
	else
	{
		assert(this->Bits <= 64);
	}
	
	SetEleLink(Ele, 0, true);
}

tElementDef gElement_ADDER = {
	NULL, "ADDER",
	4, -1,
	1,
	_Create,
	NULL,
	_Update
};
