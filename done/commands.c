/**
 * @file commands.c
 * @brief Processor commands(/instructions) simulation
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <stdio.h>
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

int program_init(program_t* program) {
	M_REQUIRE_NON_NULL(program);

	(void)memset(program->listing, 0, sizeof(program->listing));
	program->nb_lines = 0;
	program->allocated = sizeof(program->listing);
	
	return ERR_NONE;
}

/**
 * @brief add a command (line) to a program. Reallocate memory if necessary.
 * @param program (modified) the program where to add to.
 * @param command the command to be added.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_add_command(program_t* program, const command_t* command) {
	M_REQUIRE_NON_NULL(program);   
	M_REQUIRE_NON_NULL(command);
	
	if(program->nb_lines == MAX_COMMANDS) return ERR_MEM;
	
	// check validity of command
	
	// not W and I
	M_REQUIRE( !(command->order == WRITE && command->type == INSTRUCTION), ERR_BAD_PARAMETER, "illegal command: cannot write into instructions", void );

	
	// R and empty write data
	
	// data_size and write_data value are coherent
	// TODO even if read?
	M_REQUIRE( (command->data_size == sizeof(word_t)) && (command->write_data <= WORD_MAX_VALUE), ERR_SIZE, "illegal command: write data is too big", void);
	M_REQUIRE( (command->data_size == sizeof(byte_t)) && (command->write_data <= BYTE_MAX_VALUE), ERR_SIZE, "illegal command: write data is too big", void);
	
	// W and data size
	
	// addr
	// off is a multiple of size

	
	return ERR_NONE;
}

int program_shrink(program_t* program) {
	M_REQUIRE_NON_NULL(program);   

	// TODO : complete (Week 6)
	
	return ERR_NONE;
}

int program_print(FILE* output, const program_t* program) {
	M_REQUIRE_NON_NULL(output);
	M_REQUIRE_NON_NULL(program);
	
	//TODO beurk mais je sais pas a quel point le formattage est strict...
	// Léo : shouldn't we create a function which takes 1 command and formats it ?
	// this function would then be *a lot* clearer (only print, format commands + print)
	
	command_t c;	// TODO : was there a reason form making this a pointer ?
					//We don't need to modify the value pointed, just print it ?
	for(int i = 0; i < program->nb_lines; i++) {
		c = program->listing[i];
		
		uint64_t addr = virt_addr_t_to_uint64_t(&c.vaddr);
		
		if(c.order == READ) {
			if (c.type == INSTRUCTION) {
				fprintf(output, "R I @0x%016" PRIX64, addr);
			}
			else {
				if (c.data_size == sizeof(word_t)) {
					fprintf(output, "R DW @0x%016" PRIX64, addr);
				}
				else {
					fprintf(output, "R DB @0x%016" PRIX64, addr);
				}
			}
		}
		else {
			if (c.data_size == sizeof(word_t)) {
				fprintf(output, "R DW 0x%08" PRIX32 " @0x%016" PRIX64, c.write_data, addr);
			}
			else {
				fprintf(output, "R DB 0x%02" PRIX32 " @0x%016" PRIX64, c.write_data, addr);
			}
			
		}
		fprintf(output, "\n");	
	}
	
	return ERR_NONE;
}

/**
 * @brief Read a program (list of commands) from a file.
 * @param filename the name of the file to read from.
 * @param program the program to be filled from file.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
 
#define MAX_COMMAND_LENGTH 36
#define MAX_COMMAND_WORD_LENGTH 19 //"@0x" prefix, followed by 16 HEX digits = 19 characters

int program_read(const char* filename, program_t* program){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(program);
	
	FILE* input = fopen(filename, "r");
	M_REQUIRE_NON_NULL_CUSTOM_ERR(input, ERR_IO);
	
	char command_str[MAX_COMMAND_LENGTH+1];
	
	program_init(program);
	
	do{
		//Read one line of command
		M_EXIT_IF_ERR(read_command_line(input, command_str, MAX_COMMAND_LENGTH+1), "Error reading one line of command from input file");
		
		//Make it a command_t
		command_t c;
		(void)memset(&c, 0, sizeof(c));
		
		
		unsigned int index = 0;
		char word_read[MAX_COMMAND_WORD_LENGTH+1]; //+1 accounts for '\0' in last position
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_order(c, word_read), "Error trying to parse order");
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_type(c, word_read), "Error trying to parse type");
		
		if(c.type == DATA){
			M_EXIT_IF_ERR(parse_size(c, word_read), "Error trying to parse size");
		}
		
		if(c.order == WRITE){
			M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
			M_EXIT_IF_ERR(parse_data(c, word_read), "Error trying to parse data");
		}
		
		M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH+1, &index), "Error trying to parse instruction");
		M_EXIT_IF_ERR(parse_address(c, word_read), "Error trying to parse address");
		
		
		//TODO : Write parse_xx
		//NOTE : In parse_xx, always check given word has some expected length 
		//
		//size_t word_len = strlen(word_read);
		//M_EXIT_IF(word_len != someValue, ERR_BAD_PARAMETER, "Bad instruction format : some nice explaining message");
		
		
		//Note : the above only parses the input. It doesn't check in any way that the input makes sense
		//Only that it has a somewhat "legal" structure (to be parseable)
		//These checks should be donne in program_add_command
		M_EXIT_IF_ERR(program_add_command(program, &c), "Error adding command to program");
		
	} while(!feof(input) && !ferror(input));
	
	fclose(input);
	
	return ERR_NONE;
}

//TODO : should we verify args in auxilary functions ? (see MOODLE forum)
//TODO : Make every argument we can const ?

/**
 * @brief Read one line of command (a string) from given input file
 * @param input the input file (list of commands)
 * @param str (modified) the command read
 * @param str_len length of str char array
 * @return ERR_NONE if ok, appropriate error code otherwise (ERR_IO)
 */
int read_command_line(FILE* input, char* str, size_t str_len){
	fgets(str, str_len, input);
	
	int len_read = strlen(str) - 1;
	
	M_EXIT_IF(len_read < 1, ERR_IO, "Found line with only a '\n' in program input file");
	
	if(len_read >= 0 && str[len_read] == '\n'){  //strip the '\n' off
		str[len_read] = '\0';
	}
	
	return ERR_NONE;
}

/**
 * @brief Strips one word from str, starting from index (incl.)
 * 		Possibly drops (leading) spaces
 * @param str string to read one word from
 * @param read (modified) word read
 * @param index (modified) start index, modified to index of char directly following word read
 * @return ERR_NONE if ok, appropriate error code otherwise
 */
int next_word(char* str, char* read, size_t read_len, unsigned int* index){
	//TODO : TEST THIS FUNCTION on empty string, or just one char string, or just one "space" char string, etc
	// if this works, the above probably works too (and inversely !)
	int str_len = strlen(str);
	
	//"Drop" leading spaces, if any
	while(*index < str_len && isspace(str[*index])){
		(*index)++;
	}
	
	M_EXIT_IF(*index == str_len, ERR_BAD_PARAMETER, "Reached end of string, but expected more (presumably wrong command format)");
	
	//Read until next space
	size_t read_nb = 0;
	char nextChar;
	do{
		M_EXIT_IF(read_nb == read_len-1, ERR_BAD_PARAMETER, "Given read char array is too small to hold next word of instruction");
		
		nextChar = str[*index];
		read[read_nb] = nextChar;
		read_nb++;
		(*index)++;
	}while(*index < str_len && isspace(nextChar));
	
	read[read_nb] = '\0';
	
	return ERR_NONE;
}


//TODO : remove
// I leave what's below only to serve as inspiration to write the parse_xx functions
// and as a reminder to always catch unexpected values. parse_xx functions should not be nested though,
//so should be a lot simpler
/*
int extract_read_command(command_t* c, char* command_str){
	c->order = READ;
	unsigned int index = 1; //put it right after the first char, as reading it with next_word would have done
	char word_read[MAX_COMMAND_WORD_LENGTH+1]; //+1 accounts for '\0' in last position
	
	M_EXIT_IF_ERR(next_word(command_str, word_read, MAX_COMMAND_WORD_LENGTH, &index), "Error trying to parse read instruction");
	
	size_t word_len = strlen(word_read);

	
	if(type_char == 'I'){
		c->type = INSTRUCTION;
		//TODO : other stuff
	}else if(type_char == 'D'){
		
		M_EXIT_IF(word_len < 2, ERR_BAD_PARAMETER, "Data size not specified");
		
		char size_char = word_read[1];
		c->type = DATA;
		
		if(size_char == 'B'){
			c->size = 1;
			//TODO : read next_word, should be (at most) a byte
		}else if(size_char == 'W'){
			c->size = 4;
			//TODO : read next_word, should be (at most) a word (but could be shorter...)
		}else{
			M_EXIT_ERR(ERR_BAD_PARAMETER, "size character was neither 'B' (Byte) nor 'W' (Word)");
		}
	}else{
		M_EXIT_ERR(ERR_BAD_PARAMETER, "type character was neither 'I' (Instruction) nor 'D' (Data)");
	}
	
	return ERR_NONE;
}
*/
