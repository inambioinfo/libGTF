#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "murmur3.h"
#include "gtf.h"

//These are just modified hashTable methods, since cntTableElements have a cnt field.
cntTable *initCntTable(uint64_t size) {
    hashTable *ht = initHT(size);
    assert(ht);
    cntTable *ct = calloc(1, sizeof(cntTable));
    assert(ct);
    ct->ht = ht;
    ct->cnts = calloc(1, sizeof(uint32_t));
    assert(ct->cnts);
    return ct;
}

void destroyCntTable(cntTable *ct) {
    free(ct->cnts);
    destroyHT(ct->ht);
    free(ct);
}

void initCnts(cntTable *ct) {
    if(ct->ht->l) {
        ct->cnts = calloc(ct->ht->l, sizeof(uint32_t));
        assert(ct->cnts);
    }
}

//There's no way to distinguish between "doesn't exist" and "has no counts"!
uint32_t str2cnt(cntTable *ct, char *s) {
    if(!s) return -1;
    uint64_t h = hashString(s);
    hashTableElement *curr = ct->ht->elements[h%ct->ht->m];
    while(curr) {
        if(strcmp(ct->ht->str[curr->val], s) == 0) return ct->cnts[curr->val];
        curr = curr->next;
    }
    return 0;
}

void incCntTable(cntTable *ct, char *s) {
    assert(strExistsHT(ct->ht, s));
    int32_t val = str2valHT(ct->ht, s);
    assert(val >= 0);
    ct->cnts[val]++;
}

void nodes2cnt(cntTable *ct, GTFnode *n, hashTable *ht, int32_t key) {
    int i;
    GTFentry *e = n->starts;

    while(e) {
        for(i=0; i<e->nAttributes; i++) {
            if(e->attrib[i]->key == key) {
                if(!strExistsHT(ct->ht, val2strHT(ht, e->attrib[i]->val))) {
                    addHTelement(ct->ht, val2strHT(ht, e->attrib[i]->val)); //ignore the return value
                }
                break;
            }
        }
        e = e->right;
    }
    if(n->right) nodes2cnt(ct, n->right, ht, key);
    if(n->left) nodes2cnt(ct, n->left, ht, key);
}

//Hmm, in hindsight, perhaps the hashTable structure should have held key:value pairs...
cntTable *makeCntTable(GTFtree *t, hashTable *ht, char *name) {
    uint64_t i;
    int32_t val = str2valHT(ht, name);
    cntTable *ct = NULL;

    if(val<0) { //No such name in the hash table!
        return ct;
    }
    ct = initCntTable(100);

    for(i=0; i<t->n_targets; i++) {
        nodes2cnt(ct, (GTFnode*) t->chroms[i]->tree, ht, val);
    }
    initCnts(ct);

    return ct;
}
