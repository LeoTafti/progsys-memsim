/**
 * @file commands.c
 * @brief Processor commands(/instructions) simulation
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <stdio.h>
#include <stdlib.h> 	// strtoul(), memory alloc.
#include <inttypes.h> 	// PRIX macro
#include <string.h>		// strlen()
#include <ctype.h> 		// isspace()

#include "commands.h"
#include "addr_mng.h"
#include "mem_access.h"
#include "addr.h"
#include "error.h"

#define BYTE_MAX_VALUE 0xFF
#define WORD_MAX_VALUE 0xFFFFFFFF // TODO is unsigned?

#define INIT_COMMANDS_NB 10

//TODO : Make every argument we can const ?

//TODO : Remove all the (debug) printf


int program_init(program_t* program) {
	M_REQUIRE_NON_NULL(program);
	
	program->listing = calloc(INIT_COMMANDS_NB, sizeof(command_t));
	M_EXIT_IF_NULL(program->listing, 10*sizeof(command_t));
	
	program->nb_lines = 0;
	program->allocated = sizeof(program->listing);
	
	return ERR_NONE;
}

/**
 * @brief "Destructor" for program_t: free its content.
 * @param program the program to be filled from file.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_free(program_t* program){
	return 0;
}

static int program_enlarge(program_t* program);
int program_add_command(program_t* program, const command_t* command) {
	M_REQUIRE_NON_NULL(program);   
	M_REQUIRE_NON_NULL(command);
	
	// Write Instr.
	M_REQUIRE( !(command->order == WRITE && command->type == INSTRUCTION), ERR_BAD_PARAMETER, "%s",  "Illegal command: cannot write instructions" );

	// TODO : Léo – We should probably ask TA's about that (ie. what to do of unused fields).
	// R and non-empty write data
	// here we consider that the write_data is irrelevant and should not raise an error is set
	// M_REQUIRE( !(command->order == READ, ERR_BAD_PARAMETER, "illegal command: read should not have write data", void);
	
	if(command->order == WRITE){
		// data size is either word or byte
		M_REQUIRE(command->data_size == sizeof(word_t) || command->data_size == sizeof(byte_t), ERR_SIZE, "%s", "illegal command: data size is neither word nor byte");
		
		// data_size and write_data value are coherent
		int write_word_coherent = (command->data_size == sizeof(word_t)) && (command->write_data <= WORD_MAX_VALUE);
		int write_byte_coherent = (command->data_size == sizeof(byte_t)) && (command->write_data <= BYTE_MAX_VALUE);
		M_REQUIRE( write_word_coherent || write_byte_coherent, ERR_SIZE, "%s", "illegal command: write data is too big compared to declared datasize");
	}
	
	if(command->type == INSTRUCTION){
		M_REQUIRE( command->data_size == sizeof(word_t), ERR_BAD_PARAMETER, "%s", "Illegal command: Instruction data size should always be sizeof(word_t)");
	}
	
	// Address offset should be a multiple of data size
	if(command->data_size == sizeof(word_t)){
		//Should be word aligned
		M_REQUIRE((command->vaddr.page_offset & 0x3) == 0, ERR_ADDR, "%s", "Illegal command : address should be word aligned when dealing with words");
	}
	// Note : if data is in bytes, every address has valid offset
	
	
	while(program->nb_lines >= program->allocated){
		M_EXIT_IF_ERR(program_enlarge(program), "Error trying to reallocate more memory");
	}
	
	program->listing[program->nb_lines] = *command;
	(program->nb_lines)++;
	return ERR_NONE;
}

/**
 * @brief Reallocates twice as much memory to store commands in program
 * @param program (modified) the program which needs more memory
 * @return ERR_NONE if successful, ERR_MEM otherwise
 */
static int program_enlarge(program_t* program){
	program_t enlarged = *program;
	enlarged.allocated *= 2;
	if ((enlarged.allocated > SIZE_MAX / sizeof(command_t) ||
		(enlarged.listing = realloc(enlarged.listing, enlarged.allocated * sizeof(command_t))) == NULL)) {
		M_EXIT(ERR_MEM, "Error trying to reallocate to %zu bytes (possible overflow)", 2 * enlarged.allocated * sizeof(command_t));
	}
	*program = enlarged;
	
	return ERR_NONE;
}

int program_shrink(program_t* program) {
	M_REQUIRE_NON_NULL(program);   
	
	program_t shrunk = *program;
	//NOTE : Realocating DOWN can not possibly cause an overflow
	if(program->nb_lines <= 10){
		M_EXIT_IF_NULL(shrunk.listing = realloc(shrunk.listing, INIT_COMMANDS_NB * sizeof(command_t)), INIT_COMMANDS_NB * sizeof(command_t));
		shrunk.allocated = INIT_COMMANDS_NB;
	}else{
		M_EXIT_IF_NULL(shrunk.listing = realloc(shrunk.listing, shrunk.nb_lines * sizeof(command_t)), shrunk.nb_lines * sizeof(command_t));
		shrunk.allocated = shrunk.nb_lines;
	}
	
	*program = shrunk;
	
	return ERR_NONE;
}

// program_print helper functions' prototypes
static void print_order(FILE* o, command_t const * c);
static void print_type_size(FILE* const o, command_t const * c);
static void print_data(FILE* const o, command_t const * c);
static void print_addr(FILE* const o, command_t const * c);

int program_print(FILE* output, const program_t* program) {
	M_REQUIRE_NON_NULL(output);
	M_REQUIRE_NON_NULL(program);

	command_t c;
	
	for(int i = 0; i < program->nb_lines; i++) {
		c = program->listing[i];
		
		print_order(output, &c);
		fflush(output);
		
		print_type_size(output, &c);
		fflush(output);
		
		print_data(output, &c);
		fflush(output);
		
		print_addr(output, &c);
		fflush(output);
		
		fprintf(output, "\n");
		fflush(output);
	}
	
	return ERR_NONE;
}

static void print_order( FILE* const o , command_t const * c ) {
	 if(c->order == READ) fprintf(o, "R ");
	 else fprintf(o, "W ");
}
static void print_type_size( FILE* const o , command_t const * c ){ 
	if(c->type == INSTRUCTION) fprintf(o, "I  "); // I and two white spaces
	else { 
		if (c->data_size == sizeof(word_t)) fprintf(o, "DW ");
		else fprintf(o, "DB "); 
	}
}
static void print_data( FILE* const o , command_t const * c ) {
	if(c->order == WRITE) fprintf(o, "0x%08" PRIX32 " ", c->write_data);
	else fprintf(o, "           "); // 11 whites spaces
}
static void print_addr( FILE* const o , command_t const * c ) {
	fprintf(o, "@0x%016" PRIX64, virt_addr_t_to_uint64_t( &(c->vaddr)) );
}



// helper function prototypes
static int read_command_line(FILE* input, char* str, size_t str_len);
static int next_word(char* str, char* read, size_t read_len, unsigned int* index);
static int parse_order(command_t * c, char* word);
static int parse_type_and_size(command_t * c, char* word);
static int parse_data(command_t * c, char* word);
static int parse_address(command_t * c, char* word);
#define MAX_COMMAND_LENGTH 36
#define MAX_COMMAND_WORD_LENGTH 19 // ADDRESS_CHARS

int program_read(const char* filename, program_t* program){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(program);
	
	FILE* input = fopen(filename, "r");
	M_REQUIRE_NON_NULL_CUSTOM_ERR(input, ERR_IO);
	
	char command_str[MAX_COMMAND_LENGTH+1];
	
	program_init(program);
	
	/* NOTE: the underlying helper-functions only parse the input. 
	 * It doesn't check in any way that the input makes sense
	 * Only that it has a somewhat "legal" structure (to be parseable)
	 * Validity checks are performed in program_add_command
	 */

	while((read_command_line(input, command_str, MAX_COMMAND_LENGTH+1) != 0)
		&& !feof(input) && !ferror(input)){
		
		//Make a command_t of the line read
		command_t c;
		(void)memset(&c, 0, sizeof(c)); 
		
		unsigned int index = 0;
		char word_read[MAX_COMMAND_WORD_LENGTH+1]; //+1 accounts for '\0' in last position 
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_order(&c, word_read), "Error trying to parse order");
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_type_and_size(&c, word_read), "Error trying to parse type and size");
		
		if(c.order == WRITE){
			M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
			M_EXIT_IF_ERR(parse_data(&c, word_read), "Error trying to parse data");
		}
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_address(&c, word_read), "Error trying to parse address");
		
		M_EXIT_IF_ERR(program_add_command(program, &c), "Error adding command to program");
	}
	
	fclose(input);
	
	return ERR_NONE;
}

static int read_command_line(FILE* input, char* str, size_t str_len){
	fgets(str, str_len, input);
	
	int len_read = strlen(str) - 1;
	
	M_EXIT_IF(len_read < 1, ERR_IO, "%s", "Found line with only a '\n' in program input file");
	
	if(len_read >= 0 && str[len_read] == '\n'){  //strip the '\n' off
		str[len_read] = '\0';
	}
	
	return len_read;
}

/**
 * @brief Strips one word from str, starting from index (incl.) Drops (leading) spaces.
 * @param str string to read one word from
 * @param read (modified) word read
 * @param index (modified) start index, modified to index of char directly following word read
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int next_word(char* str, char* read, size_t read_len, unsigned int* index){
	//TODO : test this function on empty string, or just one char string, or just one "space" char string, etc
	int str_len = strlen(str);
	
	//"Drop" leading spaces, if any
	while(*index < str_len && isspace(str[*index])){
		(*index)++;
	}
	
	// reads only in cases it needs to. Empty fields are not read -> error
	M_EXIT_IF(*index == str_len, ERR_BAD_PARAMETER, "%s", "Reached end of string, but expected more (presumably wrong command format)");
	
	// read until next space
	size_t read_nb = 0;
	char nextChar;
	do{
		M_EXIT_IF(read_nb == read_len-1, ERR_BAD_PARAMETER, "%s", "Given 'read' char array is too small to hold next word of instruction");
		
		nextChar = str[*index];
		read[read_nb] = nextChar;
		read_nb++;
		(*index)++;
	
	} while(*index < str_len && !isspace(str[*index]));
	read[read_nb] = '\0';
	
	return ERR_NONE;
}

#define ORDER_CHARS 1 	// I or W
#define TYPE_CHARS 2 	// I or DB or DW
#define DATA_CHARS 10 	// 0x + 8 hex
#define ADDRESS_CHARS 19// @0x + 16 hex
static int parse_order(command_t * c, char* word) {
	size_t word_len = strlen(word);
	M_EXIT_IF(word_len != ORDER_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command order should only be 1 character");
	
	if(word[0] == 'R') {
		c->order = READ;
	}
	else if (word[0] == 'W') {
		c->order = WRITE;
	}
	else {
		M_EXIT(ERR_BAD_PARAMETER, "%s", "Bad instruction format: first command word is not a valid order entry.");
	}
	
	return ERR_NONE;
}
static int parse_type_and_size(command_t * c, char* word) {	
	size_t word_len = strlen(word);
	M_EXIT_IF(word_len > TYPE_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command type and size should only be at most 2 characters");
	
	if (word_len == 1 && word[0] == 'I') {
		c->type = INSTRUCTION;
		c->data_size = sizeof(word_t);
	}
	else if (word_len == 2 && word[0] == 'D') {
		c->type = DATA; 
		
		if(word[1] == 'B') {
			c->data_size = sizeof(byte_t);
		}
		else if(word[1] == 'W') {
			c->data_size = sizeof(word_t);
		} 
		else {
			M_EXIT(ERR_BAD_PARAMETER, "%s %c", "Bad instruction format: command type D should be followed by either B or W but was", word[1]);	
		}
	}
	else {
		M_EXIT(ERR_BAD_PARAMETER, "%s %c", "Bad instruction format: command type should be I, DB or DW but was:", word[0]);	
	}	
	
	return ERR_NONE;
}

/*unsigned long strtoul(const char *restrict str, char **restrict endptr, int base)
 * converts a string to an unsigned long
 * man strtoul for more information
 * */
static int parse_data(command_t * c, char* word) {
	size_t word_len = strlen(word);
	M_REQUIRE(word_len <= DATA_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command data string should be at most 10 characters (\"0x\" + at most 8 HEX digits)");
	M_REQUIRE(word[0] == '0' && word[1] == 'x', ERR_BAD_PARAMETER, "%s", "Bad instruction format : data should start with prefix '0x'");
	
	
	//TODO : strtoul is nice, using this we can check if the data is valid (ie each char is an HEX one) (see documentation)
	char* ptr;
	//TODO : I'm not 100% sure w survives upon exiting the function... See W6 Prog. C lectures -> 
	word_t w = strtoul(word, &ptr , 0); 
	//After having read the data word, we should find a '\0'. Otherwise means we stopped prematurely.
	M_REQUIRE(*ptr == '\0', ERR_BAD_PARAMETER, "%s", "Bad instruction format : data should only contain HEX digits");
	
	c->write_data = w;

	return ERR_NONE;
}
static int parse_address(command_t * c, char* word) {
	size_t word_len = strlen(word);
	M_REQUIRE(word_len == ADDRESS_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command address should take 19 chars (\"@0x\" + 16 HEX digits)");
	M_REQUIRE(word[0] == '@' && word[1] == '0' && word[2]== 'x', ERR_BAD_PARAMETER, "%s", "Bad instruction format : address should start with prefix '@0x'");
	
	char* ptr;
	//drops leading '@'
	uint64_t a_int = strtoul(&word[1], &ptr, 16); // hex base
	M_REQUIRE(*ptr == '\0', ERR_ADDR, "%s %c", "Bad instruction format : address should end here but last char was ", *ptr);
	
	virt_addr_t vaddr;
	init_virt_addr64(&vaddr, a_int);
	c->vaddr = vaddr;
		
	return ERR_NONE;
}
