#ifndef FILESYS_H
#define FILESYS_H
#include<stddef.h>
#include<time.h>
#define DEVNAME "/home/sdbv"
#define DIR_ENTRY_SIZE 32
#define SECTOR_SIZE 512
#define CLUSTER_SIZE ((bdptor.SectorsPerCluster)<<9)
#define FAT_ONE_OFFSET SECTOR_SIZE
#define FAT_TWO_OFFSET (FAT_ONE_OFFSET+(bdptor.SectorsPerFAT<<9))
#define ROOTDIR_OFFSET (FAT_ONE_OFFSET+(bdptor.SectorsPerFAT<<10))
#define DATA_OFFSET (ROOTDIR_OFFSET+(bdptor.RootDirEntries<<5))



/*属性位掩码*/
#define ATTR_READONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VLABEL 0x08
#define ATTR_SUBDIR 0x10
#define ATTR_ARCHIVE 0x20

/*时间掩码 5：6：5 */
#define MASK_HOUR 0xf800 
#define MASK_MIN 0x07e0
#define MASK_SEC 0x001f

/*日期掩码*/
#define MASK_YEAR 0xfe00
#define MASK_MONTH 0x01e0
#define MASK_DAY 0x001f

struct BootDescriptor_t{
	unsigned char Oem_name[9]; /*0x03-0x0a*/
	int BytesPerSector;/*0x0b-0x0c*/
	int SectorsPerCluster;/*0x0d*/
	int ReservedSectors;/*0x0e-0x0f*/
	int FATs;/*0x10*/
	int RootDirEntries;/*0x11-0x12*/
	int LogicSectors;/*0x13-0x14*/
	int MediaType;/*0x15*/
	int SectorsPerFAT;/*0x16-0x17*/
	int SectorsPerTrack;/*0x18-0x19*/
	int Heads;/*0x1a-0x1b*/
	int HiddenSectors;/*0x1c-0x1d*/
};

struct Entry{
	unsigned char short_name[12];/*字节0-10，11字节的短文件名*/
	unsigned char long_name[27];/*未使用，26字节的长文件名*/
	unsigned short year,month,day;/*22-23字节*/
	unsigned short hour,min,sec;/*24-25字节*/
	unsigned short FirstCluster;/*26-27字节*/
	unsigned int size;/*28-31字节*/
	/*属性值11字节
	*76543210
	*NNADVSHRN未使用
	*/

	unsigned char readonly:1;
	unsigned char hidden:1;
	unsigned char system:1;
	unsigned char vlabel:1;
	unsigned char subdir:1;
	unsigned char archive:1;
};

int fd_ls(int detail, struct Entry * curdir);
int fd_df(struct Entry * where, int addr, int size, int mode);
int fd_cf(int size, int mode);
int fd_rd(struct Entry * en, unsigned char * str, int st, int len);
int fd_ef(struct Entry * en, char * str, int mode, int len);

void findDate(unsigned short *year,
			unsigned short *month,
			unsigned short *day,
			unsigned char info[2]);

void findTime(unsigned short *hour,
			unsigned short *min,
			unsigned short *sec,
			unsigned char info[2]);
int ReadFat();
int WriteFat();
void ScanBootSector();
void ScanRootEntry();
int ScanEntry(char *entryname,struct Entry *pentry,int mode);
int GetEntry(struct Entry *entry);
void FileNameFormat(unsigned char *name);
unsigned short GetFatCluster(unsigned short prev);
void ClearFatCluster(unsigned short cluster);

int fd;
struct BootDescriptor_t bdptor;
struct Entry *curdir = NULL;
struct Entry *path = NULL;
struct Entry *tentry = NULL;
int dirno = 0, tmpno=0;/*代表目录的层数*/
int tmpaddr,tmpsize;
struct Entry* fatherdir[16];
struct Entry* fathertmp[16];
unsigned short clusterno[10000];
unsigned char *fatbuf;

#endif

