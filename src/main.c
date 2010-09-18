/*
 * 
 */
#include <common.h>
#include <element.h>
#include <stdio.h>
#include <stdlib.h>


extern tElementDef	*gpElementDefs;
extern tLink	*gpLinks;
extern tElement	*gpElements;

#define ADD_ELEDEF(name)	do {\
	extern tElementDef gElement_##name;\
	gElement_##name.Next = gpElementDefs;\
	gpElementDefs = &gElement_##name;\
} while(0)

extern int ParseFile(const char *Filename);

int main(int argc, char *argv[])
{
	tLink	*link;
	tElement	*ele;
	 int	i;
	
	ADD_ELEDEF(AND);	ADD_ELEDEF(OR);	ADD_ELEDEF(XOR);
	ADD_ELEDEF(NAND);	ADD_ELEDEF(NOR);	ADD_ELEDEF(NXOR);
	
	ADD_ELEDEF(NOT);
	ADD_ELEDEF(CLOCK);
	ADD_ELEDEF(DELAY);
	ADD_ELEDEF(PULSE);
	ADD_ELEDEF(COUNTER);
	ADD_ELEDEF(DEMUX);
	
	// Load Circuit file(s)
	for( i = 1; i < argc; i ++ )
	{
		if( ParseFile(argv[i]) ) {
			return -1;
		}
	}
	
	// Resolve links
	for( link = gpLinks; link; link = link->Next )
	{
		// Expand n-deep linking
		if( link->Link && link->Link->Link)
			link->Link = link->Link->Link;
		// Ignore unamed
		if( !link->Name[0] )
			continue;
		printf("- %p %s\n", link, link->Name);
	}
	
	#if 0
	for( ele = gpElements; ele; ele = ele->Next )
	{
		printf("%p %s\n", ele, ele->Type->Name);
		for( i = 0; i < ele->NInputs; i ++ ) {
			// Expand links
			if( ele->Inputs[i]->Link ) {
				link = ele->Inputs[i]->Link;
				// Just asking for a memory leak, but this can be in use elsewhere
				// TODO: Reference counting
				//free( ele->Inputs[i] );
				ele->Inputs[i] = link;
			}
			printf("< %s (%p) (%p)\n", ele->Inputs[i]->Name, ele->Inputs[i], ele->Inputs[i]->Link);
		}
		for( i = 0; i < ele->NOutputs; i ++ ) {
			// Expand links
			if( ele->Outputs[i]->Link ) {
				link = ele->Outputs[i]->Link;
				// Just asking for a memory leak, but this can be in use elsewhere
				// TODO: Reference counting
				//free( ele->Outputs[i] );
				ele->Outputs[i] = link;
			}
			//fflush(stdout);
			printf("> %s (%p) (%p)\n", ele->Outputs[i]->Name, ele->Outputs[i], ele->Outputs[i]->Link);
		}
	}
	#endif
	
	for( ;; )
	{
		// Clear drivers
		for( link = gpLinks; link; link = link->Next ) {
			link->NDrivers = 0;
		}
		// Update elements
		for( ele = gpElements; ele; ele = ele->Next )
		{
			ele->Type->Update( ele );
		}
		// Set values
		for( link = gpLinks; link; link = link->Next ) {
			link->Value = !!link->NDrivers;
			if( link->Name[0] == '$' )
				printf("%s = %i\n", link->Name, link->Value);
		}
		printf("--------\n");
	}
	
	return 0;
}

