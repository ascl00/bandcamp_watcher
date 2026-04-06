//
//  utils.c
//  bandcamp_watcher
//
//  String and file utilities
//

#include "utils.h"
#include <ctype.h>
#include <string.h>

/* Similar to strncasecmp however wont fail a match if puncuation characters mismatch
 * NOTE: Should this skip/compress whitespace?
 */
int name_compare(const char* s1, const char* s2, size_t len)
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    size_t n = 0;
    int result = 0;

    if (p1 == p2 || *p1 == '\0' || *p2 == '\0')
        return 0;

    do
    {
        char c1 = tolower(*p1);
        char c2 = tolower(*p2);
        
        if(isalnum(c1) && isalnum(c2))
        {
            result = c1 - c2;
            if(result != 0)
                break;
        }
    } while (*p1++ != '\0' && *p2++ != '\0' && n++ <= len);
    
    return result;
}

int is_matching_extension(const char *filename, const char *ext)
{
    const char *p = filename + strlen(filename);
    
    p-=strlen(ext); // wind p back to where ext should start, ie ".ext"
    if(p<filename) // wound back before start of string
        return 0;
    
    return strcasecmp(p, ext);
}

/* Trim whitespace from string (modifies in place, returns pointer to trimmed string) */
char *trim(char *str)
{
    if (!str) return NULL;
    
    // Trim leading
    while (isspace((unsigned char)*str)) str++;
    
    if (*str == '\0') return str;
    
    // Trim trailing
    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    
    return str;
}

/* Parse boolean string (case-insensitive) */
int parse_bool(const char *str)
{
    if (!str) return 0;
    
    if (strcasecmp(str, "true") == 0 || 
        strcasecmp(str, "yes") == 0 ||
        strcasecmp(str, "1") == 0 ||
        strcasecmp(str, "on") == 0) {
        return 1;
    }
    
    if (strcasecmp(str, "false") == 0 || 
        strcasecmp(str, "no") == 0 ||
        strcasecmp(str, "0") == 0 ||
        strcasecmp(str, "off") == 0) {
        return 0;
    }
    
    return 0;  // Default to false for unrecognized
}
