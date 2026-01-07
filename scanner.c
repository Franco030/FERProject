#include <stdio.h>
#include <string.h>

#include "common.h"
#include "scanner.h"

typedef struct {
    const char *start;
    const char *current;
    int line;
} Scanner;


Scanner scanner;

void initScanner(const char *source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
}

static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') ||
        (c >= 'A' && c <= 'Z') ||
            c == '_';
}

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static bool isAtEnd() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peekNext() {
    if (isAtEnd()) return '\0';
    return scanner.current[1];
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*scanner.current != expected) return false;
    scanner.current++;
    return true;
}

/*
 * This next function uses the scanner's start and current pointers to capture the token's lexeme.
 * It has a sister function for returning error tokens.
 */

static Token makeToken(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

/*
 * This next function is different from makeToken because "lexeme" points to the error message string instead of pointing into the user's source code.
 */

static Token errorToken(const char *message) {
    Token token;
    token.type = TOKEN_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

/*
 * Our scanner needs to handle spaces, tabs and newline, but those characters don't become part of any token's lexeme.
 * We could check for those inside the main character switch in scanToken() but it gets a little tricky to ensure that the function still correctly
 * finds the next token after the whitespace when you call it.
 * Instead, before starting the token, we shunt off to a separate function.
 *
 * This advances the scanner past any leading whitespace.
 * After this call returns, we know that the very next character is a meaningful one (or we're at the end of the source code).
 */

static void skipWhitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peekNext() == '/') {
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

/*
 * We use this for all the unbranching paths in the tree.
 * Once we've found a prefix that could only be one possible reserved word, we need to verify two things.
 * The lexeme must be exactly as long as the keyword. if the first letter is "s", the lexeme could still be "sup" or "superb".
 * And the remaining characters must match exactly.
 *
 * If we do have the right number of characters, and they're the ones we want, then it's a keyword, and we return the associated token type.
 * Otherwise, it must be a normal identifier.
 */

static TokenType checkKeyword(int start, int length, const char *rest, TokenType type) {
    if (scanner.current - scanner.start == start + length && memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TOKEN_IDENTIFIER;
}

/*
 * How should we go abut recognizing keywords? In pfer, we stuffed them all in a python dictionary and look them up by name.
 * We don't have any sort of hash table in cfer yet. And a hast table would be overkill anyway. To look up a string in hash table,
 * we need to walk the string to calculate its hash code, find the corresponding bucket in the hash table,
 * and then do a character-by-character equality comparison on any string it happens to find there.
 * Let's say we've scanned the identifier "grin". How much work should we need to do to tell if that's a reserved word?
 * Well, no Fer keyword starts with "g", so looking at the first character is enough to definitively answer no.
 * That's a lot simpler than a hash table lookup.
 *
 * We end up with a thing called a trie.
 * A trie stores a set of strings. Most other data structures for storing strings contain the raw character arrays and the wrap them inside some larger
 * construct that helps you search faster. A trie is different. Nowhere in the trie will you find a whole string.
 *
 * Instead, each string the trie "contains" is represented as a path through the tree of character nodes, as in our traversal above.
 * Nodes that match the last character in a string.
 *
 * Tries are a special case of an even more fundamental data structure: a deterministic finite automata (DFA).
 * Also known as finite state machine, or just state machine.
 *
 * In a DFA, you have a set of states with transitions between them, forming a graph.
 * At any point in time, the machine is "in" exactly one state. It gets to other states by following transitions.
 * When you use a DFA for lexical analysis, each transition is a character that gets matched from the string.
 * Each state represents a set of allowed characters.
 *
 * Our keyword tree is exactly a DFA that recognizes Fer keywords.
 * But DFAs are more powerful than simple trees because they can be arbitrary graphs.
 * Transitions can form cycles between states. That lets you recognize arbitrarily long strings.
 *
 * However, crafting that DFA by hand would be challenging. That's why Lex was created.
 * You give it a simple textual description of your lexical grammar, a bunch of regular expressions,
 * and it automatically generates a DFA for you and produces a pile of C doe that implements it.
 *
 * We just need a tiny trie for recognizing keywords.
 * The absolute simplest solution is to use a switch statement for each node with cases for each branch.
 * We'll start with the root node and handle that easy keywords.
 *
 * We have a couple of keywords where the tree branches again after the first letter.
 * If the lexeme starts with "f", it could be false, for, or fun. So we add another switch for the branches coming off the "f" node.
 * Before we switch, we need to check that there even is a second letter.
 * "f" by itself is a valid identifier too, after all. The other letter that branches is "t".
 */

static TokenType identifierType() {
    switch (scanner.start[0]) {
        case 'a': return checkKeyword(1, 2, "nd", TOKEN_AND);
        case 'b': return checkKeyword(1, 4, "reak", TOKEN_BREAK);
        case 'c':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'l': return checkKeyword(2, 3, "ass", TOKEN_CLASS);
                    case 'o': return checkKeyword(2, 6, "ntinue", TOKEN_CONTINUE);
                }
            }
        case 'e': return checkKeyword(1, 3, "lse", TOKEN_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return checkKeyword(2, 3, "lse", TOKEN_FALSE);
                    case 'o': return checkKeyword(2, 1, "r", TOKEN_FOR);
                    case 'u': return checkKeyword(2, 1, "n", TOKEN_FUN);
                }
            }
            break;
        case 'i': return checkKeyword(1, 1, "f", TOKEN_IF);
        case 'n': return checkKeyword(1, 2, "il", TOKEN_NIL);
        case 'o': return checkKeyword(1, 1, "r", TOKEN_OR);
        case 'p':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'e': return checkKeyword(2, 2, "rm", TOKEN_PERM);
                    case 'r': return checkKeyword(2, 3, "int", TOKEN_PRINT);
                }
            }
        case 'r': return checkKeyword(1, 5, "eturn", TOKEN_RETURN);
        case 's': return checkKeyword(1, 4, "uper", TOKEN_SUPER);
        case 't':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'h': return checkKeyword(2, 2, "is", TOKEN_THIS);
                    case 'r': return checkKeyword(2, 2, "ue", TOKEN_TRUE);
                }
            }
            break;
        case 'v': return checkKeyword(1, 2, "ar", TOKEN_VAR);
        case 'w': return checkKeyword(1, 4, "hile", TOKEN_WHILE);
    }
    return TOKEN_IDENTIFIER;
}

static Token identifier() {
    while (isAlpha(peek()) || isDigit(peek())) advance();
    return makeToken(identifierType());
}

static Token number() {
    while (isDigit(peek())) advance();

    // Look for fractional part
    if (peek() == '.' && isDigit(peekNext())) {
        // Consume the "."
        advance();

        while (isDigit(peek())) advance();
    }
    return makeToken(TOKEN_NUMBER);
}

static Token string() {
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') scanner.line++;
        if (peek() == '\\') advance();
        advance();
    }

    if (isAtEnd()) return errorToken("Unterminated string.");

    // The closing quote
    advance();
    return makeToken(TOKEN_STRING);
}

/*
 * Since each call to the next function scans a complete token,
 * we know we are at the beginning of a new token when we enter the function.
 * Thus, we set scanner.start to point to the current character so we remember where the lexeme we're about to scan starts
 *
 * Then we check to see if we've reached the end of the source code. If so,
 * we return an EOF token and stop. This is a sentinel value that signals to the compiler to stop asking for more tokens.
 *
 * If we aren't at the end, we do something to scan the next token
 */

Token scanToken() {
    skipWhitespace();
    scanner.start = scanner.current;

    if (isAtEnd()) return makeToken(TOKEN_EOF);

    char c = advance();
    if (isAlpha(c)) return identifier();
    if (isDigit(c)) return number();

    switch (c) {
        case '(': return makeToken(TOKEN_LEFT_PAREN);
        case ')': return makeToken(TOKEN_RIGHT_PAREN);
        case '{': return makeToken(TOKEN_LEFT_BRACE);
        case '}': return makeToken(TOKEN_RIGHT_BRACE);
        case ';': return makeToken(TOKEN_SEMICOLON);
        case ',': return makeToken(TOKEN_COMMA);
        case '.': return makeToken(TOKEN_DOT);
        case '-': return makeToken(TOKEN_MINUS);
        case '+': return makeToken(TOKEN_PLUS);
        case '/': return makeToken(TOKEN_SLASH);
        case '*': return makeToken(TOKEN_STAR);
        case '!':
            return makeToken(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
        case '=':
            return makeToken(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
        case '<':
            return makeToken(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
        case '>':
            return makeToken(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
        case '"': return string();
    }

    return errorToken("Unexpected character.");
}