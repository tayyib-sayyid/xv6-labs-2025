#include "kernel/types.h"
#include "kernel/stat.h"
#include "kernel/fs.h"
#include "kernel/fcntl.h"
#include "kernel/param.h"
#include "user/user.h"

// -------- Globals --------
static int has_exec;
static char *execv_base[MAXARG];
static int execv_basec;

// 0 = literal strcmp (default), 1 = regex (^ . * $ only)
static int g_use_regex = 0;

// -------- K&P mini-regex (xv6 grep.c) --------
// Make these static to avoid any symbol noise.
static int matchhere(char*, char*);
static int matchstar(int, char*, char*);

static int
match(char *re, char *text)
{
  if(re[0] == '^')
    return matchhere(re+1, text);
  do{
    if(matchhere(re, text))
      return 1;
  } while(*text++ != '\0');
  return 0;
}

static int
matchhere(char *re, char *text)
{
  if(re[0] == '\0') return 1;
  if(re[1] == '*')  return matchstar(re[0], re+2, text);
  if(re[0] == '$' && re[1] == '\0') return *text == '\0';
  if(*text!='\0' && (re[0]=='.' || re[0]==*text))
    return matchhere(re+1, text+1);
  return 0;
}

static int
matchstar(int c, char *re, char *text)
{
  do{
    if(matchhere(re, text)) return 1;
  } while(*text!='\0' && (*text++==c || c=='.'));
  return 0;
}
// ---------------------------------------------

static void
run_exec_on(const char *filepath)
{
  char *argv[MAXARG];
  int ac = 0;
  for (int i = 0; i < execv_basec && ac < MAXARG - 1; i++)
    argv[ac++] = execv_base[i];
  if (ac < MAXARG - 1) argv[ac++] = (char *)filepath;
  argv[ac] = 0;

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

static void
find(const char *path, const char *target)
{
  int fd = open(path, O_RDONLY);
  if (fd < 0) {
    fprintf(2, "find: cannot open %s\n", path);
    return;
  }

  struct stat st;
  if (fstat(fd, &st) < 0) {
    fprintf(2, "find: cannot stat %s\n", path);
    close(fd);
    return;
  }

  if (st.type == T_FILE) {
    // basename(path)
    const char *base = path;
    for (const char *p = path; *p; p++)
      if (*p == '/') base = p + 1;

    int is_match = 0;
    if (g_use_regex)
      is_match = match((char *)target, (char *)base);
    else
      is_match = (strcmp((char *)base, (char *)target) == 0);

    if (is_match) {
      if (has_exec) run_exec_on(path);
      else printf("%s\n", path);
    }
  } else if (st.type == T_DIR) {
    char buf[512];
    int n = strlen((char *)path);
    if (n + 1 + DIRSIZ + 1 > sizeof(buf)) {
      fprintf(2, "find: path too long: %s\n", path);
      close(fd);
      return;
    }

    struct dirent de;
    while (read(fd, &de, sizeof(de)) == sizeof(de)) {
      if (de.inum == 0) continue;

      char name[DIRSIZ + 1];
      memmove(name, de.name, DIRSIZ);
      name[DIRSIZ] = 0;

      if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        continue;

      // build child path
      strcpy(buf, (char *)path);
      buf[n] = '/';
      buf[n + 1] = '\0';
      strcpy(buf + n + 1, name);

      find(buf, target);
    }
  }

  close(fd);
}

// I walk a lonely road........
int
main(int argc, char *argv[])
{
  if (argc < 2) {
    fprintf(2, "usage: find <start-path> <name|pattern> [-E | -F] [-exec <cmd> [args...]]\n");
    exit(1);
  }

  has_exec = 0;
  execv_basec = 0;
  g_use_regex = 0;

  const char *start  = argv[1];
  const char *target = 0;

  for (int i = 2; i < argc; i++) {

    if (strcmp(argv[i], "-E") == 0) {
      g_use_regex = 1;
      if (i + 1 < argc && argv[i+1][0] != '-') {
        target = argv[i+1];
        i++;            // consume pattern token after -E
      }
      continue;
    }

    if (strcmp(argv[i], "-F") == 0) {
      g_use_regex = 0;
      if (i + 1 < argc && argv[i+1][0] != '-') {
        target = argv[i+1];
        i++;            // consume literal token after -F
      }
      continue;
    }

    if (strcmp(argv[i], "-exec") == 0) {
      has_exec = 1;
      if (i + 1 >= argc) {
        fprintf(2, "find: -exec requires a command\n");
        exit(1);
      }
      for (int j = i + 1; j < argc && execv_basec < MAXARG - 1; j++)
        execv_base[execv_basec++] = argv[j];
      execv_base[execv_basec] = 0;
      // done parsing; -exec eats the rest
      break;
    }

    // Non-flag token: set target if not yet set
    if (!target) {
      target = argv[i];
    } else {
      // Extra non-flag before -exec -> keep behavior simple
      fprintf(2, "usage: find <start-path> <name|pattern> [-E | -F] [-exec <cmd> [args...]]\n");
      exit(1);
    }
  }

  if (!target) {
    fprintf(2, "usage: find <start-path> <name|pattern> [-E | -F] [-exec <cmd> [args...]]\n");
    exit(1);
  }

  find(start, target);
  exit(0);
}
