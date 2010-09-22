/*
 * 
 */
#include <common.h>
#include <element.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>

#define PRINT_ELEMENTS	0

// === IMPORTS ===
extern tElementDef	*gpElementDefs;
extern tLink	*gpLinks;
extern tElement	*gpElements;
extern tDisplayItem	*gpDisplayItems;

extern int ParseFile(const char *Filename);
extern void	LinkValue_Ref(tLinkValue *Value);
extern void	LinkValue_Deref(tLinkValue *Value);

// === MACRO! ===
#define ADD_ELEDEF(name)	do {\
	extern tElementDef gElement_##name;\
	gElement_##name.Next = gpElementDefs;\
	gpElementDefs = &gElement_##name;\
} while(0)

// === PROTOTYPES ===
void	ReadCommand(int MaxCmd, char *Command, int MaxArg, char *Argument);
void	SigINT_Handler(int Signum);

// === GLOBALS ===
 int	gbRunSimulation = 1;
 int	gbPrintLinks = 0;

// === CODE ===
int main(int argc, char *argv[])
{
	tLink	*link;
	tElement	*ele;
	tDisplayItem	*dispItem;
	 int	i;
	 int	timestamp;
	
	// Add element definitions
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
		if( argv[i][0] == '-' ) {
			if( strcmp(argv[i], "-links") == 0 )
				gbPrintLinks = 1;
			else {
				// ERROR
			}
		}
		
		if( ParseFile(argv[i]) ) {
			return -1;
		}
	}
	
	// Resolve links
	for( link = gpLinks; link; link = link->Next )
	{
		link->Backlink = NULL;	// Clear the backlink (temp) for later
		
		// Expand n-deep linking
		while( link->Link && link->Link->Link )
			link->Link = link->Link->Link;
		
		if( link->Link )
		{
			LinkValue_Ref(link->Link->Value);
			LinkValue_Deref(link->Value);
			link->Value = link->Link->Value;
		}
		
		// Zero out
		link->Value->NDrivers = 0;
		link->Value->Value = 0;
		
		// Ignore unamed
		if( !link->Name[0] )
			continue;
		
//		printf("- %p %s\n", link, link->Name);
	}
	
	// Print links
	// > Scan all links and find ones that share a value pointer 
	if( gbPrintLinks )
	{
		for( link = gpLinks; link; link = link->Next )
		{
			tLink	*link2;
			if(link->Backlink)	continue;	// Skip ones already done
			printf("%p:", link->Value);
			for( link2 = gpLinks; link2; link2 = link2->Next )
			{
				if(link2->Backlink)	continue;
				if(link2->Value != link->Value)	continue;
				if( link2->Name[0] )
					printf(" %s", link2->Name);
				else
					printf(" %p", link2);
				link2->Backlink = link;	// Make backlink non-zero
			}
			printf("\n");
		}
	}
	
	#if 1
	for( ele = gpElements; ele; ele = ele->Next )
	{
		#if PRINT_ELEMENTS
		printf("%p %s", ele, ele->Type->Name);
		#endif
		for( i = 0; i < ele->NInputs; i ++ ) {
			#if USE_LINKS
			// Expand links
			if( ele->Inputs[i]->Link ) {
				link = ele->Inputs[i]->Link;
				ele->Inputs[i]->ReferenceCount --;
				if( !ele->Inputs[i]->ReferenceCount )
					free( ele->Inputs[i] );
				ele->Inputs[i] = link;
			}
			#endif
			#if PRINT_ELEMENTS
			printf(" %p(%s)", ele->Inputs[i], ele->Inputs[i]->Name);
			#endif
		}
		#if PRINT_ELEMENTS
		printf("   ==>");
		#endif
		for( i = 0; i < ele->NOutputs; i ++ ) {
			#if USE_LINKS
			// Expand links
			if( ele->Outputs[i]->Link ) {
				link = ele->Outputs[i]->Link;
				// Just asking for a memory leak, but this can be in use elsewhere
				// TODO: Reference counting
				ele->Outputs[i]->ReferenceCount --;
				if( !ele->Outputs[i]->ReferenceCount )
					free( ele->Outputs[i] );
				ele->Outputs[i] = link;
			}
			#endif
			#if PRINT_ELEMENTS
			printf(" %p(%s)", ele->Outputs[i], ele->Outputs[i]->Name);
			#endif
		}
		#if PRINT_ELEMENTS
		printf("\n");
		#endif
	}
	#endif
	
	signal(SIGINT, SigINT_Handler);
	
	// Go to alternte screen buffer
	printf("\x1B[?1047h");
	
	// Execute
	for( timestamp = 0; gbRunSimulation; timestamp ++ )
	{
		
		// Clear drivers
		for( link = gpLinks; link; link = link->Next ) {
			link->Value->NDrivers = 0;
			// Ensure 0 and 1 stay as 0 and 1
			if( link->Name[0] == '1' ) {
				link->Value->Value = 1;
				link->Value->NDrivers = 1;
			}
			else if( link->Name[0] == '0' )
				link->Value->Value = 0;
		}
		
		// Clear screen
		printf("\x1B[2J");
		printf("\x1B[H");
		printf("---- %6i ----\n", timestamp);
		
		// Update elements
		for( ele = gpElements; ele; ele = ele->Next )
		{
			ele->Type->Update( ele );
		}
		
		// Set values
		for( link = gpLinks; link; link = link->Next ) {
			link->Value->Value = !!link->Value->NDrivers;
			
			// Ensure 0 and 1 stay as 0 and 1
			if( link->Name[0] == '1' ) {
				link->Value->Value = 1;
				link->Value->NDrivers = 1;
			}
			else if( link->Name[0] == '0' )
				link->Value->Value = 0;
		}
		
		for( dispItem = gpDisplayItems; dispItem; dispItem = dispItem->Next )
		{
			// Check condition (if one condition line is high, the item is displayed)
			for( i = 0; i < dispItem->Condition.NItems; i ++ )
			{
				if( GetLink(dispItem->Condition.Items[i]) ) {
					break;
				}
			}
			if( i == dispItem->Condition.NItems )	continue;
			
			printf("%s: ", dispItem->Label);
			for( i = 0; i < dispItem->Values.NItems; i ++ )
			{
				//printf("%i ", dispItem->Values.Items[i]->Value->NDrivers);
				printf("%i", dispItem->Values.Items[i]->Value->Value);
			}
			printf("\n");
		}
		
		for( ;; )
		{
			char	commandBuffer[100];
			char	argBuffer[100];
			
			printf("> ");
			ReadCommand(100, commandBuffer, 100, argBuffer);
			
			// Quit
			if( strcmp(commandBuffer, "q") == 0 ) {
				gbRunSimulation = 0;
				break ;
			}
			// Continue
			else if( strcmp(commandBuffer, "c") == 0 ) {
				break;
			}
			// Display
			else if( strcmp(commandBuffer, "d") == 0 ) {
				// Find named link
				for( link = gpLinks; link; link = link->Next ) {
					if( strcmp(link->Name, argBuffer) == 0 ) {
						printf("%s: %i\n", link->Name, link->Value->Value);
						break;
					}
				}
			}
			else if( strcmp(commandBuffer, "dispall") == 0 ) {
				 int	len = strlen(argBuffer);
				for( link = gpLinks; link; link = link->Next ) {
					if( link->Name[0] && strncmp(link->Name, argBuffer, len) == 0 ) {
						printf("%s: %i\n", link->Name, link->Value->Value);
					}
				}
			}
			else {
				printf("Unknown command '%s'\n", commandBuffer);
			}
		}
		
		//sleep(1);
	}
	
	// Swap back to main buffer
	printf("\x1B[?1047l");
	
	return 0;
}

void ReadCommand(int MaxCmd, char *Command, int MaxArg, char *Argument)
{
	 int	i;
	char	ch;
	
	while( (ch = getchar()) == ' ' );
	if( ch == '\n' ) {
		//printf("ret %02x\n", ch);
		return ;
	}
	
	Argument[0] = '\0';
	i = 0;
	do {
		if( i + 1 < MaxCmd )
			Command[i++] = ch;
	} while( (ch = getchar()) != ' ' && ch != '\n' );
	Command[i] = '\0';
	
	while( ch == ' ' )	ch = getchar();
	//printf("%02x", ch);
	if( ch == '\n' ) {
		return ;
	}
	
	i = 0;
	do {
	//	printf("%02x", ch);
		if( i + 1 < MaxArg )
			Argument[i++] = ch;
	} while( (ch = getchar()) != '\n' );
	Argument[i] = '\0';
}

void SigINT_Handler(int Signum)
{
	Signum = 0;
	#if 1
	// Swap back to main buffer
	printf("\x1B[?1047l");
	exit(55);
	#else
	gbRunSimulation = 0;
	#endif
}

