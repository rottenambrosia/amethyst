/*
 * I, I wish you could swim
 * Like dolphins, dolphins can swim
 * Though nothing, nothing will keep us together
 * We can beat them forever and ever
 */

/*** Importing libraries ***/
#include <unistd.h>
#include <termios.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <ctype.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

/*** Definitions ***/
#define CTRL_KEY(k) ((k) & 0x1f)
#define BUFF_INIT {NULL, 0}
#define AMETHYST_VER "0.0.1"

enum editorKey {
	ARROW_LEFT = 1000,
	ARROW_RIGHT,
	ARROW_UP,
	ARROW_DOWN,
	PAGE_UP,
	PAGE_DOWN
};


/***  original flags data ***/
struct editorConfig {
	int cx, cy;
	int screenrows;
	int screencols;
	struct termios orig_termios;
};

struct editorConfig E;

/*** Terminal ***/
void die(const char *s) {
	write(STDOUT_FILENO, "\x1b[2J", 4);
	write(STDOUT_FILENO, "\x1b[H", 3);
	perror(s);
	exit(1);
}

void disableRawMode () {
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("Set Attribute Failed While Disabling Raw Mode");
}

void enableRawMode () {
	if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("Failed to get original attributes while enabling raw mode");
	atexit(disableRawMode);

	struct termios raw = E.orig_termios;

	/*
	--- INPUT FLAGS ---
	BRKINT -> Break condition similar to SIGINT (Ctrl + C)
	INPCK -> Enables parity checking, not for modern terminal ecosystems
	ISTRIP -> Strips (Resets) the 8th bit of each input byte to be stripped (Prolly already off)
	IXON -> Ctrl + V/O)
	*/
	raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
	
	/*
	--- OUTPUT FLAGS ---
	OPOST -> Transates "\n" to "\r\n" (Added \r\n to printf in order to see effects)
	*/
	raw.c_oflag &= ~(OPOST);
	
	/*
	--- CONTROL FLAGS ---
	CS8 -> Not a flag, a bit mask with multiple bits : sets character size to 8 bytes (default in most cases)
	*/
	raw.c_cflag |= (CS8);
	
	/*
	--- LOCAL FLAGS ---
	ECHO -> Showing what you're typing on the terminal (Raw Mode)
	IEXTEN -> Software Flow Control Flags (Ctrl + S / Q)
	ICANON -> Canonical/ cooking mode
	ISIG -> Signal flags (Termination/ Suspension)
	*/
	raw.c_lflag &= ~(ECHO | IEXTEN | ICANON | ISIG);
	
	// VMIN -> Control Character for minimum number of bytes of input needed before read()
	raw.c_cc[VMIN] = 0;
	
	// VTIME -> Control Character for maximum amount of wait time before read() returns 
	raw.c_cc[VTIME] = 1;
	
	if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("Failed to set attributes while enablind raw mode");
}

int editorReadKey () {
	int nread; // holds the {-1, 0, 1} value from read and checks error condition only if it's not 1
	char c;
	while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
		if (nread == -1 && errno != EAGAIN) die("Failed to read");
	}

	if (c == '\x1b') {
		char seq[3];

		if (read(STDOUT_FILENO, &seq[0], 1) != 1) return '\x1b';
		if (read(STDOUT_FILENO, &seq[1], 1) != 1) return '\x1b';

		if (seq[0] == '[') {
			if (seq[1] >= '0' && seq[1] <= '9') {
				if (read(STDOUT_FILENO, &seq[2], 1) != -1) return '\x1b';
				if (seq[2] == '~') {
					switch (seq[1]) {
						case '5' : 
							return PAGE_UP;
						case '6' :
							return PAGE_DOWN;
					}
				}
			} else {
				switch (seq[1]) {
					case 'A' : return ARROW_UP;
					case 'B' : return ARROW_DOWN;
					case 'C' : return ARROW_LEFT;
					case 'D' : return ARROW_RIGHT;
				}
			}
		}

		return '\x1b';
	} else {
		return c;
	}
}

int getWindowSize (int *rows, int *cols) {
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
		return -1;
	} else {
		*cols = ws.ws_col;
		*rows = ws.ws_row;
		return 0;
	}
}

/*** Appending Buffer for Write ***/

struct Buffer {
	char *b;
	int len;
};

//Buffer Append func
void bufferAppend (struct Buffer *buffer, const char *s, int len) {
	char *new = realloc (buffer->b, buffer->len + len);
	
	if (new == NULL) return;
	memcpy (&new[buffer->len], s, len);
	buffer->b = new;
	buffer->len += len;
}	

//Buffer Destructor
void bufferFree (struct Buffer *buffer) {
	free(buffer->b);
}

/*** Output ***/
void editorDrawRows(struct Buffer *buffer) {
	int y;

	for (y = 0; y < E.screenrows; y++) {
		
		if (y == E.screenrows / 5) {
			char welcome[80];
			int welcome_len = snprintf(welcome, sizeof(welcome), 
					"\e[1;3;38;2;153;102;204mAmethyst Editor\e[0m --ver%s", AMETHYST_VER);
			if (welcome_len > E.screencols) welcome_len = E.screencols;
			
			int padding = (E.screencols - welcome_len) / 2;
			if (padding) {
				bufferAppend(buffer, ">", 1);
				padding--;
			} while (padding--) bufferAppend(buffer, " ", 1);
			
			bufferAppend (buffer, welcome, welcome_len);
		} else {		
			bufferAppend (buffer, ">", 1);
		}
		bufferAppend (buffer, "\x1b[K", 3);
		//write(STDOUT_FILENO, "~", 1);
		if (y < E.screenrows - 1) bufferAppend (buffer, "\r\n", 2); //write(STDOUT_FILENO, "\r\n", 2);
	}
}

void editorRefreshScreen () {
	struct Buffer buff = BUFF_INIT;
	
	bufferAppend(&buff, "\x1b[?25l", 6);	
	bufferAppend(&buff, "\x1b[H", 3);

	editorDrawRows(&buff);
	
	char buf[32];
	snprintf(buf, sizeof(buf), "\x1b[%d,%dH", E.cx + 1, E.cy + 1);
	bufferAppend(&buff, buf, strlen(buf));	

	bufferAppend(&buff, "\x1b[?25h", 6);

	write(STDOUT_FILENO, buff.b, buff.len);
	bufferFree(&buff);
}

/*** Input ***/

void editorMoveCursor (int key) {
	switch (key) {
		case ARROW_LEFT:
			if (E.cx != 0) {
				E.cx--;
			}
			break;
		case ARROW_RIGHT:
			if (E.cx != E.screencols - 1) {
				E.cx++;
			}
			break;
		case ARROW_UP:
			if (E.cy != 0) {
				E.cy++;
			}
			break;
		case ARROW_DOWN:
			if (E.cy != E.screenrows - 1) {
				E.cy--;
			}
			break;
	}
}

void editorProcessKeypress () {
	int c = editorReadKey();
	switch (c) {
		case CTRL_KEY('q') :
			write(STDOUT_FILENO, "\x1b[2J", 4);
			write(STDOUT_FILENO, "\x1b[H", 3);
			exit (0);
			break;

		case ARROW_UP:
		case ARROW_DOWN:
		case ARROW_LEFT:
		case ARROW_RIGHT:
			editorMoveCursor(c);
			break;
	}
}

/*** init ***/
void initEditor () {
	
	E.cx = 0;
	E.cy = 0;

	if (getWindowSize(&E.screenrows, &E.screencols) == -1) die("Windows Size get failed");
}

int main () {
	enableRawMode();
	initEditor();

	while (1) {
		editorRefreshScreen();
		editorProcessKeypress();
	}
	
	return 0;
}
