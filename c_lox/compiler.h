//
//  compiler.h
//  c_lox
//
//  Created by Ian Saultz on 8/11/22.
//

#ifndef clox_compiler_h
#define clox_compiler_h

#include <stdio.h>
#include "object.h"
#include "vm.h"

ObjFunction* compile(const char* source);
void markCompilerRoots(void);

#endif /* compiler_h */
