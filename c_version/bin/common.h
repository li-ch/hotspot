#ifndef __LICH_COMMON_H__
#define __LICH_COMMON_H__

typedef struct {
    int addr;
    int time;    
} hotspot_raw_item ;

typedef struct {
	int accessed_prefetch_pages_number;
	int all_prefetched_pages_len;
	int cache_miss_pages_len;
} hotspot_result;

enum error_type {
	s_ok = 0,
	f_open_file_failed,
	f_create_root_failed,
	f_copy_from_user_failed,
};

#define default_swap_buffer_len 1024

#endif
