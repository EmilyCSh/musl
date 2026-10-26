#ifndef _FTS_H
#define _FTS_H

#include <stddef.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _fts FTS;

typedef struct _ftsent {
	struct _ftsent *fts_cycle;
	struct _ftsent *fts_parent;
	struct _ftsent *fts_link;
	long fts_number;
	void *fts_pointer;
	char *fts_accpath;
	char *fts_path;
	int fts_errno;
	int fts_symfd;
	size_t fts_pathlen;
	size_t fts_namelen;
	ino_t fts_ino;
	dev_t fts_dev;
	nlink_t fts_nlink;
	int fts_level;
	unsigned short fts_info;
	unsigned short fts_flags;
	unsigned short fts_instr;
	unsigned short fts_spare;
	struct stat *fts_statp;
	char fts_name[1];
} FTSENT;

#define FTS_COMFOLLOW 0x0001
#define FTS_LOGICAL   0x0002
#define FTS_NOCHDIR   0x0004
#define FTS_NOSTAT    0x0008
#define FTS_PHYSICAL  0x0010
#define FTS_SEEDOT    0x0020
#define FTS_XDEV      0x0040
#define FTS_WHITEOUT  0x0080
#define FTS_OPTIONMASK 0x00ff

#define FTS_NAMEONLY  0x1000

#define FTS_ROOTPARENTLEVEL (-1)
#define FTS_ROOTLEVEL 0
#define FTS_MAXLEVEL 0x7fffffff

#define FTS_D      1
#define FTS_DC     2
#define FTS_DEFAULT 3
#define FTS_DNR    4
#define FTS_DOT    5
#define FTS_DP     6
#define FTS_ERR    7
#define FTS_F      8
#define FTS_INIT   9
#define FTS_NS     10
#define FTS_NSOK   11
#define FTS_SL     12
#define FTS_SLNONE 13

#define FTS_DONTCHDIR 0x01
#define FTS_SYMFOLLOW 0x02

#define FTS_AGAIN   1
#define FTS_FOLLOW  2
#define FTS_NOINSTR 3
#define FTS_SKIP    4

FTSENT *fts_children(FTS *, int);
int fts_close(FTS *);
FTS *fts_open(char * const *, int, int (*)(const FTSENT **, const FTSENT **));
FTSENT *fts_read(FTS *);
int fts_set(FTS *, FTSENT *, int);

#ifdef __cplusplus
}
#endif

#endif