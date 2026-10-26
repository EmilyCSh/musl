#define _GNU_SOURCE
#include <fts.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

struct fts_node {
	struct fts_node *parent;
	struct fts_node *child;
	struct fts_node *tail;
	struct fts_node *next;
	char *path;
	struct stat stat;
	int stat_valid;
	int expanded;
	int names_only;
	int post_returned;
	int follow;
	FTSENT ent;
};

struct _fts {
	struct fts_node *parent;
	struct fts_node *roots;
	struct fts_node *root_tail;
	struct fts_node *current;
	int options;
	int (*compar)(const FTSENT **, const FTSENT **);
};

#define NODE(e) ((struct fts_node *)((char *)(e) - offsetof(struct fts_node, ent)))

static void free_nodes(struct fts_node *node)
{
	while (node) {
		struct fts_node *next = node->next;
		free_nodes(node->child);
		free(node->path);
		free(node);
		node = next;
	}
}

static struct fts_node *new_node(struct fts_node *parent, const char *path,
	const char *name, int level)
{
	struct fts_node *node;
	size_t pathlen = strlen(path), namelen = strlen(name);
	size_t sep = pathlen && path[pathlen-1] != '/';
	char *p;

	if (pathlen > (size_t)-1 - namelen - sep - 1 ||
	    namelen > (size_t)-1 - sizeof *node) {
		errno = ENAMETOOLONG;
		return NULL;
	}

	node = calloc(1, sizeof *node + namelen);

	if (!node) {
        return NULL;
    }

	p = malloc(pathlen + sep + namelen + 1);
	if (!p) {
		free(node);
		return NULL;
	}

	memcpy(p, path, pathlen);

	if (sep) {
        p[pathlen++] = '/';
    }

	memcpy(p + pathlen, name, namelen + 1);
	node->parent = parent;
	node->path = p;
	node->ent.fts_parent = parent ? &parent->ent : NULL;
	node->ent.fts_path = p;
	node->ent.fts_accpath = p;
	node->ent.fts_pathlen = strlen(p);
	node->ent.fts_namelen = namelen;
	node->ent.fts_level = level;
	node->ent.fts_instr = FTS_NOINSTR;
	node->ent.fts_statp = &node->stat;
	memcpy(node->ent.fts_name, name, namelen + 1);

	return node;
}

static void sync_links(struct fts_node *node)
{
	for (; node; node = node->next) {
		node->ent.fts_link = node->next ? &node->next->ent : NULL;
	}
}

static struct fts_node *sort_nodes(struct _fts *fts, struct fts_node *head)
{
	struct fts_node *sorted = NULL;
	if (!fts->compar) {
		sync_links(head);
		return head;
	}

	while (head) {
		struct fts_node *node = head;
		struct fts_node **at = &sorted;
		const FTSENT *entry = &node->ent;
		head = head->next;

		while (*at) {
			const FTSENT *next = &(*at)->ent;

			if (fts->compar(&entry, &next) < 0) {
                break;
            }

			at = &(*at)->next;
		}

		node->next = *at;
		*at = node;
	}
	sync_links(sorted);
	return sorted;
}

static unsigned short classify(struct _fts *fts, struct fts_node *node,
	const struct stat *st)
{
	struct fts_node *p;
	node->stat = *st;
	node->stat_valid = 1;
	node->ent.fts_ino = st->st_ino;
	node->ent.fts_dev = st->st_dev;
	node->ent.fts_nlink = st->st_nlink;
	node->ent.fts_errno = 0;
	node->ent.fts_cycle = NULL;

	if (S_ISDIR(st->st_mode)) {
		for (p = node->parent; p && p != fts->parent; p = p->parent) {
			if (p->stat_valid && p->ent.fts_ino == st->st_ino &&
			    p->ent.fts_dev == st->st_dev) {
				node->ent.fts_cycle = &p->ent;
				return FTS_DC;
			}
		}

		if (node->ent.fts_level > FTS_ROOTLEVEL &&
		    (!strcmp(node->ent.fts_name, ".") ||
		     !strcmp(node->ent.fts_name, ".."))) {
			return FTS_DOT;
        }

		return FTS_D;
	}

	if (S_ISLNK(st->st_mode)) {
        return FTS_SL;
    }

	if (S_ISREG(st->st_mode)) {
        return FTS_F;
    }

	return FTS_DEFAULT;
}

static int stat_node(struct _fts *fts, struct fts_node *node, int follow,
	int dirfd, const char *name)
{
	struct stat st;
	int flags = follow ? 0 : AT_SYMLINK_NOFOLLOW;

	if (fstatat(dirfd, name, &st, flags)) {
		int saved_errno = errno;

		if (follow && !fstatat(dirfd, name, &st, AT_SYMLINK_NOFOLLOW) &&
		    S_ISLNK(st.st_mode)) {
			node->stat = st;
			node->stat_valid = 1;
			node->ent.fts_ino = st.st_ino;
			node->ent.fts_dev = st.st_dev;
			node->ent.fts_nlink = st.st_nlink;
			node->ent.fts_errno = 0;
			node->ent.fts_info = FTS_SLNONE;
			return 0;
		}

		node->stat_valid = 0;
		node->ent.fts_errno = saved_errno;
		node->ent.fts_info = FTS_NS;
		return -1;
	}
	node->ent.fts_info = classify(fts, node, &st);
	return 0;
}

static struct fts_node *new_root(struct fts_node *parent, const char *path)
{
	char *copy = strdup(path);
	char *base;
	char *full;
	struct fts_node *node;
	size_t len;

	if (!copy) {
        return NULL;
    }

	len = strlen(copy);

	while (len > 1 && copy[len-1] == '/') { 
        copy[--len] = 0; 
    }

	base = strrchr(copy, '/');

	if (base && base[1]) {
        base++;
    } else if (base) {
        base = copy + len - 1;
    } else {
        base = copy;
    }

	node = new_node(parent, "", base, FTS_ROOTLEVEL);

	free(copy);
	if (!node) {
        return NULL;
    }

	full = strdup(path);
	if (!full) {
		free(node->path);
		free(node);
		return NULL;
	}

	free(node->path);
	node->path = full;
	node->ent.fts_path = full;
	node->ent.fts_accpath = full;
	node->ent.fts_pathlen = strlen(full);
	return node;
}

static int expand(struct _fts *fts, struct fts_node *node, int names_only)
{
	DIR *dir;
	struct dirent *de;
	struct fts_node *head = NULL, *tail = NULL;
	int saved_errno;

	if (node->expanded && (!node->names_only || names_only)) {
        return 0;
    }

	if (node->expanded) {
		free_nodes(node->child);
		node->child = NULL;
		node->tail = NULL;
		node->expanded = 0;
		node->names_only = 0;
	}

	dir = opendir(node->path);
	if (!dir) {
		node->ent.fts_errno = errno;
		return -1;
	}

	errno = 0;
	while ((de = readdir(dir))) {
		struct fts_node *child;
		const char *name = de->d_name;

		if (!(fts->options & FTS_SEEDOT) &&
		    (!strcmp(name, ".") || !strcmp(name, ".."))) {
			continue;
        }

		child = new_node(node, node->path, name,
		    node->ent.fts_level < FTS_MAXLEVEL ? node->ent.fts_level + 1 : FTS_MAXLEVEL);

		if (!child) {
            goto error;
        }

		if (names_only || ((fts->options & FTS_NOSTAT) &&
		    !(fts->options & FTS_LOGICAL) &&
		    de->d_type != DT_DIR && de->d_type != DT_UNKNOWN)) {
			child->ent.fts_info = FTS_NSOK;
		} else {
			stat_node(fts, child, (fts->options & FTS_LOGICAL) != 0,
			    dirfd(dir), name);
		}

		if (tail) {
            tail->next = child;
        } else {
            head = child;
        }

		tail = child;
	}

	saved_errno = errno;
	closedir(dir);

	if (saved_errno) {
		errno = saved_errno;
		goto error_no_dir;
	}

	node->child = sort_nodes(fts, head);
	node->tail = tail;

	if (fts->compar && node->child) {
		tail = node->child;

		while (tail->next) {
            tail = tail->next;
        }

		node->tail = tail;
	}

	node->expanded = 1;
	node->names_only = names_only;
	return 0;

error:
	saved_errno = errno;
	closedir(dir);
error_no_dir:
	free_nodes(head);
	errno = saved_errno;
	node->ent.fts_errno = saved_errno;
	return -1;
}

FTS *fts_open(char * const *paths, int options,
	int (*compar)(const FTSENT **, const FTSENT **))
{
	struct _fts *fts;
	struct fts_node *parent;

	if (!paths || !*paths || (options & ~FTS_OPTIONMASK) ||
	    ((options & FTS_LOGICAL) && (options & FTS_PHYSICAL))) {
		errno = EINVAL;
		return NULL;
	}

	fts = calloc(1, sizeof *fts);

	if (!fts) { 
        return NULL; 
    }

	fts->options = options;
	fts->compar = compar;

	parent = new_node(NULL, "", "", FTS_ROOTPARENTLEVEL);
	if (!parent) {
		free(fts);
		return NULL;
	}

	parent->ent.fts_info = FTS_INIT;
	fts->parent = parent;
	fts->current = parent;

	for (; *paths; paths++) {
		struct fts_node *root;
		const char *path = *paths;
		root = new_root(parent, path);
		if (!root) {
            goto fail;

        }
		if (fts->root_tail) {
            fts->root_tail->next = root;
        } else {
            fts->roots = root;
        }

		fts->root_tail = root;
		stat_node(fts, root,
		    (options & FTS_LOGICAL) || (options & FTS_COMFOLLOW), AT_FDCWD, path);
	}

	fts->roots = sort_nodes(fts, fts->roots);
	fts->parent->child = fts->roots;
	sync_links(fts->roots);
	return fts;

fail:
	free_nodes(fts->roots);
	free(parent->path);
	free(parent);
	free(fts);
	return NULL;
}

static struct fts_node *root_of(struct _fts *fts, struct fts_node *node)
{
	while (node->parent && node->parent != fts->parent) {
        node = node->parent;
    }

	return node;
}

static struct fts_node *advance(struct _fts *fts, struct fts_node *node)
{
	struct fts_node *parent;

	if (node->next) {
        return node->next;
    }

	parent = node->parent;

	if (!parent || parent == fts->parent) {
        return NULL;
    }

	if (parent->ent.fts_info == FTS_D && !parent->post_returned) {
		parent->post_returned = 1;
		parent->ent.fts_info = FTS_DP;
		return parent;
	}

	return advance(fts, parent);
}

FTSENT *fts_read(FTS *fts)
{
	struct fts_node *node;
	int instr;

	if (!fts || !fts->parent) {
		errno = EINVAL;
		return NULL;
	}

	if (fts->current == fts->parent) {
		node = fts->roots;
	} else {
		node = fts->current;
		instr = node->ent.fts_instr;
		node->ent.fts_instr = FTS_NOINSTR;
		if (instr == FTS_AGAIN || instr == FTS_FOLLOW) {
			int follow = instr == FTS_FOLLOW || (fts->options & FTS_LOGICAL) ||
			    ((fts->options & FTS_COMFOLLOW) && node->ent.fts_level == FTS_ROOTLEVEL);
			stat_node(fts, node, follow, AT_FDCWD, node->path);
			fts->current = node;
			return &node->ent;
		}

		if (node->ent.fts_info == FTS_D) {
			struct fts_node *root = root_of(fts, node);

			if (instr == FTS_SKIP ||
			    ((fts->options & FTS_XDEV) && node != root &&
			     node->ent.fts_dev != root->ent.fts_dev)) {
				node->post_returned = 1;
				node->ent.fts_info = FTS_DP;
			} else if (expand(fts, node, 0)) {
				node->ent.fts_info = FTS_DNR;
			} else if (node->child) {
				node = node->child;
			} else {
				node->post_returned = 1;
				node->ent.fts_info = FTS_DP;
			}

			fts->current = node;
			return &node->ent;
		}

		node = advance(fts, node);
	}

	if (!node) {
		fts->current = NULL;
		errno = 0;
		return NULL;
	}

	fts->current = node;
	return &node->ent;
}

FTSENT *fts_children(FTS *fts, int options)
{
	struct fts_node *node;
	if (!fts || (options && options != FTS_NAMEONLY)) {
		errno = EINVAL;
		return NULL;
	}

	node = fts->current;
	if (!node) {
		errno = 0;
		return NULL;
	}

	if (node == fts->parent) {
        return node->child ? &node->child->ent : NULL;
    }

	if (node->ent.fts_info != FTS_D) {
		errno = 0;
		return NULL;
	}

	if (expand(fts, node, options == FTS_NAMEONLY)) {
        return NULL;
    }

	errno = 0;
	return node->child ? &node->child->ent : NULL;
}

int fts_set(FTS *fts, FTSENT *ent, int instr)
{
	if (!fts || !ent || (instr != FTS_AGAIN && instr != FTS_FOLLOW &&
	    instr != FTS_NOINSTR && instr != FTS_SKIP)) {
		errno = EINVAL;
		return 1;
	}

	NODE(ent)->ent.fts_instr = instr;
	return 0;
}

int fts_close(FTS *fts)
{
	if (!fts) {
		errno = EINVAL;
		return -1;
	}
    
	free_nodes(fts->roots);
	free(fts->parent->path);
	free(fts->parent);
	free(fts);
	return 0;
}