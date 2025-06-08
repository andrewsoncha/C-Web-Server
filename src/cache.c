#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hashtable.h"
#include "cache.h"

/**
 * Allocate a cache entry
 */
struct cache_entry *alloc_entry(char *path, char *content_type, void *content, int content_length)
{
    struct cache_entry *newEntry;
	newEntry = (struct cache_entry*)malloc(sizeof(struct cache_entry));
	
	int pathLen = (strlen(path)>MX_PATH_LEN)?MX_PATH_LEN:strlen(path);
	newEntry->path = (char*)malloc(pathLen);
	strncpy(newEntry->path, path, pathLen);
	
	int typeLen = (strlen(content_type)>MX_TYPE_LEN)?MX_TYPE_LEN:strlen(content_type);
	newEntry->content_type = (char*)malloc(typeLen);
	strncpy(newEntry->content_type, content_type, typeLen);
	newEntry->created_at = time(NULL);

	newEntry->content = malloc(content_length);
	memcpy(newEntry->content, content, content_length);
	
	newEntry->content_length = content_length;
	newEntry->prev = NULL; newEntry->next = NULL;
	return newEntry;
}

/**
 * Deallocate a cache entry
 */
void free_entry(struct cache_entry *entry)
{
	free(entry);
}

/**
 * Insert a cache entry at the head of the linked list
 */
void dllist_insert_head(struct cache *cache, struct cache_entry *ce)
{
    // Insert at the head of the list
    if (cache->head == NULL) {
        cache->head = cache->tail = ce;
        ce->prev = ce->next = NULL;
    } else {
        cache->head->prev = ce;
        ce->next = cache->head;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the head of the list
 */
void dllist_move_to_head(struct cache *cache, struct cache_entry *ce)
{
    if (ce != cache->head) {
        if (ce == cache->tail) {
            // We're the tail
            cache->tail = ce->prev;
            cache->tail->next = NULL;

        } else {
            // We're neither the head nor the tail
            ce->prev->next = ce->next;
            ce->next->prev = ce->prev;
        }

        ce->next = cache->head;
        cache->head->prev = ce;
        ce->prev = NULL;
        cache->head = ce;
    }
}

/**
 * Move a cache entry to the tail of the list
 */
void dllist_move_to_tail(struct cache *cache, struct cache_entry *ce){
	if(ce!=cache->tail){
		if(ce == cache->head){
			cache->head = ce->next;
			cache->head->prev = NULL;
		}
		else{
			ce->prev->next = ce->next;
			ce->next->prev = ce->prev;
		}
		
		ce->prev = cache->tail;
		cache->tail->next = ce;
		ce->next = NULL;
		cache->tail = ce;
	}
}
/**
 * Removes the tail from the list and returns it
 * 
 * NOTE: does not deallocate the tail
 */
struct cache_entry *dllist_remove_tail(struct cache *cache)
{
    struct cache_entry *oldtail = cache->tail;

    cache->tail = oldtail->prev;
    cache->tail->next = NULL;

    cache->cur_size--;

    return oldtail;
}

/**
 * Create a new cache
 * 
 * max_size: maximum number of entries in the cache
 * hashsize: hashtable size (0 for default)
 */
struct cache *cache_create(int max_size, int hashsize)
{
	struct cache *newCache = (struct chache*)malloc(sizeof(struct cache));
	newCache->index = hashtable_create(hashsize, NULL);
	newCache->head = NULL;
	newCache->tail = NULL;
	newCache->max_size = max_size;
	newCache->cur_size = 0;
	return newCache;
}

void cache_delete(struct cache *cache, struct cache_entry *ce){
	dllist_move_to_tail(cache, ce);
	dllist_remove_tail(cache);
	free_entry(ce);
}

void cache_free(struct cache *cache)
{
    struct cache_entry *cur_entry = cache->head;

    hashtable_destroy(cache->index);

    while (cur_entry != NULL) {
        struct cache_entry *next_entry = cur_entry->next;

        free_entry(cur_entry);

        cur_entry = next_entry;
    }

    free(cache);
}

/**
 * Store an entry in the cache
 *
 * This will also remove the least-recently-used items as necessary.
 * 
 * NOTE: doesn't check for duplicate cache entries
 */
void cache_put(struct cache *cache, char *path, char *content_type, void *content, int content_length)
{
    struct cache_entry *entry;
	
	printf("\n\n\n");
	if(cache->head!=NULL){
		printf("cache->head: %s   cache->tail: %s   ",cache->head->path, cache->tail->path);
	}
	printf("new Entry: %s\n\n\n", path);
	entry = hashtable_get(cache->index, path);
	if(entry==NULL){ // if the entry is not within the cache
		printf("entry not within the cache\n");
		entry = alloc_entry(path, content_type, content, content_length);
		dllist_insert_head(cache, entry);
		hashtable_put(cache->index, path, entry);
		cache->cur_size++;
		if(cache->cur_size > cache->max_size){
			struct cache_entry *oldtail = dllist_remove_tail(cache);
			hashtable_delete(cache->index, oldtail->path);
			free_entry(oldtail);
		}
	}
	else{
		printf("entry within the cache\n");
		dllist_move_to_head(cache, entry);
	}
}

/**
 * Retrieve an entry from the cache
 */
struct cache_entry *cache_get(struct cache *cache, char *path)
{
    struct cache_entry *entry;
	entry = hashtable_get(cache->index, path);
	if(entry==NULL){
		printf("\n\n\n entry is NULL!!!\n\n\n");
		return NULL;
	}
	
	dllist_move_to_head(cache, entry);
	return entry;
}

/*
Prints the contents of the cache
*/
void print_cache(struct cache *cache){
	struct cache_entry *entry;
	int i=1;
	printf("CACHE_PRINT\n");
	printf("________________________________________________________________________\n");
	entry = cache->head;
	do{
		struct tm* timeFormat = localtime(&(entry->created_at));
		int month, day, hour, min, sec, year;
		month = timeFormat->tm_mon+1;
		day = timeFormat->tm_mday;
		hour = timeFormat->tm_hour; 
		min = timeFormat->tm_min;
		sec = timeFormat->tm_sec;
		year = 1900+timeFormat->tm_year;
		printf("Entry #%d   Path: %s    Created At: %d-%d-%d  %02d:%02d:%02d\n", i, entry->path, year, month, day, hour, min, sec);
		entry = entry->next;
		i++;
	}while(entry!=NULL);
	printf("________________________________________________________________________\n");
	printf("head: %s  tail: %s  size:%d\n",cache->head->path, cache->tail->path, cache->cur_size);
}