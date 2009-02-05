#include <libhashish.h>

#define ONO_DEBUG 1

#ifdef ONO_DEBUG
#include "trackerlogic.h"
void print_peer(ot_peer* peer);
#endif

#define ONO_RATIO_DECIMAL_PLACES 2
#define ONO_RATIO_THRESHOLD 0.1
#define ONO_COSINE_SIMILARITY_THRESHOLD 0.1
#define WANT_COSINE_SIMILARITY 1

#define DA_INITIAL_SIZE 1
#define DA_SIZE_MULTIPLE 2

/* generic dynamic array */
struct dynamicarray
{
  size_t n;
  size_t size;
  void** data; /* data is not necessarily at the beginning of the array */
               /* a slot has data if it is not NULL */
};
typedef struct dynamicarray da_t;
da_t *da_create(void* firstdata, size_t firstdata_size); /* firstdata = NULL = create empty da */
int da_add(da_t* da, void* data, size_t data_size);
int da_delete(da_t* da, void* data);
int da_pack(da_t* da, int firstn);
int da_eliminate_duplicates(da_t* da);
int da_destroy(da_t* da);
#ifdef ONO_DEBUG
void da_print(da_t* da);
#endif

/* 2 inner hts are nr and rn */
/* nr holds node -> dynamic array of pairs of replica:ratio */
/* rn holds replica -> dynamic array of nodes strongly mapped to it */
/* node is in ot_peer format (6 bytes) */
/* other sizes decided by strlen() */
/* outer ht holds cdn name -> nr, rn pair */
#define HT_INITIAL_SIZE 1
#define HT_SIZE_MULTIPLE 2
typedef struct innerht_entry innerht_entry_t;
typedef struct outerht_entry outerht_entry_t;

typedef hi_handle_t innerht_t;
innerht_t* innerht_create();
int innerht_insert(innerht_t** ht, char* key, size_t key_size, char* data, size_t data_size);
int innerht_delete(innerht_t* ht, char* key, char* data); /* data = NULL = delete whole entry */
int innerht_lookup(innerht_t* ht, char* key, da_t** da);
int innerht_destroy(innerht_t* ht);
#ifdef ONO_DEBUG
void innerht_print(innerht_t* ht);
#endif

typedef hi_handle_t outerht_t;
outerht_t* outerht_create();
int outerht_insert(outerht_t** ht, char* name, char* key, char* data, char* which); /* which = "nr" or "rn" */
int outerht_lookup(outerht_t* ht, char* name, char* replica, char* node, size_t n, da_t** da); /* given node, return random sample of ono-close peers for it to connect to */
int outerht_delete(outerht_t* ht, char* data);
int outerht_destroy(outerht_t* ht);
#ifdef ONO_DEBUG
void outerht_print(innerht_t* ht);
#endif
