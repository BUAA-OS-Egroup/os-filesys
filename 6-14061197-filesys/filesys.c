#include<stdio.h>
#include<unistd.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include<string.h>
#include<ctype.h>
#include "filesys.h"


#define RevByte(low,high) ((high)<<8|(low))
#define RevWord(lowest,lower,higher,highest) ((highest)<< 24|(higher)<<16|(lower)<<8|lowest) 

/*
*功能：打印启动项记录
*/
void ScanBootSector()
{
	unsigned char buf[SECTOR_SIZE];
	int ret,i;

	if((ret = read(fd,buf,SECTOR_SIZE))<0)
		perror("read boot sector failed");
	for(i = 0; i < 8; i++)
		bdptor.Oem_name[i] = buf[i+0x03];
	bdptor.Oem_name[i] = '\0';

	bdptor.BytesPerSector = RevByte(buf[0x0b],buf[0x0c]);
	bdptor.SectorsPerCluster = buf[0x0d];
	bdptor.ReservedSectors = RevByte(buf[0x0e],buf[0x0f]);
	bdptor.FATs = buf[0x10];
	bdptor.RootDirEntries = RevByte(buf[0x11],buf[0x12]);    
	bdptor.LogicSectors = RevByte(buf[0x13],buf[0x14]);
	bdptor.MediaType = buf[0x15];
	bdptor.SectorsPerFAT = RevByte( buf[0x16],buf[0x17] );
	bdptor.SectorsPerTrack = RevByte(buf[0x18],buf[0x19]);
	bdptor.Heads = RevByte(buf[0x1a],buf[0x1b]);
	bdptor.HiddenSectors = RevByte(buf[0x1c],buf[0x1d]);

	fatbuf=(unsigned char *)malloc((bdptor.SectorsPerFAT<<9)+16);
	printf("Oem_name \t\t%s\n"
		"BytesPerSector \t\t%d\n"
		"SectorsPerCluster \t%d\n"
		"ReservedSector \t\t%d\n"
		"FATs \t\t\t%d\n"
		"RootDirEntries \t\t%d\n"
		"LogicSectors \t\t%d\n"
		"MedioType \t\t%d\n"
		"SectorPerFAT \t\t%d\n"
		"SectorPerTrack \t\t%d\n"
		"Heads \t\t\t%d\n"
		"HiddenSectors \t\t%d\n",
		bdptor.Oem_name,
		bdptor.BytesPerSector,
		bdptor.SectorsPerCluster,
		bdptor.ReservedSectors,
		bdptor.FATs,
		bdptor.RootDirEntries,
		bdptor.LogicSectors,
		bdptor.MediaType,
		bdptor.SectorsPerFAT,
		bdptor.SectorsPerTrack,
		bdptor.Heads,
		bdptor.HiddenSectors);

	printf("FAT_one\t\t%d\n",FAT_ONE_OFFSET);
	printf("FAT_two\t\t%d\n",FAT_TWO_OFFSET);
}

/*日期*/
void findDate(unsigned short *year,
			  unsigned short *month,
			  unsigned short *day,
			  unsigned char info[2])
{
	int date;
	date = RevByte(info[0],info[1]);

	*year = ((date & MASK_YEAR)>> 9 )+1980;
	*month = ((date & MASK_MONTH)>> 5);
	*day = (date & MASK_DAY);
}

/*时间*/
void findTime(unsigned short *hour,
			  unsigned short *min,
			  unsigned short *sec,
			  unsigned char info[2])
{
	int time;
	time = RevByte(info[0],info[1]);

	*hour = ((time & MASK_HOUR )>>11);
	*min = (time & MASK_MIN)>> 5;
	*sec = (time & MASK_SEC) * 2;
}

/*
*文件名格式化，便于比较
*/
void FileNameFormat(unsigned char *name)
{
	unsigned char *p = name;
	while(*p!='\0')
		p++;
	p--;
	while(*p==' ')
		p--;
	p++;
	*p = '\0';
}

/*参数：entry，类型：struct Entry*
*返回值：成功，则返回偏移值；失败：返回负值
*功能：从根目录或文件簇中得到文件表项
*/
int GetEntry(struct Entry *pentry)
{
	int ret,i,k;
	char ext[3];
	int count = 0;
	unsigned char buf[DIR_ENTRY_SIZE], info[2];

	/*读一个目录表项，即32字节*/
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	count += ret;
	if(buf[0]==0xe5 || buf[0]== 0x00)
		return -1*count;
	else
	{
		/*长文件名，忽略掉*/
		while (buf[11]== 0x0f) 
		{
			if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
				perror("read root dir failed");
			count += ret;
		}
		if (buf[0]==0xe5 || buf[0]== 0x00) return -1*count;

		/*命名格式化，主义结尾的'\0'*/
		for (k=7;k>=0&&buf[k]==0x20;--k);
		if (k<0) k=0;
		for (i=0 ;i<=k;i++)
			pentry->short_name[i] = buf[i];
		if (buf[8]!=0x20)
		{
			k=8;
			pentry->short_name[i++]='.';
			while (k<=10&&buf[k]!=0x20) pentry->short_name[i++]=buf[k++];
		}
		pentry->short_name[i] = '\0';

		FileNameFormat(pentry->short_name); 



		info[0]=buf[22];
		info[1]=buf[23];
		findTime(&(pentry->hour),&(pentry->min),&(pentry->sec),info);  

		info[0]=buf[24];
		info[1]=buf[25];
		findDate(&(pentry->year),&(pentry->month),&(pentry->day),info);

		pentry->FirstCluster = RevByte(buf[26],buf[27]);
		pentry->size = RevWord(buf[28],buf[29],buf[30],buf[31]);

		pentry->readonly = (buf[11] & ATTR_READONLY) ?1:0;
		pentry->hidden = (buf[11] & ATTR_HIDDEN) ?1:0;
		pentry->system = (buf[11] & ATTR_SYSTEM) ?1:0;
		pentry->vlabel = (buf[11] & ATTR_VLABEL) ?1:0;
		pentry->subdir = (buf[11] & ATTR_SUBDIR) ?1:0;
		pentry->archive = (buf[11] & ATTR_ARCHIVE) ?1:0;

		return count;
	}
}

/*
*功能：显示当前目录的内容
*返回值：1，成功；-1，失败
*/
int fd_ls(int detail, struct Entry * curdir)
{
	int ret, offset,cluster_addr, curClu;
	struct Entry entry;
	unsigned char buf[DIR_ENTRY_SIZE];
	if( (ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
		perror("read entry failed");
	if (curdir!=NULL&&curdir->FirstCluster==1)
	{
		curdir=NULL;
	}
	else if (curdir!=NULL&&curdir->short_name[0]=='\0') {
		printf("ls: no such dir.\n");
		return -1;		
	}
	if(curdir==NULL)
		printf(":/\n");
	else
		printf(":/%s\n",curdir->short_name);
	if (detail) printf("\tname\tdate\t\t time\t\tcluster\tsize\t\tattr\n");

	if(curdir==NULL)  /*显示根目录区*/
	{
		/*将fd定位到根目录区的起始地址*/
		if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror("lseek ROOTDIR_OFFSET failed");

		offset = ROOTDIR_OFFSET;

		/*从根目录区开始遍历，直到数据区起始地址*/
		while(offset < (DATA_OFFSET))
		{
			ret = GetEntry(&entry);

			offset += abs(ret);
			if(ret > 0)
			{
				if (detail) 
				{
					if (strcmp(entry.short_name,"..")==0||strcmp(entry.short_name,".")==0)
						printf("%12s\n", entry.short_name);
					else
					printf("%12s\t"
					"%d/%d/%d\t"
					"%d:%d:%d   \t"
					"%d\t"
					"%d\t\t"
					"%s\n",
					entry.short_name,
					entry.year,entry.month,entry.day,
					entry.hour,entry.min,entry.sec,
					entry.FirstCluster,
					entry.size,
					(entry.subdir) ? "dir":"file");
				}
				else if (entry.short_name[0]!='.') printf("%12s", entry.short_name);
			}
		}
	}

	else /*显示子目录*/
	{
		curClu=curdir->FirstCluster;
		while (1) {
			cluster_addr = DATA_OFFSET + (curClu-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(&entry);
				offset += abs(ret);
				if(ret > 0)
				{
					if (detail) 
					{
						if (strcmp(entry.short_name,"..")==0||strcmp(entry.short_name,".")==0)
							printf("%12s\n", entry.short_name);
						else
						printf("%12s\t"
						"%d/%d/%d\t"
						"%d:%d:%d   \t"
						"%d\t"
						"%d\t\t"
						"%s\n",
						entry.short_name,
						entry.year,entry.month,entry.day,
						entry.hour,entry.min,entry.sec,
						entry.FirstCluster,
						entry.size,
						(entry.subdir) ? "dir":"file");
					} else if (entry.short_name[0]!='.') printf("%12s", entry.short_name);
				}
			}

			curClu=fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8);
			if (curClu==0xffff) break;
		}
	}
	if (!detail) printf("\n");
	return 0;
} 


/*
*参数：entryname 类型：char
：pentry    类型：struct Entry*
：mode      类型：int，mode=1，为目录表项；mode=0，为文件
：where     类型：struct Entry*，place to find
*返回值：偏移值大于0，则成功；-1，则失败
*功能：搜索当前目录，查找文件或目录项
*/
int ScanEntryIn (char *entryname,struct Entry *pentry,int mode, struct Entry * where)
{
	int ret,offset,i;
	int cluster_addr,curClu;
	char uppername[80];
	for(i=0;i< strlen(entryname);i++)
		uppername[i]= toupper(entryname[i]);
	uppername[i]= '\0';
	/*扫描根目录*/
	if(where ==NULL)  
	{
		if((ret = lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
			perror ("lseek ROOTDIR_OFFSET failed");
		offset = ROOTDIR_OFFSET;


		while(offset<DATA_OFFSET)
		{
			tmpaddr=offset;
			ret = GetEntry(pentry);
			offset +=abs(ret);
			tmpsize=abs(ret);
			if((pentry->subdir == mode||mode==3)&&!strcmp((char*)pentry->short_name,uppername))
				return offset;

		}
		return -1;
	}

	/*扫描子目录*/
	else  
	{
		curClu=where->FirstCluster;
		while (1) {
			cluster_addr = DATA_OFFSET + (curClu-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				tmpaddr=offset;
				ret = GetEntry(pentry);
				offset += abs(ret);
				tmpsize=abs(ret);
				if((pentry->subdir == mode ||mode==3)&&!strcmp((char*)pentry->short_name,uppername))
					return offset;
			}

			curClu=fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8);
			if (curClu==0xffff) break;
		}
		return -1;
	}
}

/*
*参数：entryname 类型：char
：pentry    类型：struct Entry*
：mode      类型：int，mode=1，为目录表项；mode=0，为文件
*返回值：偏移值大于0，则成功；-1，则失败
*功能：搜索当前目录，查找文件或目录项
*/
int ScanEntry (char *entryname,struct Entry *pentry,int mode)
{
	return ScanEntryIn(entryname,pentry,mode,curdir);
}



/*
*参数：dir，类型：char ; en，类型：Entry
*返回值：1，成功；-1，失败, en->short_name==""
*功能：get目录
*/
int getPath(struct Entry * en, char *dir, int mode)
{
	struct Entry *pentry;
	int ret,i,j;
	char t[15];

	if (strlen(dir)<=0) 
	{
		en->short_name[0]='\0';
		en->FirstCluster=0;
		return -1;
	}
	if (*dir=='/') 
	{
		tentry=NULL;
		tmpno=0;
		fathertmp[0]=NULL;
		i=1;
	}else 
	{
		tentry=curdir;
		for (i=0;i<=dirno;++i) 
		{
			fathertmp[i]=fatherdir[i];
			//if (fathertmp[i]!=NULL) printf("%s\n", fathertmp[i]->short_name);
		}
		tmpno=dirno;
		i=0;
	}

	while (1)
	{
		j=0;
		while (dir[i]&&dir[i]!='/'&&j<14) t[j++]=dir[i++];
		if (j>11) 
		{
			en->short_name[0]='\0';
			en->FirstCluster=0;
			return -1;
		}
		if (dir[i]=='/'||dir[i]=='\0')
		{
			t[j]='\0';
		}
		/*返回上一级目录*/
		if(!strcmp(t,"..") && tentry!=NULL)
		{
			tentry = fathertmp[tmpno];
			--tmpno; 
		}
		if (strcmp(t,".")!=0&&strcmp(t,"..")!=0&&t[0]!='\0') 
		{
			pentry = (struct Entry*)malloc(sizeof(struct Entry));

			if (dir[i]=='\0') ret = ScanEntryIn(t,pentry,mode,tentry);
			else ret = ScanEntryIn(t,pentry,1,tentry);
			if(ret < 0)
			{
				//printf("no such dir\n");
				free(pentry);
				en->short_name[0]='\0';
				en->FirstCluster=0;
				return -1;
			}
			++tmpno;
			fathertmp[tmpno] = tentry;
			tentry = pentry;
		}
		if (dir[i]=='\0') break;
		++i;
	}
	if (tentry!=NULL) *en=*tentry; else en->FirstCluster=1;
	return 1;
}

/*
*参数：dir，类型：char ; en，类型：Entry
*返回值：1，成功；-1，失败, en->short_name==""
*功能：get father目录
*/
int getfPath(struct Entry * en, char *dir, int mode)
{
	struct Entry *pentry;
	int ret,i,j;
	char t[15];

	if (strlen(dir)<=0) 
	{
		en->short_name[0]='\0';
		en->FirstCluster=0;
		printf("path not found.\n");
		return -1;
	}
	if (*dir=='/') 
	{
		tentry=NULL;
		tmpno=0;
		fathertmp[0]=NULL;
		i=1;
	}else 
	{
		tentry=curdir;
		for (i=0;i<=dirno;++i) fathertmp[i]=fatherdir[i];
			tmpno=dirno;
		i=0;
	}
	while (1)
	{
		j=0;
		while (dir[i]&&dir[i]!='/'&&j<14) t[j++]=dir[i++];
		if (j>11) 
		{
			en->short_name[0]='\0';
			en->FirstCluster=0;
			printf("path not found.\n");
			return -1;
		}
		if (dir[i]=='/'||dir[i]=='\0')
		{
			t[j]='\0';
		}
		/*返回上一级目录*/
		if(!strcmp(t,"..") && tentry!=NULL)
		{
			tentry = fathertmp[tmpno];
			--tmpno; 
		}

		if (strcmp(t,".")!=0&&strcmp(t,"..")!=0&&t[0]!='\0') 
		{
			pentry = (struct Entry*)malloc(sizeof(struct Entry));

			if (dir[i]=='\0') ret = ScanEntryIn(t,pentry,3,tentry);
			else ret = ScanEntryIn(t,pentry,1,tentry);
			if(ret < 0)
			{
				if (dir[i]=='\0'||(mode&1)&&dir[i]=='/'&&dir[i+1]=='\0') 
				{
					if (tentry==NULL)
					{
						en->FirstCluster=1;					
					}
					else *en=*tentry;
					tentry=pentry;
					strcpy(tentry->short_name,t);
					return 1;
				}
				//printf("no such dir\n");
				free(pentry);
				en->short_name[0]='\0';
				en->FirstCluster=0;
				printf("path not found.\n");
				return -1;
			}
			++tmpno;
			fathertmp[tmpno] = tentry;
			tentry = pentry;
		}
		if (dir[i]=='\0') break;
		++i;
	}
	printf("file or dir exists.\n");
	en->short_name[0]='\0';
	en->FirstCluster=0;	
	return -1;
}

/*
*参数：prev，类型：unsigned char
*返回值：下一簇
*在fat表中获得下一簇的位置
*/
unsigned short GetFatCluster(unsigned short prev)
{
	unsigned short next;
	int index;

	index = prev * 2;
	next = RevByte(fatbuf[index],fatbuf[index+1]);

	return next;
}

/*
*参数：cluster，类型：unsigned short
*返回值：void
*功能：清除fat表中的簇信息
*/
void ClearFatCluster(unsigned short cluster)
{
	int index;
	index = cluster * 2;

	fatbuf[index]=0x00;
	fatbuf[index+1]=0x00;

}


/*
*将改变的fat表值写回fat表
*/
int WriteFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(write(fd,fatbuf,bdptor.SectorsPerFAT<<9)<0)
	{
		perror("read failed");
		return -1;
	}
	if(lseek(fd,FAT_TWO_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if((write(fd,fatbuf,bdptor.SectorsPerFAT<<9))<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}

/*
*读fat表的信息，存入fatbuf[]中
*/
int ReadFat()
{
	if(lseek(fd,FAT_ONE_OFFSET,SEEK_SET)<0)
	{
		perror("lseek failed");
		return -1;
	}
	if(read(fd,fatbuf,bdptor.SectorsPerFAT<<9)<0)
	{
		perror("read failed");
		return -1;
	}
	return 1;
}


/*
*参数：filename，类型：char
*参数：mode，类型：int
*返回值：1，成功；-1，失败
*功能;删除当前目录下的文件or dir
*/
int fd_df(struct Entry * where, int addr, int size, int mode)
{
	struct Entry *pentry;
	int ret, curClu, cluster_addr, offset;
	unsigned char c;
	unsigned short seed,next;
	c=0xe5;

	if (where->FirstCluster==1)
	{
		printf("rm: attempt to delete root dir \n");
		return -1;		
	}
	if (where->short_name[0]=='\0')
	{
		if (mode) printf("rm: no such dir.\n");
		else {
			printf("rm: the target is not a file.\n");
		}
		return -1;				
	}
	if (where->readonly)
	{
		printf("rm: attempt to delete readonly file or dir '%s'\n",where->short_name);
		return -1;						
	}
	if (where->subdir) {
		pentry=(struct Entry* )malloc(sizeof(struct Entry));
		curClu=where->FirstCluster;
		while (1) {
			cluster_addr = DATA_OFFSET + (curClu-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				ret = GetEntry(pentry);
				if (ret>0)
				{
					if (strcmp(pentry->short_name,".")!=0&&strcmp(pentry->short_name,"..")!=0) 
						fd_df(pentry,offset,abs(ret),pentry->subdir);
					else {
						if(lseek(fd,offset,SEEK_SET)<0)
							perror("lseek fd_df failed");
						if(write(fd,&c,1)<0)
							perror("write failed");
					}
					if(lseek(fd,offset+abs(ret),SEEK_SET)<0)
						perror("lseek fd_df failed");
				}
				offset += abs(ret);
			}

			curClu=fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8);
			if (curClu==0xffff) break;
		}
	}


	pentry=where;

	/*清除fat表项*/
	seed = pentry->FirstCluster;
	if (seed>1)
	while((next = GetFatCluster(seed))!=0xffff)
	{
		ClearFatCluster(seed);
		seed = next;

	}

	if (seed>1) ClearFatCluster( seed );

	/*清除目录表项*/

	size+=addr;
	for (;addr<size;addr+=DIR_ENTRY_SIZE)
	{
		if(lseek(fd,addr,SEEK_SET)<0)
			perror("lseek fd_df failed");
		if(write(fd,&c,1)<0)
			perror("write failed");
	}


	// if(lseek(fd,ret-0x40,SEEK_SET)<0)
	// 	perror("lseek fd_df failed");
	// if(write(fd,&c,1)<0)
	// 	perror("write failed");

	if(WriteFat()<0)
		exit(1);



	return 1;
}

unsigned short getEmptyCluster(int clustersize)
{
	unsigned short cluster, index;
	int i=0;

		/*查询fat表，找到空白簇，保存在clusterno[]中*/
		for(cluster=2;cluster<(bdptor.SectorsPerFAT<<8);cluster++)
		{
			index = cluster<<1;
			if(fatbuf[index]==0x00&&fatbuf[index+1]==0x00)
			{
				clusterno[i] = cluster;

				i++;
				if(i==clustersize)
					break;

			}

		}
		if (i<clustersize) return -1;

		/*在fat表中写入下一簇信息*/
		for(i=0;i<clustersize-1;i++)
		{
			index = clusterno[i]<<1;

			fatbuf[index] = (clusterno[i+1] &  0x00ff);
			fatbuf[index+1] = ((clusterno[i+1] & 0xff00)>>8);


		}
		/*最后一簇写入0xffff*/
		index = clusterno[i]<<1;
		fatbuf[index] = 0xff;
		fatbuf[index+1] = 0xff;
		return clusterno[0];
}

int justWrite(unsigned char * c, int fc) {
	int ret,i=0,cluster_addr,offset;
	int index,clustersize,tmp, curClu;
	unsigned char buf[DIR_ENTRY_SIZE];
	if (fc==1)
	{
			if((ret= lseek(fd,ROOTDIR_OFFSET,SEEK_SET))<0)
				perror("lseek ROOTDIR_OFFSET failed");
			offset = ROOTDIR_OFFSET;
			while(offset < DATA_OFFSET)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}


				/*找出空目录项或已删除的目录项*/ 
				else
				{
					offset = offset-abs(ret);     


					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,c,DIR_ENTRY_SIZE)<0)
						perror("write failed");


					if(WriteFat()<0)
						exit(1);

					return 0;
				}

			}

	}
	else {
		curClu=fc;
		while (1) {
			cluster_addr = DATA_OFFSET + (curClu-2) * CLUSTER_SIZE ;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
			while(offset<cluster_addr +CLUSTER_SIZE)
			{
				if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
					perror("read entry failed");

				offset += abs(ret);

				if(buf[0]!=0xe5&&buf[0]!=0x00)
				{
					while(buf[11] == 0x0f)
					{
						if((ret = read(fd,buf,DIR_ENTRY_SIZE))<0)
							perror("read root dir failed");
						offset +=abs(ret);
					}
				}
				else
				{ 
					offset = offset - abs(ret);      

					if(lseek(fd,offset,SEEK_SET)<0)
						perror("lseek fd_cf failed");
					if(write(fd,c,DIR_ENTRY_SIZE)<0)
						perror("write failed");

					if(WriteFat()<0)
						exit(1);

					return 0;
				}


			}
			if (fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8)==0xffff) 
			{
				tmp=getEmptyCluster(1);
				if (tmp<0) 
				{
					break;
				}
				else {
					fatbuf[curClu<<1]=tmp&0xff;
					fatbuf[(curClu<<1)+1]=(tmp&0xff00)>>8;
				}
			}
			curClu=fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8);
		}
	}

	return 0;	
}


int findAndWrite(int size,int mode, int fc, int newFC)
{
	char * filename=tentry->short_name;
	unsigned char c[DIR_ENTRY_SIZE];
	int index,clustersize,tmp, curClu, date, hms,i;
	time_t now;
	struct tm *timenow;
	time(&now);
	timenow=localtime(&now);
	//printf("%d\n", timenow->tm_wday);
	date = ((timenow->tm_year - 80)<<9) | ((timenow->tm_mon+1)<<5) | timenow->tm_mday;
	hms = ((timenow->tm_hour)<<11) | (timenow->tm_min<<5) | (timenow->tm_sec>>1);
	if (newFC<0)
	{
		clustersize = (size / (CLUSTER_SIZE));
		if (size==0||size % (CLUSTER_SIZE) != 0) ++clustersize;

		index=getEmptyCluster(clustersize);
	}
	else index=newFC;

		for(i=0;i<strlen(filename);i++)
		{
			c[i]=toupper(filename[i]);
		}			
		for(;i<=10;i++)
			c[i]=' ';

		if (mode) c[11] = 0x10; else c[11] = 0x00;

		c[24] = date&0xFF;
		c[25] = date>>8;
		c[22] = hms&0xFF;
		c[23] = hms>>8;

		/*写第一簇的值*/
		c[26] = (index &  0x00ff);
		c[27] = ((index & 0xff00)>>8);

		/*写文件的大小*/
		c[28] = (size &  0x000000ff);
		c[29] = ((size & 0x0000ff00)>>8);
		c[30] = ((size& 0x00ff0000)>>16);
		c[31] = ((size& 0xff000000)>>24);
	justWrite(c,fc);
	return index;
}

int giveAndWrite(int size,int mode, int fc)
{
	return findAndWrite(size,mode,fc,-1);
}

/*
*参数：filename，类型：char，创建文件的名称
size，    类型：int，文件的大小
*返回值：1，成功；-1，失败
*功能：在当前目录下创建文件
*/
int fd_cf(int size, int mode)
{
	int newClu;
	if (path->FirstCluster==0&&path->short_name[0]=='\0'||!strcmp(tentry->short_name,".")||!strcmp(tentry->short_name,"..")) 
	{
		printf("Invalid path\n");
		return -1;
	}
		if(path->FirstCluster==1)  /*往根目录下写文件*/
		{
			if ((newClu=giveAndWrite(size,mode,path->FirstCluster))==0)
				printf("Root dir is full.\n");
			else 
			{
				if (mode) 
				{
					strcpy(tentry->short_name,".");
					findAndWrite(0,mode,newClu,newClu);
					strcpy(tentry->short_name,"..");
					findAndWrite(0,mode,newClu,path->FirstCluster);
				}
				return 1;
			}
		}
		else 
		{
			if ((newClu=giveAndWrite(size,mode,path->FirstCluster))==0)
				printf("Insufficient memory.\n");
			else 
			{
				if (mode) 
				{
					strcpy(tentry->short_name,".");
					findAndWrite(0,mode,newClu,newClu);
					strcpy(tentry->short_name,"..");
					findAndWrite(0,mode,newClu,path->FirstCluster);
				}				
				return 1;
			}
		}
	return -1;
}

int fd_ef(struct Entry * en, char * str, int mode, int len)
{
	unsigned int n,zero=0x00000000,cnt,k,i,tmp;
	unsigned char c=0x00;
	unsigned char *tmps;
	if (en->FirstCluster==1||en->subdir==1||en->short_name[0]=='\0') 
	{
		printf("cw: Invalid target.\n");
		return -1;
	}
	n=en->FirstCluster;
	if (n==0) {
		if ((n=getEmptyCluster(1))<0) 
		{
			printf("Insufficient memory.\n");
			return -1;
		}
		if(lseek(fd,tmpaddr+tmpsize-6,SEEK_SET)<0)
			perror("lseek fd_ef failed");
		c=n&0xFF;
		if(write(fd,&c,1)<0)
			perror("write failed");		
		c=(n&0xFF00)>>8;
		if(write(fd,&c,1)<0)
			perror("write failed");	
		if(write(fd,&zero,4)<0)
			perror("write failed");	
		en->size=0;
		en->FirstCluster=n;
	}
	if (fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8)<2) return -1;
	if (mode<2)
	{
		if (!mode)
		{
			if(lseek(fd,tmpaddr+tmpsize-4,SEEK_SET)<0)
				perror("lseek fd_ef failed");
			if(write(fd,&zero,4)<0)
				perror("write failed");
			for (n=en->FirstCluster;fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8)!=0xffff;n=fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8))
			{
				fatbuf[n<<1]=0x00;
				fatbuf[(n<<1)+1]=0x00;
			}
			fatbuf[n<<1]=0x00;
			fatbuf[(n<<1)+1]=0x00;
			n=en->FirstCluster;
			fatbuf[n<<1]=0xff;
			fatbuf[(n<<1)+1]=0xff;
			en->size=0;
		}
		for (n=en->FirstCluster,cnt=0;fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8)!=0xffff;n=fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8),++cnt);
		++cnt;
		if (cnt*CLUSTER_SIZE<en->size) {
			en->size=cnt*CLUSTER_SIZE;
		}
		cnt=len;
		k=en->size;
		for (n=en->FirstCluster;fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8)!=0xffff&&k>=CLUSTER_SIZE;n=fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8),k-=CLUSTER_SIZE);
		tmp=DATA_OFFSET + (n-2) * CLUSTER_SIZE + k;
		if(lseek(fd,tmp,SEEK_SET)<0)
			perror("lseek fd_ef failed");
		for (i=0;i<cnt;++i)
		{
			if (k>=CLUSTER_SIZE)
			{
				if (fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8)==0xffff)
				{
					tmp=getEmptyCluster(1);
					if (tmp<0) {
						printf("Insufficient memory.\n");
						if(WriteFat()<0)
							exit(1);
						return -1;
					}
					fatbuf[n<<1]=0xff&tmp;
					fatbuf[(n<<1)+1]=(0xff00&tmp)>>8;
					n=tmp;
				}
				else n=fatbuf[n<<1]+(fatbuf[(n<<1)+1]<<8);
				k-=CLUSTER_SIZE;
				tmp=DATA_OFFSET + (n-2) * CLUSTER_SIZE + k;
				if(lseek(fd,tmp,SEEK_SET)<0)
					perror("lseek fd_ef failed");
			}
			if(write(fd,&str[i],1)<0)
				perror("write failed");
			++k;
			++en->size;
		}
		if(lseek(fd,tmpaddr+tmpsize-4,SEEK_SET)<0)
			perror("lseek fd_ef failed");
		if(write(fd,&en->size,4)<0)
			perror("write failed");		
	}
	else if (mode==2)
	{
		if((n = open(str,O_RDONLY))<0) {
			printf("open file %s error.\n",str);
			perror("open failed");
			return -1;
		}
		tmps=(unsigned char *)malloc(1024);
		i=0;
		while ((cnt=read(n,tmps,1024))>0) {
			if (i==0) fd_ef(en,tmps,0,cnt);
			else fd_ef(en,tmps,1,cnt);
			i+=cnt;
		}
		close(n);
		free(tmps);
		printf("%d bytes are written.\n", i);
	}
	else if (mode==3)
	{
		if((n = open(str,O_WRONLY|O_CREAT))<0) {
			printf("open file %s error.\n",str);
			perror("open failed");
			return -1;
		}		
		tmps=(unsigned char *)malloc(1024);
		i=0;
		k=en->size;
		while (k) {
			cnt=k<1024?k:1024;
			if ((cnt=fd_rd(en,tmps,i,cnt))<0) 
			{
				printf("read file error.\n");
				return -1;
			}
			i+=cnt;
			k-=cnt;
			if (write(n,tmps,cnt)<0) 
			{
				printf("write file error.\n");
				return -1;
			}
		}
		close(n);
		free(tmps);
		printf("%d bytes are written.\n", i);
	}
	if(WriteFat()<0)
		exit(1);
	return 1;
}

int fd_rd(struct Entry * en, unsigned char * str, int st, int len) {
	unsigned int offset,cluster_addr,cnt,k,i,curClu,ret;
	unsigned char c=0x00;
	if (en->FirstCluster==1||en->subdir==1||en->short_name[0]=='\0') 
	{
		printf("rd: Invalid target.\n");
		return -1;
	}
	curClu=en->FirstCluster;
	i=0;
	while (st>=CLUSTER_SIZE) {
		st-=CLUSTER_SIZE;
		curClu=fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8);
		if (curClu==0xffff) {
			printf("rd: invalid offset.\n");;
			return -1;
		}
	}
		while (len) {
			cluster_addr = DATA_OFFSET + (curClu-2) * CLUSTER_SIZE ;
			if (st) cluster_addr+=st, st=0;
			if((ret = lseek(fd,cluster_addr,SEEK_SET))<0)
				perror("lseek cluster_addr failed");

			offset = cluster_addr;

			/*只读一簇的内容*/
				if (len>=cluster_addr +CLUSTER_SIZE - offset) {
					if (read(fd,str+i,cluster_addr +CLUSTER_SIZE - offset)<0) {
						return -1;
					}
					len-=cluster_addr +CLUSTER_SIZE - offset;
					i+=cluster_addr +CLUSTER_SIZE - offset;
				}
				else {
					if (read(fd,str+i,len)<0) return -1;
					i+=len;
					len=0;
					break;
				}

			curClu=fatbuf[curClu<<1]+(fatbuf[(curClu<<1)+1]<<8);
			if (curClu==0xffff) break;
		}	
	return i;
}

int cpENTRY(unsigned char * d, int mode) {
	int size=tmpsize, addr=tmpaddr,k;
	unsigned char c=0xe5;
	if(lseek(fd,tmpaddr,SEEK_SET)<0)
		perror("lseek fd_df failed");
	if (mode==0) return read(fd,d,tmpsize);
	if (mode==1) {
		k=read(fd,d,tmpsize);
		size+=addr;
		for (;addr<size;addr+=DIR_ENTRY_SIZE)
		{
			if(lseek(fd,addr,SEEK_SET)<0)
				perror("lseek fd_df failed");
			if(write(fd,&c,1)<0)
				perror("write failed");
		}
		return k;
	}
	return -1;
}

void changeName(unsigned char * c) {
	int i;
	char * filename=tentry->short_name;
	for(i=0;i<strlen(filename);i++)
	{
		c[i]=toupper(filename[i]);
	}			
	for(;i<=10;i++)
		c[i]=' ';
}

void do_usage()
{
	printf("please input a command, including followings:\n\tls [-l] [path]\t\t\tlist all files\n\tcd <dir>\t\t\tchange direcotry\n\ttouch <filename>\t\tcreate a empty file\n\tmkdir <dirname>\t\t\tcreate a empty dir\n\trm [-r] <file>\t\t\tdelete a file or direcotry\n\tcw [words|-a words|-f external_file|-e external_file] <file>\tedit or copy a file\n\texit\t\t\t\texit this system\n");
}

int getCMD(char * cmd, char *op)
{
	char c;
	int st=-1,i=0;
	while (scanf("%c",&c)!=EOF&&c!='\n')
	{
		if (c!=' ')
		{
			if (st<0)
			{
				cmd[i++]=c;
			}
			else
			{
				op[(st<<7)+(i++)]=c;
			}
		}
		else if (i>0) {
			if (st<0) cmd[i]='\0'; else op[(st<<7)+i]='\0';
			++st;
			i=0;
			if (st>=4) break;
		}
	}
	if (i>0) {
		if (st<0) cmd[i]='\0'; else op[(st<<7)+i]='\0';
		++st;
		i=0;
	}
	return st+1;
}

int main()
{
	char input[10];
	char op[512];
	char tmp[1024];
	int size=0,cnt,oa,ob,offset;
	if((fd = open(DEVNAME,O_RDWR))<0)
		perror("open failed");
	ScanBootSector();
	do_usage();
	path=(struct Entry *)malloc(sizeof(struct Entry));
	while (1)
	{
		printf(">");
		if (!(cnt=getCMD(input,op))) continue;
		if (ReadFat()<0)
			exit(1);
		if (curdir!=NULL&&curdir->FirstCluster>1&&fatbuf[curdir->FirstCluster<<1]==0x00&&fatbuf[(curdir->FirstCluster<<1)+1]==0x00)
		{
			curdir=NULL;
			dirno=0;
		}
		if (strcmp(input, "exit") == 0)
			break;
		else if (strcmp(input, "ls") == 0 && cnt<=3)
		{
			if (cnt==1) fd_ls(0,curdir);
			if (cnt==2) {
				if (strcmp(op,"-l")==0)
					fd_ls(1,curdir);
				else
				{
					getPath(path,op,1);
					fd_ls(0,path);
				}
			}
			if (cnt==3) {
				if (strcmp(op,"-l")==0)
				{
					getPath(path,op+128,1);
					fd_ls(1,path);
				}
				else if (strcmp(op+128,"-l")==0)
				{
					getPath(path,op,1);
					fd_ls(1,path);
				}
			}
		}
		else if(strcmp(input, "cd") == 0 && cnt==2)
		{
			getPath(path,op,1);
			if (path->FirstCluster==1)
			{
				curdir=NULL;
				dirno=0;
			}
			else if (path->short_name[0]!='\0')
			{
				curdir=(struct Entry*)malloc(sizeof(struct Entry));
				*curdir=*path;
				for (cnt=0;cnt<=tmpno;++cnt) 
				{
					fatherdir[cnt]=fathertmp[cnt];
				}
				dirno=tmpno;
			}
			else printf("cd: no such dir\n");
		}
		else if(strcmp(input, "rm") == 0 && cnt<=3&&cnt>1)
		{
			if (cnt==2)
			{
				getPath(path,op,0);
				fd_df(path,tmpaddr,tmpsize,0);
			}
			if (cnt==3) {
				if (strcmp(op,"-r")==0)
				{
					getPath(path,op+128,1);
					fd_df(path,tmpaddr,tmpsize,1);
				}
				else if (strcmp(op+128,"-r")==0)
				{
					getPath(path,op,1);
					fd_df(path,tmpaddr,tmpsize,1);
				}
				else printf("rm: invalid sentence\n");
			}
		}
		else if(strcmp(input, "cw") == 0 && cnt<=4&&cnt>2)
		{
			if (cnt==3)
			{
				getPath(path,op+128,0);
				fd_ef(path,op,0,strlen(op));
			}
			if (cnt==4) {
				if (strcmp(op,"-a")==0)
				{
					getPath(path,op+256,0);
					fd_ef(path,op+128,1,strlen(op+128));
				}
				else if (strcmp(op,"-f")==0)
				{
					getPath(path,op+256,0);
					fd_ef(path,op+128,2,0);
				}
				else if (strcmp(op,"-e")==0)
				{
					getPath(path,op+256,0);
					fd_ef(path,op+128,3,0);
				}
				else printf("cw: invalid sentence\n");
			}
		}
		else if(strcmp(input, "touch") == 0 && cnt==2)
		{
			getfPath(path,op,0);
			fd_cf(0,0);
		}
		else if(strcmp(input, "mkdir") == 0 && cnt==2)
		{
			getfPath(path,op,1);
			fd_cf(0,1);
		}
		else if (strcmp(input, "rd") == 0 && cnt>1 && cnt<5) {
			getPath(path,op,0);
			offset=0;
			if (cnt==2) {
				oa=80;
			}
			if (cnt==3) {
				oa=atoi(op+128);
			}
			if (cnt==4) {
				offset=atoi(op+128);				
				oa=atoi(op+256);				
			}
			while (oa) {
				ob=fd_rd(path,tmp,offset,oa<1000?oa:1000);
				if (ob<0) break;
				oa-=ob;
				offset+=ob;
				tmp[ob]='\0';
				printf("%s", tmp);
			}
			printf("\n");
		}
		else if (strcmp(input, "mv") == 0 && cnt == 3) {
			getPath(path,op,3);
			if (path->short_name[0]=='\0') continue;
			getfPath(path,op+128,path->subdir);
			if (path->FirstCluster==0&&path->short_name[0]=='\0'||!strcmp(tentry->short_name,".")||!strcmp(tentry->short_name,"..")) 
			{
				printf("Invalid path\n");
				continue;
			}
			getPath(path,op,3);
			if ((offset=cpENTRY(tmp,1))<0) continue;
			getfPath(path,op+128,path->subdir);
			changeName(tmp);
			justWrite(tmp,path->FirstCluster);
		}
		else
			do_usage();
		
	}	

	return 0;
}
