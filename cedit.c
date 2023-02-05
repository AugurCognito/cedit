/* #includes */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/ioctl.h>
// ioctl provides terminal size
#include <stdlib.h>
// for the atexit() function
#include <string.h>
// for functions related to buffer handling
#include <termios.h>
// termios.h is a header file that provides access to the POSIX terminal
// interface. These being different input and output modes.
#include <unistd.h>
// unistd.h is a header file that provides access to the POSIX operating system
// API.

/* data */
struct editorConfig {
  // This is a structure that contains the editor configuration.
  int cx, cy;
  // cx and cy are the cursor position.
  int screenrows;
  int screencols;
  struct termios orig_termios;
  // This is a structure that contains the original terminal attributes.
};
struct editorConfig E;

/* defines */
#define CEDIT_VERSION "0.0.1"
#define CTRL_KEY(k) ((k)&0x1f)

enum editorKey {
  // This is an enumeration that contains the key codes.
  ARROW_LEFT = 1000,
  ARROW_RIGHT = 1001,
  ARROW_UP = 1002,
  ARROW_DOWN = 1003
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
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';
    if (seq[0] == '[') {
      switch (seq[1]) {
        //
        case 'A': return ARROW_UP;
        case 'B': return ARROW_DOWN;
        case 'C': return ARROW_RIGHT;
        case 'D': return ARROW_LEFT;
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
  // realloc() will allocate a new block of memory and copy the contents of the
  // old block to the new block.
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
void editorDrawRows(struct abuf *ab) {
  // This function will draw the rows of the editor.
  //
  int y;

  for (y = 0; y < E.screenrows; y++) {
    if (y == E.screenrows / 3) {
      char welcome[80];
      int welcomelen = snprintf(welcome, sizeof(welcome),
                                "\e[1mCedit -- version %s\e[0m", CEDIT_VERSION);
      // snprintf() is a function that writes the output to a string and returns
      // the number of characters written.
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

    abAppend(ab, "\x1b[K", 3);
    // This will clear the line after the ~ character.
    if (y < E.screenrows - 1) {
      // if we are not at the last row, move the cursor to the next line.
      abAppend(ab, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  // This function will clear the screen and draw the welcome message.
  struct abuf ab = ABUF_INIT;

  abAppend(&ab, "\x1b[?25l", 6);
  // This will hide the cursor.
  abAppend(&ab, "\x1b[H", 3);
  // \x1b is the escape character. [H is the escape sequence that moves the
  // cursor to the top left corner of the screen.
  editorDrawRows(&ab);
  char buf[32];
  snprintf(buf, sizeof(buf), "\x1b[%d;%dH", E.cy + 1, E.cx + 1);
  abAppend(&ab, buf, strlen(buf));
  // This will move the cursor to the position of the cursor.
  abAppend(&ab, "\x1b[?25h", 6);
  // This will show the cursor.
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/* input functions */
void editorMoveCursor(int key) {
  // This function will move the cursor.
  switch (key) {
  case ARROW_LEFT:
    E.cx--;
    break;
  case ARROW_RIGHT:
    E.cx++;
    break;
  case ARROW_UP:
    E.cy--;
    break;
  case ARROW_DOWN:
    E.cy++;
    break;
  }
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
  case ARROW_LEFT:
  case ARROW_RIGHT:
  case ARROW_UP:
  case ARROW_DOWN:
    editorMoveCursor(c);
    break;
  }
}

/* Code Initialsation */
void initEditor() {
  // This function will initialise the editor.
  E.cx = 0;
  E.cy = 0;

  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };
  return 0;
}
