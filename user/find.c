#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"  
#include "user/user.h"

#define PATH_BUF 512

static int   g_has_exec = 0;
static char *g_exec_argv_base[MAXARG];      
static int   g_exec_argc_base = 0;          

static void
spawn_exec_for_match(const char *filepath)
{
  if (!g_has_exec) return;

  char *argv[MAXARG];
  int   argc = 0;

  // copy base args
  for (int i = 0; i < g_exec_argc_base && argc < MAXARG - 1; i++) {
    argv[argc++] = g_exec_argv_base[i];
  }

  // append matched path
  if (argc < MAXARG - 1) {
    argv[argc++] = (char *)filepath;
  }
  argv[argc] = 0;

  int pid = fork();
  if (pid < 0) {
    fprintf(2, "find: fork failed\n");
    return;
  }
  if (pid == 0) {
    exec(argv[0], argv);
    fprintf(2, "find: exec %s failed\n", argv[0]);
    exit(1);
  }
  wait(0);
}

static int
is_dot_or_dotdot(const char *name)
{
  return (name[0] == '.' &&
         (name[1] == 0 || (name[1] == '.' && name[2] == 0)));
}

static void
print_match_or_exec(const char *fullpath)
{
  if (g_has_exec) {
    spawn_exec_for_match(fullpath);
  } else {
    printf("%s\n", fullpath);
  }
}

static void
walk(const char *root, const char *target_name)
{
  int fd;
  struct stat st;

  if ((fd = open(root, O_RDONLY)) < 0) {
    fprintf(2, "find: cannot open %s\n", root);
    return;
  }
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", root);
    close(fd);
    return;
  }

  // if root itself is a file, test its basename
  if (st.type == T_FILE) {
    // extract last component of root
    const char *base = root;
    for (const char *p = root; *p; p++) {
      if (*p == '/') base = p + 1;
    }
    if (strcmp(base, target_name) == 0) {
      print_match_or_exec(root);
    }
    close(fd);
    return;
  }

  if (st.type == T_DIR) {
    char pathbuf[PATH_BUF];
    struct dirent de;

    int n = 0;
    for (n = 0; root[n] && n < PATH_BUF - 1; n++) {
      pathbuf[n] = root[n];
    }
    if (n >= PATH_BUF - 1) {
      fprintf(2, "find: path too long: %s\n", root);
      close(fd);
      return;
    }
    if (n == 0 || pathbuf[n-1] != '/') {
      pathbuf[n++] = '/';
    }

    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) continue;
      if (is_dot_or_dotdot(de.name)) continue;

      char *p = pathbuf + n;
      int i = 0;
      for (i = 0; i < DIRSIZ && de.name[i]; i++) {
        *p++ = de.name[i];
        if (p - pathbuf >= PATH_BUF - 1) break;
      }
      *p = 0;

      int cfd = open(pathbuf, O_RDONLY);
      if (cfd < 0) {
        continue;
      }
      struct stat cst;
      if (fstat(cfd, &cst) < 0) {
        close(cfd);
        continue;
      }

      if (cst.type == T_DIR) {
        close(cfd);
        walk(pathbuf, target_name);
      } else {
        if (strcmp(de.name, target_name) == 0) {
          print_match_or_exec(pathbuf);
        }
        close(cfd);
      }

    }
  }

  close(fd);
}

static void
usage(void)
{
  fprintf(2, "usage: find <startDir> <name> [-exec <cmd> [args...]]\n");
  exit(1);
}

int main(int argc, char *argv[])
{
  if (argc < 3) usage();

  if (argc >= 4 && strcmp(argv[3], "-exec") == 0) {
    g_has_exec = 1;

    if (argc < 5) {
      fprintf(2, "find: -exec requires a command\n");
      exit(1);
    }

    for (int i = 4; i < argc && g_exec_argc_base < MAXARG - 1; i++) {
      g_exec_argv_base[g_exec_argc_base++] = argv[i];
    }
    g_exec_argv_base[g_exec_argc_base] = 0;
  }

  walk(argv[1], argv[2]);
  exit(0);
}
