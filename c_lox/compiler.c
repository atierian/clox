//
//  compiler.c
//  c_lox
//
//  Created by Ian Saultz on 8/11/22.
//

#include <stdio.h>

#include "compiler.h"
#include "common.h"
#include "scanner.h"

void compile(const char* source) {
    initScanner(source);
    int line = -1;
    for (;;) {
        Token token = scanToken();
        if (token.line != line) {
            printf("%4d ", token.line);
            line = token.line;
        } else {
            printf("   | ");
        }
        printf("%2d '%.*s'\n", token.type,
               token.length, token.start);
        
        if (token.type == TOKEN_EOF) break;
    }
}
