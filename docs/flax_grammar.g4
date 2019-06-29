// flax-grammar.g4
// Copyright (c) 2019, zhiayang
// Licensed under the Apache License Version 2.0.

// parser rules

grammar flax_grammar;

asdf: IDENTIFIER;










// lexer rules

// keywords
DO:         'do';
IF:         'if';
AS:         'as';
IS:         'is';
FFI:        'ffi';
AS_EXLAIM:  'as!';
VAR:        'var';
LET:        'let';
FOR:        'for';
NULL:       'null';
TRUE:       'true';
ELSE:       'else';
ENUM:       'enum';
FREE:       'free';
CLASS :     'class';
USING:      'using';
FALSE:      'false';
DEFER:      'defer';
WHILE:      'while';
ALLOC:      'alloc';
UNION:      'union';
BREAK:      'break';
TYPEID:     'typeid';
STRUCT:     'struct';
PUBLIC:     'public';
EXPORT:     'export';
IMPORT:     'import';
TYPEOF:     'typeof';
RETURN:     'return';
SIZEOF:     'sizeof';
STATIC:     'static';
PRIVATE:    'private';
MUTABLE:    'mutable';
VIRTUAL:    'virtual';
FUNC:       ('fn'|'ƒ');
INTERNAL:   'internal';
CONTINUE:   'continue';
OVERRIDE:   'override';
PROTOCOL:   'protocol';
OPERATOR:   'operator';
NAMESPACE:  'namespace';
TYPEALIAS:  'typealias';
EXTENSION:  'extension';
IDENTIFIER: [a-zA-Z_]+[a-zA-Z0-9]*;

LBRACE:             '{';
RBRACE:             '}';
LPAREN:             '(';
RPAREN:             ')';
LSQUARE:            '[';
RSQUARE:            ']';
LANGLE:             '<';
RANGLE:             '>';
PLUS:               '+';
MINUS:              '-';
ASTERISK:           '*';
DIVIDE:             ('/'|'÷');
SQUOTE:             '\'';
DQUOTE:             '"';
PERIOD:             '.';
COMMA:              ',';
COLON:              ':';
EQUAL:              '=';
QUESTION:           '?';
EXCLAMATION:        '!';
SEMICOLON:          ';';
AMPERSAND:          '&';
PERCENT:            '%';
PIPE:               '|';
DOLLAR:             '$';
LOGICAL_OR:         '||';
LOGICAL_AND:        '&&';
AT:                 '@';
POUND:              '#';
TILDE:              '~';
CARENT:             '^';
LEFT_ARROW:         '<-';
RIGHT_ARROW:        '->';
// FAT_LEFT_ARROW:  '<=';
FAT_RIGHT_ARROW:    '=>';
EQUALS_TO:          '==';
NOT_EQUALS:         ('!='|'≠');
LESS_EQUALS:        ('<='|'≤');
GREATER_EQUALS:     ('>='|'≥');
DOUBLE_PLUS:        '++';
DOUBLE_MINUS:       '--';
PLUS_EQ:            '+=';
MINUS_EQ:           '-=';
MULTIPLY_EQ:        '*=';
DIVIDE_EQ:          '/=';
MOD_EQ:             '%=';
AMPERSAND_EQ:       '&=';
PIPE_EQ:            '|=;';
CARET_EQ:           '^=';
ELLIPSIS:           '...';
HALF_OPEN_ELLIPSIS: '..<';
DOUBLE_COLON:       '::';

STRING_LITERAL:     '"' .*? '"';
CHARACTER_LITERAL:  '\'' ('\\' ('\\'|'\''|'n'|'b'|'a'|'r'|'t') | .) '\'';
NEWLINE:            '\n';
COMMENT
	:   '//' .*? NEWLINE
	|   '/*' (COMMENT|.*?) '*/'
	;

NUMBER
	:   ('0b'|'0B') [0-1]+
	|   ('0x'|'0X') [0-9a-fA-F]+
	|   [0-9]*('.'?)[0-9]+ (('e'|'E')[0-9]+)?
	;

ATTR_RAW:           '@raw';
ATTR_ENTRY:         '@entry';
ATTR_NOMANGLE:      '@nomangle';
ATTR_OPERATOR:      '@operator';
ATTR_PLATFORM:      '@platform';

DIRECTIVE_RUN:      '#run';
DIRECTIVE_IF:       '#if';











