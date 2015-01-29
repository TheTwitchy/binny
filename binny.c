#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

/*NOTE: This could be curses.h or ncurses.h. Depends on the distro. */
#include <ncurses.h>



#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS /*stops VC++ from complaining about insecure somthingwhatnot*/
#include "pgetopt.h"
#define opterr popterr
#define getopt pgetopt
#define optarg poptarg
#define optind poptind
#else 
#include <getopt.h>
#endif

#define PROG_NAME 				"binny"
#define VERSION 				"1.1"
#define NEW_FILE_BUFFER_SIZE	0x10

#define BYTES_PER_LINE_DEFAULT 	0x10
#define BYTES_PER_GROUP_DEFAULT 4
#define RIGHT_OFFSET 			13
#define ASCII_OFFSET 			0
#define SEPARATOR 				'|'
#define BUFFER_LENGTH 			255
#define POPUP_WIDTH 			36
#define POPUP_HEIGHT 			3

#define MODE_BINARY	0
#define MODE_ASCII	1

FILE * fp;
char filename[BUFFER_LENGTH];
unsigned char* buffer;
char userInput[BUFFER_LENGTH];
char userOutput[BUFFER_LENGTH];
unsigned long bufferLength;
WINDOW * borderWin;
WINDOW * editorWin;
WINDOW * userWin;
WINDOW * popupWin;

int bufferModified = 0;

int curBufPos = 0;
int curBufPosHalf = 0;
int topLineOfScreen = 0; /* used to track how far down it's scrolled*/

int bytesPerLine = BYTES_PER_LINE_DEFAULT;
int bytesPerGroup = BYTES_PER_GROUP_DEFAULT;
int showASCII = 0;
int mode = MODE_BINARY;

/*FUNCTION PROTOTYPES*/
void printHelp();
int parseOptions(int argc, char ** argv);
int resizeBuffer(int newSize);
void attemptCleanExit(int status);
void resizeSignalHandler(int signum);
void sigintHandler(int signum);
int setupScreen();
void drawASCII(int n);
void drawEditorWin();
void drawBorderWin();
void drawUserWin();
void moveEditorCursorUp();
void moveEditorCursorDown();
void moveEditorCursorLeft();
void moveEditorCursorRight();
void moveCursorToScreenPos();
void handleInput(int c);
void handleScrolling();
int leastOf(int x, int y);
void inputPopup();
int saveBuffer();

int main(int argc, char** argv)
{
    int ch = 0;

    if (parseOptions(argc, argv))
    {
        return EXIT_FAILURE;
    }

    /*===FILE IO OPERATIONS=== */

    /* Check to see if the file given is a directory. */

    fp = fopen(filename, "r");
    if (fp != NULL)
    {
        /*File doesn't exist, create it*/
        /*fp = fopen(filename, "w+");
         fp = */
        /*printf("%s: Couldn't open %s.\n", PROG_NAME, filename);*/
        /*return EXIT_FAILURE;*/

        /* after this point we can attempt to exit cleanly.*/

        if (fseek(fp, 0, SEEK_END) != 0)
        {
            printf("%s: Couldn't read %s.\n", PROG_NAME, filename);
            attemptCleanExit(EXIT_FAILURE);
        }
        /*get the filesize*/
        bufferLength = (unsigned long) ftell(fp);
        if (bufferLength < 0)
        {
            printf("%s: Couldn't get size of %s.\n", PROG_NAME, filename);
            attemptCleanExit(EXIT_FAILURE);
        }
        fseek(fp, 0, SEEK_SET);
        buffer = malloc(sizeof(char) * (bufferLength));
        fread(buffer, sizeof(char), bufferLength, fp);
    }
    else
    {
        bufferLength = NEW_FILE_BUFFER_SIZE;
        buffer = malloc(sizeof(char) * (bufferLength));
        memset(buffer, 0, bufferLength);
        bufferModified = 1;
    }

    /*SIGNALS HANDLING*/
    signal(SIGINT, sigintHandler);
#ifdef SIGWINCH
    signal(SIGWINCH, resizeSignalHandler);/*when the terminal is resized*/
#endif

    if (setupScreen())
    {
        attemptCleanExit(EXIT_FAILURE);
    }

    /*begin main loop*/
    while (1)
    {
        ch = getch();
        handleInput(ch);
        drawUserWin();
        drawEditorWin();
    }

    /*We should never get here*/
    return EXIT_FAILURE;
}

void printHelp()
{
    printf("%s v%s\n", PROG_NAME, VERSION);
    printf("A simple in-place binary editor.\n");
    printf("Usage:\n\t%s [OPTIONS] FILENAME\n", PROG_NAME);
    printf("Options:\n\t-h\t\tPrint Help\n\t-a\t\tShow ASCII\n\t-l bytes\tSet bytes displayed per line, default 0x10\n\t-g bytes\tSet byte grouping, default 4\n");
    printf("Commands:\nAll commands are issued with shift-<command key>.\n");
    printf("\tQ\t\tquit - Exit the program\n");
    printf("\tS\t\tsave - Save the buffer to the file\n");
    printf("\tG\t\tgoto - Jump to a position in the buffer\n");
    printf("\tR\t\tresize - Resize the current buffer\n");
    printf("\tA\t\tascii_insert - Insert a string of ascii\n");
    printf("\tB\t\tbatch_insert - Insert a value repeatedly\n");

}

int parseOptions(int argc, char** argv)
{
    int ch;

    /*===OPTIONS PARSING===*/
    opterr = 0;
    while ((ch = getopt(argc, argv, "hal:g:")) != -1)
    {
        if (ch == 'h')
        {
	    printHelp();
            exit(EXIT_SUCCESS);
        }
        else if (ch == 'l')
        {
            if (strtol(optarg, NULL, 0) == 0 || strtol(optarg, NULL, 0) < 1)
            {
                printf("%s: Bad argument '%s' in option '%c'. Use '%s -h' for Help.\n", PROG_NAME, optarg, ch, PROG_NAME);
                return EXIT_FAILURE;
            }
            bytesPerLine = strtol(optarg, NULL, 0);
        }
        else if (ch == 'a')
        {
            showASCII = 1;
        }
        else if (ch == 'g')
        {
            if (strtol(optarg, NULL, 0) == 0 || strtol(optarg, NULL, 0) < 1)
            {
                printf("%s: Bad argument '%s' in option '%c'. Use '%s -h' for Help.\n", PROG_NAME, optarg, ch, PROG_NAME);
                return -1;
            }
            bytesPerGroup = strtol(optarg, NULL, 0);
        }
        else
        {
            printf("%s: Invalid option. Use '%s -h' for Help.\n", PROG_NAME, PROG_NAME);
            return -1;
        }
    }
    if (optind >= argc)
    {
        printf("%s: No filename specified. Use '%s -h' for Help.\n", PROG_NAME, PROG_NAME);
        return -1;
    }
    /*More Error checking on user prefs*/
    if (bytesPerLine < bytesPerGroup)
    {
        bytesPerGroup = bytesPerLine;
    }
    sprintf(filename, "%s", argv[optind]);
    return 0;
}

//Will resize the buffer and copy all of buffer (that it can) into the new one. Returns the new size or -1 on Error*/
int resizeBuffer(int newSize)
{
    if (newSize <= 0)
    {
        sprintf(userOutput, "Error: New buffer size must be greater than 0.");
        return -1;
    }
    unsigned char * newBuf = malloc(sizeof(char) * (newSize));
    if (newBuf == NULL) return -1;
    memset(newBuf, 0, newSize); /*zero the buffer just in case*/
    memcpy(newBuf, buffer, leastOf(newSize, bufferLength));
    free(buffer);

    /*basically reset everything */
    buffer = newBuf;
    bufferLength = newSize;
    curBufPos = 0;
    topLineOfScreen = 0;
    return 0;
}

/* called when the terminal resize, then redraws everything.*/
void resizeSignalHandler(int signum)
{
    endwin();
    setupScreen();
}

/*Handles CTRL-C AKA SIGINT */
void sigintHandler(int signum)
{
    attemptCleanExit(EXIT_SUCCESS);
}

/*Does basic screen setup, with error checking*/
int setupScreen()
{
    /*===NCURSES OPERATIONS===*/
    /*make sure we can init ncurses properly*/
    int rows, cols;

    borderWin = initscr();
    if (borderWin == NULL)
    {
        printf("Error: Couldn't initialize main screen.\n");
        return -1;
    }

    /*raw();*/
    cbreak();
    keypad(stdscr, TRUE);
    noecho(); /*Turns off character echoing to the screen*/
#ifdef _WIN32
    curs_set(2);
#endif

    getmaxyx(borderWin, rows, cols);
    editorWin = newwin(rows - 4, cols - 2, 1, 1);
    userWin = newwin(2, cols - 2, rows - 3, 1);

    /*first Draw*/
    drawBorderWin();
    drawUserWin();
    drawEditorWin();
    return 0;
}

void attemptCleanExit(int status)
{
    delwin(editorWin);
    endwin();
    if (fp != NULL) fclose(fp);
    free(buffer);
    exit(status);
}

void moveEditorCursorUp()
{
    curBufPos -= bytesPerLine;
    curBufPosHalf = 0;
    if (curBufPos < 0) curBufPos = 0;
}
void moveEditorCursorDown()
{
    curBufPos += bytesPerLine;
    curBufPosHalf = 0;
    if (curBufPos >= bufferLength) curBufPos = bufferLength - 1;
}
void moveEditorCursorLeft()
{
    curBufPosHalf = 0;
    curBufPos -= 1;

    if (curBufPos < 0) curBufPos = 0;
}
void moveEditorCursorRight()
{
    curBufPosHalf = 0;
    curBufPos += 1;

    if (curBufPos >= bufferLength) curBufPos = bufferLength - 1;
}

void moveCursorToScreenPos()
{
    int row = (curBufPos - (curBufPos % bytesPerLine)) / bytesPerLine;
    int x = (curBufPos - (bytesPerLine * row));
    int col = x * 2 + (x / bytesPerGroup) + curBufPosHalf + RIGHT_OFFSET;
    row = row - topLineOfScreen;

#ifdef _WIN32
    move(row+1, col+1);
#else
    wmove(editorWin, row, col);
#endif
}

void handleInput(int c)
{
    if (mode == MODE_ASCII)
    {
        if (c == KEY_END)
        {
            mode = MODE_BINARY;
            sprintf(userOutput, "ASCII mode disabled.");
        }
        else if (c == KEY_BACKSPACE)
        {
            moveEditorCursorLeft();
            buffer[curBufPos] = 0;
            bufferModified = 1;
        }
        else if (c == KEY_RIGHT)
        {
            moveEditorCursorRight();
        }
        else if (c == KEY_LEFT)
        {
            moveEditorCursorLeft();
        }
        else if (c == KEY_UP)
        {
            moveEditorCursorUp();
        }
        else if (c == KEY_DOWN)
        {
            moveEditorCursorDown();
        }
        else
        {
            buffer[curBufPos] = c;
            moveEditorCursorRight();
            bufferModified = 1;
        }
    }
    else if (mode == MODE_BINARY)
    {

        if (c == 'R')
        {
            sprintf(userOutput, "Resize Buffer to:");
            inputPopup(userOutput);
            if (strtol(userInput, NULL, 0) <= 0)
            {
                sprintf(userOutput, "Error: invalid number");
                return;
            }
            resizeBuffer(strtol(userInput, NULL, 0));
            sprintf(userOutput, "Buffer resized to 0x%lX / %ld", bufferLength, bufferLength);
            bufferModified = 1;
        }
        else if (c == 'G')
        {

            sprintf(userOutput, "Goto:");
            inputPopup(userOutput);
            if (strtol(userInput, NULL, 0) < 0)
            {
                sprintf(userOutput, "Error: invalid number");
                return;
            }
            curBufPos = leastOf(strtol(userInput, NULL, 0), bufferLength - 1);
            sprintf(userOutput, "Moved cursor");
        }
        else if (c == 'A')
        {
            mode = MODE_ASCII;
            sprintf(userOutput, "ASCII mode enabled. Press END to disable.");
            /* OLD ASCII INSERT CODE
             sprintf(userOutput, "ASCII Insert:");
             inputPopup(userOutput);
             if (strlen(userInput) == 0) {
             sprintf(userOutput, "Error: empty string");
             return;
             }
             strncpy(&buffer[curBufPos], userInput, leastOf(strlen(userInput),
             bufferLength - curBufPos));
             sprintf(userOutput, "String inserted");
             bufferModified = 1;
             */
        }
        else if (c == 'B')
        {
            /*Batch insert*/
            char charToInsert;
            int numberToInsert;
            sprintf(userOutput, "Batch Insert Character Value:");
            inputPopup(userOutput);
            if (strlen(userInput) == 0)
            {
                sprintf(userOutput, "Error: Empty string.");
                return;
            }
            else if (strtol(userInput, NULL, 0) < 0 || strtol(userInput, NULL, 0) > 255)
            {
                sprintf(userOutput, "Error: Bad character value.");
                return;
            }
            charToInsert = strtol(userInput, NULL, 0);

            sprintf(userOutput, "Number to Insert:");
            inputPopup(userOutput);
            if (strlen(userInput) == 0)
            {
                sprintf(userOutput, "Error: Empty string.");
                return;
            }
            else if (strtol(userInput, NULL, 0) < 0)
            {
                sprintf(userOutput, "Error: Bad value.");
                return;
            }
            numberToInsert = strtol(userInput, NULL, 0);
            memset(&buffer[curBufPos], charToInsert, leastOf(numberToInsert, bufferLength - curBufPos));
            sprintf(userOutput, "Character 0x%02x inserted", charToInsert);
            bufferModified = 1;
        }
        else if (c == 'S')
        {
            saveBuffer();
        }
        else if (c == 'Q')
        {
            if (bufferModified)
            {
                sprintf(userOutput, "Buffer is modified. Save? [Y/n]");
                inputPopup(userOutput);
                if (userInput[0] != 'n' && userInput[0] != 'N')
                {
                    int ret = saveBuffer();
                    if (ret)
                    {
                        return;
                        /* basically, do not exit, there was a problem. save buffer will output its own status*/
                    }
                }
            }
            attemptCleanExit(EXIT_SUCCESS);
        }
        else if (c == KEY_RIGHT)
        {
            moveEditorCursorRight();
        }
        else if (c == KEY_LEFT)
        {
            moveEditorCursorLeft();
        }
        else if (c == KEY_UP)
        {
            moveEditorCursorUp();
        }
        else if (c == KEY_DOWN)
        {
            moveEditorCursorDown();
        }
        else if (c == '0')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '1')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x10;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x01;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '2')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x20;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x02;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '3')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x30;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x03;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '4')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x40;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x04;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '5')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x50;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x05;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '6')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x60;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x06;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '7')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x70;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x07;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '8')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x80;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x08;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == '9')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0x90;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x09;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == 'a')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0xa0;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x0a;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == 'b')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0xb0;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x0b;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == 'c')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0xc0;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x0c;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == 'd')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0xd0;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x0d;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == 'e')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0xe0;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x0e;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
        else if (c == 'f')
        {
            if (curBufPosHalf == 0)
            {
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] |= 0xf0;
                curBufPosHalf = 1;
            }
            else
            {
                buffer[curBufPos] = buffer[curBufPos] >> 4;
                buffer[curBufPos] = buffer[curBufPos] << 4;
                buffer[curBufPos] |= 0x0f;
                moveEditorCursorRight();
            }
            bufferModified = 1;

        }
    }
}

void drawASCII(int line)
{
    int displayRow = line - topLineOfScreen;
    int displayCol = (bytesPerLine * 2) + (bytesPerLine / bytesPerGroup) + RIGHT_OFFSET + ASCII_OFFSET;
    int i;

    if (!showASCII) return;

    wmove(editorWin, displayRow, displayCol);
    wprintw(editorWin, "%c ", SEPARATOR);
    for (i = line * bytesPerLine; i < (line + 1) * bytesPerLine; i++)
    {
        if (i >= bufferLength)
        {
            break;
        }

        if (i == curBufPos) wattron(editorWin, A_REVERSE);

        /*check to see if its a printable ASCII char*/
        if (buffer[i] >= 0x20 && buffer[i] <= 0x7E)
        {
            wprintw(editorWin, "%c", buffer[i]);
        }
        else
        {
            wprintw(editorWin, ".");
        }
        if (i == curBufPos) wattroff(editorWin, A_REVERSE);
    }
}

/* draw the screen, which is updated with every change. Does not include the border*/
void drawEditorWin()
{
    int n, rows, startByte, endByte;
    handleScrolling();

    werase(editorWin);/*difference between clear and erase is that clear calls refresh directly after. With lots of keypresses you get flicker. Hence, erase. */
    wmove(editorWin, 0, 0);
    getmaxyx(editorWin, rows, n);
    startByte = topLineOfScreen * bytesPerLine; /* first byte on the top displayed line*/
    endByte = leastOf(bufferLength, startByte + (rows * bytesPerLine)); /* last byte to be displayed. Basically startByte + total Number of bytes that can be displayed*/

    for (n = startByte; n < endByte; n++)
    {
        if (n % bytesPerLine == 0)
        {
            /* print the line header */
            if (n != startByte) wprintw(editorWin, "\n");
            wattron(editorWin, A_BOLD);
            wprintw(editorWin, "0x%08X", n);
            wattroff(editorWin, A_BOLD);
            wprintw(editorWin, " %c ", SEPARATOR);
        }
        else if (n % bytesPerGroup == 0)
        {
            /*add a space between groups of bytes*/
            wprintw(editorWin, " ");
        }
        wprintw(editorWin, "%02x", buffer[n]);

        /*if it's the end of a line, or if it's the last char, then show the ASCII representation*/
        if ((n + 1) % bytesPerLine == 0 || n == endByte - 1)
        {
            drawASCII((n - (n % bytesPerLine)) / bytesPerLine);
        }
    }

    moveCursorToScreenPos();
    wrefresh(editorWin);
}

void drawUserWin()
{
    werase(userWin);
    wmove(userWin, 0, 0);
    wattron(userWin, A_REVERSE);
    wborder(userWin, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');
    wprintw(userWin, "Position: 0x%X / %d of 0x%X / %d bytes", curBufPos, curBufPos, bufferLength, bufferLength);
    wmove(userWin, 1, 0);
    wprintw(userWin, "Status  : %s", userOutput);
    wattroff(userWin, A_REVERSE);
    wrefresh(userWin);
}

/* the border, which does not update and will only be drawn once*/
void drawBorderWin()
{
    char temp[255];
    int y, x;
    getmaxyx(borderWin, y, x);
    erase();
    attron(A_REVERSE);
    /*draw borders*/
    wborder(borderWin, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');

    wmove(borderWin, 0, 3);
    sprintf(temp, "%s v%s - %s", PROG_NAME, VERSION, filename);
    wprintw(borderWin, temp);

    move(y - 1, 1);
    y = x;/*this is literally only so the warning about not using x will stop popping up*/
    printw("Commands: 'Q'uit 'S'ave 'G'oto 'R'esize 'A'scii_mode 'B'atch_insert");

    attroff(A_REVERSE);
    refresh();
}

/*returns the lesser of two numbers*/
int leastOf(int x, int y)
{
    if (x > y) return y;
    return x;
}

void handleScrolling()
{
    int done = 0;
    int rows, startByte, endByte;
    /*Scroll until we get to see the cursor again. Used for Gotos*/
    while (!done)
    {
        startByte = topLineOfScreen * bytesPerLine; /* first byte on the top displayed line*/
        getmaxyx(editorWin, rows, endByte);
        endByte = leastOf(bufferLength, startByte + (rows * bytesPerLine)); /* last byte to be displayed. Basically startByte + total Number of bytes that can be displayed*/

        if (curBufPos < startByte)
        {
            topLineOfScreen--;

        }
        else if (curBufPos >= endByte)
        {
            topLineOfScreen++;
        }
        else
        {
            done = 1;
        }
    }
}

void inputPopup(char * title)
{
    int totalRows, totalCols;

    getmaxyx(borderWin, totalRows, totalCols);

    /* center the new popup window*/
    popupWin = newwin(POPUP_HEIGHT, POPUP_WIDTH, (totalRows / 2) - (POPUP_HEIGHT / 2), (totalCols / 2) - (POPUP_WIDTH / 2));
    wattron(popupWin, A_REVERSE);
    wborder(popupWin, ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ');

    /*print the title*/
    mvwaddnstr(popupWin, 0, 1, title, POPUP_WIDTH - 2);

    wattroff(popupWin, A_REVERSE);

    /*move the cursor into position*/
    move((totalRows / 2) - (POPUP_HEIGHT / 2) + 1, (totalCols / 2) - (POPUP_WIDTH / 2) + 1);
    wrefresh(popupWin);

    echo();
    getnstr(userInput, POPUP_WIDTH - 2);
    noecho();
}

int saveBuffer()
{
    if (fp != NULL) close(fp);
    fp = fopen(filename, "w+");
    if (fp == NULL)
    {
        sprintf(userOutput, "Error: Couldn't save to %s", filename);
        return -1;
    }
    fseek(fp, 0, SEEK_SET); /* go to beginning of file*/
    fwrite(buffer, sizeof(unsigned char), bufferLength, fp);
    bufferModified = 0;
    sprintf(userOutput, "Buffer saved to %s", filename);
    return 0;

}

