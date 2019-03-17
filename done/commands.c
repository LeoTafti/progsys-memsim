/**
 * @file commands.c
 * @brief Processor commands(/instructions) simulation
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <stdio.h>
#include <inttypes.h> // TODO

#include "commands.h"
#include "addr_mng.h"
#include "mem_access.h"
#include "addr.h"
#include "error.h"

#define BYTE_MAX_VALUE 0xFF
#define WORD_MAX_VALUE 0xFFFFFFFF // TODO is unsigned?

//TODO : general remark – do we keep the layout with ugly //---------------// between functions ?
//		 (standardize code written in Week 4)

int program_init(program_t* program) {
	M_REQUIRE_NON_NULL(program);   

	// init listing
	for (int i = 0; i < MAX_COMMANDS; i++) {
		// TODO how to modify pointer value?
		program->listing[i] = 0;
	}
	//init nb_lines
	program->nb_lines = 0;
	//init allocated
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
	
	command_t* c;
	for(int i = 0; i < program->nb_lines; i++) {
		c = program->listing[i];
		
		uint64_t addr = virt_addr_t_to_uint64_t(c->vaddr);
		
		if(c->order == READ) {
			if (c->type == INSTRUCTION) {
				fprintf(output, "R I @0x%016" PRIX64, addr);
			}
			else {
				if (c->data_size == sizeof(word_t)) {
					fprintf(output, "R DW @0x%016" PRIX64, addr);
				}
				else {
					fprintf(output, "R DB @0x%016" PRIX64, addr);
				}
			}
		}
		else {
			if (c->data_size == sizeof(word_t)) {
				fprintf(output, "R DW 0x%08" PRIX32 " @0x%016" PRIX64, c->write_data, addr);
			}
			else {
				fprintf(output, "R DB 0x%02" PRIX32 " @0x%016" PRIX64, c->write_data, addr);
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
int program_read(const char* filename, program_t* program){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(program);
	
	return ERR_NONE;
}
