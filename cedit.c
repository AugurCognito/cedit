/* #includes */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
// ioctl provides terminal size
#include <sys/types.h>
// for the open() function
#include <stdarg.h>
// used for argument lists
#include <stdlib.h>
// for the atexit() function
#include <string.h>
// for functions related to buffer handling
#include <termios.h>
// termios.h is a header file that provides access to the POSIX terminal
// interface. These being different input and output modes.
#include <time.h>
// for the time() function
#include <unistd.h>
// unistd.h is a header file that provides access to the POSIX operating system
// API.

char *my_strdup(const char *s) {
  // due to lack of strdup() in C89
  size_t len = strlen(s) + 1;
  char *dup = malloc(len);
  if (dup == NULL) {
    return NULL; // allocation failed
  }
  memcpy(dup, s, len);
  return dup;
}

/* data */
typedef struct editor_row {
  // data type for the row
  int size;
  int rsize;
  // rsize is the size of the rendered row
  char *chars;
  char *render;
  // render is the rendered row
} editor_row;

struct editorConfig {
  // This is a structure that contains the editor configuration.
  int cx, cy;
  // cx and cy are the cursor position.
  int rx;
  // rx is the position of the cursor in the rendered row.
  int screenrows;
  int screencols;
  int numrows;
  // numrows is the number of rows in the file.
  int rof;
  int cof;
  // rof and cof are the row and column offset
  editor_row *row;
  // row is the row of the file.
  char *filename;
  char statusmsg[80];
  time_t statusmsg_time;
  struct termios orig_termios;
  // This is a structure that contains the original terminal attributes.
};
struct editorConfig E;

/* defines */
#define CEDIT_VERSION "0.0.1"
#define CEDIT_TAB_STOP 4
#define CTRL_KEY(k) ((k)&0x1f)

enum editorKey {
  // This is an enumeration that contains the key codes.
  ARROW_LEFT = 1000,
  ARROW_RIGHT = 1001,
  ARROW_UP = 1002,
  ARROW_DOWN = 1003,
  PAGE_UP = 1004,
  PAGE_DOWN = 1005,
  HOME_KEY = 1006,
  END_KEY = 1007,
  DEL_KEY = 1008
};
// This macro will return the ASCII value of the control key pressed.

/* terminal */
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);
  // Clear the screen and move the cursor to the top left corner before exiting

  perror(s);
  // print the error message and exit the program.
  exit(1);
}

void disableRawMode() {
  // This function will restore the original terminal attributes.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {

  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcgetattr");
  // tcgetattr() gets the parameters associated with the terminal and stores
  // them in the termios structure.
  atexit(disableRawMode);
  // atexit() is a stdlib function which registers the function disableRawMode()
  // to be called at exit.

  struct termios raw = E.orig_termios;

  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ICRNL | IXON);
  // Bitwise not of BRKINT disables the SIGINT signal when the user presses
  // Ctrl-C. ICRNL disables the carriage return to new line translation. INPCK
  // disables parity checking. Icrnl is a flag that converts carriage return to
  // newline on input. Bitwise not of IXON disables software flow control(ctl-s
  // and ctrl-q).
  raw.c_oflag &= ~(OPOST);
  // Bitwise not of OPOST disables output processing.
  raw.c_cflag |= (CS8);
  // Bitwise or of CS8 sets character size to 8 bits per byte.
  raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
  // Bit wise of ECHO stops the terminal from echoing the input and ICANON stops
  // the terminal  from waiting for the user to press enter before reading the
  // input. ISIG  stops the terminal from sending signals like ctrl-c and
  // ctrl-z. IEXTEN  stops the terminal from sending ctrl-v.
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  // VMIN sets the minimum number of bytes of input needed for read to return
  // VTIME sets the maximum amount of time to wait before read returns.

  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw))
    die("tcsetattr");
  // tcsetattr() sets the parameters associated with the terminal.
  // TCSAFLUSH is a flag that specifies when to apply the change.
  // TCSAFLUSH means that the change will occur after all output written to the
  // terminal has been transmitted, and all input that has been received but not
  // read will be discarded before the change is made.
}

int editorReadKey() {
  // This function will read a key from the terminal and return it.
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }

  if (c == '\x1b') {
    // If the key pressed is escape then read the next two bytes and return
    // escape sequence if it is an escape sequence.
    // If arrow keys are pressed return corresponding sequence.
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1)
      return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1)
      return '\x1b';
    if (seq[0] == '[') {
      if (seq[1] >= '0' && seq[1] <= '9') {
        if (read(STDIN_FILENO, &seq[2], 1) != 1)
          return '\x1b';

        if (seq[2] == '~') {
          // If the third byte is ~ then return the corresponding sequence.
          switch (seq[1]) {
          case '1':
            return HOME_KEY;
          case '3':
            return DEL_KEY;
          case '4':
            return END_KEY;
          case '5':
            return PAGE_UP;
          case '6':
            return PAGE_DOWN;
          case '7':
            return HOME_KEY;
          case '8':
            return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
        //
        case 'A':
          return ARROW_UP;
        case 'B':
          return ARROW_DOWN;
        case 'C':
          return ARROW_RIGHT;
        case 'D':
          return ARROW_LEFT;
        case 'H':
          return HOME_KEY;
        case 'F':
          return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
      case 'H':
        return HOME_KEY;
      case 'F':
        return END_KEY;
      }
    }

    return '\x1b';
  } else {
    return c;
  }
}

int getWindowSize(int *rows, int *cols) {
  // This function will get the size of the terminal window.
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // on sucess ioctl() returns 0 and put the size of the terminal in ws.
    return -1;
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/* row functions */
int editorRowCxtoRx(editor_row *row, int cx) {
  // creates character index to render index mapping.
  int rx = 0;
  int j;
  for (j = 0; j < cx; j++) {
    // if the character is a tab then add 8 - (rx % 8) to rx.
    // else add 1 to rx.
    if (row->chars[j] == '\t')
      rx += (CEDIT_TAB_STOP - 1) - (rx % CEDIT_TAB_STOP);
    rx++;
  }
  return rx;
}

void editorUpdateRow(editor_row *row) {
  // This function will update the row by calculating the number of tabs and
  // non-tab characters in the row.
  int tabs = 0;
  int j = 0;

  for (j = 0; j < row->size; j++)
    if (row->chars[j] == '\t')
      tabs++;

  free(row->render);
  // free the memory allocated to the render string.
  row->render = malloc(row->size + tabs * (CEDIT_TAB_STOP - 1) + 1);
  // allocate memory for the render string.

  int idx = 0;
  for (j = 0; j < row->size; j++) {
    // loop through the row and copy the characters to the render string.
    if (row->chars[j] == '\t') {
      // if the character is a tab then copy 7 spaces to the render string.
      row->render[idx++] = ' ';
      while (idx % CEDIT_TAB_STOP != 0)
        row->render[idx++] = ' ';
    } else {
      row->render[idx++] = row->chars[j];
    }
  }
  row->render[idx] = '\0';
  // add the null character at the end of the render string.
  row->rsize = idx;
}

void editorAppendRow(char *s, size_t len) {
  // This function will append a row to the end of the row array.

  E.row = realloc(E.row, sizeof(editor_row) * (E.numrows + 1));
  // realloc() will allocate memory for the new row and copy the old rows to
  // the new memory location. The size of the new row is the size of the
  // editor_row struct multiplied by the number of rows plus one.

  int at = E.numrows;
  E.row[at].size = len;
  E.row[at].chars = malloc(len + 1);
  // malloc() will allocate memory for the characters in the row.
  // The size of the memory allocated is the length of the string plus one for
  // the null character.

  memcpy(E.row[at].chars, s, len);
  // memcpy() will copy the string to the memory allocated.

  E.row[at].chars[len] = '\0';
  // The last character of the string is set to null character.
  E.row[at].rsize = 0;
  E.row[at].render = NULL;
  editorUpdateRow(&E.row[at]);

  E.numrows++;
  // Increment the number of rows.
}

void editorRowInsertChar(editor_row *row, int at, int c) {
  // This function will insert a character at a given position in the row.
  if (at < 0 || at > row->size)
    // If the position is less than 0 or greater than the size of the row
    // then set the position to the size of the row.
    at = row->size;

  row->chars = realloc(row->chars, row->size + 2);
  // size is plus 2 because we are adding a character and a null character.
  memmove(&row->chars[at + 1], &row->chars[at], row->size - at + 1);
  // memmove() will move the characters after the position at by one position
  // to the right.
  row->size++;
  row->chars[at] = c;
  editorUpdateRow(row);
}

/* editor functions */
void editorInsertChar(int c) {
  // This function will insert a character at the cursor position.
  if (E.cy == E.numrows) {
    // If the cursor is at the end of the file then append a new row.
    editorAppendRow("", 0);
  }
  editorRowInsertChar(&E.row[E.cy], E.cx, c);
  // Insert the character at the cursor position.
  E.cx++;
}

/* file input output */
void editorOpen(char *filename) {
  // This function will open the file and read the contents into the buffer.

  free(E.filename);
  E.filename = my_strdup(filename);
  // strdup() will allocate memory for the filename and copy the filename to

  FILE *fp = fopen(filename, "r"); // open file in read mode.
  if (!fp)
    // if file does not exist then kill the program.
    die("fopen");

  char *line = NULL;  // line will store the line read from the file.
  size_t linecap = 0; // linecap will store the allocated size of the line.
  ssize_t linelen;    // linelen will store the length of the line.

  linelen = getline(&line, &linecap, fp);
  //  read the first line getline() will allocate memory for line and store
  //  the size of allocated memory in linecap.It will also store the length of
  //  the line in linelen.

  while ((linelen = getline(&line, &linecap, fp)) != -1) {
    // read the rest of the lines.
    while (linelen > 0 &&
           (line[linelen - 1] == '\n' || line[linelen - 1] == '\r'))
      linelen--;
    // remove the newline character from the end of the line.
    editorAppendRow(line, linelen);
  }
  free(line);
  fclose(fp);
  // free the memory allocated to line and close the file.
}

/* buffer */
struct abuf {
  // This is a structure that contains the append buffer.
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }
// This macro will initialize the append buffer.

void abAppend(struct abuf *ab, const char *s, int len) {
  // This function will append a string to the append buffer.
  char *new = realloc(ab->b, ab->len + len);
  // realloc() will allocate a new block of memory and copy the contents of
  // the old block to the new block.
  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  // memcpy() copies len bytes from memory area s to memory area new.
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) {
  // This function will free the append buffer.
  free(ab->b);
}

/* Output functions */
void editorScroll() {
  // This function will scroll the screen if the cursor is outside the screen.
  E.rx = 0;
  if (E.cy < E.numrows) {
    // if the cursor is on a row then calculate the render index of the cursor.
    E.rx = editorRowCxtoRx(&E.row[E.cy], E.cx);
  }
  if (E.cy < E.rof) {
    // If the cursor is above the screen then scroll up.
    E.rof = E.cy;
  }
  if (E.cy >= E.rof + E.screenrows) {
    // If the cursor is below the screen then scroll down.
    E.rof = E.cy - E.screenrows + 1;
  }
  if (E.rx < E.cof)
    // If the cursor is to the left of the screen then scroll left.
    E.cof = E.rx;
  if (E.rx >= E.cof + E.screencols)
    // If the cursor is to the right of the screen then scroll right.
    E.cof = E.rx - E.screencols + 1;
}
void editorDrawRows(struct abuf *ab) {
  // This function will draw the rows of the editor.
  int y;

  for (y = 0; y < E.screenrows; y++) {
    int filerow = y + E.rof;
    if (filerow >= E.numrows) {
      // If the number of rows is less than the number of rows in the terminal
      // then print ~.
      // else print the contents of the row.
      if (y == E.screenrows / 3 && E.numrows == 0) {
        // If the number of rows is less than the number of rows in the
        // terminal then print welcome message and not when file is given
        char welcome[80];
        int welcomelen =
            snprintf(welcome, sizeof(welcome), "\e[1mCedit -- version %s\e[0m",
                     CEDIT_VERSION);
        // snprintf() is a function that writes the output to a string and
        // returns the number of characters written.
        if (welcomelen > E.screencols)
          welcomelen = E.screencols;
        int padding = (E.screencols - welcomelen) / 2;
        if (padding) {
          abAppend(ab, "~", 1);
          padding--;
        }
        while (padding--)
          abAppend(ab, " ", 1);
        abAppend(ab, welcome, welcomelen);
        // abAppend() appends a string to the append buffer.
      } else {
        abAppend(ab, "~", 1);
      }
    } else {
      int len = E.row[filerow].rsize - E.cof;
      // len is the length of the row.
      if (len < 0)
        len = 0;
      if (len > E.screencols)
        len = E.screencols;
      abAppend(ab, &E.row[filerow].render[E.cof], len);
      // abAppend() appends a string to the append buffer.
    }

    abAppend(ab, "\x1b[K", 3);
    // This will clear the line after the ~ character.
    abAppend(ab, "\r\n", 2);
    // This will move the cursor to the next line.
  }
}

void editorDrawStatusBar(struct abuf *ab) {
  // This function will draw the status bar.
  abAppend(ab, "\x1b[7m", 4);
  // This will set the background color of the status bar.

  char status[80], rst[20];
  // status will store the status of the editor.
  // rst will store the reset sequence.
  int len = snprintf(status, sizeof(status), "%.20s - %d lines",
                     E.filename ? E.filename : "[No Name]", E.numrows);
  // print the status of the editor.
  int rlen = snprintf(rst, sizeof(rst), "%d:%d/%d", E.rx, E.cy + 1, E.numrows);
  // print the reset sequence.

  if (len > E.screencols)
    len = E.screencols;
  abAppend(ab, status, len);

  while (len < E.screencols) {
    if (E.screencols - len == rlen) {
      abAppend(ab, rst, rlen);
      break;
    } else {
      abAppend(ab, " ", 1);
      len++;
    }
  }
  abAppend(ab, "\x1b[m",
           3); // This will reset the background color of the status bar.
  abAppend(ab, "\r\n", 2); // This will move the cursor to the next line.
}

void editorDrawMessageBar(struct abuf *ab) {
  abAppend(ab, "\x1b[K", 3);
  int msglen = strlen(E.statusmsg);
  if (msglen > E.screencols)
    msglen = E.screencols;
  if (msglen && time(NULL) - E.statusmsg_time < 5)
    abAppend(ab, E.statusmsg, msglen);
}

void editorRefreshScreen() {
  // This function will clear the screen and draw the welcome message.

  editorScroll();
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  // This will hide the cursor.
  abAppend(&ab, "\x1b[H", 3);
  // \x1b is the escape character. [H is the escape sequence that moves the
  // cursor to the top left corner of the screen.

  editorDrawRows(&ab);
  editorDrawStatusBar(&ab);
  editorDrawMessageBar(&ab);

  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy - E.rof + 1, E.rx - E.cof + 1);
  // This will move the cursor to the position of the cursor.
  abAppend(&ab, buf, strlen(buf));
  // This will move the cursor to the position of the cursor.
  abAppend(&ab, "\x1b[?25h", 6);
  // This will show the cursor.
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

void editorSetStatusMessage(const char *fmt, ...) {
  // This function will set the status message.
  va_list ap;
  // va_list is a type that is used to store the list of arguments.
  va_start(ap, fmt);
  // va_start() is a macro that initializes the va_list variable.
  vsnprintf(E.statusmsg, sizeof(E.statusmsg), fmt, ap);
  // vsnprintf() is a function that writes the output to a string and returns
  // the number of characters written.
  va_end(ap);
  // va_end() is a macro that cleans up the va_list variable.
  E.statusmsg_time = time(NULL);
  // time() returns the current time.
}
/* input functions */
void editorMoveCursor(int key) {
  // This function will move the cursor.
  editor_row *row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  // if position of cursor is greater than the number of rows in the file then
  // row will be NULL otherwise it will point to the row of the cursor.

  switch (key) {
  case ARROW_LEFT:
    if (E.cx != 0) {
      E.cx--;
    } else if (E.cy > 0) {
      // move to the end of the previous line.
      E.cy--;
      E.cx = E.row[E.cy].size;
    }
    break;
  case ARROW_RIGHT:
    if (row && E.cx < row->size) {
      E.cx++;
    } else if (row && E.cx == row->size) {
      // move to the start of the next line.
      E.cy++;
      E.cx = 0;
    }
    break;
  case ARROW_UP:
    if (E.cy != 0) {
      E.cy--;
    }
    break;
  case ARROW_DOWN:
    if (E.cy != E.numrows) {
      // if we are not at the last row of the file, move the cursor down.
      E.cy++;
    }
    break;
  }

  row = (E.cy >= E.numrows) ? NULL : &E.row[E.cy];
  // same as first check in function
  int rowlen = row ? row->size : 0;
  // if row is NULL then rowlen will be 0 otherwise it will be the size of the
  // row.
  if (E.cx > rowlen)
    // move cursor to the end of the row if it is greater than the size of the
    // row in file.
    E.cx = rowlen;
}

void editorProcessKeypress() {
  // This function will read a key from the terminal and process it.
  int c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    // Clear the screen before exiting
    exit(0);
    break;

  case HOME_KEY:
    E.cx = 0;
    break;
  case END_KEY:
    if (E.cy < E.numrows)
      E.cx = E.row[E.cy].size;
    break;

  case PAGE_DOWN:
  case PAGE_UP: {
    if (c == PAGE_UP) {
      // move the cursor to the top of the screen.
      E.cy = E.rof;
    } else if (c == PAGE_DOWN) {
      // move the cursor to the bottom of the screen.
      E.cy = E.rof + E.screenrows - 1;
      if (E.cy > E.numrows)
        E.cy = E.numrows;
    }
    int times = E.screenrows;
    while (times--)
      editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
  } break;

  case ARROW_LEFT:
  case ARROW_RIGHT:
  case ARROW_UP:
  case ARROW_DOWN:
    editorMoveCursor(c);
    break;
  default:
    editorInsertChar(c);
  }
}

/* Code Initialsation */
void initEditor() {
  // This function will initialise the editor.
  E.cx = 0;
  E.cy = 0;
  E.rx = 0;
  E.rof = 0;
  E.cof = 0;
  E.numrows = 0;
  E.row = NULL;
  E.filename = NULL;
  E.statusmsg[0] = '\0';
  E.statusmsg_time = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
  E.screenrows -= 2;
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  editorSetStatusMessage("HELP: Ctrl-S = save | Ctrl-Q = quit");

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };
  return 0;
}
