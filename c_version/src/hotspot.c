/*
 *  hotspot.c 
 */

#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <asm/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/ioctl.h>
#include <linux/cdev.h>
#include <linux/string.h>
#include <linux/list.h>
#include <linux/pci.h>
#include <linux/uaccess.h>
#include <asm/fcntl.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/stat.h>
#include <linux/string.h>

#include "hashmap.h"
#include "vector.h"
#include "vector.c"
#include "common.h"

MODULE_LICENSE("MIT");
MODULE_AUTHOR("li-ch");

typedef struct {
    int time;    
    bool cached;
} hotspot_cached_item ;

typedef struct {
	Vector *addr_observation_window;
	int latest_accessed_time;
	int observation_window_size;
	int delta_max_len;
	int delta;
	int delta_len;
	int prefetch_number;
	int frequency_threshold_size;
	int frequency_threshold_time;
} hotspot_final;

typedef struct {
	Vector *hotspot_list;
	int hotspot_budget;
//	int affinity_score_threshold;	
	int affinity_score_min_addr;
	int affinity_score_max_addr;
} hotspot_table;

typedef struct {
	Vector *accessed_pages;
	Vector *cache_miss_pages;

	HashMap *cached_pages;
	HashMap *all_prefetched_pages;
	hotspot_table *table;
} hotspot_root;

#define HOTSPOT_BUDGET 20
//#define AFFINITY_SCORE_THRESHOLD 0.999
#define OBSERVATION_WINDOW_SIZE 100
#define DELTA_MAX_LEN  5
#define PREFETCH_NUMBER 64
#define DELTA_FREQUENCY_THRESHOLD 0.8


// 不通过浮点数，实现分数大小的比较
bool firstbigger(int a, int b, int c, int d) {
	int n, m;//这两个变量用来装分数的整数部分
	int number = 0;//记录取倒数的次数
	int M;//储存中间变量

	if (a >= b && c <= d)
	{
		return false;
	}
	if (a <= b && c >= d)
	{
		return true;

	}
	if (a > b && c > d) 
	{
		while (true)
		{
			if (b == 0)
			{
				if (number % 2 == 0)
				{
					return false;

				}
				else
				{
					return true;
				}

			}
			else if (d == 0)
			{
				if (number % 2 == 0)
				{
					return true;
				}
				else
				{
					return false;
				}
			}
			else
			{
				if (b <=d && a>=c)
				{
					if (number % 2 == 0)
					{
						return false;
					}
					else
					{
						return true;
					}
				}
				else if (b >= d && a <= c)
				{
					if (number % 2 == 0)
					{
						return true;
					}
					else
					{
						return false;
					}
				}
				else
				{                  
					n = a / b;
					m = c / d;
					if (n < m)
					{
						if (number % 2 == 0)
						{
							return true;
						}
						else
						{
							return false;
						}
					}
					else if (n > m)
					{
						if (number % 2 == 0)
						{
							return false;
						}
						else
						{
							return true;
						}
					}
					else
					{
						M = b;
						b = a - n * b;
						a = M;
						M = d;
						d = c - n * d;
						c = M;
						number++;
					}
				}
			}

		}    
	}

	if (a < b && c < d) 
	{
		M = b;
		b = a;
		a = M;
		M = d;
		d = c;
		c = M; 
		number++;
		while (true)
		{
			if (b == 0)
			{
				if (number % 2 == 0)
				{
					return false;
				}
				else
				{
					return true;
				}

			}
			else if (d == 0)
			{
				if (number % 2 == 0)
				{
					return true;

				}
				else
				{
					return false;

				}
			}
			else
			{
				if (b <d && a>c)
				{
					if (number % 2 == 0)
					{
						return false;
					}
					else
					{
						return true;
					}
				}
				else if (b > d && a < c)
				{
					if (number % 2 == 0)
					{
						return true;
					}
					else
					{
						return false;
					}
				}
				else
				{
					n = a / b;
					m = c / d;
					if (n < m)
					{
						if (number % 2 == 0)
						{
							return true;
						}
						else
						{
							return false;
						}
					}
					else if (n > m)
					{
						if (number % 2 == 0)
						{
							return false;
						}
						else
						{
							return true;
						}
					}
					else
					{
						M = b;
						b = a - n * b;
						a = M;
						M = d;
						d = c - n * d;
						c = M;
						number++;
					}
				}
			}
		}
	}
	return false;
}

// 初始化全局数据对象
hotspot_root* init_hotspot_root(int buff_size)
{
	hotspot_root *root = kmalloc(sizeof(hotspot_root), GFP_KERNEL);
	
	if(NULL == root) 
	{
		printk ("misc in init_hotspot_root create root failed.\n");
	}
	else
	{	
		root->cache_miss_pages = kmalloc(sizeof(Vector), GFP_KERNEL);
		if(root->cache_miss_pages == NULL) 
		{
			printk ("misc in init_hotspot_root create cache_miss_pages failed.\n");
			return NULL;
		}
		else 
		{
			vector_setup(root->cache_miss_pages, buff_size, sizeof(int));
			printk ("misc in init_hotspot_root create cache_miss_pages init finished, size is %zu.\n", root->cache_miss_pages->size);
		}

		root->cached_pages = kmalloc(sizeof(HashMap), GFP_KERNEL);
		if(root->cached_pages == NULL) 
		{
			printk ("misc in init_hotspot_root create cached_pages failed.\n");
			return NULL;
		}
		else
		{
			if (0 != hashmap_create(buff_size, root->cached_pages)) 
			{
				printk ("misc in init_hotspot_root init cached_pages failed.\n");
				return NULL;
			}
		}

		root->accessed_pages = kmalloc(sizeof(Vector), GFP_KERNEL);
		if(root->accessed_pages== NULL) 
		{
			printk ("misc in init_hotspot_root create accessed_pages failed.\n");
			return NULL;
		}
		else
		{
			vector_setup(root->accessed_pages, buff_size, sizeof(int));
		}

		root->all_prefetched_pages = kmalloc(sizeof(HashMap), GFP_KERNEL);
		if(root->all_prefetched_pages== NULL) 
		{
			printk ("misc in init_hotspot_root create all_prefetched_pages failed.\n");
			return NULL;
		}
		else
		{
			if (0 != hashmap_create(buff_size, root->all_prefetched_pages)) 
			{
				printk ("misc in init_hotspot_root init all_prefetched_pages failed.\n");
				return NULL;
			}
		}

		root->table = kmalloc(sizeof(hotspot_table), GFP_KERNEL);

		if(root->table == NULL) 
		{
			printk ("misc in init_hotspot_root create table failed.\n");
			return NULL;
		}
	

		root->table->hotspot_list = kmalloc(sizeof(Vector), GFP_KERNEL);

		if(root->table->hotspot_list == NULL) 
		{
			printk ("misc in init_hotspot_root create table hotspot_list failed.\n");
			return NULL;
		}

		vector_setup(root->table->hotspot_list, buff_size, sizeof(hotspot_final));

		root->table->hotspot_budget = HOTSPOT_BUDGET;

		printk ("misc in init_hotspot_root create root finished.\n");
	}

	return root;
}


void util_update_cached_addr(HashMap *map, int addr, int time, bool cached, bool just_update) {
	char *addr_str = kmalloc(sizeof(char)*32, GFP_KERNEL);
	hotspot_cached_item *item_old;
	hotspot_cached_item *item_new;

	sprintf(addr_str, "%d", addr);

	item_old = (hotspot_cached_item*)hashmap_get(map, addr_str, strlen(addr_str));

	if(item_old != NULL) 
	{
		hashmap_remove(map, addr_str, strlen(addr_str));
		kfree(item_old);	
	}
	else if(just_update)
	{
		// If it is only updated, then nothing is done when the key does not exist
		return;
	}	

	item_new = kmalloc(sizeof(hotspot_cached_item), GFP_KERNEL);
	item_new->time = time;
	item_new->cached = cached;

	hashmap_put(map, addr_str, strlen(addr_str), item_new);
}

Vector* generate_prefetch_candidates(HashMap *cached_pages, hotspot_final *item) {
	Vector *pages_to_prefetch = kmalloc(sizeof(Vector), GFP_KERNEL);
	int i = 0;
	int page_to_prefetch;
	int start_point = item->addr_observation_window->size - item->delta_len;
	
	vector_setup(pages_to_prefetch, 1024, sizeof(int));

	for(i = 0; i < item->prefetch_number; i++)
	{
		char key[32];

		page_to_prefetch = *((int*)vector_get(item->addr_observation_window, start_point + i % item->delta_len)) + item->delta * (1 + i/item->delta_len);	

		sprintf(key, "%d", page_to_prefetch);	

		if(hashmap_get(cached_pages, key, strlen(key)) == NULL)
		{
			vector_push_back(pages_to_prefetch, &page_to_prefetch);
		}
	}

	return pages_to_prefetch;
}

static int hashmap_release_cached_item_iterate(void* const context, struct hashmap_element_s *const elem) {
	kfree(elem->key);
	kfree(elem->data);
	return -1;
}

Vector* hotspot_item_update(HashMap *cached_pages, hotspot_final *item, int addr, int time) {
	int best_delta_len = 0;
	int best_delta_size = 9999; 
	int best_delta_time = 0;
	int best_delta = 0;
	int delta_len_tried = 1;
	HashMap delta_freq_dict;

	item->latest_accessed_time = time;
	vector_push_back(item->addr_observation_window, &addr);


    	if (item->addr_observation_window->size > item->observation_window_size)
	{
		vector_pop_front(item->addr_observation_window);	
	}

	if(item->addr_observation_window->size < (item->observation_window_size/8))
	{
		return NULL;
	}

	hashmap_create(16, &delta_freq_dict); 

	for(;delta_len_tried < item->delta_max_len + 1; delta_len_tried++)
	{
    		long *tmp_deltas;
		int i = 0;
		int chosen_delta_at_len = 0; // 距离长度
		int chosen_delta_at_size = 0; // 个数
		int chosen_delta_at_time = 0; // 总的距离个数

		tmp_deltas = kmalloc((item->addr_observation_window->size - delta_len_tried) * sizeof(long), GFP_KERNEL);

		for(i = 0; (i + delta_len_tried) < item->addr_observation_window->size; i++)
		{	
			char *key = kmalloc(sizeof(char)*32, GFP_KERNEL);
			int *count = NULL;

			tmp_deltas[i] = (*(int*)vector_get(item->addr_observation_window, i + delta_len_tried)) - (*(int*)vector_get(item->addr_observation_window, i));

			sprintf(key, "%ld", tmp_deltas[i]);
			count = hashmap_get(&delta_freq_dict, key, strlen(key));			

			if(count == NULL) 
			{
				count = kmalloc(sizeof(int), GFP_KERNEL);
				*count = 1;
				hashmap_put(&delta_freq_dict, key, strlen(key), count);
			}
			else
			{
				*count = *count + 1;
			}

			if((*count) > chosen_delta_at_time)
			{
				chosen_delta_at_len = tmp_deltas[i];
				chosen_delta_at_time = *count;
				chosen_delta_at_size = item->addr_observation_window->size - delta_len_tried;

				if(best_delta_time * chosen_delta_at_size <= chosen_delta_at_time * best_delta_size)
				{
					best_delta_size = chosen_delta_at_size;
					best_delta_len = delta_len_tried;
					best_delta = chosen_delta_at_len;
					best_delta_time = chosen_delta_at_time;
				}
			}

		}
		hashmap_iterate_pairs(&delta_freq_dict, hashmap_release_cached_item_iterate, NULL);	

		kfree(tmp_deltas);
	}
	hashmap_destroy(&delta_freq_dict);

	// frequency_threshold = 0.8
	if(best_delta_time * item->frequency_threshold_size > item->frequency_threshold_time * best_delta_size)
	{
		item->delta = best_delta;
		item->delta_len = best_delta_len;

		return generate_prefetch_candidates(cached_pages,item);
	}

	return NULL;
}

// get actived hotspot
hotspot_final* affinity_to_addr(Vector *hotspot_list, int addr) {
	hotspot_final *activated_hotspot = NULL;

	int max_addr = 1;
	int index = 0;
	int max_index = 0;

	if(hotspot_list->size > 0)
	{
		activated_hotspot = (hotspot_final*)vector_get(hotspot_list, index);
		max_index = 0;
		max_addr = *(int*)vector_back(activated_hotspot->addr_observation_window);
		index++;
	}

	for (; index < hotspot_list->size; index++) 
	{
		int addr_item;
		bool need_swap= false;
		hotspot_final *item = (hotspot_final*)vector_get(hotspot_list, index);

		addr_item = *(int*)vector_back(item->addr_observation_window);

		if(addr_item > addr)
		{
			if(max_addr > addr)
			{
				if(max_addr > addr_item)
				{
					need_swap= true;
				}
			}		
			else
			{
				// max_addr <= addr
				//if((long)addr * (long)addr >(long)(max_addr) * (long)addr_item)
				if(firstbigger(addr_item, addr, addr, max_addr))
				{	
					need_swap= true;
				}
			}
		}
		else
		{
	//		addr_item <= addr
			if(max_addr < addr)
			{
				if(max_addr < addr_item)
				{	
					need_swap= true;	
				}
			}		
			else
			{
				// max_add >= addr
				if(firstbigger(addr, addr_item, max_addr, addr))
				{
					need_swap= true;
				}
			}
		}

		if(need_swap) 
		{
			max_index = index;
			max_addr = addr_item;
			activated_hotspot = item;
		}
	}

	if(activated_hotspot != NULL)
	{
		int item_addr = *(int*)vector_back(activated_hotspot->addr_observation_window);

		if(item_addr > addr)
		{
			//if(addr * 1000 > item_addr * 999)			
			if(firstbigger(item_addr, addr, 1000, 999))			
			{	
				return activated_hotspot;
			}
		} 
		else
		{
			//if(item_addr * 1000 > addr * 999)			
			if(firstbigger(addr, item_addr, 1000, 999))			
			{	
				return activated_hotspot;
			}
		}

	}
	return NULL;
}

Vector* hotspot_table_update(HashMap *cached_pages, hotspot_table *table, int addr, int time) {
	hotspot_final *activated_hotspot = affinity_to_addr(table->hotspot_list, addr);	

	if(activated_hotspot != NULL && table->hotspot_list->size > 0)
	{
		return hotspot_item_update(cached_pages, activated_hotspot, addr, time);
	}
	else
	{
		hotspot_final new_hotspot;
	
		new_hotspot.observation_window_size = OBSERVATION_WINDOW_SIZE;
		new_hotspot.delta_max_len = DELTA_MAX_LEN;
		new_hotspot.delta = 1;
		new_hotspot.delta_len = 0;
		new_hotspot.prefetch_number = PREFETCH_NUMBER;
		new_hotspot.addr_observation_window = kmalloc(sizeof(Vector), GFP_KERNEL);

		new_hotspot.frequency_threshold_size = 10;
        	new_hotspot.frequency_threshold_time = 8;

		vector_setup(new_hotspot.addr_observation_window, 1024, sizeof(int));

		vector_push_back(new_hotspot.addr_observation_window, &addr);

		new_hotspot.latest_accessed_time = time;

		vector_push_back(table->hotspot_list, &new_hotspot);

		if(table->hotspot_list->size > table->hotspot_budget) 
		{
			int to_remove_pos = -1, current_pos = 0; 	
			hotspot_final *to_remove_hotspot = NULL;

			Iterator iterator = vector_begin(table->hotspot_list);
			Iterator last = vector_end(table->hotspot_list);

			for (; !iterator_equals(&iterator, &last); iterator_increment(&iterator)) 
			{
				hotspot_final *cur_hotspot;

				cur_hotspot = (hotspot_final*)iterator_get(&iterator);
				
				if(to_remove_hotspot == NULL || (to_remove_hotspot->latest_accessed_time > cur_hotspot->latest_accessed_time))
				{
					to_remove_hotspot = cur_hotspot;	
					to_remove_pos = current_pos;
				}

				current_pos++;
			}
	
			if(to_remove_hotspot != NULL)
			{	
				vector_destroy(to_remove_hotspot->addr_observation_window);
				kfree(to_remove_hotspot->addr_observation_window);
				vector_erase(table->hotspot_list, to_remove_pos);
			}
		}
	}
	
	return NULL;
}

static int all_prefetched_pages_cached = 0;

static int all_prefetched_pages_cached_iterate(void* const context, void* const value) {
	if (((hotspot_cached_item*)value)->cached) 
	{
		all_prefetched_pages_cached++;
	}

	// Otherwise tell the iteration to keep going.
	return 1;
}

void hotspot_process(hotspot_root *root, int addr, int time) {
	hotspot_cached_item *cached_item_old = NULL;
	char addr_str[32];

	vector_push_back(root->accessed_pages, &addr);


	sprintf(addr_str, "%d", addr);
	cached_item_old = (hotspot_cached_item*)hashmap_get(root->cached_pages, addr_str, strlen(addr_str));

	if(cached_item_old == NULL)
	{
		Vector *pages_to_prefetch = NULL;

		vector_push_back(root->cache_miss_pages, &addr);

		util_update_cached_addr(root->cached_pages, addr, time, true, false);

		pages_to_prefetch = hotspot_table_update(root->cached_pages, root->table, addr, time);	

		if(pages_to_prefetch != NULL) 
		{
			int *addr_cur = NULL;
			int i = 0;

			for(; i < pages_to_prefetch->size; i++)
			{
				addr_cur = (int*)vector_get(pages_to_prefetch, i);
				util_update_cached_addr(root->cached_pages, *addr_cur, -1, false, false);
				util_update_cached_addr(root->all_prefetched_pages, *addr_cur, -1, false, false);
			}

			vector_destroy(pages_to_prefetch);
			kfree(pages_to_prefetch);
		}
	}	
	else
	{
		util_update_cached_addr(root->cached_pages, addr, time, true, false);
		util_update_cached_addr(root->all_prefetched_pages, addr, time, true, true);
	}
};

// 类似fgets，按行读取文件
static char* read_line(char *buf, int buf_len, struct file *fp)
{
        int ret;
        int i = 0;
        mm_segment_t fs;
 
        fs=get_fs();
        set_fs(KERNEL_DS);
        //ret = vfs_read(fp, buf, buf_len, &(fp->f_pos));
        ret = kernel_read(fp, buf, buf_len, &(fp->f_pos));
        set_fs(fs);
 
        if (ret <= 0)
                return NULL;
 
        while(buf[i++] != '\n' && i < ret);
 
        if(i < ret) {
                fp->f_pos += i-ret;
        }
 
        if(i < buf_len) {
                buf[i] = 0;
        }
        return buf;
}

static long hotspot_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	hotspot_root *root; 
	char *user_buffer = (char*)arg;
	int result_buffer[108] = {0};
	int result_size = 0;
	char *files_path, *files_path_token;
	char *file_path_buff;
	struct file* fp;

	root = init_hotspot_root(1024);

	if(root == NULL)
	{
		printk ("error: misc hotspot_open root is NULL.\n");
		return f_create_root_failed;
	}

	files_path_token = files_path = kmalloc(sizeof(char) * default_swap_buffer_len, GFP_KERNEL);

	if(0 != copy_from_user(files_path, user_buffer, sizeof(char) * default_swap_buffer_len))
	{
		printk ("error: misc hotspot_ioctl copy_from_user failed.\n");
		return f_copy_from_user_failed;
	}

	file_path_buff = strsep(&files_path_token, ";");

	while(file_path_buff != NULL)
	{
		hotspot_result result;

		fp = filp_open(file_path_buff, O_RDONLY, 0644);


		if(!IS_ERR(fp))
		{
			char line_buff[128];
			char *line_text;

			while((line_text = read_line(line_buff, 128, fp)) != NULL)
			{
				char *line_buff_token = line_text;
				char *value;
				int i = 0;
				int time, addr;

				value = strsep(&line_buff_token, " ");
				while(value != NULL)
				{
					if(i == 0)
					{
						if( kstrtoint(value, 10, &time) != 0)
						{
							break;
						}
					}	
					else if(i == 2)
					{
						if( kstrtoint(value, 16, &addr) == 0)
						{
							hotspot_process(root, addr, time);
							break;
						}
					}
					i++;
					value = strsep(&line_buff_token, " ");
				}
			}
			

			vector_clear(root->table->hotspot_list);	
			filp_close(fp, NULL);

			all_prefetched_pages_cached = 0;

			hashmap_iterate(root->all_prefetched_pages, all_prefetched_pages_cached_iterate, NULL); 

			result_buffer[result_size++] = result.accessed_prefetch_pages_number = all_prefetched_pages_cached;

			result_buffer[result_size++] = result.all_prefetched_pages_len = hashmap_num_entries(root->all_prefetched_pages);

			result_buffer[result_size++] = result.cache_miss_pages_len = root->cache_miss_pages->size;

			result_buffer[result_size++] = result.accessed_prefetch_pages_number + result.cache_miss_pages_len;
			
			if(result.all_prefetched_pages_len == 0)
			{
				printk("total prefetched pages is 0.\n"); 
			}
			else
			{	
				printk("Prefetch Accuracy:%d, accessed_prefetch_pages:%d, total prefetched pages:%d, coverage:%d\n", 
						result.all_prefetched_pages_len,
						result.accessed_prefetch_pages_number, 
						result.all_prefetched_pages_len,
						result.accessed_prefetch_pages_number + result.cache_miss_pages_len);
			}
		}
		else
		{
			printk ("misc %s %d debug info !\n", __FILE__, __LINE__);
		}


		file_path_buff = strsep(&files_path_token, ";");
	}
	
	if(0!=copy_to_user(user_buffer, &result_buffer, sizeof(int) * 108))
	{
		printk ("error: misc hotspot_ioctl copy_to_user failed.\n");
	}


	vector_destroy(root->cache_miss_pages);	
	kfree(root->cache_miss_pages);

	vector_destroy(root->accessed_pages);	
	kfree(root->accessed_pages);

	hashmap_iterate_pairs(root->all_prefetched_pages, hashmap_release_cached_item_iterate, NULL); 
	hashmap_destroy(root->all_prefetched_pages);
	kfree(root->all_prefetched_pages);

	hashmap_iterate_pairs(root->cached_pages, hashmap_release_cached_item_iterate, NULL); 
	hashmap_destroy(root->cached_pages);
	kfree(root->cached_pages);

	vector_destroy(root->table->hotspot_list);	
	kfree(root->table->hotspot_list);
	kfree(root->table);

	kfree(root);

	return 0;
}
 
static int hotspot_open(struct inode *inode, struct file *filp)
{
	printk ("misc in hotspot_open create root finished.\n");
	return 0;
}

static int hotspot_release(struct inode *inode, struct file *filp)
{
	printk ("misc hotspot_release root free finished.\n");
	return 0;
}
 
static ssize_t hotspot_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	printk ("misc hotspot_read\n");
	return 1;
}

static ssize_t hotspot_write(struct file *filp, const char __user *buf, size_t count, loff_t *ppos)
{
    printk ("misc hotspot_write\n");
    return 1;
}

static int hotspot_fasync(int fd, struct file *filp, int mode)
{
    printk ("misc hotspot_fasync\n");
    return 1;
} 

static struct file_operations hotspot_fops =
{
	.owner   = THIS_MODULE,
	.read    = hotspot_read,
	.write   = hotspot_write,
	.unlocked_ioctl   = hotspot_ioctl,
	.fasync = hotspot_fasync,
	.open    = hotspot_open,
	.release = hotspot_release
};
 
static struct miscdevice misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "misc_hotspot", //此名称将显示在/dev目录下面
	.fops = &hotspot_fops,
};


static int __init dev_init(void)
{
	int ret = misc_register(&misc);
	printk ("misc hotspot initialized\n");
	return ret;
}
 
static void __exit dev_exit(void)
{
	misc_deregister(&misc);
	printk("misc hotspot unloaded\n");
}

module_init(dev_init);
module_exit(dev_exit);

