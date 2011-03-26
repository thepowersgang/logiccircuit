/*
 */
#include <element.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
	tElement	Ele;
	
	tLink	*_links[];
}	t_element;

// === CODE ===
static tElement *_Create(int NParams, int *Params, int NInputs, tLink **Inputs)
{
	t_element *ret = calloc( 1, sizeof(t_element) + (1+NInputs)*sizeof(tLink*) );
	if(!ret)	return NULL;
	ret->Ele.NOutputs = 1;
	ret->Ele.NInputs = NInputs;
	ret->Ele.Outputs = &ret->_links[0];
	ret->Ele.Inputs = &ret->_links[1];
	return &ret->Ele;
}

static void _Update(tElement *Ele)
{
	 int	out = 1;
	 int	i;
	
	// --- HACK ---
	#if 0
	if( Ele->NInputs == 3 ) {
		printf("%i (%s)", GetLink(Ele->Inputs[0]), Ele->Inputs[0]->Name);
		for( i = 1; i < Ele->NInputs; i++ )
		{
			printf(" && %i (%s)", GetLink(Ele->Inputs[i]), Ele->Inputs[i]->Name);
		}
		printf("\n");
	}
	#endif
	// --- / ---
	
	for( i = 0; i < Ele->NInputs; i++ )
	{
		out = out && GetLink(Ele->Inputs[i]);
	}
	if( out ) {
		RaiseLink(Ele->Outputs[0]);
	}
}

tElementDef gElement_AND = {
	NULL, "AND",
	1, -1,
	_Create,
	_Update
};
