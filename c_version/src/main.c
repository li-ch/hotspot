#include <fcntl.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <assert.h>
#include <math.h>
#include "common.h"

#define HOTSPOT_DEV_PATH "/dev/misc_hotspot"

int main(){
	int fd = open(HOTSPOT_DEV_PATH,O_RDWR);
	char *files = malloc(sizeof(char)*default_swap_buffer_len);
	int *result;	
	int i = 0;

	memset(files, 0, sizeof(char)*default_swap_buffer_len);

	// 向内核交换缓存输入文件路径，需要保证有访问权限，为了方便直接将测试数据拷贝到/tmp目录下
	strcpy(files,  "/tmp/data/test00.txt;/tmp/data/test01.txt;/tmp/data/test02.txt;/tmp/data/test03.txt");

	if ( -1 == fd){
		printf("open hotspot node failed!\n");
		return -1;
	}

	ioctl(fd, 0, files);
	
	close(fd);

	// 从交换缓存中读取内核模块返回的结果
	result = (int*)files;	


	// 参照python脚本,显示处理结果
	for(; result[i] != 0; )
	{
		printf("Prefetch Accuracy:%1.12lf, accessed_prefetch_pages:%d, total prefetched pages:%d, coverage:%1.12lf\n", 
				(float)result[i]/result[i+1],
				result[i], 
				result[i+1],
				(float)result[i]/result[i+3]);
		i +=4;
	}

	free(files);

	return 0;
}

