#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
// for the atexit() function
#include <termios.h>
// termios.h is a header file that provides access to the POSIX terminal
// interface. These being different input and output modes.
#include <unistd.h>
// unistd.h is a header file that provides access to the POSIX operating system
// API.

struct termios orig_termios;
// This struct will store the original terminal attributes.

void disableRawMode() {
  // This function will restore the original terminal attributes.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {

  tcgetattr(STDIN_FILENO, &orig_termios);
  // tcgetattr() gets the parameters associated with the terminal and stores
  // them in the termios structure.
  atexit(disableRawMode);
  // atexit() is a stdlib function which registers the function disableRawMode()
  // to be called at exit.

  struct termios raw = orig_termios;
  raw.c_lflag &= ~(ECHO | ICANON);
  // c_lflag is a member of the termios structure that contains local modes.
  // Bit wise of ECHO stops the terminal from echoing the input and ICANON stops
  // the terminal from waiting for the user to press enter before reading the
  // input.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
  // tcsetattr() sets the parameters associated with the terminal.
  // TCSAFLUSH is a flag that specifies when to apply the change.
  // TCSAFLUSH means that the change will occur after all output written to the
  // terminal has been transmitted, and all input that has been received but not
  // read will be discarded before the change is made.
}

int main() {
  enableRawMode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q')
  // read() reads up to 1 byte from the file descriptor STDIN_FILENO(which is
  // standard input output) into the buffer starting at &c and if c reads 'q'
  // program terminates.
  {
    if (iscntrl(c)) {
      printf("%d\n", c);
    } else {
      printf("%d ('%c')\n", c, c);
    }
    // if character is a control character then print its ASCII value else print
    // its ASCII value and the character itself.
  };
  return 0;
}
