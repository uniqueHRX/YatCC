parser grammar SYsUParser;

options {
  tokenVocab=SYsULexer;
}

primaryExpression
    :   Identifier
    |   Constant
    |   parenExpression
    ;

postfixExpression
    :   primaryExpression
    |   postfixExpression LeftBracket expression RightBracket
    |   postfixExpression LeftParen expression? RightParen
    ;

unaryExpression
    :   postfixExpression
    |   unaryOperator unaryExpression
    ;

unaryOperator
    :   Plus|Minus|Not
    ;

multiplicativeExpression
    :   unaryExpression ((Star|Div|Mod) unaryExpression)*
    ;

additiveExpression
    :   multiplicativeExpression ((Plus|Minus) multiplicativeExpression)*
    ;

comparitiveExpression
    :   additiveExpression ((LT|GT|LE|GE) additiveExpression)*
    ;

equalityExpression
    :   comparitiveExpression ((EQ|NE) comparitiveExpression)*
    ;

logicalAndExpression
    :   equalityExpression (And equalityExpression)*
    ;

logicalOrExpression
    :   logicalAndExpression (Or logicalAndExpression)*
    ;

binaryExpression
    :   logicalOrExpression
    ;


assignmentExpression
    :   binaryExpression
    |   unaryExpression Equal assignmentExpression
    ;

expression
    :   assignmentExpression (Comma assignmentExpression)*
    ;


parenExpression 
    :   LeftParen expression RightParen
    ;


declaration
    :   declarationSpecifiers initDeclaratorList? Semi
    ;

declarationSpecifiers
    :   declarationSpecifier+
    ;

declarationSpecifier
    :   typeQualifier? typeSpecifier
    ;

initDeclaratorList
    :   initDeclarator (Comma initDeclarator)*
    ;

initDeclarator
    :   declarator (Equal initializer)?
    ;


typeSpecifier
    :   Int
    ;

typeQualifier
    :   Const
    ;


declarator
    :   directDeclarator
    ;

directDeclarator
    :   Identifier
    |   directDeclarator LeftBracket assignmentExpression? RightBracket
    ;

identifierList
    :   Identifier (Comma Identifier)*
    ;

initializer
    :   assignmentExpression
    |   LeftBrace initializerList? Comma? RightBrace
    ;

initializerList
    // :   designation? initializer (Comma designation? initializer)*
    :   initializer (Comma initializer)*
    ;

statement
    :   compoundStatement
    |   expressionStatement
    |   jumpStatement
    |   ifStatement
    |   whileStatement
    ;

compoundStatement
    :   LeftBrace blockItemList? RightBrace
    ;

blockItemList
    :   blockItem+
    ;

blockItem
    :   statement
    |   declaration
    ;

expressionStatement
    :   expression? Semi
    ;

ifStatement
    :   If LeftParen expression RightParen statement (Else statement)*
    ;

whileStatement
    :   While LeftParen expression RightParen statement
    ;



jumpStatement
    :   (Return expression?)
    Semi
    ;

compilationUnit
    :   translationUnit? EOF
    ;

translationUnit
    :   externalDeclaration+
    ;

externalDeclaration
    :   functionDefinition
    |   declaration
    ;

functionDefinition
    : declarationSpecifiers directDeclarator LeftParen parameterList? RightParen compoundStatement
    ;

parameterList
    :   parameter (Comma parameter)*
    |   Void
    ;

parameter
    :   declarationSpecifiers initDeclarator
    ;
