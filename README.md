# binny
A simple curses based in-place hex editor for Linux. Built to replace using xxd and emacs hexl-mode over and over to do low level editing of files. Supports resizing of files and ASCII insertion.

## Examples

### Command Help
```
root@kali:~# binny -h
```

```
binny v1.2
A simple in-place binary editor.
Usage:
	binny [OPTIONS] FILENAME
Options:
	-h		Print Help
	-a		Show ASCII
	-l bytes	Set bytes displayed per line, default 0x10
	-g bytes	Set byte grouping, default 4
Commands:
All commands are issued with shift-<command key>.
	Q		quit - Exit the program
	S		save - Save the buffer to the file
	G		goto - Jump to a position in the buffer
	R		resize - Resize the current buffer
	A		ascii_insert - Insert a string of ascii
	B		batch_insert - Insert a value repeatedly
  ```
  
### Basic Usage
```
root@kali:~# binny -a `locate gcc`
```

![](https://i.imgur.com/bkopcEI.png)
