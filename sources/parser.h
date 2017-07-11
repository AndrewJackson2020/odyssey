#ifndef OD_PARSER_H
#define OD_PARSER_H

/*
 * Odissey.
 *
 * Advanced PostgreSQL connection pooler.
*/

typedef struct od_parsertoken od_parsertoken_t;
typedef struct od_parser      od_parser_t;

enum {
	OD_PARSER_EOF,
	OD_PARSER_ERROR,
	OD_PARSER_NUM,
	OD_PARSER_NAME,
	OD_PARSER_SYMBOL,
	OD_PARSER_STRING
};

struct od_parsertoken
{
	int type;
	int line;
	union {
		uint64_t num;
		struct {
			char *pointer;
			int   size;
		} string;
	} value;
};

struct od_parser
{
	char             *pos;
	char             *end;
	int               line;
	od_parsertoken_t  backlog[4];
	int               backlog_count;
};

static inline void
od_parser_init(od_parser_t *parser, char *string, int size)
{
	parser->pos  = string;
	parser->end  = string + size;
	parser->line = 0;
	parser->backlog_count = 0;
}

static inline void
od_parser_push(od_parser_t *parser, od_parsertoken_t *token)
{
	assert(parser->backlog_count < 4);
	parser->backlog[parser->backlog_count] = *token;
	parser->backlog_count++;
}

static inline int
od_parser_next(od_parser_t *parser, od_parsertoken_t *token)
{
	/* try to use backlog */
	if (parser->backlog_count > 0) {
		*token = parser->backlog[parser->backlog_count - 1];
		parser->backlog_count--;
		return token->type;
	}
	/* skip white spaces and comments */
	for (;;) {
		while (parser->pos < parser->end && isspace(*parser->pos)) {
			if (*parser->pos == '\n')
				parser->line++;
			parser->pos++;
		}
		if (od_unlikely(parser->pos == parser->end)) {
			token->type = OD_PARSER_EOF;
			return token->type;
		}
		if (*parser->pos != '#')
			break;
		while (parser->pos < parser->end && *parser->pos != '\n')
			parser->pos++;
		if (parser->pos == parser->end) {
			token->type = OD_PARSER_EOF;
			return token->type;
		}
		parser->line++;
	}
	/* symbols */
	if (*parser->pos != '\'' && ispunct(*parser->pos)) {
		token->type = OD_PARSER_SYMBOL;
		token->line = parser->line;
		token->value.num = *parser->pos;
		parser->pos++;
		return token->type;
	}
	/* digit */
	if (isdigit(*parser->pos)) {
		token->type = OD_PARSER_NUM;
		token->line = parser->line;
		token->value.num = 0;
		while (parser->pos < parser->end && isdigit(*parser->pos)) {
			token->value.num = (token->value.num * 10) + *parser->pos - '0';
			parser->pos++;
		}
		return token->type;
	}
	/* name */
	if (isalpha(*parser->pos)) {
		token->type = OD_PARSER_NAME;
		token->line = parser->line;
		token->value.string.pointer = parser->pos;
		while (parser->pos < parser->end &&
		       (*parser->pos == '_' || isalpha(*parser->pos) ||
		         isdigit(*parser->pos)))
			parser->pos++;
		token->value.string.size =
			parser->pos - token->value.string.pointer;
		return token->type;
	}
	/* string */
	if (*parser->pos == '\'') {
		token->type = OD_PARSER_STRING;
		token->line = parser->line;
		parser->pos++;
		token->value.string.pointer = parser->pos;
		while (parser->pos < parser->end && *parser->pos != '\'') {
			if (*parser->pos == '\n') {
				token->type = OD_PARSER_ERROR;
				return token->type;
			}
			parser->pos++;
		}
		if (od_unlikely(parser->pos == parser->end)) {
			token->type = OD_PARSER_ERROR;
			return token->type;
		}
		token->value.string.size =
			parser->pos - token->value.string.pointer;
		parser->pos++;
		return token->type;
	}
	/* error */
	token->type = OD_PARSER_ERROR;
	token->line = parser->line;
	return token->type;
}

#endif /* OD_PARSER_H */
