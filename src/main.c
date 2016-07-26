#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include "org.c"

void testHTTP() {
}

int main(int argc, char** argv) {
  ORGFile* orgFile = NULL;
  int returnCode = loadORGFile(
    "C:\\Users\\thoma\\Development\\Projects\\fullteamserver\\TODOList.org",
    &orgFile);
  printf("Return: %i\n", returnCode);
  return 0;
}
