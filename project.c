#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#define MAX_TOKEN_LEN 100

typedef enum {
    TOKEN_INT, TOKEN_IDENTIFIER, TOKEN_NUMBER, TOKEN_ASSIGN,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_IF, TOKEN_EQUAL, 
    TOKEN_LBRACE, TOKEN_RBRACE, TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_SEMICOLON, TOKEN_UNKNOWN, TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char text[MAX_TOKEN_LEN];
} Token;

void getNextToken(FILE *file, Token *token) {
    int c;
    while ((c = fgetc(file)) != EOF) {
        if (isspace(c)) continue;
        
        if (isalpha(c)) {
            int len = 0;
            token->text[len++] = c;
            while (isalnum(c = fgetc(file))) {
                if (len < MAX_TOKEN_LEN - 1) token->text[len++] = c;
            }
            ungetc(c, file);
            token->text[len] = '\0';
            
            if (strcmp(token->text, "int") == 0) token->type = TOKEN_INT;
            else if (strcmp(token->text, "if") == 0) token->type = TOKEN_IF;
            else token->type = TOKEN_IDENTIFIER;
            return;
        }
        
        if (isdigit(c)) {
            int len = 0;
            token->text[len++] = c;
            while (isdigit(c = fgetc(file))) {
                if (len < MAX_TOKEN_LEN - 1) token->text[len++] = c;
            }
            ungetc(c, file);
            token->text[len] = '\0';
            token->type = TOKEN_NUMBER;
            return;
        }
        
        switch (c) {
            case '=': 
                c = fgetc(file);
                if (c == '=') {
                    token->type = TOKEN_EQUAL;
                    strcpy(token->text, "==");
                } else {
                    ungetc(c, file);
                    token->type = TOKEN_ASSIGN;
                    strcpy(token->text, "=");
                }
                return;
            case '+': token->type = TOKEN_PLUS; strcpy(token->text, "+"); return;
            case '-': token->type = TOKEN_MINUS; strcpy(token->text, "-"); return;
            case '{': token->type = TOKEN_LBRACE; strcpy(token->text, "{"); return;
            case '}': token->type = TOKEN_RBRACE; strcpy(token->text, "}"); return;
            case '(': token->type = TOKEN_LPAREN; strcpy(token->text, "("); return;
            case ')': token->type = TOKEN_RPAREN; strcpy(token->text, ")"); return;
            case ';': token->type = TOKEN_SEMICOLON; strcpy(token->text, ";"); return;
        }
    }
    token->type = TOKEN_EOF;
    token->text[0] = '\0';
}

typedef enum {
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_BIN_OP,
    NODE_NUM,
    NODE_VAR,
    NODE_IF
} NodeType;

typedef struct ASTNode {
    NodeType type;
    struct ASTNode *left;
    struct ASTNode *right;
    char value[MAX_TOKEN_LEN];
} ASTNode;

ASTNode* parseExpression(FILE *file, Token *token);
ASTNode* parseStatement(FILE *file, Token *token);

ASTNode* createNode(NodeType type, const char *value) {
    ASTNode *node = (ASTNode*)malloc(sizeof(ASTNode));
    node->type = type;
    node->left = node->right = NULL;
    if (value) strcpy(node->value, value);
    return node;
}

ASTNode* parseFactor(FILE *file, Token *token) {
    if (token->type == TOKEN_NUMBER) {
        ASTNode *node = createNode(NODE_NUM, token->text);
        getNextToken(file, token);
        return node;
    } else if (token->type == TOKEN_IDENTIFIER) {
        ASTNode *node = createNode(NODE_VAR, token->text);
        getNextToken(file, token);
        return node;
    } else if (token->type == TOKEN_LPAREN) {
        getNextToken(file, token);
        ASTNode *node = parseExpression(file, token);
        if (token->type != TOKEN_RPAREN) {
            fprintf(stderr, "Expected ')'\n");
            exit(1);
        }
        getNextToken(file, token);
        return node;
    } else {
        fprintf(stderr, "Unexpected token: %s\n", token->text);
        exit(1);
    }
}

ASTNode* parseTerm(FILE *file, Token *token) {
    ASTNode *node = parseFactor(file, token);
    
    while (token->type == TOKEN_PLUS || token->type == TOKEN_MINUS) {
        char op[MAX_TOKEN_LEN];
        strcpy(op, token->text);
        getNextToken(file, token);
        
        ASTNode *newNode = createNode(NODE_BIN_OP, op);
        newNode->left = node;
        newNode->right = parseFactor(file, token);
        node = newNode;
    }
    
    return node;
}

ASTNode* parseExpression(FILE *file, Token *token) {
    return parseTerm(file, token);
}

ASTNode* parseVarDecl(FILE *file, Token *token) {
    if (token->type != TOKEN_IDENTIFIER) {
        fprintf(stderr, "Expected identifier after 'int'\n");
        exit(1);
    }
    
    ASTNode *node = createNode(NODE_VAR_DECL, token->text);
    getNextToken(file, token);
    
    if (token->type != TOKEN_SEMICOLON) {
        fprintf(stderr, "Expected ';' after variable declaration\n");
        exit(1);
    }
    getNextToken(file, token);
    
    return node;
}

ASTNode* parseIf(FILE *file, Token *token) {
    if (token->type != TOKEN_LPAREN) {
        fprintf(stderr, "Expected '(' after 'if'\n");
        exit(1);
    }
    getNextToken(file, token);
    
    ASTNode *cond = parseExpression(file, token);
    
    if (token->type != TOKEN_RPAREN) {
        fprintf(stderr, "Expected ')' after if condition\n");
        exit(1);
    }
    getNextToken(file, token);
    
    if (token->type != TOKEN_LBRACE) {
        fprintf(stderr, "Expected '{' after if condition\n");
        exit(1);
    }
    getNextToken(file, token);
    
    ASTNode *body = parseStatement(file, token);
    
    if (token->type != TOKEN_RBRACE) {
        fprintf(stderr, "Expected '}' after if body\n");
        exit(1);
    }
    getNextToken(file, token);
    
    ASTNode *node = createNode(NODE_IF, NULL);
    node->left = cond;
    node->right = body;
    
    return node;
}

ASTNode* parseAssign(FILE *file, Token *token) {
    char varName[MAX_TOKEN_LEN];
    strcpy(varName, token->text);
    
    getNextToken(file, token);
    if (token->type != TOKEN_ASSIGN) {
        fprintf(stderr, "Expected '=' in assignment\n");
        exit(1);
    }
    getNextToken(file, token);
    
    ASTNode *expr = parseExpression(file, token);
    
    if (token->type != TOKEN_SEMICOLON) {
        fprintf(stderr, "Expected ';' after assignment\n");
        exit(1);
    }
    getNextToken(file, token);
    
    ASTNode *node = createNode(NODE_ASSIGN, varName);
    node->left = expr;
    
    return node;
}

ASTNode* parseStatement(FILE *file, Token *token) {
    if (token->type == TOKEN_INT) {
        getNextToken(file, token);
        return parseVarDecl(file, token);
    } else if (token->type == TOKEN_IF) {
        getNextToken(file, token);
        return parseIf(file, token);
    } else if (token->type == TOKEN_IDENTIFIER) {
        return parseAssign(file, token);
    } else {
        fprintf(stderr, "Unexpected token: %s\n", token->text);
        exit(1);
    }
}

ASTNode* parseProgram(FILE *file) {
    Token token;
    getNextToken(file, &token);
    
    ASTNode *program = NULL;
    ASTNode *lastStmt = NULL;
    
    while (token.type != TOKEN_EOF) {
        ASTNode *stmt = parseStatement(file, &token);
        
        if (!program) {
            program = stmt;
            lastStmt = stmt;
        } else {
            // Link statements together (using right pointer for simplicity)
            lastStmt->right = stmt;
            lastStmt = stmt;
        }
    }
    
    return program;
}

typedef struct {
    char name[MAX_TOKEN_LEN];
    int address;
} Symbol;

Symbol symbolTable[100];
int symbolCount = 0;
int nextAddress = 0;

int findOrAddSymbol(const char *name) {
    for (int i = 0; i < symbolCount; i++) {
        if (strcmp(symbolTable[i].name, name) == 0) {
            return symbolTable[i].address;
        }
    }
    
    if (symbolCount >= 100) {
        fprintf(stderr, "Symbol table overflow\n");
        exit(1);
    }
    
    strcpy(symbolTable[symbolCount].name, name);
    symbolTable[symbolCount].address = nextAddress++;
    return symbolTable[symbolCount++].address;
}

void generateExpression(ASTNode *node, FILE *out) {
    switch (node->type) {
        case NODE_NUM:
            fprintf(out, "LOADI %s\n", node->value);
            break;
        case NODE_VAR: {
            int addr = findOrAddSymbol(node->value);
            fprintf(out, "LOAD %d\n", addr);
            break;
        }
        case NODE_BIN_OP:
            generateExpression(node->left, out);
            fprintf(out, "PUSH\n");
            generateExpression(node->right, out);
            fprintf(out, "POP\n");
            if (strcmp(node->value, "+") == 0) {
                fprintf(out, "ADD\n");
            } else if (strcmp(node->value, "-") == 0) {
                fprintf(out, "SUB\n");
            }
            break;
        default:
            fprintf(stderr, "Unexpected node type in expression\n");
            exit(1);
    }
}

void generateCode(ASTNode *node, FILE *out) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR_DECL:
            // Just add to symbol table
            findOrAddSymbol(node->value);
            break;
        case NODE_ASSIGN: {
            int addr = findOrAddSymbol(node->value);
            generateExpression(node->left, out);
            fprintf(out, "STORE %d\n", addr);
            break;
        }
        case NODE_IF: {
            int labelNum = nextAddress++; // Use address counter for unique labels
            generateExpression(node->left, out);
            fprintf(out, "JUMPZ ELSE_%d\n", labelNum);
            generateCode(node->right, out);
            fprintf(out, "ELSE_%d:\n", labelNum);
            break;
        }
        default:
            fprintf(stderr, "Unexpected node type\n");
            exit(1);
    }
    
    // Generate code for next statement
    generateCode(node->right, out);
}

void generateAssembly(ASTNode *program, const char *outputFilename) {
    FILE *out = fopen(outputFilename, "w");
    if (!out) {
        perror("Failed to open output file");
        exit(1);
    }
    
    generateCode(program, out);
    fclose(out);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }
    
    FILE *input = fopen(argv[1], "r");
    if (!input) {
        perror("Failed to open input file");
        return 1;
    }
    
    ASTNode *program = parseProgram(input);
    fclose(input);
    
    generateAssembly(program, argv[2]);
    
    return 0;
}



