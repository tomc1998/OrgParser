#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// A struct which holds the data of an item in an org file - this
// includes its name, description, scheduled date, TODO type etc.
typedef struct {
  char* name;
  char* desc;
  char* todoType;
  int indentLevel;
  long scheduleTime;
} ORGItem;

// Remember to free the actual ORGItem pointer!
void freeORGItem (ORGItem* oi) {
  free(oi->name);
  free(oi->desc);
  free(oi->todoType);
  oi->name = NULL;
  oi->desc = NULL;
  oi->todoType = NULL;
}

void printItem(ORGItem* oi) {
  printf("Name: %s\n", oi->name);
  printf("Description: %s\n", oi->desc);
  printf("TODO Type: %s\n", oi->todoType);
  printf("Indent level: %i\n", oi->indentLevel);
  if (oi->scheduleTime != 0) {
    printf("Scheduled Time: %ld\n", oi->scheduleTime);
  }
  printf("\n");
}

// A struct which holds the data of an ORG file
typedef struct {
  int numItems;
  ORGItem** items;
} ORGFile;

// Remember to free the actual ORGFile pointer!
void freeORGFile(ORGFile* of) {
  int i = 0;
  for (; i < of->numItems; ++ i) {
    freeORGItem(of->items[i]);
    free(of->items[i]);
  }
  free(of->items);
  of->items = NULL;
}

// 0 = SUCCESS
// Returns lines and number of lines into the lines and numLines args
static int splitLines(char* data,
                      int const dataLen,
                      char*** lines,
                      int* numLines) {
  // Count lines in data
  *numLines = 0;
  int i = 0;
  for (; i < dataLen; ++i) {
    if (data[i] == '\n') {
      *numLines = *numLines + 1;
    }
  }
  
  *lines = (char**)malloc(sizeof(char*) * *numLines);
  // Extract lines from data
  char* line = strtok(data, "\n");
  i = 0;
  for (; i < *numLines; ++i) {
    (*lines)[i] = line;
    line = strtok(NULL, "\n");
  }
  
  return 0;
}

// 0 = SUCCESS
// 1 = FILE NOT FOUND
// Loads a file into memory
// Stores file in buf, puts length of buffer into bufLen
static int loadFile(char* path, char** buf, long* bufLen) {
  FILE* f = fopen(path, "rb");
  if (f) {
    // Load file into a buffer
    fseek(f, 0, SEEK_END);
    *bufLen = ftell(f);
    fseek(f, 0, SEEK_SET);
    *buf = malloc(*bufLen + 1);
    fread(*buf, *bufLen, 1, f);
    fclose(f);
  }
  else {
    fprintf(stderr, "File not found\n");
    return 1;
  }
  return 0;
}

// Parses a string into an ORGItem struct
// 0 = SUCCESS
static int parseORGItem(char** lines, int numLines, ORGItem** out) {
  (*out) = (ORGItem*)malloc(sizeof(ORGItem));
  // Parse initial line
  // Calculate indent level
  int indentLevel = 1;
  char* line = lines[0];
  while (line[++indentLevel] == '*') {} 
  indentLevel -= 1;
  (*out)->indentLevel = indentLevel; // Set indent level
  // Init empty strings for all data
  (*out)->name = (char*)malloc(1); 
  strcpy((*out)->name, "");
  (*out)->todoType = (char*)malloc(1); 
  strcpy((*out)->todoType, "");
  (*out)->desc = (char*)malloc(1); 
  strcpy((*out)->desc, "");
  (*out)->scheduleTime = 0;

  // Loop through lines
  int i = 1;
  for (; i < numLines; i ++) {
    line = lines[i];
    // Check if this line is a schedule
    int isSchedule = 1;
    static char* SCHEDULED_STR = "SCHEDULED";
    unsigned int SCHEDULED_STR_LEN = strlen(SCHEDULED_STR);
    if (strlen(line) < SCHEDULED_STR_LEN) {
      isSchedule = 0;
    }
    else { // Compare strings
      isSchedule = !strncmp(line, SCHEDULED_STR, SCHEDULED_STR_LEN);
    }

    if (isSchedule) {
      // Parse (*out) time
      // Find string inbetween < >
      char *begin;
      char *ptr = line;
      while(*ptr != '<') {
        ++ptr;
      }
      begin = ++ptr;
      int timeLen = 0;
      while(begin[timeLen] != '>') {
        ++timeLen;
      }
      char* timeStr = (char*)malloc(sizeof(char)*timeLen + 1);
      // Copy schedule string to timeStr;
      memcpy(timeStr, begin, timeLen);
      timeStr[timeLen] = 0; // ins nul at the end to make this a string

      // Now parse (*out) the year - month - day
      struct tm schedTimeTM;
      schedTimeTM.tm_year  = strtol(timeStr, NULL, 10) - 1900;
      schedTimeTM.tm_mon = strtol(timeStr+5, NULL, 10);
      schedTimeTM.tm_mday   = strtol(timeStr+8, NULL, 10);
      schedTimeTM.tm_hour = 0;
      schedTimeTM.tm_min = 0;
      schedTimeTM.tm_sec = 0;

      (*out)->scheduleTime = (long)mktime(&schedTimeTM);
    }
    else {
      // Add line to desc
      char* temp = (*out)->desc;
      (*out)->desc = (char*)malloc(sizeof(char)*strlen(temp)
                                    + sizeof(char)*strlen(line) + 1);
      strcpy((*out)->desc, temp);
      strcat((*out)->desc, line);
      free(temp);
    }
  }
  return 0;
}

// Parses a string into an ORGFile struct
// 0 = SUCCESS
static int parseORGFile(char* data, const int dataLen, ORGFile** out) {
  // Quickly define a linked list for dynamic storage of items
  struct ORGItemNode {
    ORGItem* item;
    struct ORGItemNode* next;
  };
  char** lines;
  int numLines;
  splitLines(data, dataLen, &lines, &numLines);

  // These are the variable we'll pass to parseORGItem
  char** itemLinePtr = NULL;
  int numItemLines;

  // Loop through lines to find items
  struct ORGItemNode* rootNode = NULL;
  struct ORGItemNode* currNode = NULL;
  int i = 0;
  for (; i < numLines; ++i) {
    char* line = lines[i];
    if (line[0] == '*') { // Line is an item
      if (currNode != NULL) {
        // Send old lines to parseORGItem
        parseORGItem(itemLinePtr, numItemLines, &(currNode->item));
        printItem(currNode->item);
      }
      // Start a new line ptr and node
      itemLinePtr = &(lines[i]);
      numItemLines = 1;
      struct ORGItemNode* newNode =
        (struct ORGItemNode*)malloc(sizeof(struct ORGItemNode));
      if (currNode == NULL) {
        rootNode = newNode;
      }
      else {
        currNode->next = newNode;
      }
      currNode = newNode;
    }
    else { 
      ++numItemLines;
    }
  }
  free(lines);
  return 0;
}

// -1 = UNKNOWN ERROR (dafuq is happening)
// 0  = SUCCESS
// 1  = FILE NOT FOUND
// Loads a file into memory
int loadORGFile(char* path, ORGFile** out) {
  char* data = NULL;
  int dataLen = 0;
  loadFile(path, &data, &dataLen);
  int i = 0;
  int loadFileReturnCode = parseORGFile(data, dataLen, out);
  free (data);
  if (loadFileReturnCode == 0) {
    return 0;
  }
  else if (loadFileReturnCode == 1) {
    return 1;
  }
  else {
    return -1;
  }
}
