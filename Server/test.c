#include <string.h>
#include <stdio.h>
int main() {
    int a = 1800;
    int b = a/1024;
    printf("b = %d\n", b);
    int i;
    for (i = 0; i < b; i++) {
        printf("hi\n");
    }
    printf("i = %d\n", i *1024);
}
while (( n = read( fd, &newChar, sizeof(newChar) ) ) > 0 ) {
    if ( nameLength == MaxName ) {
      break;
    }
    if (password[passLength] == newChar) {
      passLength++;
      if (passLength == strlen(password)) {
        auth = 1;
        break;
      }
    } else {
      passLength = 0;
    }
    
    if (doc_start && doc_finish) {
      if (newChar == ' ') {
        doc_finish = 0;
        name[nameLength] = 0;
      } else {
        name[ nameLength ] = newChar;
        nameLength++;
      }
    } else {
      if (newChar == '/') {
        doc_start = 1;
      }
    }
    printf("%c", newChar);
  }