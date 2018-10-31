/*
 * shasm.c
 * 
 * Compile with shastina
 */

#include "shastina.h"
#include <stdlib.h>
#include <string.h>

/*
 * @@TODO:
 */
int main(int argc, char *argv[]) {
  
  SNPARSER *pParser = NULL;
  SNENTITY ent;
  int retval = 0;
  long ln = 0;
  
  pParser = snparser_alloc();
  memset(&ent, 0, sizeof(SNENTITY));
  
  for(snparser_read(pParser, &ent, stdin);
      ent.status >= 0;
      snparser_read(pParser, &ent, stdin)) {
    
    ln = snparser_count(pParser);
    if (ln < LONG_MAX) {
      printf("%ld: ", ln);
    } else {
      printf("??: ");
    }
    
    if (ent.status == SNENTITY_EOF) {
      printf("End Of File\n");
      
    } else if (ent.status == SNENTITY_STRING) {
      if (ent.str_type == SNSTRING_QUOTED) {
        printf("String (%s) \"%s\"\n", ent.pKey, ent.pValue);
      } else if (ent.str_type == SNSTRING_CURLY) {
        printf("String (%s) {%s}\n", ent.pKey, ent.pValue);
      } else {
        /* Unknown string type */
        abort();
      }
      
    } else if (ent.status == SNENTITY_EMBEDDED) {
      printf("Embedded (%s)\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_BEGIN_META) {
      printf("Begin metacommand\n");
      
    } else if (ent.status == SNENTITY_END_META) {
      printf("End metacommand\n");
      
    } else if (ent.status == SNENTITY_META_TOKEN) {
      printf("Meta token %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_META_STRING) {
      if (ent.str_type == SNSTRING_QUOTED) {
        printf("Meta string (%s) \"%s\"\n", ent.pKey, ent.pValue);
      } else if (ent.str_type == SNSTRING_CURLY) {
        printf("Meta string (%s) {%s}\n", ent.pKey, ent.pValue);
      } else {
        /* Unknown string type */
        abort();
      }
      
    } else if (ent.status == SNENTITY_NUMERIC) {
      printf("Numeric %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_VARIABLE) {
      printf("Declare variable %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_CONSTANT) {
      printf("Declare constant %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_ASSIGN) {
      printf("Assign variable %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_GET) {
      printf("Get value %s\n", ent.pKey);
      
    } else if (ent.status == SNENTITY_BEGIN_GROUP) {
      printf("Begin group\n");
      
    } else if (ent.status == SNENTITY_END_GROUP) {
      printf("End group\n");
      
    } else if (ent.status == SNENTITY_ARRAY) {
      printf("Array %ld\n", ent.count);
      
    } else if (ent.status == SNENTITY_OPERATION) {
      printf("Operation %s\n", ent.pKey);
      
    } else {
      /* Unrecognized entity type */
      abort();
    }
    
    if (ent.status == SNENTITY_EOF) {
      break;
    }
  }
  if (ent.status != 0) {
    fprintf(stderr, "Error %d!\n", retval);
  }
  
  snparser_free(pParser);
  pParser = NULL;
  
  return 0;
}
