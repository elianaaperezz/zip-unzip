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

/* allocate space for and return a new string s+c */
char *strappend_char(char *s, char c);

/* read the next code from fd
 * return NUM_CODES on EOF
 * return the code read otherwise
 */
unsigned int read_code(int fd);

/* uncompress in_file_name to out_file_name */
void uncompress(char *in_file_name, char *out_file_name);

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        printf("Usage: unzip file\n");
        exit(1);
    }

    char *in_file_name = argv[1];
    char *out_file_name = strdup(in_file_name);
    out_file_name[strlen(in_file_name)-4] = '\0';

    uncompress(in_file_name, out_file_name);

    // have to free the memory for out_file_name as strdup() malloc()'ed it
    free(out_file_name);

    return 0;
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

/* read the next code from fd
 * return NUM_CODES on EOF
 * return the code read otherwise
 */
unsigned int read_code(int fd)
{
    // code words are 16-bit unsigned shorts in the file
    unsigned short actual_code;
    int read_return = read(fd, &actual_code, sizeof(unsigned short));

    // if we hit EOF, return NUM_CODES
    if (read_return == 0)
    {
        return NUM_CODES;
    }

    // if we didn't read 2 bytes, error out 
    if (read_return != sizeof(unsigned short))
    {
        perror("read");
        exit(1);
    }
    return (unsigned int)actual_code;
}

/* uncompress in_file_name to out_file_name */
void uncompress(char *in_file_name, char *out_file_name)
{
    // check for NULL args
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

    //  use an array as the dictionary
    char *dictionary[NUM_CODES];

    // initialize the first 256 codes mapped to their ASCII equivalent as a string
    unsigned int next_code;
    for (next_code=0; next_code<256; next_code++)
    {
        dictionary[next_code] = strappend_char("\0", (char)next_code);
    }

    // initialize the rest of the dictionary to be NULLs
    for (unsigned int i=256; i<NUM_CODES; ++i)
    {
        dictionary[i] = NULL;
    }

    // CurrentCode = first input code word
    unsigned int cur_code = read_code(in_fd);
    if (cur_code == NUM_CODES)
    {
        // if we hit EOF then we're done
        return;
    }

    // CurrentChar = CurrentCode's string in Dictionary
    char *cur_str = dictionary[cur_code];
    if (cur_str == NULL)
    {
        printf("Algorithm error!\n");
        exit(1);
    }
    int cur_str_length = strlen(cur_str);
    if (cur_str_length != 1)
    {
        printf("Algorithm error!\n");
        exit(1);
    }

    // Output CurrentChar
    if (write(out_fd, cur_str, cur_str_length) != cur_str_length)
    {
        perror("write");
        exit(1);
    }

    char cur_char = cur_str[0];

    // have to track if cur_str needs to be free()'ed each iteration
    int need_free = 0;

    // While there are more input code words
    // NextCode = next input code word
    unsigned int new_code = read_code(in_fd);
    while (new_code != NUM_CODES)
    {
        // check if cur_str needs to be free()'ed
        if (need_free)
        {
            free(cur_str);
            need_free = 0;
        }

        // If NextCode is NOT in Dictionary
        if (dictionary[new_code] == NULL)
        {
            // CurrentString = CurrentCode's string in Dictionary
            cur_str = dictionary[cur_code];

            // CurrentString = CurrentString+CurrentChar
            cur_str = strappend_char(cur_str, cur_char);

            // track that cur_str was allocated and needs to be free()'ed later
            need_free = 1;
        }
        else
        {
            // CurrentString = NextCode's String in Dictionary
            cur_str = dictionary[new_code];

            // note: don't free cur_str as it is pointing to memory in the dictionary
        }

        // Output CurrentString
        cur_str_length = strlen(cur_str);
        if (write(out_fd, cur_str, cur_str_length) != cur_str_length)
        {
            perror("write");
            exit(1);
        }

        // CurrentChar = first character in CurrentString
        cur_char = cur_str[0];

        // only add another mapping to dictionary if there are codes left
        if(next_code < NUM_CODES)
        {
            // OldString = CurrentCode's string in Dictionary
            // Add OldString+CurrentChar to Dictionary
            dictionary[next_code] = strappend_char(dictionary[cur_code], cur_char);
            ++next_code;
        }

        // CurrentCode = NextCode
        cur_code = new_code;
        new_code = read_code(in_fd);
    }

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
