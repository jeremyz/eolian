#ifndef LEXER_H
#define LEXER_H

typedef enum
{
   STOP = 0,
   SKIP_SPACES_TOKEN,
   KWORD_TOKEN,
   UWORD_TOKEN,
   KCHAR_TOKEN,
   UCHAR_TOKEN,
   STRING_TOKEN,
   UWORDS_LIST_TOKEN,
   STRINGS_LIST_TOKEN
} TokenEnum;

typedef enum
{
   LEFT_TO_RIGHT = 1,
   RIGHT_TO_LEFT = -1
} Parse_Direction;

#define KWORD(w)                    SKIP_SPACES_TOKEN, KWORD_TOKEN, w
#define UWORD(w)                    SKIP_SPACES_TOKEN, UWORD_TOKEN, w
#define KCHAR(c)                    SKIP_SPACES_TOKEN, KCHAR_TOKEN, c
#define UCHAR(c)                    SKIP_SPACES_TOKEN, UCHAR_TOKEN, c
#define STRING(end, ret)            SKIP_SPACES_TOKEN, STRING_TOKEN, end, ret
#define UWORDS_LIST(delim, list)    SKIP_SPACES_TOKEN, UWORDS_LIST_TOKEN, delim, list
#define STRINGS_LIST(delim, list)   SKIP_SPACES_TOKEN, STRINGS_LIST_TOKEN, delim, list

#define LEX(buf,...)                   Lexer_operate(buf, strlen(buf), LEFT_TO_RIGHT, __VA_ARGS__, STOP)
#define LEX_REVERSE(buf, max_len, ...) Lexer_operate(buf+max_len-1, max_len, RIGHT_TO_LEFT, __VA_ARGS__, STOP)

char *Lexer_operate(char *buffer, unsigned int max_length, Parse_Direction dir, ...);

#endif
