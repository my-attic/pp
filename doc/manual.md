#PP aka "PéPé le compiler"

Pépé le compiler is a freewere standard Pascal compiler for the **Android** plateform, that runs directly on the handheld. Pépé generates true native ARM executable code fom standard Pascal source program. Console applications may be generated on board in the train, while waiting the bus, while fishing, on the beach, or what ever you are, either on a tablet or on a smartphone, provided it runs with the **Android**  operating system and with an ARM processor.

##User manual

When the application is launched, a list of files appears. Files recognised by **PP** are :

- Pascal source files, with the extension `.pas`
- Executable files, with the extension `.exe`

The menu allows to

- create a new source file.
- choose the default memory location internal or external (see further). By default, the external location is set.

A short click on a source file compiles it and builds the executable.
A short click on an executable file runs it.
A long click displays a context menu. This context menu depends on the type of the file.

On a source file, the context menu allows to

- **edit** : lauch the editor in order to edit the file
- **build** : compile the file and build the executable. This is equivalent to a short click on the file.

On an executable file, the context menu allows to

- **inspect** : launch a disassembler to show the generated ARM instructions.
- **run** : execute the file

The source files may be stored either, by default in the external memory (sdcard) or in the internal memory.
The external memory is in the directory `/mnt/sdcard/Pepe/`.
The internal memory is in the directory `/data/data/pp.compiler/`, which is the internal storage location of the Pépé application.
The executable files are always stored in the internal memory, as this is the only location that may be mapped as executable zone.
The programs are executed in a console that allows all the features of the Standard Pascal.

##Language description

###Standard Pascal

	TO DO

###Implementation defined features

	TO DO

###Non standard features

	TO DO

###Inline assembler

	TO DO

##Non standard predefined functions and procedures

**Clrscr**

>**Description**		Clears the screen and set the cursor at position (1,1)

>**Declaration**		`Procedure clrscr;`

>**Comment**			This procedure is equivalent to `rewrite(Output);`

**GotoXY**

>**Description**		 Set cursor position on the screen

>**Declaration**		`procedure gotoxy(x,y:integer);`

>**Comment**			The origin is located at (1,1) on the upper left corner of the screen. If the coordinates are out of the screen bound, they are forced either to the minimum value, equal to 1, or to the maximum value that depends on the current screen dimensions.

**ScreenHeight**

>**Description**		Returns the current height of the screen

>**Declaration**		`function screenheight:integer;`

**ScreenWidth**

>**Description**		Returns the current width of the screen

>**Declaration**		`function screenwidth:integer;`

**WhereX**

>**Description**		Returns the x position of the cursor, in the range 1..ScreenWidth

>**Declaration**		`function wherex:integer;`

**WhereY**

>**Description**		Returns the y position of the cursor, in the range 1..ScreenHeight

>**Declaration**		`function wherey:integer;`

##Compiler directives

`{$i file_name}` This directive tells the compiler to include the file file_name in the compiled text.
  
`{$define name}` Defines the symbol name.

`{$undef name}` If the symbol name was previously defined, this directive undefines it.

`{$ifdef name}` Starts a conditional compliation. If the symbol name is not defined, this will skip the compilation of the text that follows it, up to the first `{$else}` or `{$endif}` directive.

If the symbol name is defined, then the compliation continues up to the `{$else}` directive.

`{$else}` This directive switches conditional compilation. If the text delimited by the previous `{$if name}` directive was ignored, then the text that follows {$else}, up to the `{$endif}` directive is normally compiled, and vice versa.

`{$endif}` This directive ends a conditional compilation sequence.

`{$echo message}` Displays a message on the console during compilation.

##How to

###Function Readkey

This function waits and returns a character hit on the keyboard. It is predefined in Turbo pascal, and may be written as follows in Standard Pascal :

	Function readkey:char;
	var c : char;
	begin
	  c:=input^;
	  get(input);
	  readkey:=c;
	end;

##System calls

System calls can be performed either with ARM assembler defined function, or inline defined function.

	mov r7,#SYSCALL_NUMBER
	swi 0 

**Example : function times**

This function returns a number of clock ticks and assign values of the user time, system time, chidren user time, and children system time.

The syscall number of this function is 43.

###assembler 

	Program syscall;
	Type tms=record
	  utime, stime, cutime, cstime : integer;
	end;
	function times(var t : tms):integer;
	asm
	  mov r7,#43
	  swi 0
	  mov pc,lr
	end;
	begin
	  writeln(times(nil));
	end.

###inline function

	Program syscall;
	Type tms=record
	  utime, stime, cutime, cstime : integer;
	end;
	function times(var t : tms):integer; inline ($e3a07000+43, $ef000000);
	begin
	  writeln(times(nil));
	end.

##Error messages

	TO DO

