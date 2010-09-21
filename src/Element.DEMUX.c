/*
 */
#include <element.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
	tElement	Ele;
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int Param, int NInputs, tLink **Inputs)
{
	t_element *ret;
	if( Param < 1 )
		Param = 1;
	
	if( NInputs != 1 + Param ) {
		return NULL;
	}
	
	ret = calloc( 1, sizeof(t_element) + (NInputs + (1<<Param))*sizeof(tLink*) );
	if(!ret)	return NULL;
	
	ret->Ele.NInputs = NInputs;
	ret->Ele.Inputs = &ret->_links[0];
	ret->Ele.NOutputs = 1 << Param;
	ret->Ele.Outputs = &ret->_links[NInputs];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	val = 0, i;
	
	// Parse inputs as a binary stream
	for( i = 1; i < Ele->NInputs; i ++ )
	{
		if( GetLink(Ele->Inputs[i]) )
			val |= 1 << (i-1);
	}
	
	// If ENABLE, drive output
	if( GetLink(Ele->Inputs[0]) )
		RaiseLink(Ele->Outputs[val]);
	
	//printf("DEMUX val=%i, ENABLE=%i, %s->NDrivers=%i\n",
	//	val, GetLink(Ele->Inputs[0]), Ele->Outputs[val]->Name, Ele->Outputs[val]->Value->NDrivers);
}

tElementDef gElement_DEMUX = {
	NULL, "DEMUX",
	2, -1,
	_Create,
	_Update
};
