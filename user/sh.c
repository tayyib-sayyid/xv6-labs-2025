// Shell.

#include "kernel/types.h"
#include "user/user.h"
#include "kernel/fcntl.h"
#include "kernel/stat.h"
#include "kernel/fs.h"

// Parsed command representation
#define EXEC  1
#define REDIR 2
#define PIPE  3
#define LIST  4
#define BACK  5

#define MAXARGS 10

static int g_interactive = 0; // Whether the shell is running interactively
static char *prompt = "$ ";
static int histfd = -1;

static int readline(char *buf, int max);
static void complete(char *buf, int *len);
static int  is_sep(int c);
static void echo_append(char *buf, int *len, const char *s);
static void de_name_to_cstr(char dst[DIRSIZ+1], const char src[DIRSIZ]);
static int  is_dir(const char *name);

struct cmd {
  int type;
};

struct execcmd {
  int type;
  char *argv[MAXARGS];
  char *eargv[MAXARGS];
};

struct redircmd {
  int type;
  struct cmd *cmd;
  char *file;
  char *efile;
  int mode;
  int fd;
};

struct pipecmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct listcmd {
  int type;
  struct cmd *left;
  struct cmd *right;
};

struct backcmd {
  int type;
  struct cmd *cmd;
};

int fork1(void);  // Fork but panics on failure.
void panic(char*);
struct cmd *parsecmd(char*);
void runcmd(struct cmd*) __attribute__((noreturn));

// Zuhair Merchant 
static void
Paleoloxodon(void)
{
  static const char *art =
  "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⣤⣤⣄⣀⣀⠀⠀⠀⠀⠀⠐⠁⠀⠀⠀⠀⠁⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣤⠞⠉⠀⠀⠀⠀⠀⠉⠉⠉⠉⠑⣦⣀⣄⣀⠀⠑⢀⣀⣀⣀⣤⣴⣤⣤⣤⣀⡀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣰⠟⠀⠀⠀⠀⠀⠀⠀⢆⠀⠀⢀⠴⠋⠁⠈⢀⠙⢉⠓⠺⠧⠀⠀⠀⠀⠀⠀⠀⠈⠙⠛⠶⣄⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠀⠀⠀⠀⢀⡼⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⢣⡀⠀⠀⠀⠀⠈⠀⠀⠁⠀⠰⠀⠀⠀⠀⣠⠀⠀⠀⠀⠀⠀⠀⠘⢷⡀⠀⠀\n"
  "⠀⠀⠀⠀⠀⠀⠀⠀⣾⠁⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠘⣿⣆⠀⠀⠀⠀⠀⠀⠀⠀⠁⠀⠀⢠⠎⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢳⡀⠀\n"
  "⠀⠀⠀⠀⠀⠀⠀⢠⢿⡄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⠏⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⢷⡀\n"
  "⠀⠀⠀⠀⠀⠀⢠⠏⠀⠻⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⣭⣵⠀⠀⠀⠀⠀⠀⠀⠀⠁⣴⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢸⡇\n"
  "⠀⠀⠀⠀⠀⢠⡏⠀⠀⠀⠀⠙⢦⡀⠀⠀⠀⠀⠀⠀⠀⠀⢷⣟⠀⠀⠀⠀⠀⠀⠄⠀⢸⢿⠃⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⢠⡟⠀\n"
  "⠀⠀⠀⠀⠀⣾⠀⠀⠀⠀⠀⠀⠀⠙⢧⡀⠀⠀⠀⠀⠀⠀⣼⣼⢠⠐⠒⠉⠉⠉⠒⠄⢸⣸⠀⠀⠀⠀⠀⠀⠀⠀⠀⢀⣠⠶⠋⠀⠀\n"
  "⠀⠀⠀⠀⢀⡇⠀⢰⠀⠀⠀⠀⠀⠀⠀⢙⢦⡀⠀⠀⠀⠀⣇⡟⠀⡂⠠⡘⠒⠚⠠⠀⢸⡯⠀⠀⠀⠀⠀⠀⣀⣤⠖⠋⠁⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠘⡇⠀⢸⡀⠀⠀⠀⠀⠀⠀⠈⡇⠙⠳⣤⣀⣠⣿⡣⠀⢏⣤⠬⠤⠤⢄⡐⢐⣿⠀⠀⠀⢀⡤⠞⠁⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⢀⡇⠀⠘⣇⠀⠀⠀⠀⠀⠀⠀⢳⠀⠀⠀⠈⠉⢸⠓⣾⠋⡄⠀⠠⠤⡀⠑⣬⣼⣀⡴⠚⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⢀⡾⣷⠀⠀⢻⡄⠀⠀⠀⠀⠀⠀⠘⣇⠀⠀⠀⠀⣏⡼⠻⣮⠤⠄⢀⠀⠈⣶⢿⡘⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⣸⢳⢻⠀⠀⠈⢷⡀⠀⠀⠀⠀⠀⠀⠸⣄⠀⠀⠀⢹⢂⠠⢳⠠⢒⣀⡀⢸⣿⠁⢳⣽⡢⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⢀⢿⠃⢸⡇⠀⠀⠀⠻⣄⠀⠀⠀⠀⠀⠀⢿⠀⠀⠢⠒⠀⡄⢸⣄⣀⣷⣷⢼⢿⠀⠀⠙⠃⠉⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⣤⣾⢾⡆⠀⡇⠀⠀⠀⠀⣨⣷⣤⣀⡀⠀⠀⠸⣧⠀⢸⠀⠀⠀⠘⣏⢙⣛⡃⢸⢸⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⣸⠁⠀⢠⠇⠀⣿⠀⠀⢠⠊⣼⠈⢧⠉⠛⠛⠶⡶⢻⣿⡳⠀⠀⠀⠀⣿⠋⠉⠁⢸⢸⠀⠀⠀⠠⠄⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠻⣧⣠⠞⠀⠀⣿⠀⠀⠀⢀⡇⠀⠈⡇⠀⠀⠀⢠⡟⠘⡇⠀⠀⠀⠀⣿⡇⠀⠀⢸⣾⡄⠀⠀⠂⠨⠄⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠉⠁⠀⠀⠀⣿⠀⠀⠀⣸⠃⠀⠀⢿⠀⠀⠀⣼⠑⠀⢿⠀⠀⠀⠀⣿⣷⠀⠀⢸⢿⡇⠀⠀⠀⠀⠠⠀⡀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠀⠀⣿⠀⠀⢠⡏⠀⠀⠀⢸⡇⠀⠀⣿⠀⠀⢸⡆⠀⠀⠀⣿⣿⠀⠀⣼⣸⠇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠀⠀⡿⠀⠀⢸⠃⠀⠀⠀⢸⠁⠀⠀⢸⡄⠀⠈⡇⠀⠀⠀⢸⣿⠀⠀⣿⠏⠀⠀⠀⠀⠀⠀⠡⠀⠀⢀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠀⣸⠇⠀⠀⢸⠆⠀⠀⠀⢸⠀⠀⠀⠸⡇⠀⠀⣷⠀⠀⠀⢸⡟⣇⠀⡇⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠓⠆⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⢠⡟⠀⠀⠀⢸⠀⠀⠀⢀⡿⠀⠀⠀⠀⣿⠀⠀⣿⠀⠀⠀⢸⡁⠙⠚⣷⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
  "⠀⠀⠀⠀⠻⠦⢤⣤⣤⡼⠇⠀⠀⠘⢧⣀⣀⠀⠀⠘⣷⢠⡇⠀⠀⠀⠈⢧⡤⠴⠟⠀⠀⠀⠀⠀⠀⢠⣤⣤⣤⣤⣤⣶⣶⠀⡄⣤⣄\n"
  "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠈⠉⠉⠉⠁⠈⠛⠒⠲⠿⠶⠾⠇⠀⠀⠀⠀⠀⠀⠀⠀⠈⣉⠉⠉⠉⠉⣛⡛⠁⠉⠉⠀\n"
  "PALEOLOXODON NAMADICUS\n";

  write(1, art, strlen(art));
}


// Execute cmd.  Never returns.
void
runcmd(struct cmd *cmd)
{
  int p[2];
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    exit(1);

  switch(cmd->type){
  default:
    panic("runcmd");

  case EXEC:
    ecmd = (struct execcmd*)cmd;
    if(ecmd->argv[0] == 0)
      exit(1);

    // Stuff for wait command
    if (ecmd->argv[0][0]=='w' && ecmd->argv[0][1]=='a' &&
        ecmd->argv[0][2]=='i' && ecmd->argv[0][3]=='t' &&
        ecmd->argv[0][4]==0) {         // exact "wait"
      while (wait(0) >= 0) ;           // reap all children
      exit(0);                         // done with this command
    }

    exec(ecmd->argv[0], ecmd->argv);
    fprintf(2, "exec %s failed\n", ecmd->argv[0]);
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    close(rcmd->fd);
    if(open(rcmd->file, rcmd->mode) < 0){
      fprintf(2, "open %s failed\n", rcmd->file);
      exit(1);
    }
    runcmd(rcmd->cmd);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    if(fork1() == 0)
      runcmd(lcmd->left);
    wait(0);
    runcmd(lcmd->right);
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    if(pipe(p) < 0)
      panic("pipe");
    if(fork1() == 0){
      close(1);
      dup(p[1]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->left);
    }
    if(fork1() == 0){
      close(0);
      dup(p[0]);
      close(p[0]);
      close(p[1]);
      runcmd(pcmd->right);
    }
    close(p[0]);
    close(p[1]);
    wait(0);
    wait(0);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    if(fork1() == 0)
      runcmd(bcmd->cmd);
    break;
  }
  exit(0);
}

int getcmd(char *buf, int nbuf) {
  if (g_interactive) {
    write(2, prompt, strlen(prompt));
  }  
  
  memset(buf, 0, nbuf);
  int n = readline(buf, nbuf);
  if (n < 0) return -1;                  // EOF: exit shell; init respawns it
  if (buf[0] == 0) return 0;             // blank line reprompt
  return 0;
}


static int readline(char *buf, int max) {
  int len = 0;
  for (;;) {
    char c;
    int r = read(0, &c, 1);
    if (r < 1) {
      if (len == 0) return -1;  
      buf[len] = 0;             
      return len;
    }

    if (c == '\r' || c == '\n') {
      buf[len] = 0;
      return len;
    }

    if (c == 0x08 || c == 0x7f) {
      if (len) len--;
      continue;
    }

    if (c == '\t') {
      // Do completion and print only the suffix
      complete(buf, &len);
      continue;
    }

    if (c >= 32 && c < 127) {
      if (len + 1 < max) buf[len++] = c;
      continue;
    }
    // ignore others
  }
}


// Autocomplete helper functions
static int
is_sep(int c) {
  return c==' ' || c=='\t' || c=='\n' || c=='|' || c==';' || c=='&' || c=='<' || c=='>';
}

static void
echo_append(char *buf, int *len, const char *s) {
  while (*s) {
    buf[*len] = *s;
    write(1, s, 1);
    (*len)++;
    s++;
  }
}

static void
de_name_to_cstr(char dst[DIRSIZ+1], const char src[DIRSIZ]) {
  int i = 0;
  while (i < DIRSIZ && src[i]) { dst[i] = src[i]; i++; }
  dst[i] = 0;
}

static int
is_dir(const char *name) {
  struct stat st;
  if (stat((char *)name, &st) < 0) return 0;
  return st.type == T_DIR;
}

// Longest common prefix among matches
static int
lcp_len(char matches[][DIRSIZ+1], int n) {
  if (n <= 0) return 0;
  for (int j = 0; ; j++) {
    char c = matches[0][j];
    if (c == 0) return j;
    for (int i = 1; i < n; i++) {
      if (matches[i][j] != c) return j;
    }
  }
}

// Tab completion
static void
complete(char *buf, int *len) {
  // 1) find token start (last separator + 1)
  int t0 = *len;
  while (t0 > 0 && !is_sep((unsigned char)buf[t0-1])) t0--;
  int plen = *len - t0;
  if (plen <= 0) { write(1, "\a", 1); return; }     // nothing to complete

  // 2) prefix string (cap at DIRSIZ)
  if (plen > DIRSIZ) plen = DIRSIZ;
  char prefix[DIRSIZ+1];
  for (int i=0; i<plen; i++) prefix[i] = buf[t0+i];
  prefix[plen] = 0;

  // 3) scan "." and collect matches
  int fd = open(".", 0);
  if (fd < 0) return;

  struct dirent de;
  char matches[64][DIRSIZ+1];
  char isdir[64];
  int m = 0;

  while (read(fd, &de, sizeof(de)) == sizeof(de)) {
    if (de.inum == 0) continue;

    char name[DIRSIZ+1];
    de_name_to_cstr(name, de.name);

    // skip "." and ".."
    if (name[0]=='.' && (name[1]==0 || (name[1]=='.' && name[2]==0)))
      continue;

    // prefix match
    int ok = 1;
    for (int i=0; i<plen; i++) { if (name[i] != prefix[i]) { ok = 0; break; } }
    if (!ok) continue;

    if (m < 64) {
      int i=0; while (i<=DIRSIZ) { matches[m][i] = name[i]; i++; }
      isdir[m] = is_dir(name);
      m++;
    }
  }
  close(fd);

  if (m == 0) { write(1, "\a", 1); return; }

  if (m == 1) {
    // single match: append remaining chars; add '/' if directory
    const char *nm = matches[0];
    echo_append(buf, len, nm + plen);
    if (isdir[0]) echo_append(buf, len, "/");
    return;
  }

  // multiple matches: extend by LCP; if no extension possible, list + redraw
  int lcp = lcp_len(matches, m);
  if (lcp > plen) {
    char tmp[DIRSIZ+1];
    int k=0;
    for (int i=plen; i<lcp; i++) tmp[k++] = matches[0][i];
    tmp[k] = 0;
    echo_append(buf, len, tmp);
    return;
  }

  // ambiguous and no progress -> print choices, then redraw
  write(1, "\n", 1);
  for (int i=0; i<m; i++) {
    write(1, matches[i], strlen(matches[i]));
    if (isdir[i]) write(1, "/", 1);
    write(1, "\n", 1);
  }
  if (g_interactive) write(2, prompt, strlen(prompt));   // same prompt guard you already have
  write(1, buf, *len);                    // restore current line
}

int
main(void)
{
  static char buf[100];
  int fd;

  // Ensure that three file descriptors are open.
  while((fd = open("console", O_RDWR)) >= 0){
    if(fd >= 3){
      close(fd);
      break;
    }
  }

  // To detect whether we are running interactively; hide '$' 
  // or other symbols otherwise
  struct stat st;
  if (fstat(0, &st) == 0 && st.type == T_DEVICE) {
    g_interactive = 1;
    histfd = open("sh_history", O_CREATE | O_RDWR);
    if (histfd >= 0) {
      // advance the file offset to EOF once; keep fd open afterwards
      char sink[128];
      while (read(histfd, sink, sizeof sink) > 0) { /* nothing */ }
    }
  } else {
    g_interactive = 0;
  }

  int IAT = 0;
  if (IAT) {Paleoloxodon();} // Sorry Zuhair

  // Read and run input commands.
  while(getcmd(buf, sizeof(buf)) >= 0){
    char *cmd = buf;

    while (*cmd == ' ' || *cmd == '\t')
      cmd++;

    // if (*cmd == '\n') // is a blank command
    //  continue;
    if (*cmd == 0) continue;  // truly empty after trimming spaces

    // append to history
    if (histfd >= 0) {
      write(histfd, cmd, strlen(cmd));
      write(histfd, "\n", 1);   // add newline for readability
    }

    if(cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' '){
      // Chdir must be called by the parent, not the child.
      // cmd[strlen(cmd)-1] = 0;  // chop \n
      if(chdir(cmd+3) < 0)
        fprintf(2, "cannot cd %s\n", cmd+3);
    } else {
      // wait command
      // if (is_cmd(cmd, "wait") == 0) {
      //   while(wait(0) >= 0) ;
      //   continue;
      // }
      
      // ---- builtin: wait ----
      // Skip leading spaces/tabs
      char *p = cmd;
      while (*p == ' ' || *p == '\t') p++;

      // Accept "wait" optionally followed by spaces/tabs and ending with '\n' or '\0'
      if (p[0]=='w' && p[1]=='a' && p[2]=='i' && p[3]=='t') {
        int i = 4;
        while (p[i] == ' ' || p[i] == '\t') i++;
        if (p[i] == '\n' || p[i] == '\0') {
          while (wait(0) >= 0) ;   // reap all children
          continue;                 // don't fork/exec
        }
      }
      
      if(fork1() == 0)
        runcmd(parsecmd(cmd));
      wait(0);
    }
  }
  exit(0);
}

void
panic(char *s)
{
  fprintf(2, "%s\n", s);
  exit(1);
}

int
fork1(void)
{
  int pid;

  pid = fork();
  if(pid == -1)
    panic("fork");
  return pid;
}

//PAGEBREAK!
// Constructors

struct cmd*
execcmd(void)
{
  struct execcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = EXEC;
  return (struct cmd*)cmd;
}

struct cmd*
redircmd(struct cmd *subcmd, char *file, char *efile, int mode, int fd)
{
  struct redircmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = REDIR;
  cmd->cmd = subcmd;
  cmd->file = file;
  cmd->efile = efile;
  cmd->mode = mode;
  cmd->fd = fd;
  return (struct cmd*)cmd;
}

struct cmd*
pipecmd(struct cmd *left, struct cmd *right)
{
  struct pipecmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = PIPE;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
listcmd(struct cmd *left, struct cmd *right)
{
  struct listcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = LIST;
  cmd->left = left;
  cmd->right = right;
  return (struct cmd*)cmd;
}

struct cmd*
backcmd(struct cmd *subcmd)
{
  struct backcmd *cmd;

  cmd = malloc(sizeof(*cmd));
  memset(cmd, 0, sizeof(*cmd));
  cmd->type = BACK;
  cmd->cmd = subcmd;
  return (struct cmd*)cmd;
}
//PAGEBREAK!
// Parsing

char whitespace[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int
gettoken(char **ps, char *es, char **q, char **eq)
{
  char *s;
  int ret;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  if(q)
    *q = s;
  ret = *s;
  switch(*s){
  case 0:
    break;
  case '|':
  case '(':
  case ')':
  case ';':
  case '&':
  case '<':
    s++;
    break;
  case '>':
    s++;
    if(*s == '>'){
      ret = '+';
      s++;
    }
    break;
  default:
    ret = 'a';
    while(s < es && !strchr(whitespace, *s) && !strchr(symbols, *s))
      s++;
    break;
  }
  if(eq)
    *eq = s;

  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return ret;
}

int
peek(char **ps, char *es, char *toks)
{
  char *s;

  s = *ps;
  while(s < es && strchr(whitespace, *s))
    s++;
  *ps = s;
  return *s && strchr(toks, *s);
}

struct cmd *parseline(char**, char*);
struct cmd *parsepipe(char**, char*);
struct cmd *parseexec(char**, char*);
struct cmd *nulterminate(struct cmd*);

struct cmd*
parsecmd(char *s)
{
  char *es;
  struct cmd *cmd;

  es = s + strlen(s);
  cmd = parseline(&s, es);
  peek(&s, es, "");
  if(s != es){
    fprintf(2, "leftovers: %s\n", s);
    panic("syntax");
  }
  nulterminate(cmd);
  return cmd;
}

struct cmd*
parseline(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parsepipe(ps, es);
  while(peek(ps, es, "&")){
    gettoken(ps, es, 0, 0);
    cmd = backcmd(cmd);
  }
  if(peek(ps, es, ";")){
    gettoken(ps, es, 0, 0);
    cmd = listcmd(cmd, parseline(ps, es));
  }
  return cmd;
}

struct cmd*
parsepipe(char **ps, char *es)
{
  struct cmd *cmd;

  cmd = parseexec(ps, es);
  if(peek(ps, es, "|")){
    gettoken(ps, es, 0, 0);
    cmd = pipecmd(cmd, parsepipe(ps, es));
  }
  return cmd;
}

struct cmd*
parseredirs(struct cmd *cmd, char **ps, char *es)
{
  int tok;
  char *q, *eq;

  while(peek(ps, es, "<>")){
    tok = gettoken(ps, es, 0, 0);
    if(gettoken(ps, es, &q, &eq) != 'a')
      panic("missing file for redirection");
    switch(tok){
    case '<':
      cmd = redircmd(cmd, q, eq, O_RDONLY, 0);
      break;
    case '>':
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE|O_TRUNC, 1);
      break;
    case '+':  // >>
      cmd = redircmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
      break;
    }
  }
  return cmd;
}

struct cmd*
parseblock(char **ps, char *es)
{
  struct cmd *cmd;

  if(!peek(ps, es, "("))
    panic("parseblock");
  gettoken(ps, es, 0, 0);
  cmd = parseline(ps, es);
  if(!peek(ps, es, ")"))
    panic("syntax - missing )");
  gettoken(ps, es, 0, 0);
  cmd = parseredirs(cmd, ps, es);
  return cmd;
}

struct cmd*
parseexec(char **ps, char *es)
{
  char *q, *eq;
  int tok, argc;
  struct execcmd *cmd;
  struct cmd *ret;

  if(peek(ps, es, "("))
    return parseblock(ps, es);

  ret = execcmd();
  cmd = (struct execcmd*)ret;

  argc = 0;
  ret = parseredirs(ret, ps, es);
  while(!peek(ps, es, "|)&;")){
    if((tok=gettoken(ps, es, &q, &eq)) == 0)
      break;
    if(tok != 'a')
      panic("syntax");
    cmd->argv[argc] = q;
    cmd->eargv[argc] = eq;
    argc++;
    if(argc >= MAXARGS)
      panic("too many args");
    ret = parseredirs(ret, ps, es);
  }
  cmd->argv[argc] = 0;
  cmd->eargv[argc] = 0;
  return ret;
}

// NUL-terminate all the counted strings.
struct cmd*
nulterminate(struct cmd *cmd)
{
  int i;
  struct backcmd *bcmd;
  struct execcmd *ecmd;
  struct listcmd *lcmd;
  struct pipecmd *pcmd;
  struct redircmd *rcmd;

  if(cmd == 0)
    return 0;

  switch(cmd->type){
  case EXEC:
    ecmd = (struct execcmd*)cmd;
    for(i=0; ecmd->argv[i]; i++)
      *ecmd->eargv[i] = 0;
    break;

  case REDIR:
    rcmd = (struct redircmd*)cmd;
    nulterminate(rcmd->cmd);
    *rcmd->efile = 0;
    break;

  case PIPE:
    pcmd = (struct pipecmd*)cmd;
    nulterminate(pcmd->left);
    nulterminate(pcmd->right);
    break;

  case LIST:
    lcmd = (struct listcmd*)cmd;
    nulterminate(lcmd->left);
    nulterminate(lcmd->right);
    break;

  case BACK:
    bcmd = (struct backcmd*)cmd;
    nulterminate(bcmd->cmd);
    break;
  }
  return cmd;
}
