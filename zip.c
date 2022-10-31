#define _POSIX_C_SOURCE 200809L // required for strdup() on cslab

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/* Use 16-bit code words */
#define NUM_CODES 65536

/* allocate space for and return a new string s+t */
char *strappend_str(char *s, char *t);

/* allocate space for and return a new string s+c */
char *strappend_char(char *s, char c);

/* look for string s in the dictionary
 * return the code if found
 * return NUM_CODES if not found 
 */
unsigned int find_encoding(char *dictionary[], char *s);

/* write the code for string s to file */
void write_code(int fd, char *dictionary[], char *s);

/* compress in_file_name to out_file_name */
void compress(char *in_file_name, char *out_file_name);

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: zip file\n");
        exit(1);
    }

    char *in_file_name = argv[1];
    char *out_file_name = strappend_str(in_file_name, ".zip");

    compress(in_file_name, out_file_name);

    // have to free the memory for out_file_name since strappend_str malloc()'ed it
    free(out_file_name);

    return 0;
}

/* allocate space for and return a new string s+t */
char *strappend_str(char *s, char *t)
{
    if (s == NULL || t == NULL)
    {
        return NULL;
    }

    // reminder: strlen() doesn't include the \0 in the length
    int new_size = strlen(s) + strlen(t) + 1;
    char *result = (char *)malloc(new_size*sizeof(char));
    strcpy(result, s);
    strcat(result, t);

    return result;
}

/* allocate space for and return a new string s+c */
char *strappend_char(char *s, char c)
{
    if (s == NULL)
    {
        return NULL;
    }

    // reminder: strlen() doesn't include the \0 in the length
    int new_size = strlen(s) + 2;
    char *result = (char *)malloc(new_size*sizeof(char));
    strcpy(result, s);
    result[new_size-2] = c;
    result[new_size-1] = '\0';

    return result;
}

/* look for string s in the dictionary
 * return the code if found
 * return NUM_CODES if not found 
 */
unsigned int find_encoding(char *dictionary[], char *s)
{
    if (dictionary == NULL || s == NULL)
    {
        return NUM_CODES;
    }

    for (unsigned int i=0; i<NUM_CODES; ++i)
    {
        /* code words are added in order, so if we get to a NULL value 
         * we can stop searching */
        if (dictionary[i] == NULL)
        {
            break;
        }

        if (strcmp(dictionary[i], s) == 0)
        {
            return i;
        }
    }
    return NUM_CODES;
}

/* write the code for string s to file */
void write_code(int fd, char *dictionary[], char *s)
{
    if (dictionary == NULL || s == NULL)
    {
        return;
    }

    unsigned int code = find_encoding(dictionary, s);
    // should never call write_code() unless s is in the dictionary
    if (code == NUM_CODES)
    {
        printf("Algorithm error!\n");
        exit(1);
    }

    // cast the code to an unsigned short to only use 16 bits per code word in the output file
    unsigned short actual_code = (unsigned short)code;
    if (write(fd, &actual_code, sizeof(unsigned short)) != sizeof(unsigned short))
    {
        perror("write");
        exit(1);
    }
}

/* compress in_file_name to out_file_name */
void compress(char *in_file_name, char *out_file_name)
{
    // check for bad args
    if (in_file_name == NULL || out_file_name == NULL)
    {
        printf("Programming error!\n");
        exit(1);
    }

    // open both files
    int in_fd = open(in_file_name, O_RDONLY);
    if (in_fd < 0)
    {
        perror(in_file_name);
        exit(1);
    }

    int out_fd = open(out_file_name, O_WRONLY|O_CREAT|O_TRUNC, 0644);
    if (out_fd < 0)
    {
        perror(out_file_name);
        exit(1);
    }

    // use an array as the dictionary
    char *dictionary[NUM_CODES];

    
    // initialize the first 256 codes mapped to their ASCII equivalent as a string
    unsigned int next_code;
    for (next_code=0; next_code<256; ++next_code)
    {
        dictionary[next_code] = strappend_char("\0", (char)next_code);
    }

    // initialize the rest of the dictionary to be NULLs
    for (unsigned int i=256; i<NUM_CODES; ++i)
    {
        dictionary[i] = NULL;
    }

    char *cur_str;
    char cur_char;

    // CurrentString = first input character
    int read_return = read(in_fd, &cur_char, sizeof(char));
    if (read_return < 0)
    {
        perror("read");
        exit(1);
    }
    if (read_return == 0)
    {
        // if the file is empty, we're done
        return;
    }
    cur_str = strappend_char("\0", cur_char);

    // CurrentChar = next input character
    read_return = read(in_fd, &cur_char, sizeof(char));
    if (read_return < 0)
    {
        perror("read");
        exit(1);
    }

    // While there are more input characters
    while (read_return != 0)
    {
        // hold CurrentString+CurrentChar in a temp variable
        char *tmp = strappend_char(cur_str, cur_char);

        // If CurrentString+CurrentChar is in Dictionary
        unsigned int code = find_encoding(dictionary, tmp);
        if (code != NUM_CODES)
        {
            // CurrentString = CurrentString+CurrentChar
            char *hold = strappend_char(cur_str, cur_char);

            // have to free the memory for cur_str before we reassign it
            free(cur_str);
            cur_str = hold;
        }
        else
        {
            // only add tmp to dictionary if there are codes left 
            if (next_code < NUM_CODES)
            {
                // Add CurrentString+CurrentChar to Dictionary
                dictionary[next_code] = strdup(tmp);
                ++next_code;
            }

            // CodeWord = CurrentString's code in Dictionary
            // Output CodeWord
            write_code(out_fd, dictionary, cur_str);
            free(cur_str);

            // CurrentString = CurrentChar
            cur_str = strappend_char("\0", cur_char);
        }

        // don't forget to free tmp
        free(tmp);

        // read the next char
        read_return = read(in_fd, &cur_char, sizeof(char));
        if (read_return < 0)
        {
            perror("read");
            exit(1);
        }
    }

    // CodeWord = CurrentString's code in Dictionary
    // Output CodeWord
    write_code(out_fd, dictionary, cur_str);

    // free the last memory allocated for cur_str
    free(cur_str);

    // close the files
    if (close(in_fd) < 0)
    {
        perror("close");
    }
    if (close(out_fd) < 0)
    {
        perror("close");
    }

    // free all memory in the dictionary
    for (unsigned int i=0; i<NUM_CODES; ++i)
    {
        if (dictionary[i] == NULL)
        {
            break;
        }
        free(dictionary[i]);
    }

    return;
}
