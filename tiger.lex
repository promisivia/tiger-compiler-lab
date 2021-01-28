%{
/* Lab2 Attention: You are only allowed to add code in this file and start at Line 26.*/
#include <string.h>
#include <ctype.h>
#include "util.h"
#include "errormsg.h"
#include "symbol.h"
#include "absyn.h"
#include "y.tab.h"

int charPos=1;
int comment_count = 0;

int yywrap(void)
{
    charPos=1;
    return 1;
}

void adjust(void)
{
    EM_tokPos=charPos;
    charPos+=yyleng;
}

/*
* Please don't modify the lines above.
* You can add C declarations of your own below.
*/

/* @function: getstr
 * @input: a string literal
 * @output: the string value for the input which has all the escape sequences
 * translated into their meaning.
 */
char *getstr(const char *str) {
    char *buf;
    int i = 1, j = 0, len = strlen(str);

    if (len == 2) {
        buf = "";
        return buf;
    }

    buf = malloc(len);
    while (i < len - 1) {
        /* change \n, \t, \^c and \ddd, \", \\ and \f---f\  */
        if (i < len - 2 && str[i] == '\\') {
            char next = str[i + 1];
            if (next >= '0' && next <= '9') {
                char d2 = str[i + 2];
                char d3 = str[i + 3];
                if (!isdigit(d2) || !isdigit(d3)) {
                    break;
                }
                int num = (next - '0') * 100 + (d2 - '0') * 10 + d3 - '0';
                if (num > 255) {  /* Out of range */
                    break;
                } else {
                    buf[j] = num;
                }
                i += 4;
            } else {
                switch (next) {
                    case 'n':
                        buf[j] = '\n';
                        i += 2;
                        break;
                    case 't':
                        buf[j] = '\t';
                        i += 2;
                        break;
                    case '\\':
                        buf[j] = '\\';
                        i += 2;
                        break;
                    case '\"':
                        buf[j] = '\"';
                        i += 2;
                        break;
                    case '^':
                        buf[j] = str[i + 2] - 'A' + 1;
                        i += 3;
                        break;
                    default:
                        do {
                            i++;
                        } while (i < len && str[i] != '\\');
                        if (i == len && str[len - 1] != '\\') {
                            EM_error(charPos, "error f---f format");
                        }
                        i++;
                        j--;
                }
            }
        } else {
            buf[j] = str[i];
            i++;
        }
        j++;
    }
    buf[j] = '\0';

    return buf;
}

%}
/* You can add lex definitions here. */

%S COMMENT

%%
[ \t]+ {adjust();}
"\n" {adjust(); EM_newline();}
<INITIAL>"," {adjust(); return COMMA;}
<INITIAL>":" {adjust(); return COLON;}
<INITIAL>";" {adjust(); return SEMICOLON;}
<INITIAL>"(" {adjust(); return LPAREN;}
<INITIAL>")" {adjust(); return RPAREN;}
<INITIAL>"[" {adjust(); return LBRACK;}
<INITIAL>"]" {adjust(); return RBRACK;}
<INITIAL>"{" {adjust(); return LBRACE;}
<INITIAL>"}" {adjust(); return RBRACE;}
<INITIAL>"." {adjust(); return DOT;}
<INITIAL>"+" {adjust(); return PLUS;}
<INITIAL>"-" {adjust(); return MINUS;}
<INITIAL>"*" {adjust(); return TIMES;}
<INITIAL>"/" {adjust(); return DIVIDE;}
<INITIAL>"=" {adjust(); return EQ;}
<INITIAL>"<>" {adjust(); return NEQ;}
<INITIAL>"<" {adjust(); return LT;}
<INITIAL>"<=" {adjust(); return LE;}
<INITIAL>">" {adjust(); return GT;}
<INITIAL>">=" {adjust(); return GE;}
<INITIAL>"&" {adjust(); return AND;}
<INITIAL>"|" {adjust(); return OR;}
<INITIAL>":=" {adjust(); return ASSIGN;}
<INITIAL>"array" {adjust(); return ARRAY;}
<INITIAL>"if" {adjust(); return IF;}
<INITIAL>"then" {adjust(); return THEN;}
<INITIAL>"else" {adjust(); return ELSE;}
<INITIAL>"while" {adjust(); return WHILE;}
<INITIAL>"for" {adjust(); return FOR;}
<INITIAL>"to" {adjust(); return TO;}
<INITIAL>"do" {adjust(); return DO;}
<INITIAL>"let" {adjust(); return LET;}
<INITIAL>"in" {adjust(); return IN;}
<INITIAL>"end" {adjust(); return END;}
<INITIAL>"of" {adjust(); return OF;}
<INITIAL>"break" {adjust(); return BREAK;}
<INITIAL>"nil" {adjust(); return NIL;}
<INITIAL>"function" {adjust(); return FUNCTION;}
<INITIAL>"var" {adjust(); return VAR;}
<INITIAL>"type" {adjust(); yylval.sval = getstr(yytext); return TYPE;}
<INITIAL>[0-9]+ {adjust(); yylval.ival = atoi(yytext); return INT;}
<INITIAL>[A-Za-z][A-Za-z0-9_]* {adjust(); yylval.sval = String(yytext); return ID;}
<INITIAL>\"(\\\"|[^\"])*\" {adjust();yylval.sval = getstr(yytext);return STRING;}
<INITIAL>"/*" {adjust(); comment_count++; BEGIN COMMENT;}
<COMMENT>"/*" {adjust(); comment_count++;}
<COMMENT>"*/" {adjust(); comment_count--;if (comment_count == 0) {BEGIN INITIAL;}}
<COMMENT><<EOF>> {EM_error(charPos, "unclosed comments");}
<COMMENT>.|\n {adjust();}
<INITIAL>. {adjust(); EM_error(charPos, yytext);}
