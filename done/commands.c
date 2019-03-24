/**
 * @file commands.c
 * @brief Processor commands(/instructions) simulation
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <stdio.h>
#include <stdlib.h> 	// strtol()
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

//TODO : Make every argument we can const ?


int program_init(program_t* program) {
	M_REQUIRE_NON_NULL(program);

	(void)memset(program->listing, 0, sizeof(program->listing));
	program->nb_lines = 0;
	program->allocated = sizeof(program->listing);
	
	return ERR_NONE;
}


int program_add_command(program_t* program, const command_t* command) {
	M_REQUIRE_NON_NULL(program);   
	M_REQUIRE_NON_NULL(command);
	
	// memory restrictions
	M_REQUIRE(program->nb_lines <= MAX_COMMANDS, ERR_MEM, "there is too many commands! max is %d", MAX_COMMANDS);
	
	// check validity of command
	// TODO brainstorming!! did I forget anything?
	
	// TODO none of these are pointers
	// no command entry is null
	/* M_REQUIRE_NON_NULL(command->order);
	M_REQUIRE_NON_NULL(command->type);
	M_REQUIRE_NON_NULL(command->data_size);
	M_REQUIRE_NON_NULL(command->write_data);
	M_REQUIRE_NON_NULL(command->vaddr);	
	*/
	
	// not W and I
	M_REQUIRE( !(command->order == WRITE && command->type == INSTRUCTION), ERR_BAD_PARAMETER, "%s",  "illegal command: cannot write into instructions" );

	// R and empty write data?
	// here we consider that the write_data is irrelevant and should not raise an error is set
	// M_REQUIRE( !(command->order == READ, ERR_BAD_PARAMETER, "illegal command: read should not have write data", void);
	if(command->order == WRITE){
	// data size is either word or byte
	M_REQUIRE(command->data_size == sizeof(word_t) || command->data_size == sizeof(byte_t), ERR_SIZE, "%s", "illegal command: data size is neither word or byte");
	
	// data_size and write_data value are coherent
	int write_word_coherent = (command->data_size == sizeof(word_t)) && (command->write_data <= WORD_MAX_VALUE);
	int write_byte_coherent = (command->data_size == sizeof(byte_t)) && (command->write_data <= BYTE_MAX_VALUE);
	M_REQUIRE( write_word_coherent || write_byte_coherent, ERR_SIZE, "%s", "illegal command: write data is too big compared to declared datasize");
	}
		
	// addr offset is a multiple of size
	int word_address_case = (command->type == INSTRUCTION || command->data_size == sizeof(word_t) ) && ((command->vaddr.page_offset & 0x11) == 0);
	M_REQUIRE( word_address_case, ERR_ADDR, "%s", "illegal command: address of word is not word-aligned");


	// finally add command
	program->listing[program->nb_lines] = *command;
	
	return ERR_NONE;
}

int program_shrink(program_t* program) {
	M_REQUIRE_NON_NULL(program);   

	// TODO : complete (Week 6)
	
	return ERR_NONE;
}

// helper function prototypes
static void print_order( FILE* const o , command_t const * c );
static void print_type_size( FILE* const o , command_t const * c );
static void print_data( FILE* const o , command_t const * c );
static void print_addr( FILE* const o , command_t const * c );

int program_print(FILE* output, const program_t* program) {
	M_REQUIRE_NON_NULL(output);
	M_REQUIRE_NON_NULL(program);
	
	command_t c;
	printf("foooo\n");

	
	for(int i = 0; i < program->nb_lines; i++) {
		c = program->listing[i];
		
		//TODO : remove
		//old code just in case
		/*uint64_t addr = virt_addr_t_to_uint64_t(&c.vaddr);
		
		if(c.order == READ) {
			if (c.type == INSTRUCTION) { fprintf(output, "R I @0x%016" PRIX64, addr);}
			else { if (c.data_size == sizeof(word_t)) { fprintf(output, "R DW @0x%016" PRIX64, addr);}
				   else { fprintf(output, "R DB @0x%016" PRIX64, addr);}
			}
		}
		else { 
			if (c.data_size == sizeof(word_t)) { fprintf(output, "R DW 0x%08" PRIX32 " @0x%016" PRIX64, c.write_data, addr);}
		    else { fprintf(output, "R DB 0x%02" PRIX32 " @0x%016" PRIX64, c.write_data, addr);}	
		}*/
		
		print_order(output, &c);
		print_type_size(output, &c);
		print_data(output, &c);
		print_addr(output, &c);
		
		fprintf(output, "\n");	
		printf("spamspam\n");
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
static int parse_type(command_t * c, char* word);
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
	 * checking is done in program_add_command
	 */

	do{
		//Read one line of command
		M_EXIT_IF_ERR(read_command_line(input, command_str, MAX_COMMAND_LENGTH+1), "Error reading one line of command from input file");
		
		//Make it a command_t
		command_t c;
		(void)memset(&c, 0, sizeof(c));
		
		
		unsigned int index = 0;
		char word_read[1+MAX_COMMAND_WORD_LENGTH]; //+1 accounts for '\0' in last position 
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, 1+MAX_COMMAND_WORD_LENGTH, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_order(&c, word_read), "Error trying to parse order");
		printf("read order\n");
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, 1+MAX_COMMAND_WORD_LENGTH, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_type(&c, word_read), "Error trying to parse type");
		printf("read type\n");

		
		/* TODO : paul - remove as this is already done in parse_type
		if(c.type == DATA){
			M_EXIT_IF_ERR(parse_size(&c, word_read), "Error trying to parse size");
		}
		*/
		
		if(c.order == WRITE){
			M_EXIT_IF_ERR(next_word(command_str, word_read, 1+MAX_COMMAND_WORD_LENGTH, &index), "Error trying to parse instruction");
			M_EXIT_IF_ERR(parse_data(&c, word_read), "Error trying to parse data");
			printf("read data\n");
		}
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, 1+MAX_COMMAND_WORD_LENGTH, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_address(&c, word_read), "Error trying to parse address");
		printf("read address\n");

		// TODO add_command already exits and throws its own error+message, do we need imbricated checks?
		M_EXIT_IF_ERR(program_add_command(program, &c), "Error adding command to program");
		
	} while(!feof(input) && !ferror(input));
	
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
	
	return ERR_NONE;
}


/**
 * @brief Strips one word from str, starting from index (incl.) Drops (leading) spaces.
 * @param str string to read one word from
 * @param read (modified) word read
 * @param index (modified) start index, modified to index of char directly following word read
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
static int next_word(char* str, char* read, size_t read_len, unsigned int* index){
	//TODO : TEST THIS FUNCTION on empty string, or just one char string, or just one "space" char string, etc
	// if this works, the above probably works too (and inversely !)
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
	
	} while(*index < str_len && isspace(nextChar));
	
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
static int parse_type(command_t * c, char* word) {	
	// FIXME : paul - given word only contains 'D' and not size. 
	// cant see where this is comming from but you seemed to have coded it in order to return two distinct words (type and size)
	// cf todo in read_program
	size_t word_len = strlen(word);
	M_EXIT_IF(word_len > TYPE_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command type and size should only be at most 2 characters");
	
	if (word_len == 1 && word[0] == 'I') {
		c->type = INSTRUCTION;
		// TODO set data_size to 0 or leave empty/random?
		// seems to be set to 0 by itself from testing (dangerous in C?)
	}
	else if (word[0] == 'D') {
		c->type = DATA; 
		
		if(word[1] == 'B') {
			c->data_size = sizeof(byte_t);
		}
		else if(word[1] == 'W') {
			c->data_size = sizeof(word_t);
		} 
		else {
			printf(" the word is %c%c-\n", word[0], word[1]);
			M_EXIT(ERR_BAD_PARAMETER, "%s %c", "Bad instruction format: command type D should be followed by either B or W but was", word[1]);	
		}
	}
	else {
		M_EXIT(ERR_BAD_PARAMETER, "%s %c", "Bad instruction format: command type should be I or D but was", word[0]);	
	}	
	
	return ERR_NONE;
}

/** unsigned long int strtoul(const char *str, char **endptr, int base);
* may lift its own errors
* discards first unrecognized chars
* deduces radix based on '0x'
* cf http://pubs.opengroup.org/onlinepubs/7908799/xsh/strtol.html
*/

//TODO : Léo – WTF is the above comment ? I'm quite sure we don't need that...
		
static int parse_data(command_t * c, char* word) {
	size_t word_len = strlen(word);
	M_EXIT_IF(word_len > DATA_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command data should only be at most 10 characters (0x...)");
	// stroul probably returns its own error if this is not the case
	M_REQUIRE(word[0] == '0' && word[1] == 'x', ERR_BAD_PARAMETER, "%s", "Bad instruction format : data should start with prefix '0x'");
	
	// we don't care about 'final string'
	// TODO : Léo – I don't understand what this is supposed to mean.. ?
	word_t w = strtoul( word, NULL , 0);
	
	c->write_data = w;

	return ERR_NONE;
}
static int parse_address(command_t * c, char* word) {
	size_t word_len = strlen(word);
	//M_EXIT_IF(word_len < ADDRESS_CHARS, ERR_BAD_PARAMETER, "%s", "Bad instruction format : command address should only be 1 character");
	// stroul probably returns its own error if this is not the case
	//M_REQUIRE(word[0] == '@' && word[1] == '0' && word[2]== 'x', ERR_BAD_PARAMETER, "%s", "Bad instruction format : address should start with prefix '@0x'");
	
	// we don't care about 'final string'
	uint64_t a_int = strtoul( word, NULL, 0);
	
	virt_addr_t vaddr;
	init_virt_addr64(&vaddr, a_int);

	c->vaddr = vaddr;
	
	return ERR_NONE;
	

}
	


/*TODO : remove
I leave what's below only to serve as inspiration to write the parse_xx functions
and as a reminder to always catch unexpected values. parse_xx functions should not be nested though,
so should be a lot simpler

int extract_read_command(command_t* c, char* command_str){
	c->order = READ;
	unsigned int index = 1; //put it right after the first char, as reading it with next_word would have done
	char word_read[1+MAX_COMMAND_WORD_LENGTH]; //+1 accounts for '\0' in last position
	
	M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH, &index), "Error trying to parse read instruction");
	
	size_t word_len = strlen(word_read);

	
	if(type_char == 'I'){
		c->type = INSTRUCTION;
	}else if(type_char == 'D'){
		
		M_EXIT_IF(word_len < 2, ERR_BAD_PARAMETER, "Data size not specified");
		
		char size_char = word_read[1];
		c->type = DATA;
		
		if(size_char == 'B'){
			c->size = 1;
		}else if(size_char == 'W'){
			c->size = 4;
		}else{
			M_EXIT_ERR(ERR_BAD_PARAMETER, "size character was neither 'B' (Byte) nor 'W' (Word)");
		}
	}else{
		M_EXIT_ERR(ERR_BAD_PARAMETER, "type character was neither 'I' (Instruction) nor 'D' (Data)");
	}
	
	return ERR_NONE;
}
*/
