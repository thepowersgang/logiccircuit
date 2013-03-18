/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>

typedef struct
{
	 int	Size;
	 int	Pos;
}	t_info;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs)
{
	 int	nOutput;
	
	if( NParams != 1 )	return NULL;

	nOutput = Params[0];

	tElement *ret = EleHelp_CreateElement(2, nOutput, sizeof(t_info));
	if(!ret)	return NULL;
	t_info	*info = ret->Info;
	
	info->Size = nOutput;
	info->Pos = 0;
	
	return ret;
}

static void _Update(tElement *Ele)
{
	t_info	*this = Ele->Info;
	
	// Reset
	if( GetLink(Ele->Inputs[0]) )
		this->Pos = 0;
	
	// Increment
	if( GetLink(Ele->Inputs[1]) )
		this->Pos ++;
	
	// Wrap
	if( this->Pos == (1 << this->Size) )
		this->Pos = 0;
	
	// Output
	for( int i = 0; i < this->Size; i ++ ) {
		if( this->Pos & (1 << i) )
			RaiseLink(Ele->Outputs[i]);
	}
}

tElementDef gElement_COUNTER = {
	NULL, "COUNTER",
	2, 2,
	_Create,
	NULL,
	_Update
};
