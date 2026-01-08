#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"

#include "debug.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

/*
 * The current and previous tokens are stored in the following struct.
 * And like we did in other modules, we have a single global variable of this struct type
 * so we don't need to pass the state around from function to function in the compiler.
 */

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

/*
 * These are all of Fer's precedence levels in order from lowers to highest. Since C implicitly gives successively larger numbers for enums,
 * this means that PREC_CALL is numerically biffer than PREC_UNARY. For example,
 * say the compiler is sitting on a chunk of code like:
 * -a.b + c
 * If we call parsePrecedence(PREC_ASSIGNMENT), then it will parse the entire expression because + has higher precedence than the assignment.
 * If instead we call parsePrecedence(PREC_UNARY), it will compile the -a.b and stop there. It doesn't keep going through the + because the addition has
 * lower precedence than unary operators.
 */

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,    // =
    PREC_OR,            // or
    PREC_AND,           // and
    PREC_EQUALITY,      // == !=
    PREC_COMPARISON,    // < > <= >=
    PREC_TERM,          // + -
    PREC_FACTOR,        // * /
    PREC_UNARY,         // ! -
    PREC_CALL,          // . () []
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

/*
 * We need a table that, given a token type, lets us find
 * - the function to compile a prefix expression starting with a token of that type,
 * - the function to compile an infix expression whose left operand is followed by a token of that type, and
 * - the precedence of an infix expression that uses that token as an operator.
 *
 * We wrap these three properties in a little struct which represents a single row in the parser table.
 */

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
    bool isPerm;
} Local;

typedef struct {
    uint8_t index;
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_INITIALIZER,
    TYPE_METHOD,
    TYPE_SCRIPT
} FunctionType;

typedef struct Loop {
    struct Loop *enclosing;
    int start;
    int scopeDepth;
    int *breakJumps;
    int breakCount;
    int breakCapacity;
} Loop;

typedef struct Compiler {
    struct Compiler *enclosing;
    ObjFunction *function;
    Loop *loop;
    FunctionType type;
    Local locals[UINT8_COUNT];
    int localCount;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

typedef struct ClassCompiler {
    struct ClassCompiler *enclosing;
    bool hasSuperclass;
} ClassCompiler;

Parser parser;
Compiler *current = NULL;
ClassCompiler *currentClass = NULL;
Chunk *compilingChunk;

static Chunk* currentChunk() {
    return &current->function->chunk;
}

/*
 * Here's where the actual work happens.
 * First, we print where the error occurred. We try to show the lexeme if it's human-readable.
 * Then we print the error message itself. After that, we set this hadError flag.
 * That records whether any errors occurred during compilation.
 *
 * We want to avoid error cascades. If the user has a mistake in their code and the parser gets confused about where it is in hte grammar,
 * we don't want it to spew out a whole pile of meaningless knock-on errors after the first one.
 *
 * We actually don't fix that in pfer (:c)
 *
 * We add a flag to track whether we're currently in panic mode. When an error occurs, we set it.
 * After that, we go ahead and keep compiling as normal as if the error never occurred. The bytecode will never get executed,
 * so it's harmless to keep on compiling. The trick is that while the panic mode flag is set, we simply suppress any other errors that get detected.
 *
 * There's a good chance the parser will go off in the weeds, but the user won't know because the errors all get swallowed.
 * Panic mode ends when the parser reaches a synchronization point. For Fer, we chose statement boundaries, so when we later add those to our compiler,
 * we'll clear the flag there.
 */

static void errorAt(Token *token, const char *message) {
    if (parser.panicMode) return;
    parser.panicMode = true;
    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char *message) {
    errorAt(&parser.previous, message);
}

/*
 * If the scanner hands us an error token, we need to actually tell the user.
 * That happens using the following function.
 *
 * We pull the location out of the current token in order to tell the user where the error occurred and forward it to errorAt().
 * More often we'll report an error at the location of the token we just consumed, so we give the shorter name "error" to other function.
 */

static void errorAtCurrent(const char *message) {
    errorAt(&parser.current, message);
}

/*
 * Just like in pfer, it steps forward through the token stream. It asks the scanner for the next token and stores it for later use.
 * Before doing that, it takes the old current token and stashes that in a previous field.
 * That will come in handy later so that we can get at the lexeme after we match a token.
 *
 * The code to read the next token is wrapped in a loop. Remember cfer's scanner doesn't report lexical error.
 * Instead, it creates special error tokens and leaves it up to the parser to report them. We do that here.
 *
 * We keep looping, reading tokens and reporting the errors, until we hit a non-error one or reach the end.
 * That way, the rest of the parser sees only valid tokens
 */

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
        if (parser.current.type != TOKEN_ERROR) break;

        errorAtCurrent(parser.current.start);
    }
}

/*
 * This following function is similar to advance() in that ir reads the next token.
 * But it also validates that the token has an expected type. If not, it reports an error.
 * This function is the foundation of most syntax errors in the compiler.
 */

static void consume(TokenType type, const char *message) {
    if (parser.current.type == type) {
        advance();
        return;
    }

    errorAtCurrent(message);
}

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

/*
 * After we parse and understand a piece of the user's program, the next step is to translate that to a series of bytecode instructions.
 * It starts with the easiest possible step: appending a single byte to the chunk.
 */

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t instruction) {
    emitByte(instruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2; // Remember that count points to the next empty space after the second 0xff
}

static void emitReturn() {
    if (current->type == TYPE_INITIALIZER) {
        emitBytes(OP_GET_LOCAL, 0);
    } else {
        emitByte(OP_NIL);
    }

    emitByte(OP_RETURN);
}

static uint8_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint8_t)constant;
}

static void emitConstant(Value value) {
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler *compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->localCount = 0;
    compiler->scopeDepth = 0;
    compiler->loop = NULL;
    compiler->function = newFunction();
    current = compiler;
    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start, parser.previous.length);
    }

    Local *local = &current->locals[current->localCount++];
    local->depth = 0;
    local->isCaptured = false;
    if (type != TYPE_FUNCTION) {
        local->name.start = "this";
        local->name.length = 4;
    } else {
        local->name.start = "";
        local->name.length = 0;
    }
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction *function = current->function;
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), function->name != NULL ? function->name->chars : "<script>");
    }
#endif
    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void endScope() {
    current->scopeDepth--;

    while (current->localCount > 0 && current->locals[current->localCount - 1].depth > current->scopeDepth) {
        if (current->locals[current->localCount - 1].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }

        current->localCount--;
    }
}

static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

static uint8_t identifierConstant(Token *name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token *a, Token *b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler *compiler, Token *name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local *local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Can't read local variable in its own initializer.");
            }
            return i;
        }
    }

    return -1;
}

static void discardLocals() {
    int i = current->localCount - 1;

    while (i >= 0 && current->locals[i].depth > current->loop->scopeDepth) {
        if (current->locals[i].isCaptured) {
            emitByte(OP_CLOSE_UPVALUE);
        } else {
            emitByte(OP_POP);
        }
        i--;
    }
}

static int addUpvalue(Compiler *compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue *upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler *compiler, Token *name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name, bool isPerm) {
    if (current->localCount == UINT8_COUNT) {
        error("Too many local variables in function.");
        return;
    }

    Local *local = &current->locals[current->localCount++];
    local->name = name;
    local->depth = -1;
    local->isCaptured = false;
    local->isPerm = isPerm;
}

static void declareVariable(bool isPerm) {
    if (current->scopeDepth == 0) return;

    Token *name = &parser.previous;
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local *local = &current->locals[i];
        if (local->depth != 1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Already a variable with this name in this scope.");
        }
    }

    addLocal(*name, isPerm);
}

static uint8_t parseVariable(const char *errorMessage, bool isPerm) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable(isPerm);
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint8_t global, bool isPerm) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }

    if (isPerm) {
        emitBytes(OP_DEFINE_GLOBAL_PERM, global);
    } else {
        emitBytes(OP_DEFINE_GLOBAL, global);
    }
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();
            if (argCount == 255) {
                error("Can't have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

/*
 * Binary operators are different because they are infix. With the other expressions, we know what we are parsing from the very first token.
 * With infix expressions we don't know we're in the middle of a binary operator until after we've parsed its left operand and then stumbled onto
 * the operator token in the middle.
 *
 * Here's an example:
 * 1 + 2
 * Let's walk through trying to compile it with what we know so far:
 * 1. We call expression(). That in turn calls parsePrecedence(PREC_ASSIGNMENT).
 * 2. That function sees the leading number token and recognizes it is parsing a number literal. It hands off control to number().
 * 3. number() creates a constant, emits an OP_CONSTANT, and returns back to parsePrecedence()
 *
 * Now what? The call to parsePrecedence() should consume the entire addition expression, so it needs to keep going somehow.
 * Fortunately, the parser is right where we need it to be. Now that we've compiled the leading number expression, the next token is +.
 * That's the exact token that parsePrecedence() needs to detect that we're in the middle of an infix expression and to realize that the expression we already
 * compiled is actually an operand to that.
 *
 * So this hypothetical array of function pointers doesn't just list functions to parse expressions that start with a given token.
 * Instead, it's a table of function pointers. One column associates prefix parser functions with token types.
 * The second column associates infix parser functions with token types.
 *
 * The function we will use as the infix parser for TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, and TOKEN_SLASH is the following.
 *
 * When a prefix parser function is called, the leading token has already been consumed. An infix parser function is even more in medias res,
 * the entire left-hand operand expression has already been compiled and the subsequent infix operator consumed.
 *
 * The fact that the left operand gets compiled first works out fine. It means at runtime, that code gets executed first.
 * When it runs, the value it produces will end up on the stack. That's right where the infix operator is going to need it.
 *
 * Then we come here to binary() to handle the rest of the arithmetic operators. This function compiles the right operand,
 * much like how unary() compiles its own trailing operand. Finally, it emits the bytecode instruction that performs the binary operation.
 *
 * When run, the VM will execute the left and right operand code, in that order,
 * leaving their values on the stack. Then it executes the instruction for the operator. That pops the two values, computes the operation,
 * and pushes the result.
 *
 * When we parse the right-hand operand, we again need to worry about precedence. Take an expression like:
 * 2 * 3 + 4
 * When we parse the right operand of the * expression, we need to just capture 3, and not 3 + 4, because + is lower precedence than *.
 * We could define a separate function for each binary operator. Each would call parsePrecedence and pass in the correct precedence level for its operand.
 *
 * But that's kind of tedious. Each binary operator's right hand operand precedence is one level higher than its own.
 * We can look that up dynamically with this getRule(). Using that, we call parsePrecedence() with one level higher than this operator's level.
 *
 * This way, we can use a single binary() function for all binary operators even though they have different precedences.
 *
 *
 * We use one higher level of precedence for the right operand because the binary operators are left-associative.
 * Given a series of the same operator, like:
 * 1 + 2 + 3 + 4
 * We want to parse it like:
 * ((1 + 2) + 3) + 4
 * Thus, when parsing the right-hand operand to the first +, we want to consume the 2, but not the rest, so we use on level above +'s precedence.
 * But if our operator was right-associative, this would be wrong.
 * Given:
 * a = b = c = d
 * Since assignment is right-associative, we want to parse it as:
 * a = (b = (c = d))
 * To enable that, we would call parsePrecedence() with the same precedence as the current operator.
 * We don't need to track the precedence of the prefix expression starting with a given token because all prefix operators in Fer have the same precedence.
 */

static void binary(bool canAssign) {
    TokenType operatorType = parser.previous.type;
    ParseRule *rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    switch (operatorType) {
        case TOKEN_BANG_EQUAL:      emitBytes(OP_EQUAL, OP_NOT); break;
        case TOKEN_EQUAL_EQUAL:     emitByte(OP_EQUAL); break;
        case TOKEN_GREATER:         emitByte(OP_GREATER); break;
        case TOKEN_GREATER_EQUAL:   emitBytes(OP_LESS, OP_NOT); break;
        case TOKEN_LESS:            emitByte(OP_LESS); break;
        case TOKEN_LESS_EQUAL:      emitBytes(OP_GREATER, OP_NOT); break;
        case TOKEN_PLUS:            emitByte(OP_ADD); break;
        case TOKEN_MINUS:           emitByte(OP_SUBTRACT); break;
        case TOKEN_STAR:            emitByte(OP_MULTIPLY); break;
        case TOKEN_SLASH:           emitByte(OP_DIVIDE); break;
        default: return; // Unreachable
    }
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect property name after '.'");
    uint8_t name = identifierConstant(&parser.previous);

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitBytes(OP_SET_PROPERTY, name);
    } else if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        emitBytes(OP_INVOKE, name);
        emitByte(argCount);
    } else {
        emitBytes(OP_GET_PROPERTY, name);
    }
}

/*
 * Since parsePrecedence() has already consumed the keyword token, all we need to do is output the proper instruction.
 * We figure that out based on the type of token we parsed.
 */

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default: return; // Unreachable
    }
}

/*
 * Many expressions start with a particular token. We call these prefix expressions. For example,
 * when we're parsing an expression and the current token is (, we know we must be looking at a parenthesized grouping expression.
 *
 * It turns out our function pointer array handles those too. The parsing function for an expression type can consume any additional tokens that it wants to,
 * just like in a regular recursive descent parser.
 *
 * Again, we assume the initial ( has already been consumed. We recursively call back into expression() to compile the expression between the parentheses,
 * then parse the closing ) at the end.
 */

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

/*
 * To compile number literals, we store a pointer to the following function at the TOKEN_NUMBER index in the array.
 * We assume the token for the number literal has already been consumed and is stored in previous. We take that lexeme and use the C standard library
 * to convert it to a double value. Then we generate the code to load that value using emitConstant().
 */

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

/*
 * This takes the string's characters directly from the lexeme.
 * The + 1 and - 2 parts trim the leading and trailing quotation marks. It then creates a string object,
 * wraps it in a Value, and stuffs it into the constant table.
 */

static void string(bool canAssign) {
    emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void list(bool canAssign) {
    uint8_t listCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET)) {
        do {
            expression();
            if (listCount == 255) {
                error("Can't have more than 255 elements in one list");
            }
            listCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list.");
    emitBytes(OP_LIST, listCount);
}

static void namedVariable(Token name, bool canAssign) {
    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else {
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        if (arg != -1 && current->locals[arg].isPerm) {
            error("Can't reassign to permanent variable");
        }

        expression();
        emitBytes(setOp, (uint8_t)arg);
    } else {
        emitBytes(getOp, (uint8_t)arg);
    }
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static Token syntheticToken(const char *text) {
    Token token;
    token.start = text;
    token.length = (int)strlen(text);
    return token;
}

static void super_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'super' outside of a class.");
    } else if (!currentClass->hasSuperclass) {
        error("Can't use 'super' in a class with no superclass.");
    }

    consume(TOKEN_DOT, "Expect '.' after 'super'.");
    consume(TOKEN_IDENTIFIER, "Expect superclass method name.");
    uint8_t name = identifierConstant(&parser.previous);

    namedVariable(syntheticToken("this"), false);
    if (match(TOKEN_LEFT_PAREN)) {
        uint8_t argCount = argumentList();
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_SUPER_INVOKE, name);
        emitByte(argCount);
    } else {
        namedVariable(syntheticToken("super"), false);
        emitBytes(OP_GET_SUPER, name);
    }
}


static void this_(bool canAssign) {
    if (currentClass == NULL) {
        error("Can't use 'this' outside of a class.");
        return;
    }

    variable(false);
}

static void at_(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitByte(OP_SET_ITEM);
    } else {
        emitByte(OP_GET_ITEM);
    }
}

static void map(bool canAssign) {
    uint8_t itemCount = 0;
    if (!check(TOKEN_RIGHT_BRACE)) {
        do {
            expression();
            consume(TOKEN_COLON, "Expect ':' key.");
            expression();

            if (itemCount == 255) {
                error("Can't have more than 255 elements in dictionary");
            }
            itemCount++;
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after dictionary.");
    emitBytes(OP_DICTIONARY, itemCount);
}

/*
 * The leading - token has been consumed and is sitting in parser.previous.
 * We grab the token type form that to note which unary operator we're dealing with. It's unnecessary right now, but this will make more sense when we
 * use this same function to compile the ! operator.
 *
 * As in grouping(), we recursively call expression() to compile the operand. After that,
 * we emit the bytecode to perform the negation. It might seem a little weird to write the negate function after its operand's bytecode
 * since the - appears on the left, but think about it in order of execution:
 * 1. We evaluate the operand first which leaves its value on the stack.
 * 2. Then we pop that value to negate it and push the result.
 *
 * So the OP_NEGATE instruction should be emitted last. This is part of the compiler's job, parsing the program in the order it appears in the source code
 * and rearranging it into the order that execution happens.
 *
 * There is one problem with this code, though. The expression() function it calls will parse any expression for the operand regardless of precedence.
 * Once we add binary operators and other syntax, that will do the wrong thing. Consider:
 * -a.b + c;
 * Here the operand to - should be just the a.b expression, not the entire a.b + c. But if unary() calls expression(),
 * the latter will happily chew through
 * all the remaining code including the +. It will erroneously treat the - as lower precedence than the +.
 *
 * When parsing the operand to unary -, we need to compile only expressions at a certain precedence level or higher.
 */

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    parsePrecedence(PREC_UNARY);

    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default: return; // Unreachable
    }
}

/*
 * We can see how grouping, and unary are slotted into the prefix parser column for their respective token types.
 * In the next column, binary is wired up to the four arithmetic infix operators. Those infix operators also have their precedences set in the last column.
 *
 * Aside from those, the rest of the table is full of NULL and PREC_NONE.
 * Most of those empty cells are because there is no expression associated with those tokens. You can't start an expression with,
 * say, else, and } would make for a pretty confusing infix operator.
 *
 * But, also, we haven't filled in the entire grammar yet. As we add new expression types, some of these slots will get functions in them.
 *
 * Now that we have the table, we are finally ready to write the code that uses it. This is where Pratt parser comes to life.
 */

ParseRule rules[] = {
    [TOKEN_LEFT_PAREN]      = {grouping, call,          PREC_CALL},
    [TOKEN_RIGHT_PAREN]     = {NULL,     NULL,          PREC_NONE},
    [TOKEN_LEFT_BRACKET]    = {list,     at_,           PREC_CALL},
    [TOKEN_RIGHT_BRACKET]   = {NULL,     NULL,          PREC_NONE},
    [TOKEN_LEFT_BRACE]      = {map,      NULL,          PREC_NONE},
    [TOKEN_RIGHT_BRACE]     = {NULL,     NULL,          PREC_NONE},
    [TOKEN_COMMA]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_DOT]             = {NULL,     dot,           PREC_CALL},
    [TOKEN_MINUS]           = {unary,    binary,        PREC_TERM},
    [TOKEN_PLUS]            = {NULL,     binary,        PREC_TERM},
    [TOKEN_SEMICOLON]       = {NULL,     NULL,          PREC_NONE},
    [TOKEN_COLON]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_SLASH]           = {NULL,     binary,        PREC_FACTOR},
    [TOKEN_STAR]            = {NULL,     binary,        PREC_FACTOR},
    [TOKEN_BANG]            = {unary,    NULL,          PREC_NONE},
    [TOKEN_BANG_EQUAL]      = {NULL,     binary,        PREC_EQUALITY},
    [TOKEN_EQUAL]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_EQUAL_EQUAL]     = {NULL,     binary,        PREC_EQUALITY},
    [TOKEN_GREATER]         = {NULL,     binary,        PREC_COMPARISON},
    [TOKEN_GREATER_EQUAL]   = {NULL,     binary,        PREC_COMPARISON},
    [TOKEN_LESS]            = {NULL,     binary,        PREC_COMPARISON},
    [TOKEN_LESS_EQUAL]      = {NULL,     binary,        PREC_COMPARISON},
    [TOKEN_IDENTIFIER]      = {variable, NULL,          PREC_NONE},
    [TOKEN_STRING]          = {string,   NULL,          PREC_NONE},
    [TOKEN_NUMBER]          = {number,   NULL,          PREC_NONE},
    [TOKEN_AND]             = {NULL,     and_,          PREC_NONE},
    [TOKEN_CLASS]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_ELSE]            = {NULL,     NULL,          PREC_NONE},
    [TOKEN_FALSE]           = {literal,  NULL,          PREC_NONE},
    [TOKEN_FOR]             = {NULL,     NULL,          PREC_NONE},
    [TOKEN_FUN]             = {NULL,     NULL,          PREC_NONE},
    [TOKEN_IF]              = {NULL,     NULL,          PREC_NONE},
    [TOKEN_NIL]             = {literal,  NULL,          PREC_NONE},
    [TOKEN_OR]              = {NULL,     or_,           PREC_NONE},
    [TOKEN_PRINT]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_RETURN]          = {NULL,     NULL,          PREC_NONE},
    [TOKEN_SUPER]           = {super_,   NULL,          PREC_NONE},
    [TOKEN_THIS]            = {this_,    NULL,          PREC_NONE},
    [TOKEN_TRUE]            = {literal,  NULL,          PREC_NONE},
    [TOKEN_VAR]             = {NULL,     NULL,          PREC_NONE},
    [TOKEN_WHILE]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_ERROR]           = {NULL,     NULL,          PREC_NONE},
    [TOKEN_EOF]             = {NULL,     NULL,          PREC_NONE},
};

/*
 * This function starts at the current token and parses any expression at the given precedence level or higher.
 * It will use the table of parsing function pointers.
 *
 * At the beginning of parsePrecedence(), we look up a prefix parser for the current token.
 * The first token is always going to belong to some kind of prefix expression, by definition (Look at expression).
 * It may turn out to be nested as an operand inside one or more infix expressions, but as you read the code from left to right,
 * the first token you hit always belongs to a prefix expression.
 *
 * After parsing that, which may consume more tokens, the prefix expression is done.
 * Now we look for an infix parser for the next token. If we find one,
 * it means the prefix expression we already compiled might be an operand for it.
 * Byt only if the call to parsePrecedence() has a precedence that is low enough to permit that infix operator.
 *
 * If the next token is too low precedence, or isn't an infix operator at all, we're done.
 * We've parsed as much expression as we can. Otherwise, we consume the operator and hand off control to the infix parser we found.
 * It consumes whatever other tokens it needs (usually the right operand) and returns back to parsePrecedence().
 * Then we loop back around and see if the next token is also a valid infix operator that can take the entire preceding expression as its operand.
 * We keep looping like that, crunching through infix operators and their operands until we hit a token that isn't an infix operator or is too low
 * precedence and stop.
 *
 */

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

/*
 * The following function simply returns the rule at the given index.
 * It's called by binary() to look up the precedence of the current operator.
 * This functions exists solely to handle a declaration cycle in the C code.
 * binary() is defined before the rules table so that the table can store a pointer to it.
 * That means the body of binary() cannot access the table directly.
 *
 * Instead, we wrap the lookup in a function. That lets us forward declare getRule() before the definition of binary(),
 * and then define getRule() after the table.
 */

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

/*
 * We map each token type to a different kind of expression.
 * We define a function for each expression that outputs the appropriate bytecode.
 * Then we build an array of function pointers. The indexes in the array correspond to the TokenType enum values,
 * and the function at each index is the code to compile an expression of that token type.
 */

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    consume(TOKEN_LEFT_PAREN, "Expect ')' after function.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Can't have more than 255 parameters");
            }
            uint8_t constant = parseVariable("Expect parameter name", false);
            defineVariable(constant, false);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    ObjFunction *function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
}

static void method() {
    consume(TOKEN_IDENTIFIER, "Expect method name.");
    uint8_t constant = identifierConstant(&parser.previous);

    FunctionType type = TYPE_METHOD;
    if (parser.previous.length == 4 && memcmp(parser.previous.start, "init", 4) == 0) {
        type = TYPE_INITIALIZER;
    }

    function(type);
    emitBytes(OP_METHOD, constant);
}

static void classDeclaration() {
    consume(TOKEN_IDENTIFIER, "Expect class name.");
    Token className = parser.previous;
    uint8_t nameConstant = identifierConstant(&parser.previous);
    declareVariable(false);

    emitBytes(OP_CLASS, nameConstant);
    defineVariable(nameConstant, false);

    ClassCompiler classCompiler;
    classCompiler.hasSuperclass = false;
    classCompiler.enclosing = currentClass;
    currentClass = &classCompiler;

    if (match(TOKEN_LESS)) {
        consume(TOKEN_IDENTIFIER, "Expect superclass name.");
        variable(false);

        if (identifiersEqual(&className, &parser.previous)) {
            error("A class can't inherit from itself");
        }

        beginScope();
        addLocal(syntheticToken("super"), false);
        defineVariable(0, false);

        namedVariable(className, false);
        emitByte(OP_INHERIT);
        classCompiler.hasSuperclass = true;
    }

    namedVariable(className, false);
    consume(TOKEN_LEFT_BRACE, "Expect '{' before class body");
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        method();
    }
    consume(TOKEN_RIGHT_BRACE, "Expect '}' after class body.");
    emitByte(OP_POP);

    if (classCompiler.hasSuperclass) {
        endScope();
    }

    currentClass = currentClass->enclosing;
}

static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.", false);
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global, false);
}

static void varDeclaration() {
    uint8_t global = parseVariable("Expect variable name", false);

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global, false);
}

static void permDeclaration() {
    uint8_t global = parseVariable("Expect variable name.", true);

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        error("Permanent variable must be initialized.");
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
    defineVariable(global, true);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void breakStatement() {
    if (current->loop == NULL) {
        error("Can't use 'break' outside of a loop.");
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");

    discardLocals();

    int jump = emitJump(OP_JUMP);
    Loop *loop = current->loop;
    if (loop->breakCapacity < loop->breakCount + 1) {
        int oldCapacity = loop->breakCapacity;
        loop->breakCapacity = GROW_CAPACITY(oldCapacity);
        loop->breakJumps = GROW_ARRAY(int, loop->breakJumps, oldCapacity, loop->breakCapacity);
    }
    loop->breakJumps[loop->breakCount++] = jump;
}

static void continueStatement() {
    if (current->loop == NULL) {
        error("Can't use 'continue' outside of a loop.");
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    discardLocals();
    emitLoop(current->loop->start);
}

static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // No initializer
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int loopStart = currentChunk()->count;
    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Cleans the stack
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);
        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect ')' after for clauses");

        emitLoop(loopStart);
        loopStart = incrementStart;
        patchJump(bodyJump);
    }

    Loop loop;
    loop.start = loopStart;
    loop.scopeDepth = current->scopeDepth;
    loop.enclosing = current->loop;
    loop.breakJumps = NULL;
    loop.breakCount = 0;
    loop.breakCapacity = 0;
    current->loop = &loop;

    statement();
    emitLoop(loopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP);
    }

    for (int i = 0; i < loop.breakCount; i++) {
        patchJump(loop.breakJumps[i]);
    }
    FREE_ARRAY(int, loop.breakJumps, loop.breakCapacity);
    current->loop = loop.enclosing;

    endScope();
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP); // IF is a statement not an expression, the stack must be in the same state as before
    statement(); // Body of the if statement, the body gets compiled so that OP_JUMP_IF_FALSE knows how many lines it has to jump
    int elseJump = emitJump(OP_JUMP); // If the expression() was true, we jump over the else statement

    patchJump(thenJump); // If the condition was false we jump over all the instructions included the OP_POP
    emitByte(OP_POP); // We emit OP_POP again to clean the false value

    if (match(TOKEN_ELSE)) statement(); // We compiled the body of the else statement if there was one
    patchJump(elseJump); // If there was a body and the condition was false, we're going to execute the body, otherwise this jump doesn't "jump".
}

static void printStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after value.");
    emitByte(OP_PRINT);
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Can't return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        if (current->type == TYPE_INITIALIZER) {
            error("Can't return a value from an initializer.");
        }

        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement() {
    int loopStart = currentChunk()->count;

    Loop loop;
    loop.start = loopStart;
    loop.scopeDepth = current->scopeDepth;
    loop.enclosing = current->loop;
    loop.breakJumps = NULL;
    loop.breakCount = 0;
    loop.breakCapacity = 0;
    current->loop = &loop;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();
    emitLoop(loopStart);

    patchJump(exitJump);
    emitByte(OP_POP);

    for (int i = 0; i < loop.breakCount; i++) {
        patchJump(loop.breakJumps[i]);
    }

    FREE_ARRAY(int, loop.breakJumps, loop.breakCapacity);
    current->loop = loop.enclosing;
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;
        switch (parser.current.type) {
            case TOKEN_CLASS:
            case TOKEN_FUN:
            case TOKEN_VAR:
            case TOKEN_FOR:
            case TOKEN_IF:
            case TOKEN_WHILE:
            case TOKEN_PRINT:
            case TOKEN_RETURN:
                return;
            default:
                ; // Do nothing
        }

        advance();
    }
}

static void declaration() {
    if (match(TOKEN_CLASS)) {
      classDeclaration();
    } else if (match(TOKEN_FUN)) {
      funDeclaration();
    } else if (match(TOKEN_VAR)) {
        varDeclaration();
    } else if (match(TOKEN_PERM)) {
        permDeclaration();
    } else {
        statement();
    }
    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_PRINT)) {
        printStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_BREAK)) {
        breakStatement();
    } else if (match(TOKEN_CONTINUE)) {
        continueStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else {
        expressionStatement();
    }
}


/*
 * In pfer, when the code ran, the scanner raced ahead and eagerly scanned the whole program, returning a list of tokens.
 * This would be a challenge here. We'd need some sort of growable array or list to store the tokens in.
 * We'd need to manage allocating and freeing the tokens, and the collection itself.
 *
 * At any point in time, the compiler needs only one or two tokens, we don't need to keep them all around at the same time.
 * Instead, the simplest solution is to not scan a token until the compiler needs one. When te scanner provides one,
 * it returns the token by value. It doesn't need to dynamically allocate anything, it can just pass tokens around on the C stack.
 */

ObjFunction* compile(const char *source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);
    parser.hadError = false;
    parser.panicMode = false;
    advance();
    while (!match(TOKEN_EOF)) {
        declaration();
    }
    ObjFunction *function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler *compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}