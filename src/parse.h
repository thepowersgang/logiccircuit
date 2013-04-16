/*
 * LogicCircuit
 * - By John Hodge (thePowersGang)
 * 
 * parse.h
 * - Parser/Lexer Interface
 */
#ifndef _PARSE_H_
#define _PARSE_H_

#include <setjmp.h>

enum eTokens
{
	TOK_NULL,
	TOK_EOF,
	
	TOK_META_COMMENT,
	TOK_META_STATEMENT,
	TOK_NUMBER,
	TOK_IDENT,
	TOK_STRING,
	
	TOK_LINE,	// $aaaa
	TOK_GROUP,	// @aaaa
	
	TOK_NEWLINE,
	TOK_COMMA,
	TOK_ASSIGN,
	TOK_COLON,
	
	TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
	
	TOK_PAREN_OPEN, TOK_PAREN_CLOSE,
	TOK_BRACE_OPEN, TOK_BRACE_CLOSE,
	TOK_SQUARE_OPEN,TOK_SQUARE_CLOSE,
	
	TOK_T_VALUE, TOK_T_LINE
};

/**
 */
typedef struct
{
	const char	*DataStart;

	jmp_buf	jmpbuf;

	char	*File;
	 int	Line;
	
	const char	*CurPos;
	
	enum eTokens	Token;
	char	*TokenStr;
	
	const char	*TokenStart;
	 int	TokenLength;
	
	struct {
		const char	*Pos;
		 int	Line;
		enum eTokens	Token;
		const char	*TokenStart;
		 int	TokenLength;
	}	Saved;
	
	char	_static[15+1];
}	tParser;

extern int	GetToken(tParser *Parser);
extern void	PutBack(tParser *Parser);

extern const char	*GetTokenStr(enum eTokens);

#endif

