
 1. About Procwin
 
 Procwin is a process explorer, showing all currently executing processes.
 For every process you can examine the loaded modules, the heaps,
 the structure of the address space and the generated threads.

 For every module there can be viewed:
  - the memory image of the module
  - the PE header of the module
  - the code section as disassembled list
  - the exported functions of the module
  - the imported functions of the module

 It is possible to change the printed values and so to
 manipulate foreign processes.

 
 2. Installation/Deinstallation

 ProcWin consists of PROCWIN.EXE and JDISASM.DLL. No registry entries
 will be created or modified. So just unzip the ProcWin package,
 preferably in its own directory, then run ProcWin.EXE. To deinstall
 simply delete the directory.


 3. Restrictions

 - ProcWin was originally written for Windows 9x, using the Toolhelp
   functions, which wasn't available for Windows NT and its successors.
   Later support for psapi.dll was added, which works for NT/XP/Vista/
   Windows 7 and (hopefully) the rest. However, only processes owned
   by the current user may be examined, for other processes no memory
   map will be displayed.
 - The "Explore" menu item might not work as expected for all Windows
   versions.


 4. Creating PROCWIN.EXE

 See MAKEFILE for details how to create ProcWin.exe.
 If you're not interested in the source, all files except ProcWin.exe,
 JDisasm.dll and readme.txt may be deleted.
 

 5. License

 ProcWin is Public Domain. You will use this program at your own risk.

 Written 2000-2009, Japheth ( http://www.japheth.de )

