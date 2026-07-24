/* CCScript lexical analyzer/scanner */


#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <errno.h>
#include "lexer.h"
#include "exception.h"
#include "utf8.h"
#include "util.h"

using namespace std;

Lexer::Lexer(string src)
{
	in = src;
	inpos = 0;
	Init();
}

void Lexer::Init()
{
	error = NULL;
	AddKeyword("if",ifsym);
	AddKeyword("else",elsesym);
	AddKeyword("menu",menusym);
	AddKeyword("default",defaultsym);
	AddKeyword("define",constsym);
	AddKeyword("command",commandsym);
	AddKeyword("or", orsym);
	AddKeyword("and", andsym);
	AddKeyword("not", notsym);
	AddKeyword("flag", flagsym);
	AddKeyword("byte", bytesym);
	AddKeyword("short", shortsym);
	AddKeyword("long", longsym);
	AddKeyword("ROM", romsym);
	AddKeyword("ROMTBL", romtblsym);
	AddKeyword("import", importsym);
	AddKeyword("insertbin", insertbinsym);
	AddKeyword("count", countsym);
	AddKeyword("setcount", setcountsym);
	AddKeyword("flyover", flyoversym);
	line = 1;
	column = 0;
	currentsym = errorsym;
	Next();
}

void Lexer::SetErrorHandler(ErrorReceiver* e)
{
	error = e;
}

void Lexer::Error(const string& msg)
{
	if(error) error->Error(msg, line, column);
}

void Lexer::Warning(const string& msg)
{
	if(error) error->Warning(msg, line, column);
}

void Lexer::AddKeyword(const string& kw, symbol sym)
{
	keywords[kw] = sym;
}

/*static*/ string Lexer::SymbolToString(symbol sym)
{
	switch(sym) {
		case finished: return "end of file";
		case identifier: return "identifier";
		case intliteral: return "int literal";
		case stringliteral: return "string literal";
		case constsym: return "define";
		case flagsym: return "flag";
		case ifsym: return "if";
		case elsesym: return "else";
		case menusym: return "menu";
		case defaultsym: return "default";
		case commandsym: return "command";
		case andsym: return "and";
		case orsym: return "or";
		case notsym: return "not";
		case bytesym: return "byte";
		case shortsym: return "short";
		case longsym: return "long";
		case romsym: return "ROM";
		case romtblsym: return "ROMTBL";
		case leftparen: return "(";
		case rightparen: return ")";
		case leftbrace: return "{";
		case rightbrace: return "}";
		case leftbracket: return "[";
		case rightbracket: return "]";
		case period: return ".";
		case colon: return ":";
		case comma: return ",";
		case equals: return "=";
		case importsym: return "import";
		case insertbinsym: return "insertbin";
		case countsym: return "count";
		case setcountsym: return "setcount";
		case flyoversym: return "flyover";
		default: return "INVALID SYMBOL";
	}
}

int Lexer::GetPosition() const
{
	return inpos;
}

void Lexer::GetCurrentToken(Token& t) const
{
	t.sym = currentsym;
	t.line = line;
	t.ival = currentint;
	t.sval = currentstr;
	t.stype = currentstype;
}


Token Lexer::GetCurrentToken() const
{
	Token t;
	GetCurrentToken(t);
	return t;
}

std::string Token::ToString()
{
	switch(sym) {
		case stringliteral:
			return stype + string("\"") + sval + '\"';
		case leftparen: return "(";
		case rightparen: return ")";
		case leftbrace: return "{";
		case rightbrace: return "}";
		case period: return ".";
		case comma: return ",";
		case colon: return ":";
		case equals: return "=";
		case finished:
		case errorsym:
			return "INVALID_TOKEN";
		default:
			return sval;
	}
}

void Lexer::Next(bool allowDecodingErrors)
{
	if(inpos >= in.length()) {
		current = eob;
		return;
	}
	auto oldpos = inpos;
	current = utf8::utf8to32(in, inpos);
	if (current == utf8::DECODING_ERROR) {
		if(!allowDecodingErrors) {
			stringstream ss;
			ss << "invalid UTF-8 sequence '" << std::setbase(16);
			for (auto pos = oldpos; pos < inpos; ++pos) {
				ss << in[pos];
			}
			ss << "'";
			Error(ss.str());
		}
		// Use Unicode replacement character? Idk what the right thing to do here is.
		// We either logged the error or this character is in a comment so we don't care.
		current = 0xFFFD;
	}
	column++;
}

void Lexer::LexSingleComment()
{
	do {
		Next(true);
	} while(current != '\n' && current != eob);
}

bool Lexer::LexBlockComment()
{
	Next(true);
	for(;;) {
		switch(current) {
			case '*':
				Next(true); // Check next character for comment terminator
				if(current == '/') { Next(); return true; }
				continue;
			case '\n':
				Newline();
				Next(true);
				continue;
			case eob:
				Error("unexpected end of file in comment");
				return false;	// fix infinite loop in unclosed comment :P
			default:
				Next(true);
		}
	}
	return true;
}

symbol Lexer::LexStringLiteral()
{
	currentstr = "";

	while(current != '\"')
	{
		switch(current) {
			case eob:
				Error("unexpected end of file in string literal");
				return errorsym;
			case '\n':
				Error("newline in string");
				Newline();
				return errorsym;
			case '\\':
				Next();
				switch(current) {
				case '\"': Next(); currentstr += '\"'; break;
				case '\\': Next(); currentstr += '\\'; break;
				default:
					Next();
					Warning("unrecognized escape character ignored");
					continue;
				}
			default:
				currentstr += utf8::utf32to8(current);
				Next();
		}
	}

	Next();
	return stringliteral;
}

symbol Lexer::LexIdentifier()
{
	currentstr = "";

	do {
		currentstr += utf8::utf32to8(current);
		Next();
	} while(util::IsIdentifier(current));

	if(keywords.find(currentstr) != keywords.end()) {
		return keywords[currentstr];
	}

	return identifier;
}

symbol Lexer::LexNumber()
{
	auto first = current;
	int radix = 0;
	bool negate = false;
	string numstr{};

	if(current == '-') {
		negate = true;
		Next();
		first = current;
	}

	Next();

	if(first == '0' && toupper(current) == 'X') {
		numstr += first;
		numstr += current;
		radix = 16;
		Next();
		while(util::IsHexDigit(current)) {
			numstr += current;
			Next();
		}
	}
	else {
		radix = 10;
		numstr += first;
		while(util::IsDigit(current)) {
			numstr += current;
			Next();
		}
	}

	if(util::IsAlphanumerical(current)) {
		Error("number has invalid suffix");
	}
	unsigned int temp = 0;
	stringstream ss(numstr);
	ss >> setbase(radix) >> temp;
	if(ss.fail()) {
		Warning("integer constant capped at 0xffffffff");
		temp = 0xffffffff;
	}
	currentint = temp;

	if(negate)
		currentint = -currentint;

	return intliteral;
}

symbol Lexer::LexSymbol()
{
	while(current != eob)
	{
		switch(current)
		{
		case '\t': case '\r': case ' ':
			Next();
			continue;

		case '\n':
			Newline();
			Next();
			continue;

		case '/':
			Next();
			switch(current) {
			case '/': LexSingleComment(); continue;
			case '*': if(!LexBlockComment()) return errorsym; continue;
			default:
				Error("unexpected character '/'");
				continue;
			}

		case '!':
		case '~':
			currentstype = static_cast<char>(current);
			Next();
			if(current != '\"') {
				Error("string expected");
				return errorsym;
			}
			Next();
			return LexStringLiteral();

		case '\"':
			currentstype = ' ';
			Next();
			return LexStringLiteral();

		case '=': Next(); return equals;
		case '(': Next(); return leftparen;
		case ')': Next(); return rightparen;
		case '{': Next(); return leftbrace;
		case '}': Next(); return rightbrace;
		case '[': Next(); return leftbracket;
		case ']': Next(); return rightbracket;
		case '.': Next(); return period;
		case ',': Next(); return comma;
		case ':': Next(); return colon;

		default:
			if(util::IsIdentifierStart(current)) {
				return LexIdentifier();
			}
			else if(util::IsDigit(current) || current == '-') {
				return LexNumber();
			}
			else {
				stringstream ss;
				ss << "unexpected character 0x" << std::setbase(16) << current;
				ss << " ('" << utf8::utf32to8(current) << "')";
				Error(ss.str());
				Next();
				continue;
			}
		}
	}
	return finished;
}

symbol Lexer::Lex()
{
	currentsym = LexSymbol();
	return currentsym;
}

symbol Lexer::Peek()
{
	// Not the most elegant way to peek, but it works...

	auto oldint = currentint;
	auto oldstr = currentstr;
	auto oldstype = currentstype;
	auto oldline = line;
	auto oldcolumn = column;
	auto oldpos = inpos;
	auto oldc = current;

	symbol sym = LexSymbol();
	
	if(sym != errorsym) {
		currentint = oldint;
		currentstr = oldstr;
		currentstype = oldstype;
		line = oldline;
		column = oldcolumn;
		inpos = oldpos;
		current = oldc;
	}

	return sym;
}

void Lexer::Newline() {
	line++;
	column = 1;
}

