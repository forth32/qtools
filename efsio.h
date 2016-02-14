// Атрибуты типов файлов, поддерживаемых EFS
#undef S_IFIFO 
#undef S_IFCHR 
#undef S_IFDIR 
#undef S_IFBLK 
#undef S_IFREG 
#undef S_IFLNK 
#undef S_IFSOCK
#undef S_IFITM 
#undef S_IFMT  

#define S_IFIFO  0010000        /**< FIFO */
#define S_IFCHR  0020000        /**< Character device */
#define S_IFDIR  0040000        /**< Directory */
#define S_IFBLK  0060000        /**< Block device */
#define S_IFREG  0100000        /**< Regular file */
#define S_IFLNK  0120000        /**< Symlink */
#define S_IFSOCK 0140000        /**< Socket */
#define S_IFITM  0160000        /**< Item File */
#define S_IFMT   0170000        /**< Mask of all values */

 // Формат пакета состояния файла
 //------------------------------------------
 // pkt+08 - атрибуты
 // pkt+0c - размер
 // pkt+10 - счетчик ссылок
 // pkt+14 - дата создания
 // pkt+18 - дата модификации
 // pkt+1c - дата последнего доступа

struct fileinfo {
  unsigned int attr;
  unsigned int size;
  unsigned int filecnt;
  time_t ctime;
  time_t mtime;
  time_t atime;
};

// элемент описания каталога

struct efs_dirent {
  uint32 dirp;             /* Directory pointer.                           */
  int32  seqno;            /* Sequence number of directory entry           */
  int32  diag_errno;       /* Error code if error, 0 otherwise             */
  int32  entry_type;       /* 0 if file, 1 if directory, 2 if symlink      */
  int32  mode;             /* File mode                                    */
  int32  size;             /* File size in bytes                           */
  int32  atime;            /* Time of last access                          */
  int32  mtime;            /* Time of last modification                    */
  int32  ctime;            /* Time of last status change                   */
  char   entry_name[80];    /* Name of directory entry (not full pathname)  */
};

extern static int efs_errno;


int send_efs_cmd(int cmd,void* reqbuf,int reqlen, void* respbuf);
int efs_stat(char* filename, struct fileinfo* fi);
int efs_opendir(char* path);
int efs_closedir(int dirp);
int efs_readdir(int dirp);

