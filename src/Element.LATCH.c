/*
 * LATCH element
 * - RS Latch
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	 int	Size;
	bool	*CurVal;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Param, int NInputs)
{
	 int	size = 0;
	
	if(NParams >= 1)	size = Param[0];
	
	if(size <= 0)	size = 1;
	
	if(NInputs != 2 + size)	return NULL;


	tElement *ret = EleHelp_CreateElement(2+size, 1+size, sizeof(t_info) + size*sizeof(bool));
	if(!ret)	return NULL;
	t_info	*info = ret->Info;
	
	info->Size = size;
	info->CurVal = (void*)(info + 1);
	memset(info->CurVal, 0, size);
	
	return ret;
}

static int _Duplicate(const tElement *Source, tElement *New)
{
	t_info	*info = New->Info;
	info->CurVal = (void*)(info + 1);
	return 0;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	
	// Pulse
	SetEleLink(Ele, 0, GetEleLink(Ele, 0));

	// Reset or set?	
	if( GetEleLink(Ele, 1) ) {
		memset(this->CurVal, 0, sizeof(*this->CurVal)*this->Size);
	}
	else
	{	// Set
		for( int i = 0; i < this->Size; i ++ ) {
			if( GetEleLink(Ele, 2+i) )
				this->CurVal[i] = 1;
		}
	}
	
	// Output
	for( int i = 0; i < this->Size; i ++ )
	{
		SetEleLink(Ele, i+1, this->CurVal[i]);
	}
}

tElementDef gElement_LATCH = {
	NULL, "LATCH",
	3, -1, 1,
	_Create,
	_Duplicate,
	_Update
};
