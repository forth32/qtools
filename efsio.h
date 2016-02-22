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


//
//  Коды диагностических EFS-команд
//
#define EFS2_DIAG_HELLO     0 /* Parameter negotiation packet               */
#define EFS2_DIAG_QUERY     1 /* Send information about EFS2 params         */
#define EFS2_DIAG_OPEN      2 /* Open a file                                */
#define EFS2_DIAG_CLOSE     3 /* Close a file                               */
#define EFS2_DIAG_READ      4 /* Read a file                                */
#define EFS2_DIAG_WRITE     5 /* Write a file                               */
#define EFS2_DIAG_SYMLINK   6 /* Create a symbolic link                     */
#define EFS2_DIAG_READLINK  7 /* Read a symbolic link                       */
#define EFS2_DIAG_UNLINK    8 /* Remove a symbolic link or file             */
#define EFS2_DIAG_MKDIR     9 /* Create a directory                         */
#define EFS2_DIAG_RMDIR    10 /* Remove a directory                         */
#define EFS2_DIAG_OPENDIR  11 /* Open a directory for reading               */
#define EFS2_DIAG_READDIR  12 /* Read a directory                           */
#define EFS2_DIAG_CLOSEDIR 13 /* Close an open directory                    */
#define EFS2_DIAG_RENAME   14 /* Rename a file or directory                 */
#define EFS2_DIAG_STAT     15 /* Obtain information about a named file      */
#define EFS2_DIAG_LSTAT    16 /* Obtain information about a symbolic link   */
#define EFS2_DIAG_FSTAT    17 /* Obtain information about a file descriptor */
#define EFS2_DIAG_CHMOD    18 /* Change file permissions                    */
#define EFS2_DIAG_STATFS   19 /* Obtain file system information             */
#define EFS2_DIAG_ACCESS   20 /* Check a named file for accessibility       */
#define EFS2_DIAG_DEV_INFO    21 /* Get flash device info             */
#define EFS2_DIAG_FACT_IMAGE_START 22 /* Start data output for Factory Image*/
#define EFS2_DIAG_FACT_IMAGE_READ  23 /* Get data for Factory Image         */
#define EFS2_DIAG_FACT_IMAGE_END   24 /* End data output for Factory Image  */
#define EFS2_DIAG_PREP_FACT_IMAGE  25 /* Prepare file system for image dump */
#define EFS2_DIAG_PUT_DEPRECATED   26 /* Write an EFS item file             */
#define EFS2_DIAG_GET_DEPRECATED   27 /* Read an EFS item file              */
#define EFS2_DIAG_ERROR    28 /* Semd an EFS Error Packet back through DIAG */
#define EFS2_DIAG_EXTENDED_INFO 29 /* Get Extra information.                */
#define EFS2_DIAG_CHOWN         30 /* Change ownership                      */
#define EFS2_DIAG_BENCHMARK_START_TEST  31 /* Start Benchmark               */
#define EFS2_DIAG_BENCHMARK_GET_RESULTS 32 /* Get Benchmark Report          */
#define EFS2_DIAG_BENCHMARK_INIT        33 /* Init/Reset Benchmark          */
#define EFS2_DIAG_SET_RESERVATION       34 /* Set group reservation         */
#define EFS2_DIAG_SET_QUOTA             35 /* Set group quota               */
#define EFS2_DIAG_GET_GROUP_INFO        36 /* Retrieve Q&R values           */
#define EFS2_DIAG_DELTREE               37 /* Delete a Directory Tree       */
#define EFS2_DIAG_PUT                   38 /* Write a EFS item file in order*/
#define EFS2_DIAG_GET                   39 /* Read a EFS item file in order */
#define EFS2_DIAG_TRUNCATE              40 /* Truncate a file by the name   */
#define EFS2_DIAG_FTRUNCATE           41 /* Truncate a file by a descriptor */
#define EFS2_DIAG_STATVFS_V2       42 /* Obtains extensive file system info */
#define EFS2_DIAG_MD5SUM              43 /* Calculate md5 hash of a file  */
#define EFS2_DIAG_HOTPLUG_FORMAT        44 /* Format a Connected device */
#define EFS2_DIAG_SHRED                 45 /* Shred obsolete file content. */
#define EFS2_DIAG_SET_IDLE_DEV_EVT_DUR  46 /* Idle_dev_evt_dur value in mins */
#define EFS2_DIAG_HOTPLUG_DEVICE_INFO   47 /* get the hotplug device info.  */
#define EFS2_DIAG_SYNC_NO_WAIT    48 /* non-blocking sync of remotefs device */
#define EFS2_DIAG_SYNC_GET_STATUS  49 /* query previously issued sync status */
#define EFS2_DIAG_TRUNCATE64       50 /* Truncate a file by the name.        */
#define EFS2_DIAG_FTRUNCATE64      51 /* Truncate a file by a descriptor.    */
#define EFS2_DIAG_LSEEK64          52 /* Seek to requested file offset.      */
#define EFS2_DIAG_MAKE_GOLDEN_COPY  53 /* Make golden copy for Remote Storage*/
#define EFS2_DIAG_FILESYSTEM_IMAGE_OPEN 54 /*Open FileSystem Image extraction*/
#define EFS2_DIAG_FILESYSTEM_IMAGE_READ 55 /* Read File System Image.        */
#define EFS2_DIAG_FILESYSTEM_IMAGE_CLOSE 56 /* Close File System Image.      */

// Структура описания файла

struct efs_filestat {
  int32 diag_errno;        /* Error code if error, 0 otherwise             */
  int32 mode;              /* File mode                                    */
  int32 size;              /* File size in bytes                           */
  int32 nlink;             /* Number of links                              */
  int32 atime;             /* Time of last access                          */
  int32 mtime;             /* Time of last modification                    */
  int32 ctime;             /* Time of last status change                   */
};


// элемент описания каталога

PACKED_ON(efs_dirent) {
  uint32 dirp;             /* Directory pointer.                           */
  int32  seqno;            /* Sequence number of directory entry           */
  int32  diag_errno;       /* Error code if error, 0 otherwise             */
  int32  entry_type;       /* 0 if file, 1 if directory, 2 if symlink      */
  int32  mode;             /* File mode                                    */
  int32  size;             /* File size in bytes                           */
  int32 atime;            /* Time of last access                          */
  int32 mtime;            /* Time of last modification                    */
  int32 ctime;            /* Time of last status change                   */
  char   name[100];         /* Name of directory entry (not full pathname)  */
};
PACKED_OFF

PACKED_ON(efs_factimage_rsp) {
  int32 diag_errno;         /* Error code if error, 0 otherwise */
  int8 stream_state;        /* 1 = more data, 0 = all done      */
  int8 info_cluster_sent;   /* Info cluster sent indicator      */
  int16 cluster_map_seqno;  /* Cluster map sequence number      */
  int32 cluster_data_seqno; /* Cluster data sequence number     */
  char page[1024];             /* Page data buffer */
};
PACKED_OFF

int send_efs_cmd(int cmd,void* reqbuf,int reqlen, void* respbuf);
int efs_stat(char* filename, struct efs_filestat* fi);
int efs_opendir(char* path);
int efs_closedir(int dirp);
int efs_readdir(int dirp, int seq,struct efs_dirent* rsp);
int efs_get_errno();
int efs_open(char* filename, int oflag);
int efs_read(int fd, char* buf, int size, int offset);
int efs_close(int fd);
int efs_write(int fd,char* buf, int size, int offset);
int efs_rmdir(char* dirname);
int efs_unlink(char* name);
int efs_mkdir(char* name, int mode);
int efs_prep_factimage();
int efs_factimage_start();
int efs_factimage_read(int state, int sent, int map, int data, struct efs_factimage_rsp* rsp);
int efs_factimage_end();
void set_altflag(int val);




