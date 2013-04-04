/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 * 
 * parse.c
 * - Parsing code
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <common.h>
#include <unistd.h>
#include <assert.h>
#include "parse.h"

// === PROTOTYPES ===
 int	ParseNumber(tParser *Parser);
 int	ParseValue(tParser *Parser, tList *destList);
tList	*ParseOperation(tParser *Parser);
 int	ParseLine(tParser *Parser);
 int	ParseFile(const char *Filename);
void	SyntaxError(tParser *Parser, const char *Fmt, ...);
void	SyntaxWarning(tParser *Parser, const char *Fmt, ...);
void	SyntaxAssert(tParser *Parser, int Got, int Expected);

// === CODE ===
unsigned long long GetNumber(tParser *Parser)
{
	const char	*start = Parser->TokenStr;
	 int	base = 0;
	assert(Parser->Token == TOK_NUMBER);
	
	// strtoll doesn't handle 0b
	if( Parser->TokenLength > 2 && Parser->TokenStr[0] == '0' && Parser->TokenStr[1] == 'b' ) {
		start += 2;
		base = 2;
	}
	
	char	*end;	
	unsigned long long	ret = strtol(start, &end, base);
	size_t	len = end - Parser->TokenStr;
	assert(len <= Parser->TokenLength);
	if(len < Parser->TokenLength)
		SyntaxError(Parser, "Number '%.*s' is invalid (%i/%i eaten)",
			Parser->TokenLength, Parser->TokenStr, len, Parser->TokenLength);
	return ret;
}

int ParseNumber_Paren(tParser *Parser)
{
	if( GetToken(Parser) == TOK_PAREN_OPEN ) {
		int ret = ParseNumber(Parser);
		SyntaxAssert( Parser, GetToken(Parser), TOK_PAREN_CLOSE );
		return ret;
	}
	SyntaxAssert( Parser, Parser->Token, TOK_NUMBER );
	//printf("ParseNumber_Paren: %.*s = %li\n",
	//	Parser->TokenLength, Parser->TokenStr,
	//	strtol(Parser->TokenStr, NULL, 0)
	//	);
	//return strtol(Parser->TokenStr, NULL, 0);
	return GetNumber(Parser);
}
int ParseNumber_MulDiv(tParser *Parser)
{
	 int	val = ParseNumber_Paren(Parser);
	
	for(;;)
	{
		switch( GetToken(Parser) )
		{
		case TOK_STAR:
			val *= ParseNumber_Paren(Parser);
			break;
		case TOK_SLASH:
			val /= ParseNumber_Paren(Parser);
			break;
		default:
			PutBack(Parser);
			return val;
		}
	}
}
int ParseNumber_AddSub(tParser *Parser)
{
	 int	val = ParseNumber_MulDiv(Parser);
	
	for(;;)
	{
		switch( GetToken(Parser) )
		{
		case TOK_PLUS:
			val += ParseNumber_MulDiv(Parser);
			break;
		case TOK_MINUS:
			val -= ParseNumber_MulDiv(Parser);
			break;
		default:
			PutBack(Parser);
			return val;
		}
	}
}
/**
 * \brief Read a number from the buffer
 */
int ParseNumber(tParser *Parser)
{
	#if 0
	// TODO: Handle arithmatic operations too.
	SyntaxAssert( Parser, GetToken(Parser), TOK_NUMBER );
	return atoi(Parser->TokenStr);
	#else
	return ParseNumber_AddSub(Parser);
	#endif
}

void ParseValue_GroupRange_Append(tParser *Parser, tList *DestList, const char *GroupName, int Idx)
{
	if( List_AppendGroupItem(DestList, GroupName, Idx) )
		SyntaxError(Parser, "Error referencing group item %s[%i]", GroupName, Idx);
}

/**
 */
int ParseValue_GroupRange(tParser *Parser, tList *destList, const char *GroupName)
{
	int	start, end;
	do
	{
		start = ParseNumber( Parser );

		if( GetToken(Parser) == TOK_COLON )
		{
			// Item Range
			end = ParseNumber( Parser );

			if( end > start )
			{
				// Count up
				for( int i = start; i <= end; i ++ )
				{
					ParseValue_GroupRange_Append(Parser, destList, GroupName, i);
				}
			}
			else
			{
				// Count down
				for( int i = start; i >= end; i -- )
				{
					ParseValue_GroupRange_Append(Parser, destList, GroupName, i);
				}
			}

			GetToken(Parser);
		}
		else
		{
			// Single item
			ParseValue_GroupRange_Append(Parser, destList, GroupName, start);
		}
	} while(Parser->Token == TOK_COMMA);
	SyntaxAssert(Parser, Parser->Token, TOK_SQUARE_CLOSE);
	return 0;
}

/**
 * \brief Parse a "value" (Constant, Line or Group)
 * \return Zero on success, 1 if no value is found
 */
int ParseValue(tParser *Parser, tList *destList)
{
	char	*tmpName;
	
	GetToken(Parser);
	switch( Parser->Token )
	{
	case TOK_LINE:
		tmpName = strdup( Parser->TokenStr );
		// Repeated line?
		if( GetToken(Parser) == TOK_STAR ) {
			 int	count = ParseNumber(Parser);
			if( count < 0 )
				SyntaxError(Parser, "Can't have a negative line repeat");
			while(count --) {
				List_AppendLine(destList, tmpName);
			}
		}
		// Single
		else {
			PutBack(Parser);
			List_AppendLine(destList, tmpName);
		}
		free(tmpName);
		break;
	
	// Group (@name)
	case TOK_GROUP:
		tmpName = strdup(Parser->TokenStr);
		// Single line? (@group[i])
		if( GetToken(Parser) == TOK_SQUARE_OPEN )
		{
			ParseValue_GroupRange(Parser, destList, tmpName);
		}
		// Entire group.
		else {
			PutBack(Parser);
			if( List_AppendGroup(destList, tmpName) )
				SyntaxError(Parser, "Error referencing group %s", tmpName);
		}
		free(tmpName);
		break;
	
	// Constant
	case TOK_NUMBER: {
		uint64_t	num = GetNumber(Parser);
		 int	count = 1;
		 int	start = 0;
		 int	end = 0;

		// Handle multi-bit numbers
		if( GetToken(Parser) == TOK_SQUARE_OPEN )
		{
			SyntaxAssert(Parser, GetToken(Parser), TOK_NUMBER);
			start = atoi(Parser->TokenStr);
			SyntaxAssert(Parser, GetToken(Parser), TOK_COLON);
			SyntaxAssert(Parser, GetToken(Parser), TOK_NUMBER);
			end = atoi(Parser->TokenStr);
			SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_CLOSE);
		}
		else {
			PutBack(Parser);
		}

		// Handle repetion of a number
		if( GetToken(Parser) == TOK_STAR )
		{
			GetToken(Parser);
			SyntaxAssert(Parser, Parser->Token, TOK_NUMBER);
			count = atoi(Parser->TokenStr);
			if( count == 0 ) {
				SyntaxWarning(Parser, "Zero count in repeated constant");
			}
		}
		else {
			PutBack(Parser);
		}

		// Make sure values are sane
		if( start == 0 && end == 0 ) {
			if( num < 0 || num > 1 ) {
				SyntaxWarning(Parser, "Non-boolean constant value (%i) used\n", num);
				num = 1;
			}
		}
		else {
			if( start >= 64 || end >= 64 ) {
				SyntaxWarning(Parser, "Start/end greater than 63 (%i-%i) used\n", start, end);
			}
		}

		// Append the constant
		while( count -- )
		{
			if( start > end )
			{
				for( int i = start; i >= end; i -- )
					List_AppendLine( destList, (num & (1 << i)) ? "1" : "0" );
			}
			else
			{
				for( int i = start; i <= end; i ++ )
					List_AppendLine( destList, (num & (1 << i)) ? "1" : "0" );
			}
		}

		break; }
	
	case TOK_PAREN_OPEN:
		{
		tList *tmplist = ParseOperation(Parser);
		List_AppendList(destList, tmplist);
		List_Free(tmplist);
		SyntaxAssert(Parser, GetToken(Parser), TOK_PAREN_CLOSE);
		}
		break;
	
	default:
		return 1;
	}
	
	return 0;
}

tList *_commitList(tList *Input)
{
	tList *ret = calloc(1, sizeof(tList)+sizeof(tLink*)*Input->NItems);
	ret->Items = (void*)(ret + 1);
	ret->NItems = Input->NItems;
	memcpy(ret->Items, Input->Items, sizeof(tLink*)*Input->NItems);
	free(Input->Items);
	return ret;
}

/**
 * \brief Parse a gate definition
 */
tList *ParseOperation(tParser *Parser)
{
	char	*name;
	tList	inputs = {0};
	const int	maxParams = 4;
	 int	numParams = 0;
	 int	params[maxParams];
	
	// Check for a constant value
	if( ParseValue(Parser, &inputs) == 0 )
	{
		return _commitList(&inputs);
	}
	
	// Check if token is an ident
	if( Parser->Token != TOK_IDENT )
	{
		SyntaxError(Parser,
			"Unexpected %s, expected TOK_IDENT/TOK_NUMBER/TOK_LINE/TOK_GROUP",
			GetTokenStr(Parser->Token)
			);
	}
	
	name = strdup( Parser->TokenStr );
	
	// Static Paramaters (e.g. DELAY{4}, AND{8,2})
	if( GetToken(Parser) == TOK_BRACE_OPEN )
	{
		do {
			if( numParams == maxParams )
				SyntaxError(Parser, "Too many parameters to a gate (> %i)", maxParams);
			params[numParams++] = ParseNumber( Parser );
		} while( GetToken(Parser) == TOK_COMMA );
		SyntaxAssert( Parser, Parser->Token, TOK_BRACE_CLOSE );
	}
	else
		PutBack(Parser);
	
	// Input lines
	if( ParseValue(Parser, &inputs) == 0 )
	{
		while(GetToken(Parser) == TOK_COMMA)
		{
			if( ParseValue(Parser, &inputs) != 0) {
				SyntaxAssert(Parser, Parser->Token, TOK_T_VALUE);
			}
		}
		PutBack(Parser);
	}
	
	// Create output
	{
		tList	*ret = Build_ReferenceUnit( name, numParams, params, &inputs );
		if(!ret) {
			List_Free(&inputs);
			SyntaxError(Parser, "Unknown unit %s", name);
		}
		free(name);
		List_Free(&inputs);
		return ret;
	}
}

/**
 * \brief Parse a line
 */
int ParseLine(tParser *Parser)
{
	tList	destArray = {0};
	tList	*outputs;
	
	switch(GetToken(Parser))
	{
	// Empty Line
	case TOK_NEWLINE:	return 0;
	// End of file
	case TOK_EOF:	return 1;
	
	// Meta statement
	case TOK_META_STATEMENT:
		// #array <name> <size>
		// - Define a group of lines with <size> lines in them
		// (Defines @<name>[0] to @<name>[<size>-1])
		if( strcmp(Parser->TokenStr, "#array") == 0 ) {
			char	*name;
			 int	len;
			SyntaxAssert(Parser, GetToken(Parser), TOK_IDENT );
			name = strndup( Parser->TokenStart, Parser->TokenLength );
			len = ParseNumber( Parser );
			
			Build_CreateGroup( name, len );
			free( name );
		}
		// Define a unit
		else if( strcmp(Parser->TokenStr, "#defunit") == 0 ) {
			SyntaxAssert(Parser, GetToken(Parser), TOK_IDENT );
			if( Unit_IsInUnit() )
				SyntaxError(Parser, "#defunit used within a unit (nesting not allowed)");
			Unit_DefineUnit( Parser->TokenStr );
		}
		// Close a unit definition
		else if( strcmp(Parser->TokenStr, "#endunit") == 0 ) {
			if( !Unit_IsInUnit() )
				SyntaxError(Parser, "#endunit used not in a unit");
			Unit_CloseUnit();
		}
		// Set input lines
		else if( strcmp(Parser->TokenStr, "#input") == 0 ) {
			if( !Unit_IsInUnit() )
				SyntaxError(Parser, "#input used not in a unit");
			do {
				GetToken(Parser);
				switch( Parser->Token )
				{
				case TOK_LINE:
					Unit_AddSingleInput( Parser->TokenStr );
					break;
				case TOK_GROUP:
					{
					char	name[Parser->TokenLength+1];
					 int	len;
					strcpy(name, Parser->TokenStr);
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_OPEN);
					len = ParseNumber( Parser );
					Unit_AddGroupInput( name, len );
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_CLOSE);
					}
					break;
				default:
					SyntaxAssert(Parser, Parser->Token, TOK_T_LINE);
					break;
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);
		}
		// Set output lines
		else if( strcmp(Parser->TokenStr, "#output") == 0 ) {
			// Check for unit
			if( !Unit_IsInUnit() )
				SyntaxError(Parser, "#output used not in a unit");
			// Comma separated list of lines
			do {
				GetToken(Parser);
				switch( Parser->Token )
				{
				// Single
				// "$line"
				case TOK_LINE:
					Unit_AddSingleOutput( Parser->TokenStr );
					break;
				// Group (with size)
				// "@group[size]"
				case TOK_GROUP:
					{
					char	name[Parser->TokenLength+1];
					 int	len;
					strcpy(name, Parser->TokenStr);
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_OPEN);
					len = ParseNumber( Parser );
					Unit_AddGroupOutput( name, len );
					SyntaxAssert(Parser, GetToken(Parser), TOK_SQUARE_CLOSE);
					}
					break;
				default:
					SyntaxAssert(Parser, Parser->Token, TOK_T_LINE);
					break;
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			
			PutBack(Parser);	// Put back non-comma character
		}
		// Set display values
		else if( strcmp(Parser->TokenStr, "#display") == 0 ) {
			tList	cond = {0}, values = {0};
			char	*title;
			// Condition - Set of values (all must be met)
			do {
				if( ParseValue(Parser, &cond) ) {
					SyntaxError(Parser,
						"Unexpected %s, expected %s",
						GetTokenStr(TOK_T_VALUE), GetTokenStr(Parser->Token));
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);
			// Display Title - String
			SyntaxAssert(Parser, GetToken(Parser), TOK_STRING);
			title = strdup(Parser->TokenStr);
			// Comma separated list of values to print
			do {
				if( ParseValue(Parser, &values) ) {
					SyntaxError(Parser,
						"Unexpected %s, expected %s",
						GetTokenStr(TOK_T_VALUE), GetTokenStr(Parser->Token));
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);	// Put back non-comma token
			
			// Remove quotes
			title[strlen(title)-1] = '\0';
			title ++;
			
			Build_AddDisplayItem(title, &cond, &values);
		
			title --;	//  Reverse the ++ above
		
			free(title);
			List_Free(&cond);
			List_Free(&values);
		}
		// Set breakpoint
		else if( strcmp(Parser->TokenStr, "#breakpoint") == 0 ) {
			tList	cond = {0};
			char	*title;
			// Condition - Single value (well, should be :)
			ParseValue(Parser, &cond);
			// Display Title - String
			SyntaxAssert(Parser, GetToken(Parser), TOK_STRING);
			title = strdup(Parser->TokenStr);
			
			// Remove quotes
			title[strlen(title)-1] = '\0';
			title ++;
			
			Build_AddBreakpoint(title, &cond);
		
			title --;	//  Reverse the ++ above
		
			free(title);
			List_Free(&cond);
		}
		// -- Test cases --
		else if( strcmp(Parser->TokenStr, "#testcase") == 0 ) {
			int max_length;
			// Max Cycles - Number
			max_length = ParseNumber(Parser);
			// Name - String
			SyntaxAssert(Parser, GetToken(Parser), TOK_STRING);
	
			// If tests are disabled, find the end of the test block
			if( gbDisableTests ) {
				do {
					while( Parser->Token != TOK_NEWLINE )
						GetToken(Parser);
				} while( GetToken(Parser) != TOK_META_STATEMENT
					|| strcmp(Parser->TokenStr, "#endtestcase") != 0 );
	 		}
			else {		
				if( Test_IsInTest() )
					SyntaxError(Parser, "#testcase can't be nested");
				Test_CreateTest(max_length, Parser->TokenStr+1, Parser->TokenLength-2);
			}
		}
		else if( strcmp(Parser->TokenStr, "#testassert") == 0 ) {
			tList	cond = {0}, have = {0}, expected = {0};
			
			// Sanity check please
			if( !Test_IsInTest() )
				SyntaxError(Parser, "#testassert outside of a test");
			
			// Condition (single value)
			do {
				ParseValue(Parser, &cond);
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);	// Put back non-comma token
			
			// Values to check
			do {
				if( ParseValue(Parser, &have) ) {
					SyntaxAssert(Parser, Parser->Token, TOK_T_VALUE);
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);	// Put back non-comma token

			// Expected values
			do {
				if( ParseValue(Parser, &expected) ) {
					SyntaxAssert(Parser, Parser->Token, TOK_T_VALUE);
				}
				GetToken(Parser);
			} while(Parser->Token == TOK_COMMA);
			PutBack(Parser);	// Put back non-comma token
	
			if( Test_AddAssertion(&cond, &have, &expected) ) {
				SyntaxError(Parser, "Error in #testassert");
			}
			List_Free(&cond);
			List_Free(&have);
			List_Free(&expected);
		}
		else if( strcmp(Parser->TokenStr, "#testcomplete") == 0 )
		{
			tList cond = {0};
			
			// Sanity check please
			if( !Test_IsInTest() )
				SyntaxError(Parser, "#testcomplete outside of a test");
			
			// Condition (single value)
			ParseValue(Parser, &cond);
			
			Test_AddCompletion(&cond);
			
			List_Free(&cond);
		}
		else if( strcmp(Parser->TokenStr, "#endtestcase") == 0 ) {
			if( !Test_IsInTest() )
				SyntaxError(Parser, "#endtestcase without #testcase");
			Test_CloseTest();
		}
		else {
			SyntaxError(Parser, "Unknown meta-statement '%s'",
				Parser->TokenStr);
			return -1;
		}
		SyntaxAssert(Parser, GetToken(Parser), TOK_NEWLINE );
		return 0;
	
	// [<lines> =] GATE <input>
	default:
		// If there are parameters
		if( Parser->Token != TOK_IDENT )
		{
			// Standard Assignment Line
			PutBack(Parser);
			while( ParseValue(Parser, &destArray) == 0 )
			{
				if( GetToken(Parser) != TOK_COMMA )
					break;
			}
			
			// Check for assignmenr
			SyntaxAssert(Parser, Parser->Token, TOK_ASSIGN);
		}
		
		// Get gate (or gate chain)
		outputs = ParseOperation( Parser );
		
		// Assign
		if( List_EquateLinks( &destArray, outputs ) )
			SyntaxError(Parser,
				"Mismatch of left and right counts (%i != %i)",
				destArray.NItems, outputs->NItems
				);
		
		// Ensure a newline
		SyntaxAssert( Parser, GetToken(Parser), TOK_NEWLINE );
		
		// Clean up
		List_Free(outputs);
		free(destArray.Items);
		return 0;
	}
	// never reached
	assert(0);
}

/**
 * \brief Create a heap string using \a fmt
 */
char *MakeFmtStr( const char *fmt, ... )
{
	 int	len;
	va_list	args, args2;
	char	*ret;
	
	va_start(args, fmt);
	
	va_copy(args2, args);
	len = vsnprintf(NULL, 0, fmt, args2);
	
	ret = malloc( len + 1 );
	vsnprintf(ret, len+1, fmt, args);
	
	va_end(args);
	return ret;
}

/**
 * \brief Parse a file
 */
int ParseFile(const char *Filename)
{
	tParser	parser;
	 int	flen;
	FILE	*fp;
	char	*buf;
	
	memset(&parser, 0, sizeof(tParser));
	
	#if 1
	{
	char	tmpFileName[] = "/tmp/logic_cct.cct.XXXXXX";
	char	*cmdString;
	
	//char	*fname = strrchr(Filename, '/');
	// int	pathLen = (intptr_t)fname - (intptr_t)Filename + 1;
	//char	*path = malloc( pathLen + 1 );
	//memcpy(path, Filename, pathLen);
	//path[pathLen] = '\0';
	
	close( mkstemp(tmpFileName) );
	
	//cmdString = MakeFmtStr( "nasm -e \"%s\" -i \"%s\" | grep -v \"^%\" > \"%s\"", Filename, path, tmpFileName );
	//cmdString = MakeFmtStr( "yasm -e \"%s\" | grep -v \"^%%\" > \"%s\"", Filename, tmpFileName );
	cmdString = MakeFmtStr( "yasm -e \"%s\" > \"%s\"", Filename, tmpFileName );
	printf("%s\n", cmdString);
	fflush(stdout);
	
	if( system(cmdString) != 0 ) {
		fprintf(stderr, "Executing '%s' failed\n", cmdString);
		free(cmdString);
		return 1;
	}
	free(cmdString);
	//free(path);
	fp = fopen(tmpFileName, "r");
	unlink( tmpFileName );
	}
	#else
	fp = fopen(Filename, "r");
	#endif
	
	if(!fp) {
		perror("Unable to open file");
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	flen = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	
	buf = malloc( flen + 1 );
	fread(buf, 1, flen, fp);
	buf[flen] = '\0';
	
	fclose(fp);
	
	//printf("\n----\n%s\n----\n", buf);
	
	parser.File = strdup(Filename);
	parser.Line = 1;
	parser.TokenStr = parser._static;
	parser.DataStart = buf;
	parser.CurPos = parser.DataStart;
	
	if( setjmp( parser.jmpbuf ) != 0 ) {
		free( buf );
		return -1;
	}
	
	while( ParseLine( &parser ) == 0 );
	
	free( (char*)parser.File );
	free( buf );
	return 0;
}

/**
 * \brief Print a syntax error
 */
void SyntaxError(tParser *Parser, const char *Fmt, ...)
{
	va_list	args;
	va_start(args, Fmt);
	fprintf(stderr, "%s:%i: error: ", Parser->File, Parser->Line);
	vfprintf(stderr, Fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
	longjmp(Parser->jmpbuf, 1);
}

/**
 * \brief Print a warning message
 */
void SyntaxWarning(tParser *Parser, const char *Fmt, ...)
{
	va_list	args;
	va_start(args, Fmt);
	fprintf(stderr, "%s:%i: warning: ", Parser->File, Parser->Line);
	vfprintf(stderr, Fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

/**
 * \brief Assertion with a syntax error
 */
void SyntaxAssert(tParser *Parser, int Got, int Expected)
{
	if( Got != Expected ) {
		SyntaxError(Parser, "Expected %s, got %s '%s'",
			GetTokenStr(Expected), GetTokenStr(Got),
			Parser->TokenStr);
	}
}
