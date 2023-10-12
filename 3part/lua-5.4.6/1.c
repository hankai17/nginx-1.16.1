#include "stdio.h"

int add (int a, int b) {
    return a + b;
}

int main () {
    int a = 1;
    int b = 2;
    int c = add(a, b);

    printf("a + b = %d\n", c);

    return 0;
}

// gcc -g -c 1.c -o 1.o
// objdump -S -l 1.o

/*
1.o:     file format elf64-x86-64

Disassembly of section .text:

0000000000000000 <add>:
add():
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:3
objdump: Warning: source file /root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c is more recent than object file
#include "stdio.h"

int add (int a, int b) {
   0:   55                      push   %rbp
   1:   48 89 e5                mov    %rsp,%rbp
   4:   89 7d fc                mov    %edi,-0x4(%rbp)
   7:   89 75 f8                mov    %esi,-0x8(%rbp)
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:4
    return a + b;
   a:   8b 55 fc                mov    -0x4(%rbp),%edx
   d:   8b 45 f8                mov    -0x8(%rbp),%eax
  10:   01 d0                   add    %edx,%eax
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:5
}
  12:   5d                      pop    %rbp
  13:   c3                      retq   

0000000000000014 <main>:
main():
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:7

int main () {
  14:   55                      push   %rbp
  15:   48 89 e5                mov    %rsp,%rbp
  18:   48 83 ec 10             sub    $0x10,%rsp
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:8
    int a = 1;
  1c:   c7 45 fc 01 00 00 00    movl   $0x1,-0x4(%rbp)
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:9
    int b = 2;
  23:   c7 45 f8 02 00 00 00    movl   $0x2,-0x8(%rbp)
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:10
    int c = add(a, b);
  2a:   8b 55 f8                mov    -0x8(%rbp),%edx
  2d:   8b 45 fc                mov    -0x4(%rbp),%eax
  30:   89 d6                   mov    %edx,%esi
  32:   89 c7                   mov    %eax,%edi
  34:   e8 00 00 00 00          callq  39 <main+0x25>
  39:   89 45 f4                mov    %eax,-0xc(%rbp)
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:12

    printf("a + b = %d\n", c);
  3c:   8b 45 f4                mov    -0xc(%rbp),%eax
  3f:   89 c6                   mov    %eax,%esi
  41:   bf 00 00 00 00          mov    $0x0,%edi
  46:   b8 00 00 00 00          mov    $0x0,%eax
  4b:   e8 00 00 00 00          callq  50 <main+0x3c>
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:14

    return 0;
  50:   b8 00 00 00 00          mov    $0x0,%eax
/root/CLionProjects/nginx-1.16.1/3part/lua-5.4.6/1.c:15
}
  55:   c9                      leaveq 
  56:   c3                      retq   
*/

/*
[root@localhost lua-5.4.6]# gcc -O1 -c 1.c -o 1.o
[root@localhost lua-5.4.6]# objdump -S -l 1.o 

1.o:     file format elf64-x86-64


Disassembly of section .text:

0000000000000000 <add>:
add():
   0:   8d 04 37                lea    (%rdi,%rsi,1),%eax
   3:   c3                      retq   

0000000000000004 <main>:
main():
   4:   48 83 ec 08             sub    $0x8,%rsp
   8:   be 03 00 00 00          mov    $0x3,%esi
   d:   bf 00 00 00 00          mov    $0x0,%edi
  12:   b8 00 00 00 00          mov    $0x0,%eax
  17:   e8 00 00 00 00          callq  1c <main+0x18>
  1c:   b8 00 00 00 00          mov    $0x0,%eax
  21:   48 83 c4 08             add    $0x8,%rsp
  25:   c3                      retq   
*/
