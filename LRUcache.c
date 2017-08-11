/* 
 * This csim.c file is created for 15513-cache lab
 * It takes a memory trace as input
 * Simulates hits/miss/eviction behaviour of cache memorty
 * Outputs the total number of hits, misses and evictions
 *
*/

#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>

#include "cachelab.h"

/* Struct for cache parameters, include both inputs and outputs */
typedef struct {
	int s;  // S = 2^s set index bits
	int E;  // associativity, E lines per set
	int b;  // B = 2^b block bits 

	long hit_count; // number of hits
	long miss_count; // number of misses
	long eviction_count; // number of evictions 
} cache_parameter;

/* Struct for lines in set */
typedef struct {
	int valid;  // valid bit
	long tag;  // tag
	unsigned long long int address; // addr, 64 bit hex
	int* block;  // block
	int access_time;  // last access time
} cache_line;

/* Struct for set */
typedef struct {
	cache_line* lines;
} cache_set;

/* Struct for cache */
typedef struct {
	cache_set* sets;
} cache;

cache init_cache(cache_parameter input_para);
int get_set(unsigned long long int addr, cache_parameter para);
long get_tag(unsigned long long int addr, cache_parameter para);
void free_cache(cache cache_cur, cache_parameter para);
cache_parameter visit_cache(cache_parameter para, cache *cur_cache, 
		unsigned long long int addr, int cnt, int verbo);
int LRU_earliest(cache_parameter para, cache_set *cur_set);

/* Function that initilize cache */
cache init_cache(cache_parameter input_para) {
	int S = 1 << input_para.s;
	int B = 1 << input_para.b;
	int E = input_para.E;
	cache new_cache;
	new_cache.sets = (cache_set*)malloc(sizeof(cache_set) * S);
	for(int i = 0; i < S; i++) {
		cache_set set_cur;
		set_cur.lines = (cache_line*)malloc(sizeof(cache_line) * E);
		new_cache.sets[i] = set_cur;
		for(int j = 0; j < E; j++) {
			cache_line line_cur;
			line_cur.block = (int*)malloc(sizeof(int) * B);
			line_cur.valid = 0;
			line_cur.tag = 0;
			line_cur.access_time = 0;
			line_cur.address = 0;
			new_cache.sets[i].lines[j] = line_cur;
		}
	}
	return new_cache;
}

/* Get set number from address */
int get_set(unsigned long long int addr, cache_parameter para) {
	unsigned int mask = (1 << para.s) - 1;
	return (addr >> para.b) & mask;
}

/* Get tag from address */
long get_tag(unsigned long long int addr, cache_parameter para) { 
	return addr >> (para.s + para.b);
}

/* Free the cache */
void free_cache(cache cache_cur, cache_parameter para) {
	int S = 1 << para.s;
	for(int i = 0; i < S; i++) {
		for(int j = 0; j < para.E; j++) {
			if(cache_cur.sets[i].lines[j].block != NULL) {
				free(cache_cur.sets[i].lines[j].block);
			}
		}
		if(cache_cur.sets[i].lines != NULL) {	
			free(cache_cur.sets[i].lines);
		}	
	}
	if(cache_cur.sets != NULL) {
		free(cache_cur.sets);
	}
}

/* Function for visiting cache, and update count number for output */
cache_parameter visit_cache(cache_parameter para, cache *cur_cache, 
			unsigned long long int addr, int cnt, int verbo) {
	int set_num = get_set(addr, para);
	long tag_num = get_tag(addr, para);
	cache_set *cur_set = &cur_cache->sets[set_num];
	// seach in current set
	for(int i = 0; i < para.E; i++)	{
		cache_line *cur_line = &cur_set->lines[i];
		if(cur_line->valid == 1 && cur_line->tag == tag_num) {
			para.hit_count++;
			cur_line->access_time = cnt;
			if(verbo == 1) {
				printf("hit\n");
			}
			cur_line = NULL;
			free(cur_line);
			return para;
		}
	}
	// not hit
	para.miss_count++;
	// search empty line, no eviction
	for(int i = 0; i < para.E; i++) {
		cache_line *cur_line = &cur_set->lines[i];
		if(cur_line->valid == 0) {
			cur_line->valid = 1;
			cur_line->tag = tag_num;
			cur_line->access_time = cnt;
			if(verbo == 1) {
				printf("miss\n");
			}
			cur_line = NULL;
			free(cur_line);	
			return para;
		}
	}
	// need eviction
	if(verbo == 1) {
		printf("miss eviction\n");
	}
	para.eviction_count++;
	int evict_idx = LRU_earliest(para, cur_set);
	cache_line *cur_line = &cur_set->lines[evict_idx];
	cur_line->valid = 1;
	cur_line->tag = tag_num;
	cur_line->access_time = cnt;
	cur_line = NULL;
	free(cur_line);	
	cur_set = NULL;
	free(cur_set);
	return para;	
}
	
/* Find the line to be evicted, by using LRU policy */
int LRU_earliest(cache_parameter para, cache_set *cur_set) {
	int min_time = cur_set->lines[0].access_time;
	int idx = 0;
	for(int i = 1; i < para.E; i++) {
		if(cur_set->lines[i].access_time < min_time) {
			min_time = cur_set->lines[i].access_time;
			idx = i;
		}
	}
	return idx;
}	

int main(int argc, char **argv) {
	int v = 0;
	cache_parameter para;
	para.hit_count = 0;
	para.miss_count = 0;
	para.eviction_count = 0;
	// get opt from command line
	char *trace_file = NULL;
	char c;
	while((c = getopt (argc, argv, "s:E:b:t:v")) != -1) {
		switch(c) {
			case 'v':
				v = 1;
				break;
			case 's':
				para.s = atoi(optarg);
			case 'E':
				para.E = atoi(optarg);
			case 'b':
				para.b = atoi(optarg);
			case 't':
				trace_file = optarg;
			default:
				break;
		}
	}	
	if(para.s == 0 || para.E == 0 || para.b == 0 || trace_file == NULL)	{
		printf("err input\n");
	}
	// initialize cache
	cache new_cache = init_cache(para);
	// get memory trace
	int size;
	unsigned long long addr;
	char opt;	
	FILE *file = fopen(trace_file, "r");
	if(file != NULL) {
		int cnt = 1;  // record access time
		while(fscanf(file, " %c %llx, %d", &opt, &addr, &size) == 3) {
			if(v == 1) {
				printf("%c %llx,%d ", opt, addr, size);
			}
			if(opt == 'L') {
				para = visit_cache(para, &new_cache, addr, cnt, v);
			}else if(opt == 'S') {
				para = visit_cache(para, &new_cache, addr, cnt, v);
			}else if(opt == 'M') {
				para = visit_cache(para, &new_cache, addr, cnt, v);
				para = visit_cache(para, &new_cache, addr, cnt, v);
			}
			cnt++;
		}
	}else {
		printf("trace file cannot be opened.\n");
	}
	fclose(file);
	file = NULL;
	free(file);
	trace_file = NULL;
	free(trace_file);
	free_cache(new_cache, para);
	printSummary(para.hit_count, para.miss_count, para.eviction_count);	
	return 0;
}

