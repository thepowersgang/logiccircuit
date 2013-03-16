/*
 * LogicCircuit
 * By John Hodge (thePowersGang)
 *
 * compiled.c
 * - Reads/writes compiled versions of nets
 */
#include <common.h>
#include <stdio.h>
#include <stdlib.h>	// malloc
#include <stdint.h>
#include <string.h>
#include "include/compiled.h"

extern tElementDef	*gpElementDefs;

void WriteCompiledVersion(const char *Path, int bBinary)
{
	 int	n_links = 0;
	 int	n_eletypes = 0;
	 int	n_elements = 0;
	FILE	*fp;
	tLink	*link;
	tElement	*ele;
	 int	i;
	
	for( link = gRootUnit.Links; link; link = link->Next ) {
		link->Backlink = (void*)(intptr_t)n_links;	// Save
		n_links ++;
	}
	for( ele = gRootUnit.Elements; ele; ele = ele->Next )
		n_elements ++;
	for( tElementDef *ed = gpElementDefs; ed; ed = ed->Next )
		n_eletypes ++;

	void _Write8(FILE *fp, uint8_t val) {
		fputc(val, fp);
	}
	void _Write16(FILE *fp, uint16_t val) {
		fputc(val & 0xFF, fp);
		fputc(val >> 8, fp);
	}
	void _Write32(FILE *fp, uint32_t val) {
		_Write16(fp, val & 0xFFFF);
		_Write16(fp, val >> 16);
	}
	
	fp = fopen(Path, "wb");
	
	if( bBinary ) {
		_Write16(fp, n_links);
		_Write8(fp, n_eletypes);
		_Write16(fp, n_elements);

		// Element type names
		for( tElementDef *ed = gpElementDefs; ed; ed = ed->Next ) {
			_Write8(fp, strlen(ed->Name));
			fwrite(ed->Name, 1, strlen(ed->Name), fp);
		}
	}
	else {
		fprintf(fp, "%x %x\n", n_links, n_elements);
		// NOTE: No need for n_eletypes, no strings
	}
	
	// Element info
	for( ele = gRootUnit.Elements; ele; ele = ele->Next )
	{
		if( bBinary ) {
			 int	typeid = 0;
			for( tElementDef *ed = gpElementDefs; ed && ed != ele->Type; ed = ed->Next )
				typeid ++;
			_Write8(fp, typeid);
			_Write8(fp, ele->NParams);
			_Write16(fp, ele->NInputs);
			_Write16(fp, ele->NOutputs);
			for( i = 0; i < ele->NParams; i ++ )
				_Write32(fp, ele->Params[i]);
			for( i = 0; i < ele->NInputs; i ++ )
				_Write16(fp, (intptr_t)ele->Inputs[i]->Backlink);
			for( i = 0; i < ele->NOutputs; i ++ )
				_Write16(fp, (intptr_t)ele->Outputs[i]->Backlink);
		}
		else {
			fprintf(fp,
				"%s %2x %2x %2x",
				ele->Type->Name, ele->NParams, ele->NInputs, ele->NOutputs
				);
			for( i = 0; i < ele->NParams; i ++ )
				fprintf(fp, " %x", ele->Params[i]);
			for( i = 0; i < ele->NInputs; i ++ )
				fprintf(fp, " %x", (int)(intptr_t)ele->Inputs[i]->Backlink);
			for( i = 0; i < ele->NOutputs; i ++ )
				fprintf(fp, " %x", (int)(intptr_t)ele->Outputs[i]->Backlink);
			fprintf(fp, "\n");
		}
	}
	
	fclose(fp);
}

void ReadCompiledVersion(const char *Path, int bBinary)
{
	int n_links, n_elements;
	char	**elenames;
	 int	n_elenames;
	FILE	*fp;

	tElement	*first_ele = NULL;
	tElement	*ele, *last_ele = NULL;
	tLink	*links = NULL;
	tLinkValue	*vals = NULL;

	uint8_t _Read8(FILE *FP) {
		return fgetc(FP);
	}
	uint16_t _Read16(FILE *FP) {
		uint16_t rv = fgetc(FP);
		rv |= (uint16_t)fgetc(FP) << 8;
		return rv;
	}
	uint32_t _Read32(FILE *FP) {
		uint32_t rv = _Read16(FP);
		rv |= (uint32_t)_Read16(FP) << 16;
		return rv;
	}

	if( bBinary )
		fp = fopen(Path, "rb");
	else
		fp = fopen(Path, "r");

	if( bBinary )
	{
		n_links    = _Read16(fp);
		n_elenames = _Read8(fp);
		n_elements = _Read16(fp);
		printf("%i links, %i elements, %i ele types\n", n_links, n_elements, n_elenames);
		
		elenames = calloc( n_elenames, sizeof(elenames[0]));
		if( !elenames ) {
			// TODO: Sad
			goto _err;
		}
		for( int i = 0; i < n_elenames; i ++ )
		{
			int len = fgetc(fp);
			//printf("%i byte string\n", len);
			if( len == -1 ) {
				goto _err;
			}
			elenames[i] = malloc( len + 1 );
			fread(elenames[i], 1, len, fp);
			elenames[i][len] = '\0';
		}
	}
	else
	{
		fscanf(fp, "%04x %04x\n", &n_links, &n_elements);
	}
	
	// Create links
	links = calloc( n_links, sizeof(tLink) );
	vals = calloc( n_links, sizeof(*vals) );
	for( int i = 0; i < n_links-1; i ++ ) {
		links[i].Next = &links[i+1];
		links[i].Value = &vals[i];
	}
	links[n_links-1].Value = &vals[n_links-1];
	
	// Create elements
	for( int j = 0; j < n_elements; j ++ )
	{
		char	txtname[16];
		char	*name;
		int	params[MAX_PARAMS];
		 int	n_params, n_input, n_output;
		
		if( bBinary ) {
			int typeid;
			typeid   = _Read8(fp);
			n_params = _Read8(fp);
			n_input  = _Read16(fp);
			n_output = _Read16(fp);
			if( n_params > MAX_PARAMS )
				n_params = MAX_PARAMS;
			for( int i = 0; i < n_params; i ++ )
				params[i] = _Read32(fp);
			if( typeid >= n_elenames )
				name = "";
			else
				name = elenames[typeid];
		}
		else {
			fscanf(fp, "%16s %x %x %x", txtname, &n_params, &n_input, &n_output);
			name = txtname;
			if( n_params > MAX_PARAMS )
				n_params = MAX_PARAMS;
			for( int i = 0; i < n_params; i ++ )
				fscanf(fp, " %x", &params[i]);
		}

		// Locate type
		tElementDef *def;
		for( def = gpElementDefs; def; def = def->Next )
		{
			if( strcmp(name, def->Name) == 0 )
				break;
		}
		if(!def) {
			fprintf(stderr, "Unknown unit '%s' (#%i)\n", name, j);
			goto _err;
		}
	
		// Create unit
		ele = def->Create(n_params, params, n_input, NULL);
		if( !ele ) {
			fprintf(stderr, "Unit %s errored\n", name);
			goto _err;
		}
		if( ele->NOutputs != n_output ) {
			fprintf(stderr, "%s with %i inputs did not return %i outputs (%i instead)\n",
				name, n_input, n_output, ele->NOutputs
				);
			goto _err;
		}

		// Link up inputs/outputs
		if( bBinary ) {
			for( int i = 0; i < ele->NInputs; i ++ ) {
				int idx = _Read16(fp);
				ele->Inputs[i] = &links[idx];
			}
			for( int i = 0; i < ele->NOutputs; i ++ ) {
				int idx = _Read16(fp);
				ele->Outputs[i] = &links[idx];
			}
		}
		else {
			for( int i = 0; i < ele->NInputs; i ++ ) {
				int idx;
				fscanf(fp, " %x", &idx);
				ele->Inputs[i] = &links[idx];
			}
			for( int i = 0; i < ele->NOutputs; i ++ ) {
				int idx;
				fscanf(fp, " %x", &idx);
				ele->Outputs[i] = &links[idx];
			}
			fscanf(fp, "\n");
		}
		
		// Append to list
		if( last_ele )
			last_ele->Next = ele;
		else
			first_ele = ele;
		last_ele = ele;
	}
	
	links[n_links-1].Next = gRootUnit.Links;
	gRootUnit.Links = links;
	last_ele->Next = gRootUnit.Elements;
	gRootUnit.Elements = first_ele;
	goto _common;
	
_err:
	free(links);

_common:
	if( bBinary ) {
		for( int i = 0; i < n_elenames; i ++ )
			free(elenames[i]);
		free(elenames);
	}
	fclose(fp);
}
