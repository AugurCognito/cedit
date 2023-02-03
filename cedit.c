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
  int screenrows;
  int screencols;
  struct termios orig_termios;
  // This is a structure that contains the original terminal attributes.
};
struct editorConfig E;

/* defines */
#define CTRL_KEY(k) ((k)&0x1f)
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

char editorReadKey() {
  // This function will read a key from the terminal and return it.
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
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

/* Output functions */
void editorDrawRows() {
  // This function will draw the rows of the editor.
  int y;
  for (y = 0; y < E.screenrows; y++) {

    if (y < E.screenrows - 1) {
      // if we are not at the last row, move the cursor to the next line.
      write(STDOUT_FILENO, "\r\n", 2);
    }
  }
}

void editorRefreshScreen() {
  // This function will clear the screen and draw the welcome message.
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // \x1b is the escape character. [2J is the escape sequence that clears the
  // screen.
  write(STDOUT_FILENO, "\x1b[H", 3);
  // \x1b is the escape character. [H is the escape sequence that moves the
  // cursor to the top left corner of the screen.
  editorDrawRows();
  write(STDOUT_FILENO, "\x1b[H", 3);
  // \x1b is the escape character. [H is the escape sequence that moves the
  // cursor to the top left corner of the screen.
}

/* input functions */
void editorProcessKeypress() {
  // This function will read a key from the terminal and process it.
  char c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    // Clear the screen before exiting

    exit(0);
    break;
  }
}

/* Code Initialsation */
void initEditor() {
  // This function will initialise the editor.
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
