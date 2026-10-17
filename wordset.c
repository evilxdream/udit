#include "wordset.h"
#include <string.h>
#include <stdlib.h>

#define HASH_SIZE 4096

typedef struct Node {
    char *word;
    struct Node *next;
} Node;

Node *table[HASH_SIZE] = {0};

unsigned long hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
        hash = ((hash << 5) + hash) + c;
    return hash % HASH_SIZE;
}

void wordset_add(const char *word) {
    if (strlen(word) < 3) return;
    
    unsigned long h = hash(word);
    Node *n = table[h];
    
    while (n) {
        if (strcmp(n->word, word) == 0) return;
        n = n->next;
    }
    
    Node *new_node = malloc(sizeof(Node));
    new_node->word = strdup(word);
    new_node->next = table[h];
    table[h] = new_node;
}

char **wordset_get_matches(const char *prefix, int *count) {
    *count = 0;
    if (strlen(prefix) < 3) return NULL;
    
    char **matches = malloc(sizeof(char *) * 100);
    int found = 0;
    
    for (int i = 0; i < HASH_SIZE && found < 100; i++) {
        Node *n = table[i];
        while (n) {
            if (strncmp(n->word, prefix, strlen(prefix)) == 0) {
                matches[found++] = n->word;
            }
            n = n->next;
        }
    }
    *count = found;
    return matches;
}

void wordset_free_matches(char **matches) {
    if (matches) free(matches);
}
