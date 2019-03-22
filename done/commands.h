#pragma once

/**
 * @file commands.h
 * @brief Processor commands(/instructions) simulation
 *
 * @author Jean-Cédric Chappelier, Leo Tafti, Paul Juillard
 * @date 2018-19
 */

#include <stdio.h> // for size_t, FILE
#include <stdint.h> // for uint32_t

#include "mem_access.h" // for mem_access_t
#include "addr.h" // for virt_addr_t, word_t
 
#define MAX_COMMANDS 100
 
typedef enum {
	READ,
	WRITE
} command_word_t;

typedef struct {
	command_word_t order;
	mem_access_t type;
	size_t data_size; //always in bytes
	word_t write_data;
	virt_addr_t vaddr;
} command_t;

typedef struct{
	command_t listing[MAX_COMMANDS];
	size_t nb_lines;
	size_t allocated;
} program_t;


/**
 * @brief A useful macro to loop over all program lines.
 * X is the name of the variable to be used for the line;
 * and P is the program to be looped over.
 * X will be of type `const command_t*`
 * and P has to be of type `program_t*`.
 *
 * Example usage:
 *    for_all_lines(line, program) { do_something_with(line); }
 *
 */
#define for_all_lines(X, P) const command_t* end_pgm_ = (P)->listing + (P)->nb_lines; \
    for(const command_t* X = (P)->listing; X < end_pgm_; ++X)


/**
 * @brief "Constructor" for program_t: initialize a program.
 * @param program (modified) the program to be initialized.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_init(program_t* program);

/**
 * @brief add a command (line) to a program. Reallocate memory if necessary.
 * @param program (modified) the program where to add to.
 * @param command the command to be added.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_add_command(program_t* program, const command_t* command);

/**
 * @brief Tool function to down-reallocate memory to the minimal required size. Typically used once a program will no longer be extended.
 * @param program (modified) the program to be rescaled.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_shrink(program_t* program);

/**
 * @brief Print the content of a program to a stream.
 * @param output the stream to print to.
 * @param program the program to be printed.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_print(FILE* output, const program_t* program);


//TODO : Refactor. Helper (auxilary) functions should static AND ONLY DEFINED IN commands.C
//This applies to the "print" auxilary functions, as well as the various "parse" and others

/**
 * @brief program_print helper function. Prints the order (READ or WRITE) of a command
 * @param o the output stream
 * @param c the command to print
 */
static void print_order( FILE* const o , command_t const * c );

/**
 * @brief program_print helper function. Prints the type of a command (I or D) and the size of the data if needed.
 * @param o the output stream
 * @param c the command to print
 */
static void print_type_size( FILE* const o , command_t const * c );

/**
 * @brief program_print helper function. Prints the data (byte or word or nothing) of a command.
 * @param o the output stream
 * @param c the command to print
 */
static void print_data( FILE* const o , command_t const * c );

/**
 * @brief program_print helper function. Prints the hex target address of a command.
 * @param o the output stream
 * @param c the command to print
 */
static void print_addr( FILE* const o , command_t const * c );

/**
 * @brief Read a program (list of commands) from a file.
 * @param filename the name of the file to read from.
 * @param program the program to be filled from file.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_read(const char* filename, program_t* program);

/**
 * @brief Read one line of command (a string) from given input file
 * @param input the input file (list of commands)
 * @param str (modified) the command read
 * @param str_len length of str char array
 * @return ERR_NONE if ok, appropriate error code otherwise (ERR_IO)
 */
static int read_command_line(FILE* input, char* str, size_t str_len);

/**
 * @brief Strips one word from str, starting from index (incl.). Drops (leading) spaces.
 * @param str string to read one word from
 * @param read (modified) word read, guarantees '\0' as last char
 * @param index (modified) start index, modified to index of char directly following word read
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int next_word(char* str, char* read, size_t read_len, unsigned int* index);

/**
 * @brief program_read helper function. Parses order to command entry.
 * @param c (modified) the command to fill
 * @param word start of string to parse
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int parse_order(command_t * c, char* word);
 
 /**
 * @brief program_read helper function. Parses type and potential data size to command entry.
 * @param c (modified) the command to fill
 * @param word start of string to parse
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int parse_type(command_t * c, char* word);
 
 /**
 * @brief program_read helper function. Parses write data to command entry.
 * @param c (modified) the command to fill
 * @param word start of string to parse
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int parse_data(command_t * c, char* word);
 
 /**
 * @brief program_read helper function. Parses address to command entry.
 * @param c (modified) the command to fill
 * @param word start of string to parse
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int parse_address(command_t * c, char* word);
