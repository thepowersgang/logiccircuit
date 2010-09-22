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
	
	 int	InstanceCount;	//!< Number of times the unit has been used
	
	char	Name[];
}	tUnitTemplate;

// === GLOBALS ===
tLink	*gpLinks = NULL;
tGroupDef	*gpGroups = NULL;
tElement	*gpElements;
tElement	*gpLastElement = (tElement*)&gpElements;
tElementDef	*gpElementDefs;
tUnitTemplate	*gpUnits;
tUnitTemplate	*gpCurUnit;
tDisplayItem	*gpDisplayItems;

// === CODE ===
/**
 * \brief Define a unit (#defunit)
 */
int Unit_DefineUnit(const char *Name)
{
	tUnitTemplate	*tpl, *prev = NULL;
	
	// Nesting is a no-no
	if(gpCurUnit) {
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
	gpCurUnit = NULL;
	return 0;
}

int Unit_IsInUnit(void)
{
	return !!gpCurUnit;
}

tDisplayItem *AddDisplayItem(const char *Name, const tList *Condition, const tList *Values)
{
	tDisplayItem	*dispItem, *prev = NULL;
	 int	pos, i;
	tDisplayItem	**listHead;
	
	
	if( gpCurUnit )
		listHead = &gpCurUnit->DisplayItems;
	else
		listHead = &gpDisplayItems;
	
	for( dispItem = *listHead; dispItem; prev = dispItem, dispItem = dispItem->Next )
	{
		if( strcmp(Name, dispItem->Label) == 0 ) {
			return NULL;
		}
	}
	
	// Create new
	pos = strlen(Name)+1;
	dispItem = malloc( sizeof(tDisplayItem) + pos + (Condition->NItems+Values->NItems)*sizeof(tLink*) );
	dispItem->Condition.NItems = Condition->NItems;
	dispItem->Condition.Items = (void*)&dispItem->Label[pos];
	for( i = 0; i < Condition->NItems; i ++ )
		dispItem->Condition.Items[i] = Condition->Items[i];
	pos += Condition->NItems * sizeof(tLink*);
	
	dispItem->Values.NItems = Values->NItems;
	dispItem->Values.Items = (void*)&dispItem->Label[pos];
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

void CreateGroup(const char *Name, int Size)
{
	tGroupDef	*ret, *def, *prev = NULL;
	
	ret = malloc( sizeof(tGroupDef) + 1 + strlen(Name) + 1 );
	ret->Size = Size;
	ret->Name[0] = '@';
	strcpy(&ret->Name[1], Name);
	
	if( gpCurUnit )
		def = gpCurUnit->Groups;
	else
		def = gpGroups;
	
	//printf("ret->Name = '%s', def = %p\n", ret->Name, def);
	for( ; def; prev = def, def = def->Next )
	{
		if(strcmp(ret->Name, def->Name) == 0) {
			fprintf(stderr, "ERROR: Redefinition of %s", ret->Name);
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
		if( gpCurUnit ) {
			ret->Next = gpCurUnit->Groups;
			gpCurUnit->Groups = ret;
		}
		else {
			ret->Next = gpGroups;
			gpGroups = ret;
		}
	}
}

/**
 * \param Name	Name including leading '@'
 */
tGroupDef *GetGroup(const char *Name)
{
	tGroupDef	*def, *prev;
	
	if( gpCurUnit )
		def = gpCurUnit->Groups;
	else
		def = gpGroups;
	
	for( ; def; prev = def, def = def->Next )
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
	tLink	*ret = malloc(sizeof(tLink)+1);
	ret->Value = LinkValue_Create();
	ret->Link = NULL;
	ret->Name[0] = '\0';
	ret->ReferenceCount = 0;
	
	if( gpCurUnit ) {
		ret->Next = gpCurUnit->Links;
		gpCurUnit->Links = ret;
	}
	else {
		ret->Next = gpLinks;
		gpLinks = ret;
	}
	
	return ret;
}

/**
 * \brief Create a named link (line)
 */
tLink *CreateNamedLink(const char *Name)
{
	tLink	*ret, *def, *prev = NULL;
	
	if( gpCurUnit )
		def = gpCurUnit->Links;
	else
		def = gpLinks;
	
	for( ; def; prev = def, def = def->Next )
	{
		if(strcmp(Name, def->Name) == 0)	return def;
		if(strcmp(Name, def->Name) < 0)	break;
	}
	
	ret = malloc(sizeof(tLink) + strlen(Name) + 1);
	ret->Value = LinkValue_Create();
	strcpy(ret->Name, Name);
	ret->Link = NULL;
	ret->ReferenceCount = 1;
	
	if(prev) {
		ret->Next = prev->Next;
		prev->Next = ret;
	}
	else {
		if( gpCurUnit ) {
			ret->Next = gpCurUnit->Links;
			gpCurUnit->Links = ret;
		}
		else {
			ret->Next = gpLinks;
			gpLinks = ret;
		}
	}
	
	return ret;
}

/**
 * \brief Append a #defunit unit to the state
 */
tList *AppendUnit(tUnitTemplate *Unit, tList *Inputs)
{
	tLink	*link, *newLink;
	tElement	*ele;
	tDisplayItem	*dispItem;
	tList	*ret;
	 int	prefixLen = snprintf(NULL, 0, "%s#%i#", Unit->Name, Unit->InstanceCount);
	char	namePrefix[prefixLen+1];
	 int	i;
	
	// Check input counts
	if( Inputs->NItems != Unit->Inputs.NItems ) {
		// TODO: Error message?
		fprintf(stderr, "ERROR: %s takes %i lines, %i given\n",
			Unit->Name, Unit->Inputs.NItems, Inputs->NItems);
		return NULL;
	}
	
	// Create the prefix to add to the name
	snprintf(namePrefix, prefixLen+1, "%s#%i", Unit->Name, Unit->InstanceCount);
	Unit->InstanceCount ++;
	
	// Create duplicates of all links (renamed)
	for( link = Unit->Links; link; link = link->Next )
	{
		tLink	*def, *prev = NULL;
		 int	len = 0;
		
		// Only append prefix to named links
		if( link->Name[0] )	len = prefixLen + strlen(link->Name);
		
		// Create link
		newLink = malloc( sizeof(tLink) + len + 1 );
		// Only append prefix to named links
		if( link->Name[0] ) {
			strcpy(newLink->Name, namePrefix);
			strcat(newLink->Name, link->Name);
		}
		else
			newLink->Name[0] = '\0';
		newLink->Link = NULL;
		newLink->Value = LinkValue_Create();
		
		link->Backlink = newLink;	// Set a back link to speed up remapping inputs
		
		// Append to the current list
		def = gpCurUnit ? gpCurUnit->Links : gpLinks;
		for( ; def; prev = def, def = def->Next )
		{
			if(strcmp(newLink->Name, def->Name) < 0)	break;
		}
		if(prev) {
			newLink->Next = prev->Next;
			prev->Next = newLink;
		}
		else {
			if( gpCurUnit ) {
				newLink->Next = gpCurUnit->Links;
				gpCurUnit->Links = newLink;
			}
			else {
				newLink->Next = gpLinks;
				gpLinks = newLink;
			}
		}
	}
	
	// Re-resolve links
	for( link = Unit->Links; link; link = link->Next )
	{
		if( link->Link ) {
			// Set new links's ->Link field
			link->Backlink->Link = link->Link->Backlink;
			// And merge the two value
			LinkValue_Ref(link->Link->Backlink->Value);
			LinkValue_Deref(link->Backlink->Value);
			link->Backlink->Value = link->Link->Backlink->Value;
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
		
		newItem = AddDisplayItem(dispItem->Label, &dispItem->Condition, &dispItem->Values);
		
		for( i = 0; i < newItem->Condition.NItems; i ++ )
			newItem->Condition.Items[i] = newItem->Condition.Items[i]->Backlink;
		
		for( i = 0; i < newItem->Values.NItems; i ++ )
			newItem->Values.Items[i] = newItem->Values.Items[i]->Backlink;
	}
	
	// Duplicate elements and replace links
	for( ele = Unit->Elements; ele; ele = ele->Next )
	{
		tElement	*newele;
		
		newele = ele->Type->Create( ele->Param, ele->NInputs, ele->Inputs );
		newele->Type = ele->Type;
		newele->Param = ele->Param;
		newele->NInputs = ele->NInputs;
		for( i = 0; i < ele->NInputs; i ++ ) {
			newele->Inputs[i] = ele->Inputs[i]->Backlink;
			#if USE_LINKS
			if( newele->Inputs[i]->Link ) {
				//printf("linked input %i to %p\n", i, newele->Inputs[i]->Link);
				newele->Inputs[i] = newele->Inputs[i]->Link;
			}
			#endif
		}
		for( i = 0; i < ele->NOutputs; i ++ ) {
			newele->Outputs[i] = ele->Outputs[i]->Backlink;
			#if USE_LINKS
			if( newele->Outputs[i]->Link )
				newele->Outputs[i] = newele->Outputs[i]->Link;
			#endif
		}
		
		if( gpCurUnit ) {
			newele->Next = gpCurUnit->Elements;
			gpCurUnit->Elements = newele;
		}
		else {
			gpLastElement->Next = newele;
			gpLastElement = newele;
		}
	}
	
	// Create output return
	ret = malloc( sizeof(tList) + Unit->Outputs.NItems * sizeof(tLink*) );
	ret->NItems = Unit->Outputs.NItems;
	ret->Items = (void *)( (intptr_t)ret + sizeof(tList) );
	for( i = 0; i < Unit->Outputs.NItems; i ++ )
		ret->Items[i] = Unit->Outputs.Items[i]->Backlink;
	
	return ret;	
}

/**
 * \brief Create a new unit instance
 */
tList *CreateUnit(const char *Name, int Param, tList *Inputs)
{
	tElement	*ele;
	tList	*ret = NULL;
	 int	i;
	tElementDef	*def;
	tUnitTemplate	*tpl;
	
	// First, check for template units (#defunit)
	for( tpl = gpUnits; tpl; tpl = tpl->Next )
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
	ele = def->Create(Param, Inputs->NItems, Inputs->Items);
	if(!ele)	return NULL;
	ele->Type = def;	// Force type to be set
	ele->Param = Param;
	for( i = 0; i < Inputs->NItems; i ++ ) {
		ele->Inputs[i] = Inputs->Items[i];
	}
	
	// Append to element list
	if( gpCurUnit ) {
		ele->Next = gpCurUnit->Elements;
		gpCurUnit->Elements = ele;
	}
	else {
		gpLastElement->Next = ele;
		gpLastElement = ele;
	}
	
	// Create return list
	ret = malloc( sizeof(tList) + ele->NOutputs*sizeof(tLink*) );
	ret->NItems = ele->NOutputs;
	ret->Items = (void *)( (intptr_t)ret + sizeof(tList) );
	for( i = 0; i < ele->NOutputs; i ++ ) {
		ele->Outputs[i] = CreateAnonLink();
		ret->Items[i] = ele->Outputs[i];
		//printf(" ret->Items[%i] = %p\n", i, ret->Items[i]);
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
	
	len = snprintf(NULL, 0, "%s[%i]", Name, grp->Size-1);
	
	for( i = grp->Size-1; i; i /= 10 )	digits ++;
	
	ofs = Dest->NItems;
	Dest->NItems += grp->Size;
	Dest->Items = realloc( Dest->Items, Dest->NItems * sizeof(tLink*) );
	
	for( i = grp->Size; i --; )
	{
		char	newname[len + 1];
		snprintf(newname, 100, "%s[%*i]", Name, digits, i);
		
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
