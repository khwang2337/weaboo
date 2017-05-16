/*========== my_main.c ==========

  This is the only file you need to modify in order
  to get a working mdl project (for now).

  my_main.c will serve as the interpreter for mdl.
  When an mdl script goes through a lexer and parser, 
  the resulting operations will be in the array op[].

  Your job is to go through each entry in op and perform
  the required action from the list below:

  push: push a new origin matrix onto the origin stack
  pop: remove the top matrix on the origin stack

  move/scale/rotate: create a transformation matrix 
                     based on the provided values, then 
		     multiply the current top of the
		     origins stack by it.

  box/sphere/torus: create a solid object based on the
                    provided values. Store that in a 
		    temporary matrix, multiply it by the
		    current top of the origins stack, then
		    call draw_polygons.

  line: create a line based on the provided values. Store 
        that in a temporary matrix, multiply it by the
	current top of the origins stack, then call draw_lines.

  save: call save_extension with the provided filename

  display: view the image live
  
  jdyrlandweaver
  =========================*/

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "parser.h"
#include "symtab.h"
#include "y.tab.h"

#include "matrix.h"
#include "ml6.h"
#include "display.h"
#include "draw.h"
#include "stack.h"

/*======== void first_pass() ==========
  Inputs:   
  Returns: 

  Checks the op array for any animation commands
  (frames, basename, vary)
  
  Should set num_frames and basename if the frames 
  or basename commands are present

  If vary is found, but frames is not, the entire
  program should exit.

  If frames is found, but basename is not, set name
  to some default value, and print out a message
  with the name being used.

  jdyrlandweaver
  ====================*/
void first_pass() {
  //in order to use name and num_frames
  //they must be extern variables
  extern int num_frames;
  extern char name[128]; 
	
	int i;
	char vary_exist = 0, basename_exist = 0, frames_set = 0;
	
	for (i = 0; i < lastop; i++) {
		switch (op[i].opcode) {
			case BASENAME:
				basename_exist = 1;
				strcpy(name, op[i].op.basename.p->name);
				break;
			
			case VARY:
				vary_exist = 1;
				break;
			
			case FRAMES:
				frames_set = 1;
				num_frames = op[i].op.frames.num_frames;
				break;
		}
	}
	
	if (vary_exist && ! frames_set) {
		printf("first_pass error: vary without set frames\n");
		exit(0);
	}
	
	if (frames_set && ! basename_exist) {
		printf("first pass warning: frames set without basename\n    basename is by default set to \"bravely\"\n");
		strcpy(name, "bravely");
	}

  return;
}

/*======== struct vary_node ** second_pass() ==========
  Inputs:   
  Returns: An array of vary_node linked lists

  In order to set the knobs for animation, we need to keep
  a separate value for each knob for each frame. We can do
  this by using an array of linked lists. Each array index
  will correspond to a frame (eg. knobs[0] would be the first
  frame, knobs[2] would be the 3rd frame and so on).

  Each index should contain a linked list of vary_nodes, each
  node contains a knob name, a value, and a pointer to the
  next node.

  Go through the opcode array, and when you find vary, go 
  from knobs[0] to knobs[frames-1] and add (or modify) the
  vary_node corresponding to the given knob with the
  appropirate value. 

  jdyrlandweaver
  ====================*/
struct vary_node ** second_pass() {
	
	int i, curr_f;
	struct vary_node ** knobs = calloc(MAX_SYMBOLS, sizeof(struct vary_node *));
	
	double start_f, start_v, end_f, end_v, slope;
	start_f = op[i].op.vary.start_frame;
	start_v = op[i].op.vary.start_val;
	end_f = op[i].op.vary.end_frame;
	end_v = op[i].op.vary.end_val;
	slope = (end_v - start_v) / (end_f - start_f);
	
	
	for (i = 0; i < lastop; i++) {
		switch (op[i].opcode) {
			//knobs[i]->next = NULL; //CALLOC INTIALIZES NULL THANK GOD
			case VARY:
				for (curr_f = 0; curr_f < num_frames; curr_f++) {
					struct vary_node * tmp;
					strcpy(tmp->name, op[i].op.vary.p->name);
					//if (curr_f >= op[i].op.vary.start_frame && curr_f <= op[i].op.vary.end_frame) tmp->value = ( (op[i].op.vary.end_val - op[i].op.vary.start_val) / (op[i].op.vary.end_frame - op[i].op.vary.start_frame) ) * (curr_f - op[i].op.vary.start_frame) + op[i].op.vary.start_val;
					if (curr_f >= start_f && curr_f <= end_f) tmp->value = start_v + slope * (curr_f - start_f);
					else tmp->value = 0;
					tmp->next = knobs[curr_f];
					knobs[curr_f] = tmp;
				}
				break;
		}
	}
  return NULL;
}


/*======== void print_knobs() ==========
Inputs:   
Returns: 

Goes through symtab and display all the knobs and their
currnt values

jdyrlandweaver
====================*/
void print_knobs() {
  
  int i;

  printf( "ID\tNAME\t\tTYPE\t\tVALUE\n" );
  for ( i=0; i < lastsym; i++ ) {

    if ( symtab[i].type == SYM_VALUE ) {
      printf( "%d\t%s\t\t", i, symtab[i].name );

      printf( "SYM_VALUE\t");
      printf( "%6.2f\n", symtab[i].s.value);
    }
  }
}


/*======== void my_main() ==========
  Inputs: 
  Returns: 

  This is the main engine of the interpreter, it should
  handle most of the commadns in mdl.

  If frames is not present in the source (and therefore 
  num_frames is 1, then process_knobs should be called.

  If frames is present, the enitre op array must be
  applied frames time. At the end of each frame iteration
  save the current screen to a file named the
  provided basename plus a numeric string such that the
  files will be listed in order, then clear the screen and
  reset any other data structures that need it.

  Important note: you cannot just name your files in 
  regular sequence, like pic0, pic1, pic2, pic3... if that
  is done, then pic1, pic10, pic11... will come before pic2
  and so on. In order to keep things clear, add leading 0s
  to the numeric portion of the name. If you use sprintf, 
  you can use "%0xd" for this purpose. It will add at most
  x 0s in front of a number, if needed, so if used correctly,
  and x = 4, you would get numbers like 0001, 0002, 0011,
  0487

  jdyrlandweaver
  ====================*/
/*  
void process_knobs() {
	int i;
  struct matrix *tmp;
  struct stack *s;
  screen t;
  color g;
  double step = 0.1;
  
  s = new_stack();
  tmp = new_matrix(4, 1000);
  clear_screen( t );

  for (i=0;i<lastop;i++) {  
    switch (op[i].opcode) {
      
      case PUSH:
        push(s);
        break;
      
      case POP:
        pop(s);
        break;
        
      case MOVE:
        tmp = make_translate(op[i].op.move.d[0], op[i].op.move.d[1], op[i].op.move.d[2]);
        matrix_mult(peek(s), tmp);
        copy_matrix(tmp, peek(s));
        tmp->lastcol = 0;
        break;
      
      case ROTATE:
        if (op[i].op.rotate.axis == 0) tmp = make_rotX(op[i].op.rotate.degrees * M_PI / 180);
        else if (op[i].op.rotate.axis == 1) tmp = make_rotY(op[i].op.rotate.degrees * M_PI / 180);
        else if (op[i].op.rotate.axis == 2) tmp = make_rotZ(op[i].op.rotate.degrees * M_PI / 180);
        
        matrix_mult(peek(s), tmp);
        copy_matrix(tmp, peek(s));
        tmp->lastcol = 0;
        break;
      
      case SCALE:
        tmp = make_scale(op[i].op.scale.d[0], op[i].op.scale.d[1], op[i].op.scale.d[2]);
        matrix_mult(peek(s), tmp);
        copy_matrix(tmp, peek(s));
        tmp->lastcol = 0;
        break;
      
      case BOX:
        add_box(tmp, op[i].op.box.d0[0], op[i].op.box.d0[1], op[i].op.box.d0[2], op[i].op.box.d1[0], op[i].op.box.d1[1], op[i].op.box.d1[2]);
        matrix_mult(peek(s), tmp);
        draw_polygons(tmp, t, g);
        tmp->lastcol = 0;
        break;
      
      case SPHERE:
        add_sphere(tmp, op[i].op.sphere.d[0], op[i].op.sphere.d[1], op[i].op.sphere.d[2], op[i].op.sphere.r, step);
        matrix_mult(peek(s), tmp);
        draw_polygons(tmp, t, g);
        tmp->lastcol = 0;
        break;
      
      case TORUS:
        add_torus(tmp, op[i].op.torus.d[0], op[i].op.torus.d[1], op[i].op.torus.d[2], op[i].op.torus.r0, op[i].op.torus.r1, step);
        matrix_mult(peek(s), tmp);
        draw_polygons(tmp, t, g);
        tmp->lastcol = 0;
        break;
      
      case LINE:
        add_edge(tmp, op[i].op.line.p0[0], op[i].op.line.p0[1], op[i].op.line.p0[2], op[i].op.line.p1[0], op[i].op.line.p1[1], op[i].op.line.p1[2]);
        matrix_mult(peek(s), tmp);
        draw_lines(tmp, t, g);
        tmp->lastcol = 0;
        break;
        
      case SAVE:
        save_extension(t, op[i].op.save.p->name);
        break;
      
      case DISPLAY:
        display(t);
        break;
    }
  }
}
*/ //for when num_frames is 1
void my_main() {

  int i, c;
  struct matrix *tmp;
  struct stack *systems;
  struct vary_node * curr_v
  screen t;
  color g;
  double step = 0.1;
  double theta;
  
  systems = new_stack();
  tmp = new_matrix(4, 1000);
  clear_screen( t );
  g.red = 0;
  g.green = 0;
  g.blue = 0;
	
	for (c = 0; c < num_frames; c++) {
		
		curr_v = knobs[c];
		while (curr_v->next != NULL) {
			set_value(lookup_symbol(curr_v->name), curr_v->value);
			curr_v = curr_v->next;
		}
		
	  for (i = 0; i < lastop; i++) {
			printf("%d: ",i);
	    switch (op[i].opcode) {
				case SPHERE:
				  printf("Sphere: %6.2f %6.2f %6.2f r=%6.2f",
					 op[i].op.sphere.d[0],op[i].op.sphere.d[1],
					 op[i].op.sphere.d[2],
					 op[i].op.sphere.r);
					 
				  if (op[i].op.sphere.constants != NULL) { 
				  	printf("\tconstants: %s",op[i].op.sphere.constants->name); 
				  }
				  
			  	add_sphere(tmp, op[i].op.sphere.d[0],
				     op[i].op.sphere.d[1],
				     op[i].op.sphere.d[2],
				     op[i].op.sphere.r, step);	
				     
				  if (op[i].op.sphere.cs != NULL) {
				  	scalar_mult(op[i].op.sphere.cs->value, tmp);
				  }
				  matrix_mult( peek(systems), tmp );
				  draw_polygons(tmp, t, g);
				  tmp->lastcol = 0;
				  break;
				  
				case TORUS:
				  printf("Torus: %6.2f %6.2f %6.2f r0=%6.2f r1=%6.2f",
					 op[i].op.torus.d[0],op[i].op.torus.d[1],
					 op[i].op.torus.d[2],
					 op[i].op.torus.r0,op[i].op.torus.r1);
					 
				  if (op[i].op.torus.constants != NULL) {
				  	printf("\tconstants: %s",op[i].op.torus.constants->name);
				  }
				  
				  add_torus(tmp,
					    op[i].op.torus.d[0],
					    op[i].op.torus.d[1],
					    op[i].op.torus.d[2],
					    op[i].op.torus.r0,op[i].op.torus.r1, step);
					    
					if (op[i].op.torus.cs != NULL) {
				  	scalar_mult(op[i].op.torus.cs->value, tmp);
				  }
				  matrix_mult( peek(systems), tmp );
				  draw_polygons(tmp, t, g);
				  tmp->lastcol = 0;	  
				  break;
				  
				case BOX:
				  printf("Box: d0: %6.2f %6.2f %6.2f d1: %6.2f %6.2f %6.2f",
					 op[i].op.box.d0[0],op[i].op.box.d0[1],
					 op[i].op.box.d0[2],
					 op[i].op.box.d1[0],op[i].op.box.d1[1],
					 op[i].op.box.d1[2]);
					 
				  if (op[i].op.box.constants != NULL) {
				  	printf("\tconstants: %s",op[i].op.box.constants->name);
				  }
				  
				  add_box(tmp,
					  op[i].op.box.d0[0],op[i].op.box.d0[1],
					  op[i].op.box.d0[2],
					  op[i].op.box.d1[0],op[i].op.box.d1[1],
					  op[i].op.box.d1[2]);
					
					if (op[i].op.box.cs != NULL) {
				  	scalar_mult(op[i].op.box.cs->value, tmp);
				  }
				  matrix_mult( peek(systems), tmp );
				  draw_polygons(tmp, t, g);
				  tmp->lastcol = 0;
				  break;
				  
				case LINE:
				  printf("Line: from: %6.2f %6.2f %6.2f to: %6.2f %6.2f %6.2f",
					 op[i].op.line.p0[0],op[i].op.line.p0[1],
					 op[i].op.line.p0[1],
					 op[i].op.line.p1[0],op[i].op.line.p1[1],
					 op[i].op.line.p1[1]);
				  
				  if (op[i].op.line.constants != NULL) {
				  	printf("\n\tConstants: %s",op[i].op.line.constants->name);
				  }
				  
				  add_point(tmp, op[i].op.line.p0[0], op[i].op.line.p0[1], op[i].op.line.p0[2]);
				  if (op[i].op.line.cs0 != NULL) scalar_mult(op[i].op.line.cs0->value, tmp);
				  
				  if (op[i].op.line.cs1 != NULL) add_point(tmp, op[i].op.line.p1[0]*op[i].op.line.cs1->value, op[i].op.line.p1[1]*op[i].op.line.cs1->value, op[i].op.line.p1[2]*op[i].op.line.cs1->value);
				  else add_point(tmp, op[i].op.line.p1[0], op[i].op.line.p1[1], op[i].op.line.p1[2])
				  
				  draw_lines(tmp, t, g);
				  tmp->lastcol = 0;
				  break;
				  
				case MOVE:
				  printf("Move: %6.2f %6.2f %6.2f",
					 op[i].op.move.d[0],op[i].op.move.d[1],
					 op[i].op.move.d[2]);

				  tmp = make_translate( op[i].op.move.d[0],
							op[i].op.move.d[1],
							op[i].op.move.d[2]);
					
					if (op[i].op.move.p != NULL) {
				  	scalar_mult(op[i].op.move.p->value, tmp);
				  }
				  matrix_mult(peek(systems), tmp);
				  copy_matrix(tmp, peek(systems));
				  tmp->lastcol = 0;
				  break;
				  
				case SCALE:
				  printf("Scale: %6.2f %6.2f %6.2f",
					 op[i].op.scale.d[0],op[i].op.scale.d[1],
					 op[i].op.scale.d[2]);

				  tmp = make_scale( op[i].op.scale.d[0],
						    op[i].op.scale.d[1],
						    op[i].op.scale.d[2]);
						    
					if (op[i].op.scale.p != NULL) {
				  	scalar_mult(op[i].op.scale.p->value, tmp);
				  }
				  matrix_mult(peek(systems), tmp);
				  copy_matrix(tmp, peek(systems));
				  tmp->lastcol = 0;
				  break;
				  
				case ROTATE:
				  printf("Rotate: axis: %6.2f degrees: %6.2f",
					 op[i].op.rotate.axis,
					 op[i].op.rotate.degrees);

				  theta =  op[i].op.rotate.degrees * (M_PI / 180);
				  if (op[i].op.rotate.axis == 0 )
				    tmp = make_rotX( theta );
				  else if (op[i].op.rotate.axis == 1 )
				    tmp = make_rotY( theta );
				  else
				    tmp = make_rotZ( theta );
				  
				  if (op[i].op.rotate.p != NULL) {
				  	scalar_mult(op[i].op.rotate.p->value, tmp);
				  }
				  matrix_mult(peek(systems), tmp);
				  copy_matrix(tmp, peek(systems));
				  tmp->lastcol = 0;
				  break;
				  
				case PUSH:
				  printf("Push");
				  push(systems);
				  break;
				case POP:
				  printf("Pop");
				  pop(systems);
				  break;
				case SAVE:
				  printf("Save: %s",op[i].op.save.p->name);
				  save_extension(t, op[i].op.save.p->name);
				  break;
				case DISPLAY:
				  printf("Display");
				  display(t);
				  break;
	  	}
	      printf("\n");
	    }
	}
}
