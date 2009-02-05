#include "ono.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>

#ifdef ONO_DEBUG
#include <stdio.h>
#include <netinet/in.h>
#endif

#ifdef ONO_DEBUG
void print_peer(ot_peer* peer) /* for some reason this forced cast is necessary even if what's passed is not an ot_peer struct, just to get it to print correctly */
{
  unsigned short* port = malloc(sizeof(unsigned short));
  memmove(port, ((uint8_t*)peer->data)+4, 2);
  printf("%d.%d.%d.%d:%d\n", 
	 (int) peer->data[0], 
	 (int) peer->data[1], 
	 (int) peer->data[2], 
	 (int) peer->data[3], 
	 ntohs(*port));
  free(port);
}
#endif

da_t *da_create(void* firstdata, size_t firstdata_size)
{
  da_t* da = malloc(sizeof(da_t));
  if (!da)
  {
    return NULL;
  }
  da->n = 0;
  da->size = DA_INITIAL_SIZE;
  da->data = malloc(DA_INITIAL_SIZE*sizeof(void*));
  if (!da->data)
  {
    free(da);
    return NULL;
  }
  unsigned int i;
  for (i = 0; i < da->size; i++) da->data[i] = NULL;

  if (firstdata)
  {
    char* firstdata_copy = malloc(firstdata_size);
    if (!firstdata_copy) 
    {
      free(da);
      free(da->data);
      return NULL;
    }
    memcpy(firstdata_copy, firstdata, firstdata_size);

    da->n = 1;
    da->data[0] = firstdata_copy;
  }

  return da;
}

int da_add(da_t* da, void* data, size_t data_size)
{
  unsigned int i;
  for (i = 0; i < da->size; i++)
    if (da->data[i] && !strcmp(data, da->data[i]))
      return 0; /* don't introduce duplicates */

  if (da->n >= da->size)
  {
    void** olddata = da->data;
    size_t newsize = DA_SIZE_MULTIPLE*da->n;
    da->data = malloc(newsize*sizeof(void*));
    if (!da->data)
    {
      da->data = olddata;
      return -1;
    }
    unsigned int i;
    for (i = 0; i < da->size; i++) da->data[i] = olddata[i]; /* copy everything, even the NULLs */
    for (; i < newsize; i++) da->data[i] = NULL;
    free(olddata);
    da->size = newsize;
  }

  char* data_copy = malloc(data_size);
  if (!data_copy) return -1;
  memmove(data_copy, data, data_size);
  for (i = 0; da->data[i] && i < da->size; i++); /* find first empty slot */
  da->data[i] = data_copy;
  da->n++;

  return 0;
}

int da_delete(da_t* da, void* data)
{
  unsigned int i;
  for (i = 0; i < da->size; i++)
    if (da->data[i])
      if (!strcmp(da->data[i], data))
	break;
  if (i >= da->size) return -1;
  free(da->data[i]);
  da->data[i] = NULL;
  da->n--;
  return 0;
}

int da_findnextnonemptyslot(da_t* da, unsigned int start)
{
  if (da->size < start) return -1;
  unsigned int i;
  for (i = start; i < da->size; i++)
    if (da->data[i])
      break;
  if (i < da->size && da->data[i]) return i; else return -1;
}

int da_findnextemptyslot(da_t* da, unsigned int start)
{
  if (da->size < start) return -1;
  unsigned int i;
  for (i = start; i < da->size; i++)
    if (!da->data[i])
      break;
  if (i < da->size && !da->data[i]) return i; else return -1;
}

int da_pack(da_t* da, int firstn) /* pack first n slots full of data */
{
  int nextnonempty;
  int nextempty;
  for (nextnonempty = 0, nextempty = 0; nextempty < firstn;)
  {
    nextempty = da_findnextemptyslot(da, nextempty);
    if (nextempty == -1) return 0; /* all slots full, mission already accomplished */
    nextnonempty = nextempty+1;
    nextnonempty = da_findnextnonemptyslot(da, nextnonempty);
    if (nextnonempty == -1) /* no more nonempty slots */
    {
      if (nextempty < firstn)
	return nextempty; /* still not enough, return how much we managed to pack */
      else
	return 0;
    }
    da->data[nextempty] = da->data[nextnonempty];
    da->data[nextnonempty] = NULL;
    nextempty++;
    nextnonempty++;
  }
  return 0;
}

int da_eliminate_duplicates(da_t* da)
{
  unsigned int i;
  for (i = 0; i < da->size; i++)
  {
    if (da->data[i])
    {
      unsigned int j;
      for (j = i+1; j < da->size; j++)
      {
	if (da->data[j] && !strcmp(da->data[i], da->data[j]))
	{
	  free(da->data[j]);
	  da->data[j] = NULL;
	  da->n--;
	}
      }
    }
  }
  return 0;
}

int da_destroy(da_t* da)
{
  unsigned int i;
  for (i = 0; i < da->size; i++) if (da->data[i]) free(da->data[i]);
  free(da->data);
  free(da);
  da = NULL;
  return 0;
}

#ifdef ONO_DEBUG
void da_print(da_t* da)
{
  printf("da_print:\nn: %d\nsize: %d\n", da->n, da->size);
  unsigned int i;
  printf("da->data:\n");
  for (i = 0; i < da->size; i++) if (da->data[i]) printf("%s\n", (char*) da->data[i]);
}
#endif

/* -- */

struct innerht_entry
{
  void* key;
  void* data;
};

struct outerht_entry
{
  void* key;
  innerht_t* nr; /* node -> replica da */
  innerht_t* rn; /* replica -> node da */
};

/* -- */

innerht_t* innerht_create()
{
  hi_handle_t* ht;
  if (hi_init_str(&ht, HT_INITIAL_SIZE)) return NULL; /* success returns 0 in libhashish */
  return ht;
}

int my_hi_rehash(hi_handle_t** ht_, size_t new_table_size)
{
  hi_handle_t* ht;
  hi_handle_t* ht_old = *ht_;
  if (hi_init_str(&ht, new_table_size)) return -1;
  hi_iterator_t* i;
  if (hi_iterator_create(ht_old, &i)) 
  {
    hi_fini(ht);
    return -1;
  }
  void* key;
  void* data;
  unsigned int keylen;
  while (!hi_iterator_getnext(i, &data, &key, &keylen)) 
  {
    if (hi_insert(ht, key, keylen, data))
    {
      hi_fini(ht);
      hi_iterator_fini(i);
      return -1;
    }
  }
  hi_iterator_fini(i);
  *ht_ = ht;
  hi_fini(ht_old);
  return 0;
}

int innerht_insert_new_key(innerht_t** ht, char* key, size_t key_size, da_t* da)
{
  size_t nelements = (*ht)->no_objects;
  if (nelements > HT_SIZE_MULTIPLE*(*ht)->table_size) my_hi_rehash(ht, HT_SIZE_MULTIPLE*nelements);

  char* key_copy = malloc(key_size);
  if (!key_copy) return -1;
  memmove(key_copy, key, key_size);

  innerht_entry_t* hte = malloc(sizeof(innerht_entry_t));
  if (!hte)
  {
    free(key_copy);
    return -1;
  }
  hte->key = key_copy;
  hte->data = da;

  if (hi_insert(*ht, key_copy, key_size, hte))
  {
    free(key_copy);
    free(hte);
    return -1;
  }

  return 0;
}

int innerht_insert(innerht_t** ht, char* key, size_t key_size, char* data, size_t data_size)
{
  innerht_entry_t* hte = NULL;
  if (hi_get(*ht, key, key_size, (void*) (void*) &hte)) /* key is not yet in table */
  {
    da_t* da = da_create(data, data_size);
    if (!da) return -1;
    if (innerht_insert_new_key(ht, key, key_size, da))
    {
      da_destroy(da);
      return -1;
    }
    return 0;
  }
  else
    return da_add((da_t*) hte->data, data, data_size);
}

int innerht_delete(innerht_t* ht, char* key, char* data)
{
  innerht_entry_t* hte = NULL;
  if (!data)
  {
    if (hi_remove(ht, key, strlen(key)+1, (void*) (void*) &hte)) return -1;
    free(hte->key);
    da_destroy((da_t*) hte->data);
    free(hte);
    return 0;
  }

  if (hi_get(ht, key, strlen(key)+1, (void*) (void*) &hte)) return -1;
  if (da_delete((da_t*) hte->data, data)) return -1;
  if ((int) ((da_t*) hte->data)->n == 0)
  {
    if (hi_remove(ht, key, strlen(key)+1, (void*) (void*) &hte)) return -1;
    free(hte->key);
    da_destroy((da_t*) hte->data);
    free(hte);
  }
  return 0;
}

int innerht_lookup(innerht_t* ht, char* key, da_t** da)
{
  innerht_entry_t* hte = NULL;
  if (hi_get(ht, key, strlen(key)+1, (void*) (void*) &hte)) return -1;
  *da = hte->data;
  return 0;
}

int innerht_destroy(innerht_t* ht)
{
  hi_iterator_t* i;
  if (hi_iterator_create(ht, &i))
  {
    hi_fini(ht);
    return 0;
  }
  void* key;
  void* data;
  unsigned int keylen;
  while (!hi_iterator_getnext(i, &data, &key, &keylen))
  {
    innerht_entry_t* hte = data;
    free(hte->key);
    da_destroy((da_t*) hte->data);
    free(hte);
  }
  hi_iterator_fini(i);
  hi_fini(ht);
  return 0;
}

#ifdef ONO_DEBUG
void innerht_print(innerht_t* ht)
{
  printf("innerht_print:\ntable_size: %d\nno_objects: %d\n", ht->table_size, ht->no_objects);
  printf("contents:\n");
  hi_iterator_t* i;
  if (hi_iterator_create(ht, &i)) return;
  void* key;
  void* data;
  unsigned int keylen;
  while (!hi_iterator_getnext(i, &data, &key, &keylen))
  {
    printf("%s ->\n", (char*) (((innerht_entry_t*) data)->key));
    da_print((da_t*) (((innerht_entry_t*) data)->data));
  }
  hi_iterator_fini(i);
}
#endif

/* -- */

outerht_t* outerht_create()
{
  hi_handle_t* ht;
  if (hi_init_str(&ht, HT_INITIAL_SIZE)) return NULL;
  return ht;
}

int outerht_insert(outerht_t** ht, char* name, char* key, char* data, char* which) /* which = "nr" or "rn" */
{
  if (strcmp(which, "nr") && strcmp(which, "rn")) return -1;

  outerht_entry_t* hte = NULL;
  if (!hi_get(*ht, name, strlen(name)+1, (void*) (void*) &hte)) /* name already exists in table */
  {
    if (!strcmp(which, "nr")) return innerht_insert((innerht_t**) &(hte->nr), key, 7, data, strlen(data)+1);
    if (!strcmp(which, "rn")) return innerht_insert((innerht_t**) &(hte->rn), key, strlen(key)+1, data, 7);
  }

  hte = malloc(sizeof(outerht_t));
  if (!hte) return -1;
  size_t name_size = strlen(name)+1;
  char *name_copy = malloc(name_size);
  if (!name_copy)
  {
    free(hte);
    return -1;
  }
  memmove(name_copy, name, name_size);
  hte->key = name_copy;
  hte->nr = innerht_create();
  if (!hte->nr)
  {
    free(name_copy);
    free(hte);
    return -1;
  }
  hte->rn = innerht_create();
  if (!hte->rn)
  {
    innerht_destroy(hte->nr);
    free(name_copy);
    free(hte);
    return -1;
  }
  if (!strcmp(which, "nr"))
  {
    if (innerht_insert((innerht_t**) &hte->nr, key, 7, data, strlen(data)+1))
    {
      innerht_destroy(hte->nr);
      innerht_destroy(hte->rn);
      free(name_copy);
      free(hte);
      return -1;
    }
  }
  if (!strcmp(which, "rn"))
  {
    if (innerht_insert((innerht_t**) &hte->rn, key, strlen(key)+1, data, 7))
    {
      innerht_destroy(hte->nr);
      innerht_destroy(hte->rn);
      free(name_copy);
      free(hte);
      return -1;
    }
  }
  if (hi_insert(*ht, name_copy, name_size, hte))
  {
    innerht_destroy(hte->nr);
      innerht_destroy(hte->rn);
      free(name_copy);
      free(hte);
      return -1;
  }
  return 0;
}

inline long myrandom(long a, long b)
{
  return (random()%(b-a+1))+a;
}

int random_sample(da_t* a, da_t* b, unsigned int k) /* random sample of a of k members from rn */
{
  if (a->n < k) k = a->n;
  unsigned int t = 0; 
  unsigned int m = 0;
  while (m < k)
  {
    if (((unsigned int) myrandom(0, a->n-t-1)) < k-m)
    {
      if (da_add(b, a->data[t], 7)) return -1; /* ot_peer format is 6 long */
      m++;
      t++;
    }
    else
      t++;
  }
  return 0;
}

double cosine_similarity(da_t* a, da_t* b) /* not terribly efficient */
{
  if (a == b)
  {
#ifdef ONO_DEBUG
    printf("cosine_similarity: a == b, returning 0...\n");
#endif
    return 0; /* 0 instead of 1 to prevent returning self as peer */
  }

  da_t* temp;
  if (a->n > b->n)
  {
    temp = a;
    a = b;
    b = temp;
  }
  double numerator = 0;
  double aratiosquaresum = 0;
  double bratiosquaresum = 0;
  unsigned int i;
  for (i = 0; i < a->size; i++)
  {
    if (a->data[i])
    {
      char* acolon = strchr(a->data[i], ':');
      *acolon = '\0';
      double aratio = strtod(acolon+1, NULL);
#ifdef ONO_DEBUG
      printf("cosine_similarity: a->data[i]: %s\n", (char*) a->data[i]);
      printf("cosine_similarity: aratio: %f\n", aratio);
#endif
      unsigned int j;
      double bratiosquaresumtemp = 0;
      for (j = 0; j < b->size; j++)
      {
	if (b->data[j])
	{
	  char* bcolon = strchr(b->data[j], ':');
	  *bcolon = '\0';
	  double bratio = strtod(bcolon+1, NULL);
	  if (!strcmp(a->data[i], b->data[j])) numerator += aratio*bratio;
	  if (!bratiosquaresum) 
	  {
#ifdef ONO_DEBUG
	    printf("cosine_similarity: b->data[j]: %s\n", (char*) b->data[j]);
	    printf("cosine_similarity: bratio: %f\n", bratio);
#endif
	    bratiosquaresumtemp += bratio*bratio; /* only collect these once */
	  }
	  *bcolon = ':';  
	}
      }
      aratiosquaresum += aratio*aratio;
      bratiosquaresum += bratiosquaresumtemp;
      *acolon = ':';
    }
  }
  double cs = numerator / sqrt(aratiosquaresum*bratiosquaresum);
#ifdef ONO_DEBUG
  printf("cosine_similarity: %f\n", cs);
#endif
  return cs;
}

int outerht_lookup(outerht_t* ht, char* name, char* replica, char* node, size_t n, da_t** da)
{
  outerht_entry_t* ohte = NULL;
  if (hi_get(ht, name, strlen(name)+1, (void*) (void*) &ohte)) return -1;
  innerht_entry_t* ihte = NULL;
  if (hi_get(ohte->rn, replica, strlen(replica)+1, (void*) (void*) &ihte)) return -1;
  da_t* candidates = ihte->data;
  da_t* passed = candidates;
  da_eliminate_duplicates(candidates);
#ifdef WANT_COSINE_SIMILARITY /* further process candidates */
  passed = da_create(NULL, 0);
  if (hi_get(ohte->nr, node, 7, (void*) (void*) &ihte)) 
  {
    da_destroy(passed);
    return -1;
  }
  da_t* thisratiomap = ihte->data;
  unsigned int i;
  for (i = 0; i < candidates->size; i++)
  {
    if (candidates->data[i])
    {
      if (!hi_get(ohte->nr, candidates->data[i], 7, (void*) (void*) &ihte))
      {
	da_t* thatratiomap = ihte->data;
	if (cosine_similarity(thisratiomap, thatratiomap) >= ONO_COSINE_SIMILARITY_THRESHOLD)
	{
	  da_add(passed, candidates->data[i], 7);
	}
      }
    }
  }  
#endif /* if don't want cosine similarity calculation, all candidates pass */
  da_eliminate_duplicates(passed);
#ifdef ONO_DEBUG
  printf("outerht_lookup: candidates:\n");
  for (i = 0; i < candidates->size; i++) if (candidates->data[i]) print_peer(candidates->data[i]);
  printf("outerht_lookup: passed:\n");
  for (i = 0; i < passed->size; i++) if (passed->data[i]) print_peer(passed->data[i]);
#endif
  node = NULL;
  int r;
  if ((r = da_pack(passed, n))) n = r;
  if (random_sample(passed, *da, n))
  {
    if (passed != candidates) da_destroy(passed);
    return -1;
  }
  if (passed != candidates) da_destroy(passed);
  return 0;
}

int outerht_delete(outerht_t* ht, char* data)
{
  hi_iterator_t* i;
  if (hi_iterator_create(ht, &i)) return -1;
  void *key;
  void *val;
  unsigned int keylen;
  while (!hi_iterator_getnext(i, &val, &key, &keylen)) /* go through all cdn names */
  {
    outerht_entry_t* hte = val;
    da_t* da = NULL;
    if (!innerht_lookup(hte->nr, data, &da)) /* da will have all replicas this node is mapped to */
    {
      unsigned int j;
      for (j = 0; j < da->size; j++) /* go through each of those replicas */
      {
	if (da->data[j])
	{
	  if (innerht_delete(hte->rn, da->data[j], data)) /* and delete the node */
	  {
	    hi_iterator_fini(i);
	    return -1;
          }
	}
      }
#ifdef WANT_COSINE_SIMILARITY
      if (innerht_delete(hte->nr, data, NULL)) /* delete the node */
      {
	hi_iterator_fini(i);
	return -1;
      }
#endif
    }
  }
  /* since there are few cdns, don't delete the whole entry even though it might be empty */
  hi_iterator_fini(i);
  return -1;
}

int outerht_destroy(outerht_t* ht)
{
  hi_iterator_t* i;
  if (hi_iterator_create(ht, &i))
  {
    hi_fini(ht);
    return 0;
  }
  void* key;
  void* data;
  unsigned int keylen;
  while (!hi_iterator_getnext(i, &data, &key, &keylen))
  {
    outerht_entry_t* hte = data;
    free(hte->key);
    innerht_destroy((innerht_t*) hte->nr);
    innerht_destroy((innerht_t*) hte->rn);
    free(hte);
  }
  hi_iterator_fini(i);
  hi_fini(ht);
  return 0;
}

#ifdef ONO_DEBUG
void outerht_print(outerht_t* ht)
{
  printf("outerht_print:\ntable_size: %d\nno_objects: %d\n", ht->table_size, ht->no_objects);
  printf("contents:\n");
  hi_iterator_t* i;
  if (hi_iterator_create(ht, &i)) return;
  void *key;
  void *data;
  unsigned int keylen;
  while (!hi_iterator_getnext(i, &data, &key, &keylen))
  {
    outerht_entry_t* hte = data;
    printf("%s ->\n", (char*) hte->key);
    printf("nr:\n");
    innerht_print(hte->nr);
    printf("rn:\n");
    innerht_print(hte->rn);
  }
  hi_iterator_fini(i);
}
#endif
