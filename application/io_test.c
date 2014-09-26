#include <stdio.h>
//#include <time.h>
#include <math.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

void function()/*用来耗用一定的时间而已，无实际用处的函数*/
{
	FILE *fp;
	printf("1\n"); 
	if((fp=fopen("/tmp/test.dat","wb"))<0)
		printf("open %s\n", strerror(errno));
	printf("2\n"); 
	int i,j,nbytes;
	for(i=0;i<102400;i++)
	{
		for(j=0;j<256;j++)
		if((nbytes=fwrite(&i,sizeof(int),1,fp))<0)
			printf("write %s\n", strerror(errno));
	}
	printf("3\n"); 
	fclose(fp);
printf("4\n"); 
}

int main(void)
{
  struct timeval tpstart,tpend;
  float timeuse;

  gettimeofday(&tpstart,NULL);

  function();

  gettimeofday(&tpend,NULL);
  timeuse=1000000*(tpend.tv_sec-tpstart.tv_sec)+tpend.tv_usec-tpstart.tv_usec;
  timeuse/=1000000;
  printf("Used Time:%f\n",timeuse);
  return 0;
}
