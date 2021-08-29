//
//   Identifier Module
//
// Functions and storage for parsed identifiers
//
// Created by User on 8/27/2021.
//

#include <stdio.h>      // needed for printf
#include <stdlib.h>     // needed for malloc and free
#include <string.h>     // needed for strcmp

#include "identifiers.h"

//-------------------------------------------------------
static int hashItemCount = 0;
static int hashMemoryUsed = 0;

void *HASH_allocMem(int size) {
    hashMemoryUsed += size;
    return malloc(size);
}

void HASH_freeMem(void *mem) {
    free(mem);
}


//-------------------------------------------------------

typedef struct hashNode {
    struct hashNode *next;
    char *name;
    void *value;
} hashNode;

#define HASH_TABLE_SIZE 101
static struct hashNode *identHT[HASH_TABLE_SIZE];

/**
 * Generate hash value for string using djb2 method
 *
 * LINK:  http://www.cse.yorku.ca/~oz/hash.html
 */
unsigned int hash(char *str)
{
    unsigned long hash = 5381;
    int c;

    while (c = *str++)
        hash = ((hash << 5) + hash) + c; // hash * 33 + c

    return hash % HASH_TABLE_SIZE;
}

struct hashNode * lookup(char *s) {
    struct hashNode *np;
    for (np = identHT[hash(s)]; np != NULL; np = np->next)
        if (strcmp(s, np->name) == 0)
            return np;
    return NULL;
}

char *copyString(const char *src) {
    char *dst = HASH_allocMem(strlen (src) + 1);
    if (dst == NULL) return NULL;
    strcpy(dst, src);
    return dst;
}

struct hashNode * install(char *name, void *value) {
    struct hashNode *np;
    unsigned int hashVal;

    if ((np = lookup(name)) == NULL) {
        np = (struct hashNode *) HASH_allocMem(sizeof(*np));
        np->name = copyString(name);
        np->value = value;
        hashVal = hash(name);
        np->next = identHT[hashVal];
        identHT[hashVal] = np;
        hashItemCount++;
    }
    return np;
}

//---------------------------------------------

char * Ident_lookup(char *lookupName) {
    struct hashNode *np = lookup(lookupName);
    return np ? np->name : 0;
}


char * Ident_add(char *name, void *value) {
    struct hashNode *np = install(name, value);
    return np ? np->name : 0;
}

void printHashTable() {
    struct hashNode *np;
    int maxDepth = 0;
    for (int index = 0; index < HASH_TABLE_SIZE; index++) {
        int depth = 0;
        for (np=identHT[index]; np != NULL; np = np->next) {
            depth++;
            if (depth > maxDepth) maxDepth = depth;
            printf("%-20s  %d\n\t", np->name, np->value);
        }
        printf("\n");
    }
    printf("Number of items in hash table: %d\n", hashItemCount);
    printf("Max depth of hash search: %d\n", maxDepth);
    printf("Hash table memory usage: %d\n", hashMemoryUsed);
}