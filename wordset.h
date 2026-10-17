#ifndef WORDSET_H
#define WORDSET_H

void wordset_add(const char *word);
char **wordset_get_matches(const char *prefix, int *count);
void wordset_free_matches(char **matches);

#endif
