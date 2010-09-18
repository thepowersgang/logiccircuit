/*
 * 
 */
#include <common.h>
#include <element.h>
#include <stdio.h>


extern tElementDef	*gpElementDefs;
extern tLink	*gpLinks;

#define ADD_ELEDEF(name)	do {\
	extern tElementDef gElement_##name;\
	gElement_##name.Next = gpElementDefs;\
	gpElementDefs = &gElement_##name;\
} while(0)

extern int ParseFile(const char *Filename);

int main(int argc, char *argv[])
{
	tLink	*link;
	 int	i;
	
	ADD_ELEDEF(AND);	ADD_ELEDEF(OR);	ADD_ELEDEF(XOR);
	ADD_ELEDEF(NAND);	ADD_ELEDEF(NOR);	ADD_ELEDEF(NXOR);
	
	ADD_ELEDEF(NOT);
	ADD_ELEDEF(CLOCK);
	ADD_ELEDEF(DELAY);
	ADD_ELEDEF(DEMUX);
	
	// Load Circuit file(s)
	for( i = 1; i < argc; i ++ )
		ParseFile(argv[i]);
	
	// Execute
	for( link = gpLinks; link; link = link->Next )
	{
		if( !link->Name[0] )	continue;
		printf("%p %s\n", link, link->Name);
	}
	
	return 0;
}

