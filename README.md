# c-word-ladder-tester
C program that tests c-word-ladder.<br>

This program tests a version of the program from c-word-ladder repository to see if matches the expected output.<br>
It tests to see whether its stdout and stderr is the same as what is expected.<br>

This program demonstrates the use of multiple processes, piping, and signalling in C.<br>

It makes 3 processes using fork(), where the 1st process runs word-ladder program and sends the stdout to 2nd process and stderr to 3rd process using pipes.<br>
Then the 2nd and 3rd processes check whether it matches the expected stdout and stderr.<br>
