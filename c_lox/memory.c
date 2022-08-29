#include <stdlib.h>

#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#ifdef DEBUG_LOG_GC
#include <stdio.h>
#include "debug.h"
#endif

void* reallocate(void* pointer, size_t oldSize, size_t newSize) {
	if (newSize > oldSize) {
#ifdef DEBUG_STRESS_GC
		collectGarbage();
#endif
	}
	
	if (newSize == 0) {
		free(pointer);
		return NULL;
	}
	
	void* result = realloc(pointer, newSize);
	if (result == NULL) exit(1);
	return result;
}

void markObject(Obj* object) {
	if (object == NULL) return;
	if (object->isMarked) return;
#ifdef DEBUG_LOG_GC
	printf("%p mark ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif
	object->isMarked = true;
	
	if (vm.grayCapacity < vm.grayCount + 1) {
		vm.grayCapacity = GROW_CAPACITY(vm.grayCapacity);
		vm.grayStack = (Obj**)realloc(vm.grayStack, sizeof(Obj*) * vm.grayCapacity);
		
		if (vm.grayStack == NULL) exit(1);
	}
	
	vm.grayStack[vm.grayCount++] = object;
}

void markValue(Value value) {
	if (IS_OBJ(value)) markObject(AS_OBJ(value));
}

static void markArray(ValueArray* array) {
	for (int i = 0; i < array->count; i++) {
		markValue(array->values[i]);
	}
}

static void blackenObject(Obj* object) {
#ifdef DEBUG_LOG_GB
	printf("%p blacken ", (void*)object);
	printValue(OBJ_VAL(object));
	printf("\n");
#endif
	switch (object->type) {
		case OBJ_CLOSURE: {
			ObjClosure* closure = (ObjClosure*)object;
			markObject((Obj*)closure->function);
			for (int i = 0; i < closure->upvalueCount; i++) {
				markObject((Obj*)closure->upvalues[i]);
			}
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			markObject((Obj*)function->name);
			markArray(&function->chunk.constants);
			break;
		}
		case OBJ_UPVALUE:
			markValue(((ObjUpvalue*)object)->closed);
			break;
		case OBJ_NATIVE:
		case OBJ_STRING:
			break;
	}
}

static void freeObject(Obj* object) {
#ifdef DEBUG_LOG_GC
	printf("%p free type %d\n", (void*)object, object->type);
#endif
	
	switch (object->type) {
		case OBJ_CLOSURE: {
			ObjClosure* closure = (ObjClosure*)object;
			FREE_ARRAY(ObjUpvalue*, closure->upvalues,
								 closure->upvalueCount);
			FREE(ObjClosure, object);
			break;
		}
		case OBJ_FUNCTION: {
			ObjFunction* function = (ObjFunction*)object;
			freeChunk(&function->chunk);
			FREE(ObjFunction, object);
			break;
		}
		case OBJ_NATIVE:
			FREE(ObjNative, object);
			break;
		case OBJ_STRING: {
			ObjString* string = (ObjString*)object;
			FREE_ARRAY(char, string->chars, string->length);
			FREE(ObjString, object);
			break;
		}
		case OBJ_UPVALUE:
			FREE(ObjUpvalue, object);
			break;
	}
}

void freeObjects(void) {
	Obj* object = vm.objects;
	while (object != NULL) {
		Obj* next = object->next;
		freeObject(object);
		object = next;
	}
	
	free(vm.grayStack);
}

static void markRoots() {
	for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
		markValue(*slot);
	}
	
	// walk the stack of CallFrames the VM holds to access
	// constants and upvalues.
	for (int i = 0; i < vm.frameCount; i++) {
		markObject((Obj*)vm.frames[i].closure);
	}
	
	// walk the upvalue list that the VM can reach.
	for (ObjUpvalue* upvalue = vm.openUpvalues;
			 upvalue != NULL;
			 upvalue = upvalue->next) {
		markObject((Obj*)upvalue);
	}
	
	markTable(&vm.globals);
	markCompilerRoots();
}

static void traceReferences() {
	while (vm.grayCount > 0) {
		Obj* object = vm.grayStack[--vm.grayCount];
		blackenObject(object);
	}
}

static void sweep() {
	Obj* previous = NULL;
	Obj* object = vm.objects;
	// walks the linked list of every object
	// in the heap.
	while (object != NULL) {
		// if the object is marked black.
		if (object->isMarked) {
			// move on to next object.
			object->isMarked = false;
			previous = object;
			object = object->next;
		} else { // if the object is unmarked (white)
			// unlink it from the list
			Obj* unreached = object;
			object = object->next;
			if (previous != NULL) {
				previous->next = object;
			} else {
				vm.objects = object;
			}
			
			// then free it.
			freeObject(unreached);
		}
	}
}

void collectGarbage() {
#ifdef DEBUG_LOG_GC
	printf("-- gc begin\n");
#endif
	markRoots();
	traceReferences();
	tableRemoveWhite(&vm.strings);
	sweep();
	
#ifdef DEBUG_LOG_GC
	printf("-- gc end\n");
#endif
}
