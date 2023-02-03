/* #includes */
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
// for the atexit() function
#include <termios.h>
// termios.h is a header file that provides access to the POSIX terminal
// interface. These being different input and output modes.
#include <unistd.h>
// unistd.h is a header file that provides access to the POSIX operating system
// API.

/* data */
struct termios orig_termios;
// This struct will store the original terminal attributes.

/* defines */
#define CTRL_KEY(k) ((k)&0x1f)
// This macro will return the ASCII value of the control key pressed.

/* terminal */
void die(const char *s) {
  // This function will print the error message and exit the program.
  perror(s);
  exit(1);
}

void disableRawMode() {
  // This function will restore the original terminal attributes.
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios) == -1)
    die("tcsetattr");
}

void enableRawMode() {

  if (tcgetattr(STDIN_FILENO, &orig_termios) == -1)
    die("tcgetattr");
  // tcgetattr() gets the parameters associated with the terminal and stores
  // them in the termios structure.
  atexit(disableRawMode);
  // atexit() is a stdlib function which registers the function disableRawMode()
  // to be called at exit.

  struct termios raw = orig_termios;

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

/* Output functions */
void editorRefreshScreen() {
  // This function will clear the screen and draw the welcome message.
  write(STDOUT_FILENO, "\x1b[2J", 4);
  // \x1b is the escape character. [2J is the escape sequence that clears the
  // screen.
}

/* input functions */
void editorProcessKeypress() {
  // This function will read a key from the terminal and process it.
  char c = editorReadKey();
  switch (c) {
  case CTRL_KEY('q'):
    exit(0);
    break;
  }
}

/* Code Initialsation */
int main() {
  enableRawMode();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  };
  return 0;
}
