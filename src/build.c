/*
 *
 */
#include <common.h>
#include <string.h>
#include <element.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#define	USE_LINKS	1

/**
 * \brief Line array (group) definition structure
 */
typedef struct sGroupDef
{
	struct sGroupDef	*Next;
	 int	Size;	//!< Size of the group
	char	Name[];	//!< Name
} tGroupDef;

/**
 * \brief #defunit structure
 */
typedef struct sUnitTemplate
{
	struct sUnitTemplate	*Next;
	
	tLink	*Links;
	tGroupDef	*Groups;
	tElement	*Elements;
	tList	Inputs;	//!< Input links
	tList	Outputs;	//!< Output links
	tDisplayItem	*DisplayItems;
	tBreakpoint	*Breakpoints;
	
	 int	InstanceCount;	//!< Number of times the unit has been used
	
	char	Name[];
}	tUnitTemplate;

tLinkValue	gValue_Zero;
tLinkValue	gValue_One;

// === GLOBALS ===
tLink	*gpLinks = NULL;
tGroupDef	*gpGroups = NULL;
tElement	*gpElements;
tElement	*gpLastElement = (tElement*)&gpElements;
tElementDef	*gpElementDefs;
tUnitTemplate	*gpUnits;
tTestCase	*gpTests;
tBreakpoint	*gpBreakpoints;
tDisplayItem	*gpDisplayItems;
tUnitTemplate	*gpCurUnit;
tTestCase	*gpCurTest;

// === CODE ===
/**
 * \brief Define a unit (#defunit)
 */
int Unit_DefineUnit(const char *Name)
{
	tUnitTemplate	*tpl, *prev = NULL;
	
	// Nesting is a no-no
	if(gpCurUnit || gpCurTest) {
		return -1;
	}
	
	// Check names
	for( tpl = gpUnits; tpl; prev = tpl, tpl = tpl->Next )
	{
		if( strcmp(Name, tpl->Name) == 0 )	return -2;
		if( strcmp(Name, tpl->Name) > 0)	break;
	}
	
	// Create new one
	tpl = calloc( 1, sizeof(tUnitTemplate) + strlen(Name) + 1 );
	strcpy(tpl->Name, Name);
	
	// Add to list
	if(prev) {
		tpl->Next = prev->Next;
		prev->Next = tpl;
	}
	else {
		tpl->Next = gpUnits;
		gpUnits = tpl;
	}
	
	gpCurUnit = tpl;
	
	return 0;
}

int Unit_AddSingleInput(const char *Name)
{
	if(!gpCurUnit)	return -1;
	AppendLine( &gpCurUnit->Inputs, Name );
	return 0;
}

int Unit_AddGroupInput(const char *Name, int Size)
{
	if(!gpCurUnit)	return -1;
	CreateGroup( Name+1, Size );
	AppendGroup( &gpCurUnit->Inputs, Name );
	return 0;
}

int Unit_AddSingleOutput(const char *Name)
{
	if(!gpCurUnit)	return -1;
	AppendLine( &gpCurUnit->Outputs, Name );
	return 0;
}

int Unit_AddGroupOutput(const char *Name, int Size)
{
	if(!gpCurUnit)	return -1;
	CreateGroup( Name+1, Size );
	AppendGroup( &gpCurUnit->Outputs, Name );
	return 0;
}

int Unit_CloseUnit(void)
{
	if(!gpCurUnit)	return -1;

	// Reduce all links to at most 1 level deep
	for(tLink *link = gpCurUnit->Links; link; link = link->Next)
	{
		while(link->Link && link->Link->Link)
			link->Link = link->Link->Link;
		if(link->Link)
		{
			// Join values
			LinkValue_Ref(link->Link->Value);
			LinkValue_Deref(link->Value);
			link->Value = link->Link->Value;
		}
		
		link->Backlink = NULL;
	}

	void _merge(tLink **Links, int Count)
	{
		for( int i = 0; i < Count; i ++ )
		{
			if(Links[i]->Link)
				Links[i] = Links[i]->Link;
			Links[i]->Backlink = (void*)1;
		}
	}

	// Reduce to a single layer
	for(tElement *ele = gpCurUnit->Elements; ele; ele = ele->Next)
	{
		_merge(ele->Inputs,  ele->NInputs);
		_merge(ele->Outputs, ele->NOutputs);
	}
	for( tDisplayItem *disp = gpCurUnit->DisplayItems; disp; disp = disp->Next )
	{
		_merge(disp->Condition.Items, disp->Condition.NItems);
		_merge(disp->Values.Items, disp->Values.NItems);
	}
	for( tBreakpoint *bp = gpCurUnit->Breakpoints; bp; bp = bp->Next )
	{
		_merge(bp->Condition.Items, bp->Condition.NItems);
	}

	// Remove any unused links
	 int	nTrimmed = 0;
	for(tLink *link = gpCurUnit->Links, *prev = (void*)&gpCurUnit->Links; link; )
	{
		if( link->Backlink )
		{
			link->Backlink = NULL;
			prev = link;
			link = link->Next;
			continue ;
		}
		
		prev->Next = link->Next;
		free(link);
		link = prev->Next;
		nTrimmed ++;
	}
	printf("Trimmed %i links in close of '%s'\n", nTrimmed, gpCurUnit->Name);

	gpCurUnit = NULL;
	return 0;
}

int Unit_IsInUnit(void)
{
	return !!gpCurUnit;
}

// --------------------------------------------------------------------
int Test_CreateTest(int MaxLength, const char *Name, int NameLength)
{
	if( gpCurTest || gpCurUnit )
		return -1;
	
	gpCurTest = calloc(1, sizeof(tTestCase) + NameLength + 1);
	gpCurTest->MaxLength = MaxLength;
	memcpy(gpCurTest->Name, Name, NameLength);
	gpCurTest->Name[NameLength] = '\0';

	tTestCase *p;
	for( p = gpTests; p && p->Next; p = p->Next )
		;
	if(p)
		p->Next = gpCurTest;
	else
		gpTests = gpCurTest;
	
	return 0;
}

int Test_AddAssertion(const tList *Condition, const tList *Values, const tList *Expected)
{
	if(!gpCurTest)
		return -1;
	
	if( Values->NItems != Expected->NItems ) {
		printf("Count mismatch in assertion (%i != %i)\n", Values->NItems, Expected->NItems);
		return -1;
	}

	tAssertion *a = malloc( sizeof(tAssertion)
		+ (Condition->NItems + Values->NItems + Expected->NItems)*sizeof(tLink*) );
	
	a->Condition.NItems = Condition->NItems;
	a->Expected.NItems = Values->NItems;
	a->Values.NItems = Expected->NItems;
	a->Condition.Items = (void*)(a + 1);
	memcpy(a->Condition.Items, Condition->Items, Condition->NItems*sizeof(tLink*));
	a->Expected.Items = a->Condition.Items + Condition->NItems;
	memcpy(a->Expected.Items, Expected->Items, Expected->NItems*sizeof(tLink*));
	a->Values.Items = a->Expected.Items + Expected->NItems;
	memcpy(a->Values.Items, Values->Items, Values->NItems*sizeof(tLink*));

	// Append on to the end of the assertion list
	tAssertion *p;
	for( p = gpCurTest->Assertions; p && p->Next; p = p->Next )
		;
	if(p)
		p->Next = a;
	else
		gpCurTest->Assertions = a;
	a->Next = NULL;

	return 0;
}

int Test_AddCompletion(const tList *Condition)
{
	if( !gpCurTest )
		return -1;
	if( Condition->NItems != 1 )
		return -1;
	
	if( gpCurTest->CompletionCondition )
		return -1;
	
	gpCurTest->CompletionCondition = Condition->Items[0];
	return 0;
}

int Test_CloseTest(void)
{
	if(!gpCurTest)	return -1;
	gpCurTest = NULL;
	return 0;
}

int Test_IsInTest(void)
{
	return !!gpCurTest;
}

// --------------------------------------------------------------------

tBreakpoint *AddBreakpoint(const char *Name, const tList *Condition)
{
	tBreakpoint	*bp, *prev = NULL;
	 int	pos, i;
	tBreakpoint	**listHead;
	
	
	if( gpCurUnit )
		listHead = &gpCurUnit->Breakpoints;
	else if( gpCurTest ) {
		// TODO: Breakpoints in tests?
		return NULL;
	}
	else
		listHead = &gpBreakpoints;
	
	for( bp = *listHead; bp; prev = bp, bp = bp->Next )
	{
		if( strcmp(Name, bp->Label) == 0 ) {
			return NULL;
		}
	}
	
	// Create new
	pos = strlen(Name)+1;
	bp = malloc( sizeof(tBreakpoint) + pos + (Condition->NItems)*sizeof(tLink*) );
	bp->Condition.NItems = Condition->NItems;
	bp->Condition.Items = (void*)&bp->Label[pos];
	for( i = 0; i < Condition->NItems; i ++ )
		bp->Condition.Items[i] = Condition->Items[i];
	
	strcpy(bp->Label, Name);
	
	if( prev ) {
		bp->Next = prev->Next;
		prev->Next = bp;
	}
	else {
		bp->Next = NULL;
		*listHead = bp;
	}
	
	return bp;
}

tDisplayItem *AddDisplayItem(const char *Name, const tList *Condition, const tList *Values)
{
	tDisplayItem	*dispItem, *prev = NULL;
	 int	namelen, i;
	tDisplayItem	**listHead;
	
	
	if( gpCurUnit )
		listHead = &gpCurUnit->DisplayItems;
	else if( gpCurTest )
		listHead = &gpCurTest->DisplayItems;
	else
		listHead = &gpDisplayItems;
	
	for( dispItem = *listHead; dispItem; prev = dispItem, dispItem = dispItem->Next )
	{
		if( strcmp(Name, dispItem->Label) == 0 ) {
			return NULL;
		}
	}

	// Create new
	namelen = strlen(Name)+1;
	dispItem = malloc( sizeof(tDisplayItem) + namelen + (Condition->NItems+Values->NItems)*sizeof(tLink*) );
	dispItem->Condition.NItems = Condition->NItems;
	dispItem->Condition.Items = (void*)&dispItem->Label[namelen];
	dispItem->Values.NItems = Values->NItems;
	dispItem->Values.Items = dispItem->Condition.Items + Condition->NItems;
	for( i = 0; i < Condition->NItems; i ++ )
		dispItem->Condition.Items[i] = Condition->Items[i];
	for( i = 0; i < Values->NItems; i ++ )
		dispItem->Values.Items[i] = Values->Items[i];
	
	strcpy(dispItem->Label, Name);
	
	if( prev ) {
		dispItem->Next = prev->Next;
		prev->Next = dispItem;
	}
	else {
		dispItem->Next = NULL;
		*listHead = dispItem;
	}
	
	return dispItem;
}

/**
 * \brief Create a new group
 */
void CreateGroup(const char *Name, int Size)
{
	tGroupDef	**listhead;
	tGroupDef	*ret, *def, *prev = NULL;

	ret = malloc( sizeof(tGroupDef) + 1 + strlen(Name) + 1 );
	ret->Size = Size;
	ret->Name[0] = '@';
	strcpy(&ret->Name[1], Name);
	
	if( gpCurUnit )
		listhead = &gpCurUnit->Groups;
	else if( gpCurTest )
		listhead = &gpCurTest->Groups;
	else
		listhead = &gpGroups;
	
	//printf("ret->Name = '%s', def = %p\n", ret->Name, def);
	for( def = *listhead; def; prev = def, def = def->Next )
	{
		if(strcmp(ret->Name, def->Name) == 0) {
			fprintf(stderr, "ERROR: Redefinition of %s\n", ret->Name);
			if( gpCurUnit )
				fprintf(stderr, " in unit '%s'\n", gpCurUnit->Name);
			else if( gpCurTest )
				fprintf(stderr, " in test '%s'\n", gpCurTest->Name);
			else
				fprintf(stderr, " in root scope\n");
			free(ret);
			return;
		}
		//if(strcmp(Name, def->Name) > 0)	break;
	}
	
	if(prev) {
		ret->Next = prev->Next;
		prev->Next = ret;
	}
	else {
		ret->Next = *listhead;
		*listhead = ret;
	}
	
//	printf("'%s' added to %s\n", ret->Name, (gpCurUnit?gpCurUnit->Name:"."));
}

/**
 * \param Name	Name including leading '@'
 */
tGroupDef *GetGroup(const char *Name)
{
	tGroupDef	*def;
	
	if( gpCurUnit )
		def = gpCurUnit->Groups;
	else if( gpCurTest )
		def = gpCurTest->Groups;
	else
		def = gpGroups;
	
	for( ; def; def = def->Next )
	{
		//printf("strcmp(%s, %s)\n", Name, def->Name);
		if(strcmp(Name, def->Name) == 0)	return def;
		//if(strcmp(Name, def->Name) < 0)	return NULL;
	}
	return NULL;
}

/**
 * \brief Create a new link value
 */
tLinkValue *LinkValue_Create(void)
{
	tLinkValue	*ret;
	ret = malloc(sizeof(tLinkValue));
	//if(!ret)	return NULL;
	ret->NDrivers = 0;
	ret->Value = 0;
	ret->ReferenceCount = 1;
	return ret;
}

void LinkValue_Ref(tLinkValue *Value)
{
	Value->ReferenceCount ++;
}

void LinkValue_Deref(tLinkValue *Value)
{
	Value->ReferenceCount --;
	if( !Value->ReferenceCount ) {
		free(Value);
	}
}

/**
 * \brief Create an unnamed link
 */
tLink *CreateAnonLink(void)
{
	tLink	**first;
	tLink	*ret = malloc(sizeof(tLink)+1);
	ret->Name[0] = '\0';
	ret->Value = LinkValue_Create();
	ret->Link = NULL;
	ret->ReferenceCount = 1;
	ret->Backlink = NULL;

	if( gpCurUnit )
		first = &gpCurUnit->Links;
	else if( gpCurTest )
		first = &gpCurTest->Links;
	else
		first = &gpLinks;
	ret->Next = *first;
	*first = ret;
	
	return ret;
}

/**
 * \brief Create a named link (line)
 */
tLink *CreateNamedLink(const char *Name)
{
	tLink	**first;
	tLink	*ret, *def, *prev = NULL;

	if( gpCurUnit )
		first = &gpCurUnit->Links;
	else if( gpCurTest )
		first = &gpCurTest->Links;
	else
		first = &gpLinks;
	
	for(def = *first; def; prev = def, def = def->Next )
	{
		if(strcmp(Name, def->Name) == 0)	return def;
		if(strcmp(Name, def->Name) < 0)	break;
	}
	
	ret = malloc(sizeof(tLink) + strlen(Name) + 1);
	strcpy(ret->Name, Name);
	ret->Link = NULL;
	ret->Backlink = NULL;
	ret->ReferenceCount = 1;

	if( Name[0] == '0' && Name[1] == '\0' ) {
		ret->Value = &gValue_Zero;
	}
	else if( Name[0] == '1' && Name[1] == '\0' ) {
		ret->Value = &gValue_One;
	}
	else
		ret->Value = LinkValue_Create();
	
	
	if(prev) {
		ret->Next = prev->Next;
		prev->Next = ret;
	}
	else {
		ret->Next = *first;
		*first = ret;
	}
	
	return ret;
}

/**
 * \brief Append a #defunit unit to the state
 */
tList *AppendUnit(tUnitTemplate *Unit, tList *Inputs)
{
	tElement	**listhead;
	tLink	**linkhead;
	
	tLink	*link, *newLink;
	tDisplayItem	*dispItem;
	tBreakpoint	*bp;
	tList	*ret;
	 int	i;

	printf("Importing %s\n", Unit->Name);
	
	// Check input counts
	if( Inputs->NItems != Unit->Inputs.NItems ) {
		// TODO: Error message?
		fprintf(stderr, "ERROR: %s takes %i lines, %i given\n",
			Unit->Name, Unit->Inputs.NItems, Inputs->NItems);
		return NULL;
	}
	
	if( gpCurUnit ) {
		listhead = &gpCurUnit->Elements;
		linkhead = &gpCurUnit->Links;
	}
	else if( gpCurTest ) {
		listhead = &gpCurTest->Elements;
		linkhead = &gpCurTest->Links;
	}
	else {
		listhead = &gpElements;
		linkhead = &gpLinks;
	}

	// Create the prefix to add to the name
	 int	prefixLen = snprintf(NULL, 0, "%s#%i", Unit->Name, Unit->InstanceCount);
	char	namePrefix[prefixLen+1];
	snprintf(namePrefix, prefixLen+1, "%s#%i", Unit->Name, Unit->InstanceCount);
	Unit->InstanceCount ++;
	
	// Create duplicates of all links (renamed)
	for( link = Unit->Links; link; link = link->Next )
	{
		 int	len = 0;
		 int	bPrefix = 1;
		
		// Only append prefix to named links
		if( link->Name[0] == '\0' )	bPrefix = 0;
		if( link->Name[0] == '0' && link->Name[1] == '\0' )	bPrefix = 0;
		if( link->Name[0] == '1' && link->Name[1] == '\0' )	bPrefix = 0;
		
		len = strlen(link->Name);
		if( bPrefix )	len += prefixLen;
		
		// Create link
		newLink = malloc( sizeof(tLink) + len + 1 );
		newLink->Name[0] = '\0';
		// Only append prefix to named links
		if( bPrefix ) {
			strcpy(newLink->Name, namePrefix);
		}
		strcat(newLink->Name, link->Name);
		newLink->Link = NULL;
		newLink->Backlink = NULL;
		newLink->Value = LinkValue_Create();
		
		link->Backlink = newLink;	// Set a back link to speed up remapping inputs
		
		// Append to the current list
		tLink	*prev = NULL;
		for( tLink *def = *linkhead; def; prev = def, def = def->Next )
		{
			if(strcmp(newLink->Name, def->Name) < 0)
				break;
		}
		if(prev) {
			newLink->Next = prev->Next;
			prev->Next = newLink;
		}
		else {
			newLink->Next = *linkhead;
			*linkhead = newLink;
		}
		
//		if( bPrefix ) {
//			printf("Duplicated %p %s as %p %s\n", link, link->Name, newLink, newLink->Name);
//		}
	}
	
	// Re-resolve links
	for( link = Unit->Links; link; link = link->Next )
	{
		if( link->Link ) {
			tLink	*newlink = link->Backlink;
			tLink	*target = link->Link->Backlink;
			// Set new links's ->Link field
			newlink->Link = target;
			// And merge the two values
			LinkValue_Ref(target->Value);
			LinkValue_Deref(newlink->Value);
			newlink->Value = target->Value;
		}
	}
	
	// Replace input lines
	for( i = 0; i < Unit->Inputs.NItems; i ++ )
	{
		link = Unit->Inputs.Items[i]->Backlink;
		#if USE_LINKS
		if( link->Link )
			link->Link->Link = Inputs->Items[i];
		link->Link = Inputs->Items[i];
		#endif
		
		// Link values
		LinkValue_Ref( Inputs->Items[i]->Value );
		LinkValue_Deref( link->Value );
		link->Value = Inputs->Items[i]->Value;
	}
	
	// Append display items
	for( dispItem = Unit->DisplayItems; dispItem; dispItem = dispItem->Next )
	{
		tDisplayItem	*newItem;
		char	newName[ prefixLen + strlen(dispItem->Label) + 1 ];
		
		strcpy(newName, namePrefix);
		strcat(newName, dispItem->Label);

		newItem = AddDisplayItem(newName, &dispItem->Condition, &dispItem->Values);
		if( !newItem ) {
			fprintf(stderr, "Warning: AddDisplayItem('%s', %p, %p) failed\n",
				newName, &dispItem->Condition, &dispItem->Values);
			continue ;
		}
		// Resolve backlinks
		for( i = 0; i < newItem->Condition.NItems; i ++ )
			newItem->Condition.Items[i] = newItem->Condition.Items[i]->Backlink;
		
		for( i = 0; i < newItem->Values.NItems; i ++ )
			newItem->Values.Items[i] = newItem->Values.Items[i]->Backlink;
	}
	
	// Append display items
	for( bp = Unit->Breakpoints; bp; bp = bp->Next )
	{
		tBreakpoint	*newBP;
		
		newBP = AddBreakpoint(dispItem->Label, &dispItem->Condition);
		
		// Resolve backlinks
		for( i = 0; i < newBP->Condition.NItems; i ++ )
			newBP->Condition.Items[i] = newBP->Condition.Items[i]->Backlink;
	}
	
	// Duplicate elements and replace links
	for( tElement *ele = Unit->Elements; ele; ele = ele->Next )
	{
		tElement	*newele;		

		newele = ele->Type->Duplicate( ele );
		if(!newele) {
			fprintf(stderr, "Error in creating copy of %s", ele->Type->Name);
			return NULL;
		}
		if(ele->NInputs > 2) {
			printf("Duplicated '%s' %p -> %p\n", ele->Type->Name, ele, newele);
		}

		for( i = 0; i < ele->NInputs; i ++ ) {
			if( ele->Inputs[i]->Backlink ) {
				if( ele->NInputs > 2 )
					printf("IN  %p %s -> %p %s\n",
						ele->Inputs[i], ele->Inputs[i]->Name,
						ele->Inputs[i]->Backlink, ele->Inputs[i]->Backlink->Name
						);
				newele->Inputs[i] = ele->Inputs[i]->Backlink;
			}
			else
				newele->Inputs[i] = ele->Inputs[i];
		}
		for( i = 0; i < ele->NOutputs; i ++ ) {
			if( ele->Outputs[i]->Backlink ) {
				if( ele->NInputs > 2)
					printf("OUT %p %s -> %p %s\n",
						ele->Outputs[i], ele->Outputs[i]->Name,
						ele->Outputs[i]->Backlink, ele->Outputs[i]->Backlink->Name
						);
				newele->Outputs[i] = ele->Outputs[i]->Backlink;
			}
			else {
				printf("No backlink on %s output %i %p (%s)\n",
					ele->Type->Name, i, ele->Outputs[i], ele->Outputs[i]->Name);
				newele->Outputs[i] = ele->Outputs[i];
			}
		}
		
		newele->Next = *listhead;
		*listhead = newele;
	}
	
	// Create output return
	ret = malloc( sizeof(tList) + Unit->Outputs.NItems * sizeof(tLink*) );
	ret->NItems = Unit->Outputs.NItems;
	ret->Items = (void *)( ret + 1 );
	for( i = 0; i < Unit->Outputs.NItems; i ++ )
		ret->Items[i] = Unit->Outputs.Items[i]->Backlink;
	
	// Clean up after ourselves
	for( link = Unit->Links; link; link = link->Next )
	{
		link->Backlink = NULL;
	}
	
	return ret;	
}

/**
 * \brief Create a new unit instance
 */
tList *CreateUnit(const char *Name, int NParams, int *Params, tList *Inputs)
{
	tElement	**listhead;
	
	tElement	*ele;
	tList	*ret = NULL;
	 int	i;
	tElementDef	*def;
	
	// First, check for template units (#defunit)
	for( tUnitTemplate *tpl = gpUnits; tpl; tpl = tpl->Next )
	{
		// We can't recurse, so also check that
		if( tpl != gpCurUnit && strcmp(Name, tpl->Name) == 0 ) {
			return AppendUnit(tpl, Inputs);
		}
	}

	// Next, check for builtins
	for( def = gpElementDefs; def; def = def->Next )
	{
		if( strcmp(Name, def->Name) == 0 )
			break;
	}
	if(!def)	return NULL;
	
	// Determine current list
	if( gpCurUnit )
		listhead = &gpCurUnit->Elements;
	else if( gpCurTest )
		listhead = &gpCurTest->Elements;
	else
		listhead = &gpElements;
	
	// Sanity check input numbers
	if( Inputs->NItems < def->MinInput ) {
		printf("%s requires at least %i input lines\n", def->Name, def->MinInput);
		return NULL;
	}
	if( def->MaxInput != -1 && Inputs->NItems > def->MaxInput ) {
		printf("%s takes at most %i input lines\n", def->Name, def->MaxInput);
		return NULL;
	}
	
	// Create an instance
	ele = def->Create(NParams, Params, Inputs->NItems, Inputs->Items);
	if(!ele) {
		fprintf(stderr, "Error in creating '%s'\n", def->Name);
		return NULL;
	}
	ele->Type = def;	// Force type to be set
	//ele->Param = Param;
	for( i = 0; i < Inputs->NItems; i ++ ) {
		ele->Inputs[i] = Inputs->Items[i];
	}
	
	// Append to element list
	ele->Next = *listhead;
	*listhead = ele;
	
	// Create return list
	ret = malloc( sizeof(tList) + ele->NOutputs*sizeof(tLink*) );
	ret->NItems = ele->NOutputs;
	ret->Items = (void *)( (intptr_t)ret + sizeof(tList) );
	for( i = 0; i < ele->NOutputs; i ++ ) {
		ele->Outputs[i] = CreateAnonLink();
		ret->Items[i] = ele->Outputs[i];
	}
	
	return ret;
}

/**
 */
void List_Free(tList *List)
{
	if( List->Items != (void *)( (intptr_t)List + sizeof(tList) ) )
		free(List->Items);
	else
		free(List);
}

/**
 * \brief Append one list to another
 */
void AppendList(tList *Dest, tList *Src)
{
	 int	oldOfs = Dest->NItems;
	Dest->NItems += Src->NItems;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	memcpy( &Dest->Items[oldOfs], Src->Items, Src->NItems*sizeof(tLink*) );
}

/**
 * \brief Append a named line to a list
 */
void AppendLine(tList *Dest, const char *Name)
{
	Dest->NItems ++;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	Dest->Items[Dest->NItems-1] = CreateNamedLink(Name);
}

/**
 * \brief Append a single line of a group to a list
 */
int AppendGroupItem(tList *Dest, const char *Name, int Index)
{
	 int	len, i, digits;
	tGroupDef	*grp = GetGroup(Name);
	
	//printf("AppendGroupItem: (%p, %s, %i)\n", Dest, Name, Index);
	if(!grp) {
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	if( Index < 0 || Index >= grp->Size ) {
		fprintf(stderr, "ERROR: Index out of bounds on %s[%i > %i]\n",
			Name, Index, grp->Size - 1);
		return 2;
	}
	
	digits = 0;
	for( i = grp->Size-1; i; i /= 10 )	digits ++;
	if( digits == 0 )	digits = 1;
	
	// Get string length
	len = snprintf(NULL, 0, "%s[%*i]", Name, digits, Index);
	
	{
		char	newname[len + 1];
		snprintf(newname, len+1, "%s[%*i]", Name, digits, Index);
		
		Dest->NItems ++;
		Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
		Dest->Items[Dest->NItems-1] = CreateNamedLink(newname);
	}
	return 0;
}

/**
 * \brief Append every line in a group to a list
 */
int AppendGroup(tList *Dest, const char *Name)
{
	tGroupDef	*grp = GetGroup(Name);
	 int	i, ofs, len;
	 int	digits = 0;
	
	//printf("AppendGroup: (%p, %s)\n", Dest, Name);
	if(!grp) {
		// ERROR:
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	for( i = grp->Size-1; i; i /= 10 )	digits ++;
	if( digits == 0 )	digits = 1;	
	
	len = snprintf(NULL, 0, "%s[%*i]", Name, digits, grp->Size-1);

	ofs = Dest->NItems;
	Dest->NItems += grp->Size;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	
	for( i = 0; i < grp->Size; i ++ )
	{
		char	newname[len + 1];
		snprintf(newname, len+1, "%s[%*i]", Name, digits, i);
		
		Dest->Items[ofs+i] = CreateNamedLink(newname);
	}
	
	return 0;
}

/**
 * \brief Append every line in a group to a list (reversed)
 */
int AppendGroupRev(tList *Dest, const char *Name)
{
	tGroupDef	*grp = GetGroup(Name);
	 int	i, ofs, len;
	 int	digits = 0;
	
	//printf("AppendGroup: (%p, %s)\n", Dest, Name);
	if(!grp) {
		// ERROR:
		fprintf(stderr, "ERROR: Unknown group %s\n", Name);
		return 1;
	}
	
	len = snprintf(NULL, 0, "%s[%i]", Name, grp->Size-1);
	
	for( i = grp->Size-1; i; i /= 10 )	digits ++;
	
	ofs = Dest->NItems;
	Dest->NItems += grp->Size;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	
	for( i = 0; i < grp->Size; i ++ )
	{
		char	newname[len + 1];
		snprintf(newname, len, "%s[%*i]", Name, digits, grp->Size - i - 1);
		
		Dest->Items[ofs+i] = CreateNamedLink(newname);
	}
	
	return 0;
}

/**
 * \brief Link two sets of links (Sets the aliases for \a Src to \a Dest)
 * \return Boolean Failure
 */
int MergeLinks(tList *Dest, tList *Src)
{
	 int	i;
	if( Dest->NItems != Src->NItems ) {
		fprintf(stderr, "ERROR: MergeLinks - Mismatch of source and destination counts\n");
		return -1;
	}
	
	for( i = 0; i < Dest->NItems; i ++ )
	{
		//printf("%p(%s) <= %p(%s)\n",
		//	Dest->Items[i], Dest->Items[i]->Name,
		//	Src->Items[i], Src->Items[i]->Name
		//	);
		LinkValue_Ref( Src->Items[i]->Value );
		LinkValue_Deref( Dest->Items[i]->Value );
		Dest->Items[i]->Value = Src->Items[i]->Value;
		
		#if USE_LINKS
		Src->Items[i]->Link = Dest->Items[i];
		#endif
	}
	return 0;
}
